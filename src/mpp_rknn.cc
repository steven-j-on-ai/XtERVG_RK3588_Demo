// Copyright (c) 2023 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*-------------------------------------------
								Includes
-------------------------------------------*/
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"

#include "rk3588.h"

static int frame_index = 0;

extern long g_start_vts;

typedef struct
{
	int width;
	int height;
	int width_stride;
	int height_stride;
	int format;
	char *virt_addr;
	int fd;
} image_frame_t;

#ifdef __cplusplus
	extern "C" {
#endif
long getTimeMsec(void);
void print_bin_data(uint8_t *data, int len);
#ifdef __cplusplus
	}
#endif
void send_xftp_frame(uint8_t *buffer, int len, uint32_t timestamp);
void send_script_frame(detect_result_t *results, int count, uint32_t timestamp);

/*-------------------------------------------
									Functions
-------------------------------------------*/
static void dump_tensor_attr(rknn_tensor_attr *attr)
{
	printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
				"zp=%d, scale=%f\n",
				attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
				attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
				get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz)
{
	unsigned char *data;
	int ret;

	data = NULL;

	if (NULL == fp)
	{
		return NULL;
	}

	ret = fseek(fp, ofst, SEEK_SET);
	if (ret != 0)
	{
		printf("blob seek failure.\n");
		return NULL;
	}

	data = (unsigned char *)malloc(sz);
	if (data == NULL)
	{
		printf("buffer malloc failure.\n");
		return NULL;
	}
	ret = fread(data, 1, sz, fp);
	return data;
}

static unsigned char *read_file_data(const char *filename, int *model_size)
{
	FILE *fp;
	unsigned char *data;

	fp = fopen(filename, "rb");
	if (NULL == fp)
	{
		printf("Open file %s failed.\n", filename);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);

	data = load_data(fp, 0, size);

	fclose(fp);

	*model_size = size;
	return data;
}

static int init_model(const char *model_path, rknn_app_context_t *app_ctx)
{
	int ret;
	rknn_context ctx;

	/* Create the neural network */
	printf("Loading mode...\n");
	int model_data_size = 0;
	unsigned char *model_data = read_file_data(model_path, &model_data_size);
	if (model_data == NULL)
	{
		return -1;
	}

	ret = rknn_init(&ctx, model_data, model_data_size, 0, NULL);
	if (ret < 0)
	{
		printf("rknn_init error ret=%d\n", ret);
		return -1;
	}

	if (model_data)
	{
		free(model_data);
	}

	rknn_sdk_version version;
	ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
	if (ret < 0)
	{
		printf("rknn_query RKNN_QUERY_SDK_VERSION error ret=%d\n", ret);
		return -1;
	}
	printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);

	ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &app_ctx->io_num, sizeof(rknn_input_output_num));
	if (ret < 0)
	{
		printf("rknn_query RKNN_QUERY_IN_OUT_NUM error ret=%d\n", ret);
		return -1;
	}
	printf("model input num: %d, output num: %d\n", app_ctx->io_num.n_input, app_ctx->io_num.n_output);

	rknn_tensor_attr *input_attrs = (rknn_tensor_attr *)malloc(app_ctx->io_num.n_input * sizeof(rknn_tensor_attr));
	memset(input_attrs, 0, app_ctx->io_num.n_input * sizeof(rknn_tensor_attr));
	for (int i = 0; i < app_ctx->io_num.n_input; i++)
	{
		input_attrs[i].index = i;
		ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
		if (ret < 0)
		{
			printf("rknn_query RKNN_QUERY_INPUT_ATTR error ret=%d\n", ret);
			return -1;
		}
		dump_tensor_attr(&(input_attrs[i]));
	}

	rknn_tensor_attr *output_attrs = (rknn_tensor_attr *)malloc(app_ctx->io_num.n_output * sizeof(rknn_tensor_attr));
	memset(output_attrs, 0, app_ctx->io_num.n_output * sizeof(rknn_tensor_attr));
	for (int i = 0; i < app_ctx->io_num.n_output; i++)
	{
		output_attrs[i].index = i;
		ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
		if (ret < 0)
		{
			printf("rknn_query RKNN_QUERY_OUTPUT_ATTR error ret=%d\n", ret);
			return -1;
		}
		dump_tensor_attr(&(output_attrs[i]));
	}

	app_ctx->input_attrs = input_attrs;
	app_ctx->output_attrs = output_attrs;
	app_ctx->rknn_ctx = ctx;

	if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
	{
		printf("model is NCHW input fmt\n");
		app_ctx->model_channel = input_attrs[0].dims[1];
		app_ctx->model_height = input_attrs[0].dims[2];
		app_ctx->model_width = input_attrs[0].dims[3];
	}
	else
	{
		printf("model is NHWC input fmt\n");
		app_ctx->model_height = input_attrs[0].dims[1];
		app_ctx->model_width = input_attrs[0].dims[2];
		app_ctx->model_channel = input_attrs[0].dims[3];
	}
	printf("model input height=%d, width=%d, channel=%d\n", app_ctx->model_height, app_ctx->model_width, app_ctx->model_channel);

	return 0;
}

