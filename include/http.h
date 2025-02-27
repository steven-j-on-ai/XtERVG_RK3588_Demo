#ifndef _HTTP_UTILS_H
#define _HTTP_UTILS_H

#include <unistd.h>
#include <stdio.h>
#include <curl/curl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <time.h>
#include <ctype.h>

int get_now(char *buf, int len);
char *urlEncode(const char *str);
int get_upgrade_info(cJSON *mix, void *result);
int httpRequest(const char *url, int(*cb)(cJSON *obj, void *result), void *arg);

#endif
