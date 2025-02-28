#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>

#include "ip.h"
#include "http.h"
#include "sqlite.h"
#include "sps_parser.h"
#include "config_file.h"
#include "string_split.h"
#include "rtsp_utils.h"
#include "xftp_live_sdk.h"
#include "xttp_rtc_sdk.h"
#include "frame_cir_buff.h"
#include "annotation_info.h"

#include "rk3588.h"

#define MSGID_NUM 32
#define XTTP_RETRY_MAX 3
#define MIN_PACKET_SIZE 480
#define SCRIPT_INNER_TYPE 0x01
#define ONE_MILLION_BASE 1000000

#define WEB_AGENT "19188888888"
#define SRS_AGENT "19199999999"
#define GB28181_AGENT "19099999999"

// 应用KEY
#define APP_KEY ""
// 应用SECRET
#define APP_SECRET ""
// 应用LICENSE
#define LICENSE_KEY ""

int g_msgid_cur = 0, g_is_online = 0, g_has_sending_iframe = 0,
	g_is_transfer_to_mp4 = 0, g_xttp_login_times = 0, g_is_video_has_started = 0, 
	g_index = 0, g_is_check_video_pulling = 0, g_is_check_video_pull_pid = 0, 
	g_camera_type = 1, g_stream_quality = 1;
char g_msg_ids[MSGID_NUM][33] = {0}, g_channel_no[128] = {0}, 
	g_stream_url[1500] = {0}, g_stream_protocol[16] = {0}, 
	g_rtsp_play_url[1500] = {0}, g_rtsp_url[1200] = {0}, g_rtsp_user[128] = {0}, 
	g_rtsp_pwd[128] = {0}, g_rtsp_server_ip[512] = {0};
uint16_t g_rtsp_port = 0;
uint16_t g_download_port = 0;
uint16_t g_remote_server_port = 0;
uint32_t g_uidn = 0, g_ssrc = 0;
char g_remote_server_name[128] = {0};
char g_remote_file_path[128] = {0};
char g_peer_name[256] = {0};
char g_recv_msg[1500] = {0};
char g_sid[256] = {0};
char g_stream_name[256] = {0};
uint16_t g_web_port = 0;
char g_web_server[32] = {0};
uint16_t g_v_width = 1920, g_v_height = 1080;
uint8_t xftp_frame_buffer[1024*1024] = {0};

int g_is_rate = 0;
int g_is_open_rate = 0;
int g_is_rate_iframe = 0;

int g_recv_pkts = 0;
int g_get_bitrate = 0;
int g_bitrate = 2000;
int g_prev_bitrate = 0;
int g_init_bitrate = 2000;
long g_cur_ts = 0;
long g_start_count_ts = 0;
double g_mfactor = 1.0;
rknn_app_context_t g_app_ctx;

int g_is_open_started = 1;

void stop_session(void);
void myStopXttpCallback(void);

void print_bin_data(uint8_t *data, int len)
{
	int iter = 0;

	if (!data || len <= 0) {
		return;
	}
	while (iter < len) {
		fprintf(stderr, "%02x ", data[iter++]);
		if (iter % (8 * 4) == 0) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "\n");
}
// 视频流推到流媒体服务器
int add_xftp_frame(const char *h264oraac, int insize, int type, uint32_t timestamp)
{
	uint8_t nalu_type = 0;
	uint8_t send_buffer[1500] = {0};
	uint16_t send_len = 0;

	if (!h264oraac || insize <= 0 || type <= 0) {
		fprintf(stderr, "[add_xftp_frame] error: h264oraac:%p, insize:%d, type:%d, g_start_vts:%ld, return -1;\n", h264oraac, insize, type, g_start_vts);
		return -1;
	}

	nalu_type = h264oraac[0] & 0x1F;
	if (nalu_type == 0x01 && insize < MIN_PACKET_SIZE) {
		memcpy(send_buffer, h264oraac, insize);
		send_len = MIN_PACKET_SIZE;
		MuxToXtvf((const char *)send_buffer, send_len, type, (int)timestamp);
	} else {
		MuxToXtvf(h264oraac, insize, type, (int)timestamp);
	}

	return 0;
}
// 推理结果推到流媒体服务器
int add_script_frame(const char *script_data, int script_len, int inner_type, uint32_t timestamp)
{
	if (!script_data || script_len <= 0) {
		fprintf(stderr, "[add_script_frame] error: script_data:%p, insize:%d, inner_type:%d, return -1;\n", script_data, script_len, inner_type);
		return -1;
	}

	return MuxScriptToXtvf(script_data, script_len, inner_type, timestamp);
}