static int release_model(rknn_app_context_t *app_ctx)
{
	if (app_ctx->rknn_ctx != 0)
	{
		rknn_destroy(app_ctx->rknn_ctx);
	}
	free(app_ctx->input_attrs);
	free(app_ctx->output_attrs);
	deinitPostProcess();
	return 0;
}

static int inference_model(rknn_app_context_t *app_ctx, image_frame_t *img, detect_result_group_t *detect_result)
{
	int ret;
	rknn_context ctx = app_ctx->rknn_ctx;
	int model_width = app_ctx->model_width;
	int model_height = app_ctx->model_height;
	int model_channel = app_ctx->model_channel;

	const float nms_threshold = NMS_THRESH;
	const float box_conf_threshold = BOX_THRESH;
	// You may not need resize when src resulotion equals to dst resulotion
	void *resize_buf = nullptr;
	// init rga context
	rga_buffer_t src;
	rga_buffer_t dst;
	im_rect src_rect;
	im_rect dst_rect;
	memset(&src_rect, 0, sizeof(src_rect));
	memset(&dst_rect, 0, sizeof(dst_rect));
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));

	// fprintf(stderr, "[inference_model] input image %dx%d stride %dx%d format=%d\n", img->width, img->height, img->width_stride, img->height_stride, img->format);

	float scale_w = (float)model_width / img->width;
	float scale_h = (float)model_height / img->height;

	rknn_input inputs[1];
	memset(inputs, 0, sizeof(inputs));
	inputs[0].index = 0;
	inputs[0].type = RKNN_TENSOR_UINT8;
	inputs[0].size = model_width * model_height * model_channel;
	inputs[0].fmt = RKNN_TENSOR_NHWC;
	inputs[0].pass_through = 0;

	// fprintf(stderr, "[inference_model] resize with RGA!\n");
	resize_buf = malloc(model_width * model_height * model_channel);
	memset(resize_buf, 0, model_width * model_height * model_channel);

	src = wrapbuffer_virtualaddr((void *)img->virt_addr, img->width, img->height, img->format, img->width_stride, img->height_stride);
	dst = wrapbuffer_virtualaddr((void *)resize_buf, model_width, model_height, RK_FORMAT_RGB_888);
	ret = imcheck(src, dst, src_rect, dst_rect);
	if (IM_STATUS_NOERROR != ret)
	{
		fprintf(stderr, "[inference_model] %d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
		return -1;
	}
	IM_STATUS STATUS = imresize(src, dst);

	inputs[0].buf = resize_buf;

	rknn_inputs_set(ctx, app_ctx->io_num.n_input, inputs);

	rknn_output outputs[app_ctx->io_num.n_output];
	memset(outputs, 0, sizeof(outputs));
	for (int i = 0; i < app_ctx->io_num.n_output; i++)
	{
		outputs[i].want_float = 0;
	}

	ret = rknn_run(ctx, NULL);
	ret = rknn_outputs_get(ctx, app_ctx->io_num.n_output, outputs, NULL);
	// fprintf(stderr, "[inference_model] post process config: box_conf_threshold = %.2f, nms_threshold = %.2f\n", box_conf_threshold, nms_threshold);

	std::vector<float> out_scales;
	std::vector<int32_t> out_zps;
	for (int i = 0; i < app_ctx->io_num.n_output; ++i)
	{
		out_scales.push_back(app_ctx->output_attrs[i].scale);
		out_zps.push_back(app_ctx->output_attrs[i].zp);
	}
	BOX_RECT pads;
	memset(&pads, 0, sizeof(BOX_RECT));

	post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf, model_height, model_width,
							 box_conf_threshold, nms_threshold, pads, scale_w, scale_h, out_zps, out_scales, detect_result);
	ret = rknn_outputs_release(ctx, app_ctx->io_num.n_output, outputs);

	if (resize_buf)
	{
		free(resize_buf);
	}
	return 0;
}

