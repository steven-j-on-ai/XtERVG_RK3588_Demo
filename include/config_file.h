#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <syslog.h>

#define CONFIG_FILEPATH "/usr/local/xt/conf/xt.conf"

#define KEYVALLEN 512
#define CONFIG_ITEM_LEN 128

#define KEY_APP_KEY "app_key"
#define KEY_APP_SECRET "app_secret"
#define KEY_APP_LICENSE "app_license"

extern char g_app_key[];
extern char g_app_secret[];
extern char g_app_license[];

int load_config(void);

#endif