void send_script_frame(detect_result_t *results, int count, uint32_t timestamp)
{
	int i, rt = 0;
	FRAME_INFO f_info;
	ANNOTATION_ITEM this_item;
	int enc_buff_len = 0;
	uint8_t enc_buff[1500] = {0};
	uint32_t ts;

	if(!results || count <= 0){
		fprintf(stderr, "[send_script_frame] results = %p, count = %d\n", results, count);
		return;
	}

	if (init_annotation_item_arr(&g_annotation_item_set)) {
		fprintf(stderr, "[send_script_frame]--init_annotation_item_arr failed, exit\n");
		return;
	}
	for (i = 0; i < count; i++) {
		this_item.id = results[i].id;
		this_item.conf_level_1m = results[i].prop * ONE_MILLION_BASE;
		this_item.xmin_1m = results[i].box.left * ONE_MILLION_BASE;
		this_item.ymin_1m = results[i].box.top * ONE_MILLION_BASE;
		this_item.xmax_1m = results[i].box.right * ONE_MILLION_BASE;
		this_item.ymax_1m = results[i].box.bottom * ONE_MILLION_BASE;
		rt = add_annotioan_item_to_set(&g_annotation_item_set, &this_item);
		// fprintf(stderr, "[send_script_frame] i:%ld, id=%d->(%s, %f) <(%d, %d), (%d, %d)>\n", i, 
		// 			results[i].id, results[i].name, results[i].prop, results[i].box.left, results[i].box.top,
		// 			results[i].box.right, results[i].box.bottom);
	}

	if(g_is_rate && g_is_open_rate){
		ts = timestamp;
	}else{
		rt = frame_cir_buff_dequeue(&g_frame_cir_buff, &f_info);
		if (rt) {
			fprintf(stderr, "[send_script_frame]--failed--return\n");
			return;
		}
		ts = f_info.timestamp;
	}
	if (g_annotation_item_set.annotion_item_len) {
		g_annotation_item_set.anno_ts = ts;
		if (!(enc_buff_len = encode_annotation_set_buff(&g_annotation_item_set, enc_buff))) {
			fprintf(stderr, "[send_script_frame] ERROR: failed in encode_annotation_set_buff\n");
		} else {
			rt = add_script_frame((char *)enc_buff, enc_buff_len, SCRIPT_INNER_TYPE, ts);
			// fprintf(stderr, "[send_script_frame] add_script_frame=%d, enc_buff_len=%d, timestamp=%u\n", rt, enc_buff_len, ts);
		}
	}
}

