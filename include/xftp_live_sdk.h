#ifndef XFTP_LIVE_SDK_H
#define XFTP_LIVE_SDK_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
    extern "C" {
#endif

#define NAME_LEN 1024
#define RECV_SESSION_MAX 6

typedef struct{
    unsigned char frameLengthFlag;
    unsigned char dependsOnCoreCoder;
    unsigned char extensionFlag;
} aac_property;

typedef struct _AudioEncoderConfigure{
    unsigned char audioObjectType;
    unsigned char samplingFrequencyIndex;
    unsigned char channelConfiguration;
    union{
        unsigned char opusMsIndex[3];
        aac_property aac;
    } properties;
} AudioEncoderConfigure;

typedef enum{
    DEMUX_NOERROR = 0, // 正常
    DEMUX_ERRORNONE = 1, // 读到结束符正常结束
    DEMUX_LESSBYTE = 2, // 缺少字节
    DEMUX_UNKNOWERROR = 3, // 文件格式错误
    DEMUX_ADTSERROR = 4,
    DEMUX_NOSTARTCODE = 5, // 缺少起始码
	DEMUX_READ_CONTINUE = 0x10, // 继续从头执行
	DEMUX_AUDIO_PAUSE = 0x11 // 音频数据暂停
}DEMUX_RETURN_VAULE;

typedef enum{
	XTVF_VIDEO_TYPE=0x08,
	XTVF_AUDIO_TYPE=0x09,
	XTVF_SCRIPT_TYPE=0x12
} XTVF_FRAME_TYPE;

typedef enum{
	XTVF_MEDIA_AUDIO = 0,
	XTVF_MEDIA_VIDEO,
	XTVF_MEDIA_VIDEO_AUDIO_MIX,
    XTVF_MEDIA_SCREEN_SHARE_VIDEO
} XTVF_MEDIA_TYPE;

typedef enum {
    XFTP_SESSION_DID_START = 0,
    XFTP_SESSION_DID_FINISHED,
    XFTP_SESSION_START_INVALID_PARAM,
    XFTP_SESSION_START_FAILED,
    XFTP_SESSION_TERMINATED,
    XFTP_SESSION_DID_DISCONNECTED
} XFTP_SESSION_STATUS;

typedef enum{
    DOWNLOAD_SESSION_ERR_PTHREAD_CREATE = 301,
    DOWNLOAD_SESSION_ERR_AUTH_FAILED,
    DOWNLOAD_SESSION_ERR_SERVER_ERR,
    DOWNLOAD_SESSION_ERR_IN_CONN,
    DOWNLOAD_SESSION_ERR_IN_PTHREAD,
    DOWNLOAD_SESSION_ERR_IN_PARAM,
    DOWNLOAD_SESSION_ERR_IN_SOCKET,
    DOWNLOAD_SESSION_ERR_IS_RUNNING,
    DOWNLOAD_SESSION_ERR_OTHER,
    DOWNLOAD_SESSION_ERR_THREAD_IS_OVER,
    DOWNLOAD_SESSION_ERR_SESSION_DISCONNECTED
} DOWNLOAD_SESSION_ERR_CODE;

typedef struct xftp_upload_session_info_type
{
	XTVF_MEDIA_TYPE media_type;
	char local_path[NAME_LEN];
	char server_name[NAME_LEN];
	short server_port;
	int session_index;
	int is_finished;
	int is_recording;
	int preview_is_ready;
} XFTP_UPLOAD_SESSION_INFO;

#define F_BUFFER_MAX_LEN 1024 * 1024

typedef struct frame_buffer_t {
    int buffer_length;
    int frame_type;
    int timestamp;
    int sub_type;
    int status;
    int v_width;
    int v_height;
    unsigned long fpos;
    int media_type;
    AudioEncoderConfigure audio_enc_configure;
    uint8_t buffer[F_BUFFER_MAX_LEN];
} FRAME_BUFFER;

typedef void (*xftpSessionDidStartCb)(long uidn, long ssrc, const char *remoteFilePath,
		const char *remoteServerName, int remoteServerPort, int downloadPort);
typedef void (*xftpSessionFailedStateCb)(int state, const char *msg);
typedef void (*xftpSessionTransferSuccessCb)(long uidn, long ssrc,
		const char *remoteFilePath, const char *remoteServerName, int remoteServerPort, int downloadPort);
typedef void (*xftpSessionDidStartNewCb)(int session_number, long uidn, long ssrc, const char *remoteFilePath,
        const char *remoteServerName, int remoteServerPort, int downloadPort);
typedef void (*xftpSessionFailedStateNewCb)(int session_number, int state, const char *msg);
typedef void (*xftpSessionTransferSuccessNewCb)(int session_number, long uidn, long ssrc,
        const char *remoteFilePath, const char *remoteServerName, int remoteServerPort, int downloadPort);

/*建议调整码率回调接口*/
typedef void (*xftpSessionChangeBitrateCb)(long uidn, long ssrc, double factor);
typedef void (*xftpDownloadSessionDidStartCb)(uint32_t uidn, uint32_t ssrc, const char *download_server,
		int is_live, int start_seq, int start_ms, int session_number);
typedef void (*xftpDownloadSessionFailedStateCb)(uint32_t uidn, uint32_t ssrc, const char *download_server, const char *download_url, int err_code, int session_number);
typedef void (*xftpDownloadSessionDidChangedCb)(uint32_t uidn, uint32_t ssrc, const char *download_server, const char *download_url,
		const char *path, int received_pkts, int max_pkt_number, int total_pkts, int session_number, int payload_size);
typedef void (*xftpDownloadSessionReceivedDataCb)(int session_number);
typedef void (*xftpDownloadSessionDidFinishedCb)(uint32_t uidn, uint32_t ssrc, const char *download_server, const char *download_url, int receivedPkts,
		long long file_size, int session_number);

//初始化APP_KEY、APP_SECRET与LICENSE
//RETURN:
// 0 - success
// other - failed
int initAppkeySecretLicense(const char *key, const char *secret, const char *license);

/*单路音视频推流接口*/
int initMuxToXtvfNew(const char *xtvf, int framerate, int videowidth, int videoheight,
        int profile_index, int samplerate_index, int channel_index, int sample_rate, 
        int samples, const char *username, const char *userpass, const char *servername, 
        int server_port, int no_need_to_encrypt, xftpSessionDidStartCb didStartCb, 
        xftpSessionFailedStateCb failedStateCb, xftpSessionTransferSuccessCb transferSuccessCb);
//变更视频宽高参数以便接收侧正确解码
int updateMuxVideoMetaInfo(int videowidth, int videoheight);
int initXftpMuxSession(const char *xtvf, int framerate, int videowidth, int videoheight,
        int profile_index, int samplerate_index, int channel_index, int sample_rate, 
        int samples, const char *username, const char *userpass, const char *servername, 
        int server_port, int no_need_to_encrypt, xftpSessionDidStartCb didStartCb, 
        xftpSessionFailedStateCb failedStateCb, xftpSessionTransferSuccessCb transferSuccessCb,
        xftpSessionChangeBitrateCb changeBitrateCb);

int MuxScriptToXtvf(const char *script_data, int script_len, int inner_type, int timestamp);
int MuxToXtvf(const char *h264oraac, int insize, int type, int timestamp);
int MuxMuteToXtvf(int timestamp, int length);
int closeXtvf();
int stopSend();	

/*多路音视频推流接口*/
int initVideoMuxToXtvfSession(const char *xtvf, int framerate, int videowidth, int videoheight,
         const char *username, const char *userpass, const char *servername, int server_port,
        int no_need_to_encrypt, XFTP_UPLOAD_SESSION_INFO *session_array_p, xftpSessionDidStartNewCb didStartCb,
        xftpSessionFailedStateNewCb failedStateCb, xftpSessionTransferSuccessNewCb transferSuccessCb);
int initVideoMuxToXtvfSessionNew(const char *xtvf, int framerate, int videowidth, int videoheight,
         const char *username, const char *userpass, const char *servername, int server_port,
        int no_need_to_encrypt, XFTP_UPLOAD_SESSION_INFO *session_array_p, xftpSessionDidStartNewCb didStartCb,
        xftpSessionFailedStateNewCb failedStateCb, xftpSessionTransferSuccessNewCb transferSuccessCb,
        xftpSessionChangeBitrateCb changeBitrateCb);
int initAudioMuxToXtvfSession(const char *xtvf, int profile_index, int samplerate_index, int channel_index, int sample_rate,
        int samples, const char *username, const char *userpass, const char *servername, int server_port,
        int no_need_to_encrypt, XFTP_UPLOAD_SESSION_INFO *session_array_p, xftpSessionDidStartNewCb didStartCb,
        xftpSessionFailedStateNewCb failedStateCb, xftpSessionTransferSuccessNewCb transferSuccessCb);

//变更视频宽高参数以便接收侧正确解码
int updateMuxVideoMetaInfoBySession(int session_number, int videowidth, int videoheight);
int startSingleXftpUploadSession(int i);
int MuxToXtvfBySession(int session_number, uint8_t *h264oraac, int insize, int type, int timestamp);
int MuxMuteToXtvfBySession(int session_number, int timestamp, int length);
int closeXtvfBySession(int session_number);
int stopSendNew(int sessionIndex);

int initDownloadFile(const char *localfilepath, const char *username, const char *userpass,
        long uidn, long ssrc, const char * downloadurl, int server_port,
        const char *source_server_name, int source_server_port, int need_download,
        const char *relayApiHost, int no_need_to_encrypt, xftpDownloadSessionDidStartCb didStartCb,
        xftpDownloadSessionFailedStateCb failedStateCb, xftpDownloadSessionDidChangedCb didChangedCb,
        xftpDownloadSessionReceivedDataCb receivedDataCb, xftpDownloadSessionDidFinishedCb didFinishedCb);

int startDownload();
int startDownloadSession(int sessionNumber);
int stopDownload();
int stopDownloadSession(int sessionNumber);
int initXtvfDeMux(const char *xtvfpath, int isLocal, long uidn, long ssrc, 
	int media_type, int session_number);
int XtvfDeMux(int session_number, FRAME_BUFFER *frame_info_p);
void XtvfDemuxCloseSession(int session_number);
void XtvfDemuxClose();
long getAvaliblePos(int session_number);
long getAvailableTs(int session_number, long fromMs);
long getAvailableTsByFilePath(const char *local_filepath, int source_server_port);
long rewindDemuxPos(int session_number, long startMs, long fromMs);
long rewindDemuxFilePos(int session_number, long startPos);
long getLastTimestampBySessionNumber(int session_number);
long getLastTimestampByPath(const char *local_filepath);
int fixXtvfFile(const char *fullFilePath);
int closeXtvfFile();

#ifdef __cplusplus
    }
#endif

#endif
