#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_LEN 128

typedef struct
{
	char web_ip[CONFIG_LEN];
	int web_port;
	int web_id;
	char video_ip[CONFIG_LEN];
	int video_port;
	int video_id;
	char msg_ip[CONFIG_LEN];
	int msg_port;
	int msg_id;

	int sip_id;
	int sip_server_id;
	char sip_ip[CONFIG_LEN];
	int sip_port;
	char sip_no[CONFIG_LEN];
	char sip_pwd[CONFIG_LEN];
	int expiry;
	int heartbeat;
	char user_id[CONFIG_LEN];
	char server_domain[CONFIG_LEN];

	int device_id;
	char device_no[CONFIG_LEN];
	char device_pwd[CONFIG_LEN];
	char device_name[CONFIG_LEN];
	int channel_num;
	int is_acitved;
	int is_ai;
	int is_rate;
	int is_mp4;
	int max_space;
	char ver[CONFIG_LEN];

	char channel_no[CONFIG_LEN];
	char channel_pwd[CONFIG_LEN];
} config_t;

typedef struct
{
	int channel_id;
	int is_gb28181;
	int is_open_rate;
	char channel_no[CONFIG_LEN];
	char channel_pwd[CONFIG_LEN];
	char channel_sip[CONFIG_LEN];
	char channel_name[CONFIG_LEN];
	char protocol[CONFIG_LEN];
	char stream_url[CONFIG_LEN * 2];
	char channel_desc[1024];
} channel_t;

typedef struct
{
	channel_t *channel;
	int size;
} CHANNELS;

typedef struct
{
	config_t *c;
	CHANNELS *ch;
} upgrade_t;

#endif