void send_xftp_frame(uint8_t *buffer, int len, uint32_t timestamp)
{
	int rt = 0;

	if(!buffer || len <= 4){
		return ;
	}

	rt = add_xftp_frame((char *)&buffer[4], len - 4, XTVF_VIDEO_TYPE, timestamp);
}
void get_data(int type, uint8_t *buffer, int len)
{
	int rt = 0;
	FRAME_INFO f_info;
	uint32_t timestamp = 0;
	struct timeval tv;

	if (!buffer || len <= 4) {
		return ;
	}
	g_app_ctx.decoder->Decode((uint8_t *)buffer, len, 0);

	if (g_is_rate && g_is_open_rate) {
		gettimeofday(&tv, NULL);
		if (!g_start_count_ts) {
			if ((buffer[4] & 0x1F) == 0x05) {
				g_start_count_ts = tv.tv_sec * 1000 + tv.tv_usec / 1000;
				g_recv_pkts = len;
			}
		} else if ((buffer[4] & 0x1F) == 0x01) {
			g_cur_ts = tv.tv_sec * 1000 + tv.tv_usec / 1000;
			g_recv_pkts += len;
		} else if ((buffer[4] & 0x1F) == 0x05) {
			if (g_cur_ts && g_cur_ts > g_start_count_ts) {
				g_get_bitrate = (g_recv_pkts * 8) / (g_cur_ts - g_start_count_ts);
			}
			g_start_count_ts = tv.tv_sec * 1000 + tv.tv_usec / 1000;
			g_recv_pkts = len;
		}
	} else {
		timestamp = getTimeMsec() - g_start_vts;
		if (((buffer[4] & 0x1F) == 0x01) || ((buffer[4] & 0x1F) == 0x05)) {
			f_info.timestamp = timestamp;
			// f_info.seqno = g_frame_seqno++;
			rt = frame_cir_buff_enqueue(&g_frame_cir_buff, &f_info);
		}
		rt = add_xftp_frame((char *)&buffer[4], len - 4, XTVF_VIDEO_TYPE, timestamp);
	}
}
void rtsp_session_did_received_cb(int type, uint8_t *h264oraac, int insize)
{
	int rt, video_width, video_height;

	if (!g_is_open_started) {
		// 从SPS中获取视频原始的分辨率
		if ((h264oraac[0] & 0x1F) == 0x07 && !parse_sps(h264oraac, insize, &video_width, &video_height)) {
			// 更新摄像头实际的分辨率
			updateMuxVideoMetaInfo(video_width, video_height);
			g_v_width = video_width;
			g_v_height = video_height;
			g_is_open_started = 1;
			if (g_is_rate && g_is_open_rate) {
				g_get_bitrate = 0;
				g_prev_bitrate = 0;
				g_is_rate_iframe = 0;
				g_start_count_ts = 0;
			} else {
				frame_cir_buff_init(&g_frame_cir_buff);
			}
			// 开启视频帧解码并进行推理线程
			rt = init_rk3588(&g_app_ctx, MODEL_FILE, 264, g_is_open_rate);
			fprintf(stderr, "[rtsp_session_did_received_cb] init_rk3588 rt = %d\n", rt);
		} else {
			fprintf(stderr, "[rtsp_session_did_received_cb] h264oraac[0] = 0x0%d\n", h264oraac[0] & 0x1F);
			return ;
		}
	}
	if (g_has_sending_iframe) {
		memcpy(&xftp_frame_buffer[4], h264oraac, insize);
		get_data(type, xftp_frame_buffer, insize + 4);
	}
}
// 拉流结束的回调
void video_session_did_stop_cb(void)
{
	fprintf(stderr, "[video_session_did_stop_cb] ++++++++++++++++++++++++++++ \n");
}
// 启动 rtsp 拉流
int start_pull_video(void)
{
	int rt = 0;

	if (!strcmp(g_stream_protocol, "rtsp")) {
		rt = start_open_rtsp_thread(g_rtsp_url, g_rtsp_port, g_rtsp_user, g_rtsp_pwd, g_rtsp_server_ip, rtsp_session_did_received_cb, video_session_did_stop_cb);
		if (rt) {
			fprintf(stderr, "[start_pull_video] start_open_rtsp_thread failed. rt = %d\n", rt);
			return -1;
		}
		fprintf(stderr, "[start_pull_video] start_open_rtsp_thread success = %d\n", rt);
	} else {
		fprintf(stderr, "[start_pull_video] error g_stream_protocol = %s\n", g_stream_protocol);
		return -3;
	}
	return rt;
}
// 通知对方已经开始推流
int send_session_info_to_receiver(char *receiver)
{
	int rt = 0;
	MSG_SENT_RESULT sent_result;
	char recver[32] = {0}, callback_msg[1024] = {0};

	if (!receiver || !strlen(receiver)) {
		fprintf(stderr, "[send_session_info_to_receiver] Error: Invalid param.\n");
		return -1;
	}
	if (g_start_vts > 0 && strlen(g_peer_name) && strlen(g_remote_server_name) && g_uidn && g_ssrc && g_download_port > 0) {
		if (!strcmp(g_peer_name, GB28181_AGENT)) {
			strcpy(recver, GB28181_AGENT);
			sprintf(callback_msg, "type=6;control_type=22;uidn=%u;ssrc=%u;index=%d;download_port=%d", g_uidn, g_ssrc, g_index, g_download_port);
		} else if (!strcmp(receiver, WEB_AGENT)) {
			strcpy(recver, SRS_AGENT);
			sprintf(callback_msg, "type=6;control_type=7;uidn=%u;ssrc=%u;sid=%s;web_agent=%s;stream_name=%s;download_port=%d", g_uidn, g_ssrc, g_sid, g_peer_name, g_stream_name, g_download_port);
		} else if (strlen(receiver)) {
			strcpy(recver, receiver);
			sprintf(callback_msg, "type=6;control_type=2;uidn=%u;ssrc=%u;server_name=%s;download_port=%d", g_uidn, g_ssrc, g_remote_server_name, g_download_port);
		} else {
			fprintf(stderr, "[send_session_info_to_receiver] receiver --> %s\n", receiver);
			return -2;
		}
		fprintf(stderr, "[send_session_info_to_receiver] msg --> %s\n", callback_msg);
		rt = send_control_msg(callback_msg, recver, &sent_result);
		usleep(1000);
		rt = send_control_msg(callback_msg, recver, &sent_result);
		usleep(1000);
		rt = send_control_msg(callback_msg, recver, &sent_result);
		if (rt) {
			fprintf(stderr, "[send_session_info_to_receiver] send_control_msg failed, ret=%d\n", rt);
		}
	} else {
		fprintf(stderr, "[send_session_info_to_receiver] return -3 : uidn=%u | ssrc=%u |g_start_vts=%ld | g_remote_server_name=%s | g_download_port=%d |g_peer_name=%s\n",
			g_uidn, g_ssrc, g_start_vts, g_remote_server_name, g_download_port, g_peer_name);
		return -3;
	}

	return rt;
}
// live SDK推流初始化成功回调
void xftpDidStart(long uidn, long ssrc, const char *remoteFilePath, const char *remoteServerName, int remoteServerPort, int downloadPort)
{
	int rt = 0;

	fprintf(stderr, "[xftpDidStart] %ld | %ld | %s | %s | %d | %d\n", uidn, ssrc, remoteFilePath, remoteServerName, remoteServerPort, downloadPort);

	g_start_vts = getTimeMsec();
	g_uidn = uidn;
	g_ssrc = ssrc;
	g_download_port = downloadPort;
	g_remote_server_port = remoteServerPort;
	strcpy(g_remote_server_name, remoteServerName);
	strcpy(g_remote_file_path, remoteFilePath);

	g_is_open_started = 0;
	// 启动拉取摄像头视频流
	rt = start_pull_video();
	if (rt) {
		fprintf(stderr, "[xftpDidStart] start_pull_video failed. rt = %d\n", rt);
		return;
	}
	// 推送消息给观看端
	send_session_info_to_receiver(g_peer_name);
	g_has_sending_iframe = 1;
}
// live SDK推流结束回调
void xftpTransferSuccess(long uidn, long ssrc, const char *remoteFilePath, const char *remoteServerName, int remoteServerPort, int downloadPort)
{
	fprintf(stderr, "[xftpTransferSuccess] : %ld | %ld | %s | %s | %d | %d\n", uidn, ssrc, remoteFilePath, remoteServerName, remoteServerPort, downloadPort);

	g_start_vts = 0;
	g_download_port = 0;
	g_uidn = 0;
	g_ssrc = 0;
}
// live SDK推流失败回调
void xftpFailedState(int state, const char *msg)
{
	fprintf(stderr, "[xftpFailedState] state:%d, msg:%s\n", state, msg ? msg : "NULL");

	if (g_start_vts && state == 13) {
		g_start_vts = 0;
		stop_session();
	}
}
void xftpChangeBitrate(long uidn, long ssrc, double factor)
{
	if (g_is_rate && g_is_open_rate && g_get_bitrate > 10) {
		g_mfactor *= factor;
		if (g_mfactor >= 1.0){
			g_mfactor = 1.0;
		} else if (g_mfactor < 0.1){
			g_mfactor = 0.1;
		}
		g_bitrate = (int)(g_mfactor * g_get_bitrate);
		fprintf(stderr,"[xftpChangeBitrate] g_mfactor = %f, g_prev_bitrate = %d, g_bitrate = %d, g_get_bitrate = %dkbps\n", g_mfactor, g_prev_bitrate, g_bitrate, g_get_bitrate);
		if (g_bitrate > 3000) g_bitrate = 3000;
		g_prev_bitrate = g_bitrate;
		g_app_ctx.encoder->ChangeBps(g_bitrate * 1000);
	}
}
// 初始化推流SDK
int start_xftp_request(int width, int height)
{
	if (width <= 0 || height <= 0) {
		fprintf(stderr, "[start_xftp_request] invalid param!\n");
		return -1;
	}
	stopSend();
	closeXtvf();
	fprintf(stderr, "[start_xftp_request] Before initXftpMuxSession, g_xftp_server=%s, g_xftp_port=%d, g_xftp_user=%s, g_xftp_pwd=%s\n", g_xftp_server, g_xftp_port, g_xftp_user, g_xftp_pwd);
	return initXftpMuxSession(NULL, 30, width, height, -1, -1, -1, 0, 0, g_xftp_user, g_xftp_pwd, g_xftp_server,
					g_xftp_port, 0, xftpDidStart, xftpFailedState, xftpTransferSuccess, xftpChangeBitrate);
}
// 开启SDK推流
int start_live(void)
{
	return start_xftp_request(g_v_width, g_v_height);
}
void *screenshot_and_upload_thread(void *arg)
{
	char record_str[2048] = {0};
	struct timeval cur_tv;
	uint64_t cur_ms = 0;

	gettimeofday(&cur_tv, NULL);
	cur_ms = cur_tv.tv_sec * 1000  + cur_tv.tv_usec / 1000;
	snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/ffmpeg -i '%s' -vframes 1 '/usr/local/xt/screenshot/%lu.jpg' > /dev/null 2>&1", g_stream_url, cur_ms);
	int rt = system(record_str);
	snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/curl -X POST -T '/usr/local/xt/screenshot/%lu.jpg' 'http://%s:%d/live/debug/info?type=1&filename=%lu.jpg&device_no=%s' > /dev/null 2>&1", cur_ms, g_web_server, g_web_port, cur_ms, g_xftp_user);
	rt = system(record_str);

	return NULL;
}
int screenshot_and_upload(void)
{
	pthread_t pid;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&pid, &attr, screenshot_and_upload_thread, NULL) != 0) {
		fprintf(stderr, "[screenshot_and_upload] pthread_create screenshot_and_upload_thread failed return -1\n");
		return -1;
	}
	pthread_attr_destroy(&attr);
	return 0;
}
// 停止SDK推流
int stop_xftp_session(void)
{
	closeXtvf();
	fprintf(stderr, "closeXtvf ... ... \n");
	stopSend();
	fprintf(stderr, "stopSend ... ... \n");

	return 0;
}
// SDK停止推流并截图
int stop_live(void)
{
	screenshot_and_upload();
	if (!strcmp(g_stream_protocol, "rtsp")) {
		stop_rtsp_over_tcp_thread(); // 停止 rtsp 拉流
	}

	stop_xftp_session();

	return 0;
}
// 结束推流/推理
void stop_session(void)
{
	fprintf(stderr, "[stop_session] start ... ... \n");

	g_start_vts = 0;
	g_download_port = 0;
	g_uidn = 0;
	g_ssrc = 0;
	g_has_sending_iframe = 0;

	fprintf(stderr, "[stop_session] start, before stop_live\n");
	stop_live();
	fprintf(stderr, "[stop_session] stop_live, END ... ... \n");

	if (!g_is_rate || !g_is_open_rate) {
		FRAME_INFO f_info = {.seqno = 0, .timestamp = 0};
		frame_cir_buff_enqueue(&g_frame_cir_buff, &f_info);
	}

	deinit_rk3588(&g_app_ctx);
}
// 结束推流
void stop_session0(uint32_t uidn, uint32_t ssrc)
{
	fprintf(stderr, "[stop_session0] uidn = %u, g_uidn = %u, ssrc = %u, g_ssrc = %u\n", uidn, g_uidn, ssrc, g_ssrc);
	if (uidn != g_uidn || ssrc != g_ssrc) {
		return;
	}
	stop_session();
}