void mpp_decoder_frame_callback(void *userdata, int width_stride, int height_stride, int width, int height, int format, int fd, void *data)
{
	int ret = 0;
	image_frame_t img;
	uint32_t timestamp = 0;
	detect_result_group_t detect_result;
	rknn_app_context_t *ctx = (rknn_app_context_t *)userdata;

	char *enc_data;
	int enc_buf_size;
	void *mpp_frame = NULL;
	int enc_data_size;

	if (ctx->is_rate) {
		int mpp_frame_fd = 0;
		rga_buffer_t origin;
		rga_buffer_t src;
		if (ctx->encoder == NULL) {
			MppEncoder *mpp_encoder = new MppEncoder();
			MppEncoderParams enc_params;
			memset(&enc_params, 0, sizeof(MppEncoderParams));
			enc_params.width = width;
			enc_params.height = height;
			enc_params.hor_stride = width_stride;
			enc_params.ver_stride = height_stride;
			enc_params.fmt = MPP_FMT_YUV420SP;
			// enc_params.type = MPP_VIDEO_CodingHEVC;
			// Note: rk3562只能支持h264格式的视频流
			enc_params.type = MPP_VIDEO_CodingAVC;
			mpp_encoder->Init(enc_params, NULL);
			ctx->encoder = mpp_encoder;
		}

		enc_buf_size = ctx->encoder->GetFrameSize();
		enc_data = (char *)malloc(enc_buf_size);
		mpp_frame = ctx->encoder->GetInputFrameBuffer();
		mpp_frame_fd = ctx->encoder->GetInputFrameBufferFd(mpp_frame);
		origin = wrapbuffer_fd(fd, width, height, RK_FORMAT_YCbCr_420_SP, width_stride, height_stride);
		src = wrapbuffer_fd(mpp_frame_fd, width, height, RK_FORMAT_YCbCr_420_SP, width_stride, height_stride);
		imcopy(origin, src);
	}

	img.width = width;
	img.height = height;
	img.width_stride = width_stride;
	img.height_stride = height_stride;
	img.fd = fd;
	img.virt_addr = (char *)data;
	img.format = RK_FORMAT_YCbCr_420_SP;

	memset(&detect_result, 0, sizeof(detect_result_group_t));
	ret = inference_model(ctx, &img, &detect_result);
	if (ret != 0) {
		fprintf(stderr, "inference model fail\n");
		goto RET;
	}
	if (ctx->is_rate) {
		timestamp = getTimeMsec() - g_start_vts;
	}
	send_script_frame(detect_result.results, detect_result.count, timestamp);

	if (ctx->is_rate) {
		memset(enc_data, 0, enc_buf_size);
		enc_data_size = ctx->encoder->Encode(mpp_frame, enc_data, enc_buf_size);
		send_xftp_frame((uint8_t *)enc_data, enc_data_size, timestamp);
		// print_bin_data((uint8_t *)enc_data, 64);
	}

RET:
	if (ctx->is_rate) {
		if (enc_data != nullptr) {
			free(enc_data);
		}
	}
}

void *get_frame_and_send_script(void *arg)
{
	rknn_app_context_t *ctx = (rknn_app_context_t *)arg;
	ctx->decoder->GetFrame();
	fprintf(stderr, "[get_frame_and_send_script] thread end ...... \n");
	return NULL;
}
int start_get_frame_and_send_script_thread(rknn_app_context_t *ctx)
{
	pthread_t pid;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&pid, &attr, get_frame_and_send_script, ctx) != 0) {
		fprintf(stderr, "[start_get_frame_and_send_script_thread] pthread_create get_frame_and_send_script failed return -1\n");
		return -1;
	}
	pthread_attr_destroy(&attr);
	fprintf(stderr, "[start_get_frame_and_send_script_thread] pthread_create start ...... \n");
	return 0;
}
int init_rk3588(rknn_app_context_t *ctx, const char *model_name, int video_type, int is_rate)
{
	int ret;

	if (!ctx || !model_name || video_type != 264) {
		fprintf(stderr, "[init_rk3588] parameters err !\n");
		return -1;
	}
	memset(ctx, 0, sizeof(rknn_app_context_t));
	ret = init_model(model_name, ctx);
	if (ret) {
		fprintf(stderr, "[init_rk3588] init model fail\n");
		return -2;
	}

	ctx->is_rate = is_rate;
	if (!ctx->decoder) {
		MppDecoder *decoder = new MppDecoder();
		decoder->Init(video_type, 30, ctx);
		decoder->SetCallback(mpp_decoder_frame_callback);
		ctx->decoder = decoder;
		start_get_frame_and_send_script_thread(ctx);
	}
	fprintf(stderr, "ctx = %p decoder = %p\n", ctx, ctx->decoder);

	return 0;
}
int deinit_rk3588(rknn_app_context_t *ctx)
{
	if (!ctx) {
		fprintf(stderr, "[deinit_rk3588] parameters err !\n");
		return -1;
	}
	if (ctx->decoder != nullptr) {
		ctx->decoder->StopGetFrame();
		delete (ctx->decoder);
		ctx->decoder = nullptr;
	}
	if (ctx->encoder != nullptr) {
		delete (ctx->encoder);
		ctx->encoder = nullptr;
	}

	release_model(ctx);

	frame_index = 0;

	return 0;
}
