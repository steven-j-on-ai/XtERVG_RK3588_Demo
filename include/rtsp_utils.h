#ifndef _RTSP_UTILS_H
#define _RTSP_UTILS_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
    extern "C" {
#endif

extern int g_is_udp;
extern int g_sock_fd;
extern int g_is_running;
extern long g_start_vts;
extern char g_xftp_server[512];
extern uint16_t g_xftp_port;
extern char g_xttp_server[512];
extern uint16_t g_xttp_port;
extern char g_xftp_gw_server[512];
extern char g_xftp_user[256];
extern char g_xftp_pwd[128];
extern char g_user_name[128];
extern char g_user_pwd[128];
extern char g_realm[64];
extern char g_nounce[64];
extern char g_psession[50];
extern char g_play_url[1056];
extern int g_got_h264_meta_data;
extern int g_start_rtp_retrive_times;

typedef void (*rtspSessionDidReceivedCb)(int frame_type, uint8_t *frame_data, int frame_length);
typedef void (*rtspSessionDidStopCb)(void);

long getTimeMsec(void);
void print_bin_data(uint8_t *data, int len);
int stop_rtsp_over_tcp_thread(void);
int start_open_rtsp_thread(const char *rtsp, uint16_t rtsp_port, 
    const char *user_name, const char *user_pwd, const char *rtsp_server_ip,
    rtspSessionDidReceivedCb receivedCallback, rtspSessionDidStopCb stopCallback);

#ifdef __cplusplus
    }
#endif

#endif