int update_open_rate(int is_open_rate)
{
	int rt;
	char update_sql_channel[1024] = {0};

	snprintf(update_sql_channel, sizeof(update_sql_channel) - 1, "update m_channel set is_open_rate = %d where channel_no = '%s'", is_open_rate, g_channel_no);
	rt = write_data(update_sql_channel);
	if (rt) {
		fprintf(stderr, "[update_open_rate] write_data update %s is_open_rate to %d error rt = %d\n", g_channel_no, is_open_rate, rt);
		return -1;
	}
	return 0;
}
// 更新在线状态
int update_channel_online(int is_online)
{
	int rt, is_local_normal = 1;
	char update_sql_channel[1024] = {0}, url[1024] = {0};

	snprintf(update_sql_channel, sizeof(update_sql_channel) - 1, "update m_channel set is_normal = %d, is_online = %d where channel_no = '%s'", is_local_normal, is_online, g_channel_no);
	rt = write_data(update_sql_channel);
	if (rt) {
		fprintf(stderr, "[update_channel_online] write_data update %s online to %d error rt = %d\n", g_channel_no, is_online, rt);
		return -2;
	}
	if (!g_web_port || !strlen(g_web_server)) {
		fprintf(stderr, "[update_channel_online] empty g_web_port = %d, g_web_server = %s\n", g_web_port, g_web_server);
		return -3;
	}
	snprintf(url, sizeof(url) - 1, "http://%s:%d/live/channel/modChannelXtOnline?device_no=%s&is_online_xt=%d&is_normal=%d", g_web_server, g_web_port, g_xftp_user, is_online, is_local_normal);
	rt = httpRequest(url, NULL, NULL);
	if (rt < 0) {
		fprintf(stderr, "[update_channel_online] url = %s, modChannelXtOnline error rt = %d\n", url, rt);
		return -4;
	}

	return 0;
}
// 消息SDK初始化成功回调
void myRegisterSuccessCallback(int state, const char *from, const char *servername, const int serverport)
{
	g_is_online = 1;
	g_xttp_login_times = 0;
	update_channel_online(1);
	fprintf(stderr, "[myRegisterSuccessCallback] state=%d, from=%s, servername=%s, serverport=%d\n", state, from, servername, serverport);
}
// 消息SDK初始化失败回调
void myRegisterFailedCallback(int state, const char *msg)
{
	g_is_online = 0;
	update_channel_online(0);
	fprintf(stderr, "[myRegisterFailedCallback] times = %d, state=%d, msg=%s\n", g_xttp_login_times, state, msg);
}
// 消息SDK接收到消息回调
void myReceiveMsgCallback(const char *msg, const char *from, const char *msgid, int msg_type, const char *pid, const char *msgatime, int need_transfer_encode)
{
	MSG_SENT_RESULT sent_result;
	char ftxt[1024] = {0}, channel_no[8] = {0}, msg_from[256] = {0}, response_msg[512] = {0};
	StringSplit *key_value_pairs = NULL, *item_pairs = NULL;
	StringSplitItem *item = NULL, *key = NULL, *value = NULL;
	StringSplitHandler msg_split_handler, key_value_split_handler;
	int i = 0, n = 0, rt = 0, type = -1, control_type = -1, is_online = -1, is_open_rate = 0;
	uint32_t uidn, ssrc;

	if (!msg || !from || !msgid || strlen(msg) >= 1500) {
		fprintf(stderr, "[myReceiveMsgCallback] invalid msg: %s, from: %s, msgid: %s\n", msg, from, msgid);
		return;
	}
	if (msgid) {
		// 消息去重
		for(; n < MSGID_NUM; n++){
			if (!strcmp(g_msg_ids[n], msgid)) {
				return;
			}
		}
		strcpy(g_msg_ids[g_msgid_cur], msgid);
		g_msgid_cur = (g_msgid_cur + 1) % MSGID_NUM;
	}
	fprintf(stderr, "[myReceiveMsgCallback] from = %s, msg = %s, msgid = %s\n", from, msg, msgid);
	strcpy(g_recv_msg, msg);
	if (init_string_split_handler(&msg_split_handler)) {
		fprintf(stderr, "[myReceiveMsgCallback] init_string_split_handler(1) failed!\n");
		return;
	}

	// 解析消息字段
	key_value_pairs = string_split_handle(';', g_recv_msg, &msg_split_handler);
	if (key_value_pairs->items != NULL) {
		for (i = 0; i < key_value_pairs->number; i++) {
			item = key_value_pairs->items[i];
			if (item != NULL && item->length) {
				if (init_string_split_handler(&key_value_split_handler)) {
					fprintf(stderr, "[myReceiveMsgCallback] init_string_split_handler(2) failed!\n");
					break;
				}

				if (strstr(item->str, "=")) {
					item_pairs = string_split_handle('=', item->str, &key_value_split_handler);
					if (item_pairs->items != NULL) {
						if (item_pairs->number != 2) {
							fprintf(stderr, "not key-value: number:%d\n", item_pairs->number);
							string_split_free(item_pairs, &key_value_split_handler);
							break;
						}
						key = item_pairs->items[0];
						value = item_pairs->items[1];

						if (!key->length || !value->length) {
							break;
						}
						if (!strcasecmp(key->str, "type")) {
							type = atoi(value->str);
						} else if (!strcasecmp(key->str, "control_type")) {
							control_type = atoi(value->str);
						} else if (!strcasecmp(key->str, "is_online")) {
							is_online = atoi(value->str);
						} else if (!strcasecmp(key->str, "from")) {
							strcpy(msg_from, value->str);
						} else if (!strcasecmp(key->str, "sid")) {
							strcpy(g_sid, value->str);
						} else if (!strcasecmp(key->str, "stream_name")) {
							strcpy(g_stream_name, value->str);
						} else if (!strcasecmp(key->str, "index")) {
							g_index = atoi(value->str);
						} else if (!strcasecmp(key->str, "uidn")) {
							uidn = atoi(value->str);
						} else if (!strcasecmp(key->str, "ssrc")) {
							ssrc = atoi(value->str);
						} else if (!strcasecmp(key->str, "is_open_rate")) {
							is_open_rate = atoi(value->str);
						} else if (!strcasecmp(key->str, "channel_no")) {
							strcpy(channel_no, value->str);
						}
					} else {
						string_split_free(item_pairs, &key_value_split_handler);
						break;
					}
					string_split_free(item_pairs, &key_value_split_handler);
				} else {
					break;
				}
			} else {
				fprintf(stderr, "item:%d is NULL\n", i);
				break;
			}
		}
	}
	string_split_free(key_value_pairs, &msg_split_handler);
	if (type != 6){
		return;
	}

	switch (control_type) {
		case 1: //收到摄像头开始推流指令
		case 6:
		case 21: //收到GB28181网关摄像头开始推流指令
			strcpy(g_peer_name, from);
			if (g_has_sending_iframe) { // 正在推流
				rt = send_session_info_to_receiver(g_peer_name);
				fprintf(stderr, "[myReceiveMsgCallback] %d send_session_info_to_receiver rt = %d, g_peer_name = %s\n", control_type, rt, g_peer_name);
			} else { // 开启推流
				fprintf(stderr, "[myReceiveMsgCallback] %d should start camera and start live.\n", control_type);
				// 初始化连接多媒体服务器
				rt = start_live(); // 启动SKDK推流
				fprintf(stderr, "[myReceiveMsgCallback] %d start_live() = %d, g_peer_name = %s\n", control_type, rt, g_peer_name);
			}
			break;
		case 3: //停止推流
		case 24:
			// 关闭多媒体服务器的连接, 停止推流/推理
			stop_session0(uidn, ssrc);
			break;
		case 5: //对方询问是否在线
			if (strlen(msg_from)) {
				sprintf(response_msg, "type=6;control_type=4;from=%s;is_online=1", g_xftp_user);
				// 回复‘在线’消息
				rt = send_control_msg(response_msg, msg_from, &sent_result);
				if (rt) {
					fprintf(stderr, "[myReceiveMsgCallback] send_control_msg failed(%s), rt=%d\n", response_msg, rt);
				}
			} else {
				fprintf(stderr, "[myReceiveMsgCallback] Error: The message hasn't from info(%s)\n", g_recv_msg);
			}
			break;
		case 25:
			fprintf(stderr, "[myReceiveMsgCallback] g_is_rate = %d, g_is_open_rate = %d, is_open_rate = %d\n", g_is_rate, g_is_open_rate, is_open_rate);
			if (!g_is_rate || g_is_open_rate == is_open_rate) break;
			rt = update_open_rate(is_open_rate);
			fprintf(stderr, "[myReceiveMsgCallback] update_open_rate(%d) rt = %d\n", is_open_rate, rt);
			
			snprintf(ftxt, sizeof(ftxt) - 1, "ps -ef | grep -v grep | grep -v tail | grep 'bin/xftp %s' | awk '{print $2}' | xargs kill -9", channel_no);
			rt = system(ftxt);
			break;
		default:
			break;
	}
}
void myReceiveBinaryMsgCallback(uint8_t *data, int size, const char *from, const char *msgid, int type) {}
void mySentMsgResponseCallback(const char *msgid, const char *pid, const char *msgatime) {}
// 初始化消息SDK
int start_msg_client(void)
{
	int rt = -100;

	update_channel_online(0);
	fprintf(stderr, "[start_msg_client] Before start_xttp_client g_is_online = %d, g_xttp_port = %d, g_xttp_server = %s\n", g_is_online, g_xttp_port, g_xttp_server);
	if (!g_is_online) {
		// 启动消息SDK，连接消息服务器，设置消息回调
		rt = start_xttp_client(g_xftp_user, g_xftp_pwd, g_xttp_server, g_xttp_port, 
				0, myRegisterSuccessCallback, 
				myRegisterFailedCallback, myReceiveMsgCallback,
				myReceiveBinaryMsgCallback, mySentMsgResponseCallback, myStopXttpCallback);
		fprintf(stderr, "[start_msg_client] start_xttp_client rt = %d\n", rt);
	}
	return rt;
}
// 消息SDK停止回调, 重连消息服务器
void myStopXttpCallback(void)
{
	int rt = 0;

	fprintf(stderr, "[myStopXttpCallback] .............. \n");
	g_is_online = 0;
	update_channel_online(0);
	++g_xttp_login_times;
	if (g_xttp_login_times < XTTP_RETRY_MAX) {
		// 重连消息服务器
		rt = start_msg_client();
		fprintf(stderr, "[myStopXttpCallback] 0 start_msg_client rt=%d\n", rt);
	} else {
		g_xttp_login_times = 0;
		sleep(60);
		if (!g_is_online) {
			rt = start_msg_client();
			fprintf(stderr, "[myStopXttpCallback] 1 start_msg_client rt=%d\n", rt);
		}
	}
}

