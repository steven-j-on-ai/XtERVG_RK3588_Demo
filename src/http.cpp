#include "http.h"
#include "config.h"

int get_now(char *buf, int len)
{
	struct timeval tv;
	struct tm *tm;

	if (gettimeofday(&tv, NULL) == -1) {
		return -1;
	}
	if ((tm = localtime((const time_t *)&tv.tv_sec)) == NULL) {
		return -2;
	}

	snprintf(buf, len, "%d-%02d-%02d %02d:%02d:%02d", 1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	return 0;
}
char *urlEncode(const char *str)
{
	const char *hex = "0123456789ABCDEF";
	size_t len = strlen(str);
	char *encoded = (char *)malloc(3 * len + 1);
	size_t j = 0;
	for (size_t i = 0; i < len; i++) {
		if (isalnum((unsigned char)str[i]) || str[i] == '-' || str[i] == '_' || str[i] == '.' || str[i] == '~') {
			encoded[j++] = str[i];
		} else if (str[i] == ' ') {
			encoded[j++] = '+';
		} else {
			encoded[j++] = '%';
			encoded[j++] = hex[(str[i] >> 4) & 0xF];
			encoded[j++] = hex[str[i] & 0xF];
		}
	}
	encoded[j] = '\0';
	return encoded;
}
int get_upgrade_info(cJSON *mix, void *result)
{
	int i, len, flag = 0, ret = -1;
	cJSON *data, *id, *ip, *port, *item, *obj;
	upgrade_t *r = (upgrade_t *)result;
	config_t *config = r->c;
	CHANNELS *channels = r->ch;

	if(!cJSON_IsNull(mix) && cJSON_IsObject(mix)) {
		data = cJSON_GetObjectItemCaseSensitive(mix, "obj");
		if(!cJSON_IsNull(data) && cJSON_IsObject(data)) {
			item = cJSON_GetObjectItemCaseSensitive(data, "device_no");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				if(strcmp(config->device_no, item->valuestring)){
					return -2;
				}
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "device_pwd");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				strcpy(config->device_pwd, item->valuestring);
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "device_name");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				strcpy(config->device_name, item->valuestring);
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "channel_num");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->channel_num = item->valueint;
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "is_actived");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->is_acitved = item->valueint;
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "is_mp4");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->is_mp4 = item->valueint;
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "max_space");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->max_space = item->valueint;
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "ver");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				strcpy(config->ver, item->valuestring);
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "is_ai");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->is_ai = item->valueint;
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "is_rate");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->is_rate = item->valueint;
			}

			item = cJSON_GetObjectItemCaseSensitive(data, "server_no");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				strcpy(config->sip_no, item->valuestring);
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "device_sip");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				strcpy(config->user_id, item->valuestring);
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "password");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				strcpy(config->sip_pwd, item->valuestring);
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "expiry");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->expiry = item->valueint;
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "heartbeat");
			if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
				config->heartbeat = item->valueint;
			}
			item = cJSON_GetObjectItemCaseSensitive(data, "server_domain");
			if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
				strcpy(config->server_domain, item->valuestring);
			}
			flag++;
		}else{
			return -3;
		}

		data = cJSON_GetObjectItemCaseSensitive(mix, "server_list");
		if (!cJSON_IsNull(data) && cJSON_IsArray(data)) {
			len = cJSON_GetArraySize(data);
			if(!len){
				return -4;
			}
			for(i = 0; i < len; i++){
				obj = cJSON_GetArrayItem(data, i);
				if(!cJSON_IsNull(obj) && cJSON_IsObject(obj)) {
					item = cJSON_GetObjectItemCaseSensitive(obj, "type");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						id = cJSON_GetObjectItemCaseSensitive(obj, "id");
						ip = cJSON_GetObjectItemCaseSensitive(obj, "ip");
						port = cJSON_GetObjectItemCaseSensitive(obj, "port");
						if (cJSON_IsNull(id) || !cJSON_IsNumber(id)) {
							continue;
						}
						if (cJSON_IsNull(ip) || !cJSON_IsString(ip) || !ip->valuestring) {
							continue;
						}
						if (cJSON_IsNull(port) || !cJSON_IsNumber(port)) {
							continue;
						}
						if(!strcmp(item->valuestring, "web")){
							strcpy((char *)config->web_ip, ip->valuestring);
							config->web_port = port->valueint;
							config->web_id = id->valueint;
						}else if(!strcmp(item->valuestring, "msg")){
							strcpy((char *)config->msg_ip, ip->valuestring);
							config->msg_port = port->valueint;
							config->msg_id = id->valueint;
						}else if(!strcmp(item->valuestring, "sip")){
							strcpy((char *)config->sip_ip, ip->valuestring);
							config->sip_port = port->valueint;
							config->sip_id = id->valueint;
						}else if(!strcmp(item->valuestring, "video")){
							strcpy((char *)config->video_ip, ip->valuestring);
							config->video_port = port->valueint;
							config->video_id = id->valueint;
						}
					}
				}
			}
			flag++;
		}else{
			return -5;
		}

		data = cJSON_GetObjectItemCaseSensitive(mix, "channel_list");
		if (!cJSON_IsNull(data) && cJSON_IsArray(data)) {
			len = cJSON_GetArraySize(data);
			if(!len){
				return flag;
			}
			channels->channel = (channel_t *)calloc(len, sizeof(channel_t));
			for(i = 0; i < len; i++){
				obj = cJSON_GetArrayItem(data, i);
				if(!cJSON_IsNull(obj) && cJSON_IsObject(obj)) {
					item = cJSON_GetObjectItemCaseSensitive(obj, "id");
					if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
						channels->channel[i].channel_id = item->valueint;
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "is_gb28181");
					if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
						channels->channel[i].is_gb28181 = item->valueint;
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "is_open_rate");
					if (!cJSON_IsNull(item) && cJSON_IsNumber(item)) {
						channels->channel[i].is_open_rate = item->valueint;
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "device_no");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						strcpy(channels->channel[i].channel_no, item->valuestring);
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "device_pwd");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						strcpy(channels->channel[i].channel_pwd, item->valuestring);
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "device_sip");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						strcpy(channels->channel[i].channel_sip, item->valuestring);
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "device_name");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						strcpy(channels->channel[i].channel_name, item->valuestring);
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "protocol");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						strcpy(channels->channel[i].protocol, item->valuestring);
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "stream_url");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						strcpy(channels->channel[i].stream_url, item->valuestring);
					}
					item = cJSON_GetObjectItemCaseSensitive(obj, "channel_desc");
					if (!cJSON_IsNull(item) && cJSON_IsString(item) && (item->valuestring != NULL)) {
						strcpy(channels->channel[i].channel_desc, item->valuestring);
					}
				}
			}
			channels->size = len;
			flag++;
		}else{
			return -7;
		}
		ret = flag;
	}

	return ret;
}
size_t WriteCb(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t total_size = size * nmemb;
	char **response_ptr = (char**)userp;

	// 动态重新分配内存以容纳新数据
	*response_ptr = (char *)realloc(*response_ptr, strlen(*response_ptr) + total_size + 1);
	if(*response_ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		return 0;
	}

	// 追加新数据到响应缓冲区末尾
	strncat(*response_ptr, (char *)contents, total_size);

	return total_size;
}
int httpRequest(const char *url, int(*cb)(cJSON *obj, void *result), void *arg)
{
	CURL *curl;
	CURLcode res;
	char *response = (char *)calloc(1, sizeof(char)); // 初始化响应缓冲区
	int is_success = -1;

	// 确保分配内存成功
	if (!url || response == NULL) {
		fprintf(stderr, "calloc() failed\n");
		return -1;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		} else {
			// fprintf(stderr, "Response: %s\n", response);

			// 解析 JSON 数据
			cJSON *json = cJSON_Parse(response);
			if (json == NULL) {
				fprintf(stderr, "cJSON_Parse() failed\n");
			} else {
				cJSON *msg = cJSON_GetObjectItemCaseSensitive(json, "msg");
				cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
				cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");

				if (!cJSON_IsNull(msg) && cJSON_IsString(msg) && (msg->valuestring != NULL)) {
					// fprintf(stderr, "MSG: %s\n", msg->valuestring);
				} else {
					// fprintf(stderr, "MSG is NULL\n");
				}

				if (!cJSON_IsNull(code) && cJSON_IsNumber(code)) {
					if(code->valueint == 200){
						is_success = 0;
					}else{
						fprintf(stderr, "CODE: %d\n", code->valueint);
					}
				} else {
					fprintf(stderr, "CODE is NULL\n");
				}

				if(!is_success){
					if(!cJSON_IsNull(data) && cJSON_IsObject(data)) {
						cJSON *flag = cJSON_GetObjectItemCaseSensitive(data, "flag");
						cJSON *obj = NULL;
						cJSON *list = NULL;
						if(!cJSON_IsNull(flag) && cJSON_IsNumber(flag)){
							switch(flag->valueint){
								case 1:
									obj = cJSON_GetObjectItemCaseSensitive(data, "list");
									break;
								case 2:
									obj = cJSON_GetObjectItemCaseSensitive(data, "obj");
									break;
								case 3:
									is_success = 1;
									break;
								case 4:
									obj = cJSON_GetObjectItemCaseSensitive(data, "mix");
									break;
								default:
									fprintf(stderr, "flag is unknown \n");
									is_success = -1;
									break;
							}
							if(!is_success && obj){
								if(!cb){
									is_success = 1;
								}else{
									is_success = cb(obj, arg); // 1 - success; <0 - fail
								}
							}
						}else{
							fprintf(stderr, "data is error object \n");
							is_success = -1;
						}
					}else if (!cJSON_IsNull(data) && cJSON_IsString(data) && (data->valuestring != NULL)) {
						if(cb) {
							is_success = cb(data, arg);
						}
					}else{
						fprintf(stderr, "data is NULL \n");
						is_success = -1;
					}
				}
				cJSON_Delete(json);
			}
		}
		curl_easy_cleanup(curl);
	}
	free(response);
	curl_global_cleanup();

	return is_success;
}
