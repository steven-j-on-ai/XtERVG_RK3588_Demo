#include "config_file.h"

char g_app_key[CONFIG_ITEM_LEN];
char g_app_secret[CONFIG_ITEM_LEN];
char g_app_license[CONFIG_ITEM_LEN];

static char *l_trim(char *szOutput, const char *szInput)
{
	assert(szInput != NULL);
	assert(szOutput != NULL);
	assert(szOutput != szInput);
	for(NULL; *szInput != '\0' && isspace(*szInput); ++szInput) {
		;
	}
	return strcpy(szOutput, szInput);
}
static char *r_trim(char *szOutput, const char *szInput)
{
	char *p = NULL;
	assert(szInput != NULL);
	assert(szOutput != NULL);
	assert(szOutput != szInput);
	strcpy(szOutput, szInput);
	for(p = szOutput + strlen(szOutput) - 1; p >= szOutput && isspace(*p); --p){
		;
	}
	*(++p) = '\0';
	return szOutput;
}
static char *a_trim(char *szOutput, const char *szInput)
{
	char *p = NULL;
	assert(szInput != NULL);
	assert(szOutput != NULL);
	l_trim(szOutput, szInput);
	for(p = szOutput + strlen(szOutput) - 1;p >= szOutput && isspace(*p); --p){
		;
	}
	*(++p) = '\0';
	return szOutput;
}
static int GetProfileString(const char *profile, const char *AppName, const char *KeyName, char *KeyVal)
{
	char appname[32], keyname[32];
	char *buf, *c;
	char buf_i[KEYVALLEN], buf_o[KEYVALLEN];
	FILE *fp;
	int found = 0;

	if((fp = fopen(profile,"r")) == NULL){
		return -1;
	}
	fseek(fp, 0, SEEK_SET);
	memset(appname, 0, sizeof(appname));
	sprintf(appname, "[%s]", AppName);

	while(!feof(fp) && fgets(buf_i, KEYVALLEN, fp) != NULL){
		l_trim(buf_o, buf_i);
		if(strlen(buf_o) <= 0)
			continue;
		buf = NULL;
		buf = buf_o;

		if(found == 0){
			if(buf[0] != '[') {
				continue;
			} else if (strncmp(buf,appname, strlen(appname)) == 0){
				found = 1;
				continue;
			}
		} else if(found == 1){
			if(buf[0] == '#'){
				continue;
			} else if (buf[0] == '[') {
				break;
			} else {
				if((c = (char*)strchr(buf, '=')) == NULL)
					continue;
				memset(keyname, 0, sizeof(keyname));
				sscanf(buf, "%[^=|^ |^\t]", keyname);
				if(strcmp(keyname, KeyName) == 0){
					sscanf(++c, "%[^\n]", KeyVal);
					char *KeyVal_o = (char *)malloc(strlen(KeyVal) + 1);
					if(KeyVal_o != NULL){
						memset(KeyVal_o, 0, strlen(KeyVal) + 1);
						a_trim(KeyVal_o, KeyVal);
						if(KeyVal_o && strlen(KeyVal_o) > 0)
							strcpy(KeyVal, KeyVal_o);
						free(KeyVal_o);
						KeyVal_o = NULL;
					}
					found = 2;
					break;
				} else {
					continue;
				}
			}
		}
	}
	fclose(fp);
	if(found == 2){
		return 0;
	}
	else
		return -1;
}

int load_config(void)
{
	char value[KEYVALLEN] = {0};

	if(GetProfileString(CONFIG_FILEPATH, "app", KEY_APP_KEY, value) != 0){
		return -1;
	}
	strcpy(g_app_key, value);
	
	if(GetProfileString(CONFIG_FILEPATH, "app", KEY_APP_SECRET, value) != 0){
		return -2;
	}
	strcpy(g_app_secret, value);

	if (GetProfileString(CONFIG_FILEPATH, "app", KEY_APP_LICENSE, value) != 0) {
		return -3;
	}
	strcpy(g_app_license, value);

	return 0;
}
