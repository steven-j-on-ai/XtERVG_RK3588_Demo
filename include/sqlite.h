#ifndef __SQLITE_H_
#define __SQLITE_H_

#include <stdio.h>
#include <sqlite3.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define COLUMN_SIZE 32
#define VALUE_SIZE 512
#define FIELD_SIZE 8

typedef struct
{
	char col[COLUMN_SIZE];
	char val[VALUE_SIZE];
} TABLE_FIELD;
typedef struct
{
	TABLE_FIELD fields[FIELD_SIZE];
	int size;
} TABLE_LINE;
typedef struct
{
	TABLE_LINE *lines;
	int count;
	int current;
} TABLE_DATA;

extern char g_db_name[];

int write_data(char *sql);
int read_data(TABLE_DATA *data, char *sql);
int read_list(TABLE_DATA *data, char *count_sql, char *select_sql);

#endif