// 解析 rtsp URL
int get_rtsp_info(const char *url)
{
	char *ptr = NULL, *ptr2 = NULL, *real_url = NULL, user_auth_part[512] = {0}, port_str[512] = {0}, tmp_url[512] = {0};

	if (!url) {
		return -1;
	}
	strcpy(tmp_url, url);
	ptr = strstr(tmp_url, "rtsp://");
	if (!ptr) {
		fprintf(stderr, "Invalid format: No rtsp protocol info.\n");
		return -2;
	}

	ptr += strlen("rtsp://");
	ptr2 = strstr(ptr, "@");
	if (ptr2) {
		memcpy(user_auth_part, ptr, ptr2 - ptr);
		ptr = ptr2;
		ptr2 = strstr(user_auth_part, ":");
		if (!ptr2) {
			fprintf(stderr, "Invalid format: wrong auth info.\n");
			return -3;
		}
		memset(g_rtsp_user, 0, sizeof(g_rtsp_user));
		memcpy(g_rtsp_user, user_auth_part, ptr2 - user_auth_part);
		ptr2 += strlen(":");
		memset(g_rtsp_pwd, 0, sizeof(g_rtsp_pwd));
		memcpy(g_rtsp_pwd, ptr2, strlen(ptr2));
		ptr += strlen("@");
	}

	real_url = ptr;
	ptr2 = strstr(ptr, ":");
	if (ptr2) {
		memset(g_rtsp_server_ip, 0, sizeof(g_rtsp_server_ip));
		memcpy(g_rtsp_server_ip, ptr, ptr2 - ptr);
		ptr = ptr2 + strlen(":");
		ptr2 = strstr(ptr, "/");
		if (!ptr2) {
			fprintf(stderr, "Invalid format: wrong server and port info.\n");
			return -4;
		}
		memset(port_str, 0, sizeof(port_str));
		memcpy(port_str, ptr, ptr2 - ptr);

		g_rtsp_port = (uint16_t)atoi(port_str);
		if (!g_rtsp_port || g_rtsp_port > 65535) {
			fprintf(stderr, "Invalid format: wrong port info.\n");
			return -5;
		}
		ptr = ptr2 + strlen("/");
	} else {
		ptr2 = strstr(ptr, "/");
		if (!ptr2) {
			fprintf(stderr, "Invalid format: wrong server info.\n");
			return -6;
		}
		memset(g_rtsp_server_ip, 0, sizeof(g_rtsp_server_ip));
		memcpy(g_rtsp_server_ip, ptr, ptr2 - ptr);
		g_rtsp_port = 554;
		ptr = ptr2 + strlen("/");
	}
	memset(g_rtsp_url, 0, sizeof(g_rtsp_url));
	sprintf(g_rtsp_url, "rtsp://%s", real_url);
	strcpy(g_rtsp_play_url, url);

	return 0;
}
// 读取配置
int read_config_xtvf(const char *channel_no)
{
	int rt;
	TABLE_DATA data;
	char select_sql_server[256] = {0}, device_no[33] = {0}, tmp_channel_no[4] = {0}, select_sql_device[256] = {0}, select_sql_channel[256] = {0};

	if (!channel_no || !strlen(channel_no)) {
		fprintf(stderr, "[read_config_xtvf] param error.\n");
		return -1;
	}
	// 获取设备号
	snprintf(select_sql_device, sizeof(select_sql_device) - 1, "select id,device_no,device_pwd,device_name,is_mp4,is_rate from m_device");
	rt = read_data(&data, select_sql_device);
	if (rt) {
		fprintf(stderr, "[read_config_xtvf] Not find device. rt = %d\n", rt);
		return -2;
	}
	strcpy(device_no, data.lines[0].fields[1].val);
	g_is_rate = atoi(data.lines[0].fields[5].val);
	free(data.lines);

	// 获取通道号
	snprintf(select_sql_channel, sizeof(select_sql_channel) - 1, "select id,channel_no,channel_pwd,channel_sip,protocol,stream_url,is_open_rate,camera_type,stream_quality from m_channel where channel_no = \"%s\"", channel_no);
	rt = read_data(&data, select_sql_channel);
	if (rt) {
		fprintf(stderr, "[read_config_xtvf] Not find channel %s.\n", channel_no);
		return -3;
	}
	strncpy(tmp_channel_no, data.lines[0].fields[1].val, 3);
	snprintf(g_xftp_user, sizeof(g_xftp_user) - 1, "%s%s", device_no, tmp_channel_no);
	strcpy(g_xftp_pwd, data.lines[0].fields[2].val);
	strcpy(g_stream_protocol, data.lines[0].fields[4].val);
	strcpy(g_stream_url, data.lines[0].fields[5].val);
	g_is_open_rate = atoi(data.lines[0].fields[6].val);
	g_camera_type = atoi(data.lines[0].fields[7].val);
	g_stream_quality = atoi(data.lines[0].fields[8].val);
	free(data.lines);

	// 获取web服务器地址及端口
	snprintf(select_sql_server, sizeof(select_sql_server) - 1, "select id,ip,port,type from m_server where type = 'web'");
	rt = read_data(&data, select_sql_server);
	if (rt) {
		fprintf(stderr, "[read_config_xtvf] Not find web server ip and port. rt = %d\n", rt);
		return -4;
	}
	strcpy(g_web_server, data.lines[0].fields[1].val);
	g_web_port = atol(data.lines[0].fields[2].val);
	free(data.lines);

	// 获取消息服务器地址及端口
	snprintf(select_sql_server, sizeof(select_sql_server) - 1, "select id,ip,port,type from m_server where type = 'msg'");
	rt = read_data(&data, select_sql_server);
	if (rt) {
		fprintf(stderr, "[read_config_xtvf] Not find msg server ip and port. rt = %d\n", rt);
		return -5;
	}
	strcpy(g_xttp_server, data.lines[0].fields[1].val);
	g_xttp_port = atol(data.lines[0].fields[2].val);
	free(data.lines);

	// 获取流媒体服务器地址及端口
	snprintf(select_sql_server, sizeof(select_sql_server) - 1, "select id,ip,port,type from m_server where type = 'video'");
	rt = read_data(&data, select_sql_server);
	if (rt) {
		fprintf(stderr, "[read_config_xtvf] Not find video server ip and port. rt = %d\n", rt);
		return -6;
	}
	strcpy(g_xftp_server, data.lines[0].fields[1].val);
	g_xftp_port = atol(data.lines[0].fields[2].val);
	free(data.lines);

	if (!strcmp(g_stream_protocol, "rtsp")) {
		// 解析rtsp流地址
		rt = get_rtsp_info(g_stream_url);
		if (rt) {
			fprintf(stderr, "[read_config_xtvf] stream_url error. rt = %d, url = %s\n", rt, g_stream_url);
			return -7;
		}
	} else {
		fprintf(stderr, "[read_config_xtvf] No the protocol = %s", g_stream_protocol);
		return -9;
	}

	return 0;
}
// 信号处理
void IntHandle(int signo)
{
	int rt = 0;

	if (SIGINT == signo || SIGTERM == signo) {
		fprintf(stderr, "[IntHandle] before stop_session\n");
		// 停止live SDK
		stop_session();
		fprintf(stderr, "[IntHandle] stop_session after\n");
		// 停止消息 SDK
		rt = stop_xttp_client();
		fprintf(stderr, "[IntHandle] stop_xttp_client rt = %d\n", rt);
		g_is_running = 0;
	}
}

