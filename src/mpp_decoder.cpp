#include <stdio.h>
#include <sys/time.h>

#include "mpp_decoder.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

#define LOGD fprintf
// #define LOGD

int g_is_push = 1;

static unsigned long GetCurrentTimeMS() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec*1000+tv.tv_usec/1000;
}

MppDecoder::MppDecoder()
{

}

MppDecoder::~MppDecoder() {
	if (loop_data.packet) {
		mpp_packet_deinit(&loop_data.packet);
		loop_data.packet = NULL;
	}
	if (frame) {
		mpp_frame_deinit(&frame);
		frame = NULL;
	}
	if (mpp_ctx) {
		mpp_destroy(mpp_ctx);
		mpp_ctx = NULL;
	}

	if (loop_data.frm_grp) {
		mpp_buffer_group_put(loop_data.frm_grp);
		loop_data.frm_grp = NULL;
	}
}

int MppDecoder::Init(int video_type, int fps, void* userdata)
{
	MPP_RET ret         = MPP_OK;
	this->userdata = userdata;
	this->fps = fps;
	this->last_frame_time_ms = 0;
	if(video_type == 264) {
		mpp_type  = MPP_VIDEO_CodingAVC;
	} else if (video_type == 265) {
		mpp_type  =MPP_VIDEO_CodingHEVC;
	} else {
		LOGD(stderr, "unsupport video_type %d", video_type);
		return -1;
	}
	LOGD(stderr, "mpi_dec_test start ");
	memset(&loop_data, 0, sizeof(loop_data));
	LOGD(stderr, "mpi_dec_test decoder test start mpp_type %d ", mpp_type);

	MppDecCfg cfg       = NULL;

	MppCtx mpp_ctx          = NULL;
	ret = mpp_create(&mpp_ctx, &mpp_mpi);
	if (MPP_OK != ret) {
		LOGD(stderr, "mpp_create failed ");
		return 0;
	}

	mpp_dec_cfg_init(&cfg);

	/* get default config from decoder context */
	ret = mpp_mpi->control(mpp_ctx, MPP_DEC_GET_CFG, cfg);
	if (ret) {
		LOGD(stderr, "%p failed to get decoder cfg ret %d ", mpp_ctx, ret);
		return -1;
	}
	ret = mpp_dec_cfg_set_u32(cfg, "base:fast_out", 1);
	if (ret) {
		LOGD(stderr, "%p failed to set fast_out ret %d ", mpp_ctx, ret);
		return -1;
	}

	/*
	 * split_parse is to enable mpp internal frame spliter when the input
	 * packet is not aplited into frames.
	 */
	ret = mpp_dec_cfg_set_u32(cfg, "base:split_parse", need_split);
	if (ret) {
		LOGD(stderr, "%p failed to set split_parse ret %d ", mpp_ctx, ret);
		return -1;
	}

	ret = mpp_mpi->control(mpp_ctx, MPP_DEC_SET_CFG, cfg);
	if (ret) {
		LOGD(stderr, "%p failed to set cfg %p ret %d ", mpp_ctx, cfg, ret);
		return -1;
	}

	mpp_dec_cfg_deinit(cfg);

	ret = mpp_init(mpp_ctx, MPP_CTX_DEC, mpp_type);
	if (ret) {
		LOGD(stderr, "%p mpp_init failed ", mpp_ctx);
		return -1;
	}

	loop_data.ctx            = mpp_ctx;
	loop_data.mpi            = mpp_mpi;
	loop_data.eos            = 0;
	loop_data.packet_size    = packet_size;
	loop_data.frame          = 0;
	loop_data.frame_count    = 0;
	loop_data.loop_end       = 0;
	return 1;
}

int MppDecoder::Reset() {
	if (mpp_mpi != NULL) {
		mpp_mpi->reset(mpp_ctx);
	}
	return 0;
}

int MppDecoder::Decode(uint8_t* pkt_data, int pkt_size, int pkt_eos)
{
	MpiDecLoopData *data = &loop_data;
	MPP_RET ret = MPP_OK;
	MppCtx ctx  = data->ctx;
	MppApi *mpi = data->mpi;

	// LOGD(stderr, "receive packet size = %d ", pkt_size);
	if (!packet) {
		ret = mpp_packet_init(&packet, NULL, 0);
	}

	///////////////////////////////////////////////
	mpp_packet_set_data(packet, pkt_data);
	mpp_packet_set_size(packet, pkt_size);
	mpp_packet_set_pos(packet, pkt_data);
	mpp_packet_set_length(packet, pkt_size);
	do {
		ret = mpi->decode_put_packet(ctx, packet);
		if (MPP_OK == ret){
			g_is_push = 1;
			break;
		}else{
			LOGD(stderr, "decode_put_packet failed ret = %d \n", ret);
			g_is_push = 0;
		}
		usleep(3 * 1000);
	} while (1);
	mpp_packet_deinit(&packet);

	return ret;
}

