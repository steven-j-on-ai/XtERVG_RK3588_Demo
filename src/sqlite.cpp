#include "sqlite.h"

char g_db_name[] = "/usr/local/xt/db/xt.db";

int write_data(char *sql)
{
	int rt = -1;
	sqlite3 *db;
	char *errmsg;

	if(sqlite3_open(g_db_name, &db)){
		sqlite3_close(db);
		return -1;
	}
	do{
		rt = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
		if(rt == SQLITE_OK){
			break;
		}
		if (rt == SQLITE_BUSY) {
			sleep(1);
			continue;
		}
		sqlite3_close(db);
		return -2;
	}while(1);

	if(sqlite3_close(db)){
		return -3;
	}

	return 0;
}
int data_cb(void *arg, int n, char **colValue, char **colName)
{
	int i, cur;
	TABLE_DATA *data;

	data = (TABLE_DATA *)arg;
	cur = data->current;
	for(i = 0; i < n; i++){
		strcpy(data->lines[cur].fields[i].col, colName[i]);
		strcpy(data->lines[cur].fields[i].val, colValue[i]);
	}
	data->lines[cur].size = n;
	data->current++;
	return 0;
}
int select_data(char *sql, void *data)
{
	int rt = -1;
	sqlite3 *db;
	char *errmsg;

	if(sqlite3_open(g_db_name, &db)){
		sqlite3_close(db);
		return -1;
	}

	do{
		rt = sqlite3_exec(db, sql, data_cb, data, &errmsg);
		if(rt == SQLITE_OK){
			break;
		}
		if (rt == SQLITE_BUSY) {
			sleep(1);
			continue;
		}
		sqlite3_close(db);
		return -2;
	}while(1);

	if(sqlite3_close(db)){
		return -3;
	}
	return 0;
}
int read_data(TABLE_DATA *data, char *select_sql)
{
	int rt, count = 1;

	if(!data || !select_sql){
		return -1;
	}

	data->lines = (TABLE_LINE *)calloc(count, sizeof(TABLE_LINE));
	data->current = 0;

	rt = select_data(select_sql, data);
	if(rt || !data->current){
		free(data->lines);
		return -2;
	}
	data->count = count;

	return 0;
}
int read_list(TABLE_DATA *data, char *count_sql, char *select_sql)
{
	int rt, count;

	if(!data || !count_sql || !select_sql){
		return -1;
	}
	rt = read_data(data, count_sql);
	if(rt){
		return -2;
	}
	count = atoi(data->lines[0].fields[0].val);
	free(data->lines);
	if(count < 1){
		return -3;
	}

	data->lines = (TABLE_LINE *)calloc(count, sizeof(TABLE_LINE));
	data->current = 0;

	rt = select_data(select_sql, data);
	if(rt || !data->current){
		free(data->lines);
		return -4;
	}
	data->count = count;

	return 0;
}