// 主程序
int main(int argc, char *argv[])
{
	int rt, i = 3;

	if (argc != 4) {
		fprintf(stderr, "USAGE: %s channel_no video_width video_height\n", argv[0]);
		return -1;
	}
	g_v_width = atoi(argv[2]); // 视频帧宽度
	g_v_height = atoi(argv[3]); // 视频帧高度
	if (strlen(argv[1]) != 3 || g_v_width <= 0  || g_v_height <= 0) {
		fprintf(stderr, "USAGE: %s channel_no video_width video_height\n", argv[0]);
		return -2;
	}
	// rt = load_config();
	// if (rt) {
	// 	fprintf(stderr, "[%s] load_config failed, rt = %d\n", argv[0], rt);
	// 	return -3;
	// }
	// 验证应用ID
	rt = initAppkeySecretLicense(APP_KEY, APP_SECRET, LICENSE_KEY);
	if (rt != 0) {
		fprintf(stderr, "[%s] initAppkeySecretLicense failed, rt = %d\n", argv[0], rt);
		return -4;
	}

	g_is_udp = 0;
	xftp_frame_buffer[0] = 0;
	xftp_frame_buffer[1] = 0;
	xftp_frame_buffer[2] = 0;
	xftp_frame_buffer[3] = 1;
	strcpy(g_channel_no, argv[1]); // 通道号
	// 读取配置信息，获取设备/通道号/服务器地址端口
	rt = read_config_xtvf(g_channel_no);
	if (rt) {
		fprintf(stderr, "[%s] read_config_xtvf failed, rt = %d\n", argv[0], rt);
	}
	// 登录信令服务器
	// 登录成功会回调 myRegisterSuccessCallback
	// 登录失败会回调 myRegisterFailedCallback
	// 收到消息会回调 myReceiveMsgCallback, 此回调中去处理收到相应消息的逻辑
	// 停止时会回调 myStopXttpCallback, 此回调中去处理消息服务重连的逻辑
	while (i--) {
		rt = start_msg_client();
		fprintf(stderr, "[%s] 1 start start_msg_client, rt = %d\n", argv[0], rt);
		if (!rt) break;
		sleep(1);
	}
	if (rt) update_channel_online(0);
	start_check_ip_thread();

	g_is_running = 1;
	signal(SIGINT, IntHandle);
	signal(SIGTERM, IntHandle);
	while (g_is_running) {
		sleep(1);
		if (g_is_running && !g_is_online) { // 不在线则需重新登陆
			rt = start_msg_client();
			fprintf(stderr, "[%s] 2 while start_msg_client, rt = %d\n", argv[0], rt);
		}
	}

	return 0;
}