int MppDecoder::GetFrame(void)
{
	MpiDecLoopData *data = &loop_data;
	RK_U32 err_info = 0;
	MPP_RET ret = MPP_OK;
	MppCtx ctx  = data->ctx;
	MppApi *mpi = data->mpi;

	do {
		RK_S32 times = 5;

		try_again:
		ret = mpi->decode_get_frame(ctx, &frame);
		if (MPP_ERR_TIMEOUT == ret) {
			if (times > 0) {
				times--;
				usleep(2000);
				LOGD(stderr, "decode_get_frame try_again times = %d \n", 5 - times);
				goto try_again;
			}
			LOGD(stderr, "decode_get_frame failed too much time \n");
		}

		if (MPP_OK != ret) {
			LOGD(stderr, "decode_get_frame failed ret %d \n", ret);
			usleep(2000);
			continue;
		}

		if (frame) {
			RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
			RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
			RK_U32 hor_width = mpp_frame_get_width(frame);
			RK_U32 ver_height = mpp_frame_get_height(frame);
			RK_U32 buf_size = mpp_frame_get_buf_size(frame);
			RK_S64 pts = mpp_frame_get_pts(frame);
			RK_S64 dts = mpp_frame_get_dts(frame);

			// LOGD(stderr, "decoder require buffer w:h [%d:%d] stride [%d:%d] buf_size %d pts=%lld dts=%lld ",
			// 		hor_width, ver_height, hor_stride, ver_stride, buf_size, pts, dts);

			if (mpp_frame_get_info_change(frame)) {
				if (NULL == data->frm_grp) {
					/* If buffer group is not set create one and limit it */
					ret = mpp_buffer_group_get_internal(&data->frm_grp, MPP_BUFFER_TYPE_DRM);
					if (ret) {
						LOGD(stderr, "%p get mpp buffer group failed ret %d \n", ctx, ret);
						goto END;
					}

					/* Set buffer to mpp decoder */
					ret = mpi->control(ctx, MPP_DEC_SET_EXT_BUF_GROUP, data->frm_grp);
					if (ret) {
						LOGD(stderr, "%p set buffer group failed ret %d \n", ctx, ret);
						goto END;
					}
				} else {
					/* If old buffer group exist clear it */
					ret = mpp_buffer_group_clear(data->frm_grp);
					if (ret) {
						LOGD(stderr, "%p clear buffer group failed ret %d \n", ctx, ret);
						goto END;
					}
				}

				/* Use limit config to limit buffer count to 24 with buf_size */
				ret = mpp_buffer_group_limit_config(data->frm_grp, buf_size, 24);
				if (ret) {
					LOGD(stderr, "%p limit buffer group failed ret %d \n", ctx, ret);
					goto END;
				}

				/*
				 * All buffer group config done. Set info change ready to let
				 * decoder continue decoding
				 */
				ret = mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
				if (ret) {
					LOGD(stderr, "%p info change ready failed ret %d \n", ctx, ret);
					goto END;
				}
				this->last_frame_time_ms = GetCurrentTimeMS();
			} else {
				err_info = mpp_frame_get_errinfo(frame) | mpp_frame_get_discard(frame);
				if (err_info) {
					LOGD(stderr, "decoder_get_frame get err info:%d discard:%d. \n",
							mpp_frame_get_errinfo(frame), mpp_frame_get_discard(frame));
				}
				if (callback != nullptr && g_is_push) {
					MppFrameFormat format = mpp_frame_get_fmt(frame);
					char *data_vir =(char *) mpp_buffer_get_ptr(mpp_frame_get_buffer(frame));
					int fd = mpp_buffer_get_fd(mpp_frame_get_buffer(frame));
					callback(this->userdata, hor_stride, ver_stride, hor_width, ver_height, format, fd, data_vir);
					g_is_push = 0;
				}
			}
			END:
			ret = mpp_frame_deinit(&frame);
			frame = NULL;
		}
	} while (!data->loop_end);

	return ret;
}

int MppDecoder::SetCallback(MppDecoderFrameCallback callback) {
	this->callback = callback;
	return 0;
}

void MppDecoder::StopGetFrame(void) {
	loop_data.loop_end = 1;
}
