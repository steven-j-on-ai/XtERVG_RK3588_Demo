#ifndef _XTTP_RTC_SDK_H
#define _XTTP_RTC_SDK_H

#ifdef __cplusplus
	extern "C" {
#endif

#define RESULT_STR_LEN 1024
typedef struct _msg_sent_result {
	int code;
	char result_str[RESULT_STR_LEN];
} MSG_SENT_RESULT;

static const int STATE_REG_OK = 0;
static const int STATE_REG_ERR_SOCKET_CREATE = 1;
static const int STATE_REG_ERR_SEND_REQUEST = 2;
static const int STATE_REG_ERR_IN_REGISTER = 3;
static const int STATE_REG_ERR_OTHER = 4;
static const int STATE_REG_ERR_SOCKET_TIMEOUT = 5;
static const int SEND_MSG_OK = 0;
static const int SOCKET_ERROR = -1001;
static const int ERROR_IN_USERINFO = -1002;
static const int ERROR_IN_PARAM = -1003;
static const int ERROR_IN_SENDTO = -1004;

typedef void (*xttpReceiveMsgCb)(const char *msg, const char *from, 
		const char *msgid, int type, const char *pid, 
		const char *msgatime, int need_transfer_encode);
typedef void (*xttpReceiveBinaryMsgCb)(uint8_t *data, int size, 
		const char *from, const char *msgid, int type);
typedef void (*xttpRegisterFailedCb)(int state, const char *from);
typedef void (*xttpRegisterSuccessCb)(int state, const char *from, 
		const char *servername, const int serverport);
typedef void (*xttpSentMsgResponseCb)(const char *msgid, const char *pid, 
				const char *msgatime);
typedef void (*xttpReceiveMsgStateCb)(int state, const char *from, const char *msgid);
typedef void (*xttpRevertMsgReceivedCb)(const char *msgid, const char *pid);
typedef void (*xttpStopXttpClientCb)(void);

int stop_xttp_client(void);
int start_xttp_client(const char *username, const char *password, const char *servername, 
			int serverport, int no_need_encrypt, xttpRegisterSuccessCb registerSuccessCb, 
			xttpRegisterFailedCb registerFailedCb, xttpReceiveMsgCb receiveMsgCb,
			xttpReceiveBinaryMsgCb receiveBinaryMsgCb, xttpSentMsgResponseCb sentMsgResponseCb, 
			xttpStopXttpClientCb stopXttpClienCb);

int send_text(const char *text, const char *receiver, MSG_SENT_RESULT *sent_result_p);
int send_binary_msg(const char *in_data, int in_size, const char *receiver, MSG_SENT_RESULT *sent_result_p);
int send_control_msg(const char *text, const char *receiver, MSG_SENT_RESULT *sent_result_p);
int send_control_msg_by_times(const char *text, const char *receiver, int times, MSG_SENT_RESULT *sent_result_p);
int resend_text_msg(const char *text, const char *receiver, const char *msg_id, MSG_SENT_RESULT *sent_result_p);
int get_encode_str(const char *from_str, char *to_str);
int get_magic_str(const char *from_str, char *to_str);
int get_magic_secrets(const char *from_str, char *to_str);

#ifdef __cplusplus
}
#endif

#endif
