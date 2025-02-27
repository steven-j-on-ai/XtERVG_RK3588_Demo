/*
 * @Author: wq<779508400@qq.com>
 * @Date: 2021-11-03 09:54:42
 * @LastEditTime: 2021-11-03 11:18:01
 * @LastEditors: Please set LastEditors
 * @Description: string slit for delims
 * @FilePath: /string_split/src/string_split.c
 */

#include <string.h>

#include "string_split.h"

int init_string_split_handler(StringSplitHandler *handler_p)
{
	if (!handler_p) {
		return -1;
	}

	handler_p->delimnumber_head = NULL;
	handler_p->delimnumber_end = NULL;
	handler_p->delimnumber_next = NULL;
	return 0;
}
static void delimnumber_expand(int value, StringSplitHandler *handler_p)
{
	DelimNumber *delimnumber = NULL;

	if (!handler_p) {
		return;
	}

	delimnumber = (DelimNumber*) malloc(sizeof(DelimNumber));
	delimnumber->value = value;
	delimnumber->next = NULL;

	if (handler_p->delimnumber_head == NULL) {
		handler_p->delimnumber_head = delimnumber;
	} else {
		handler_p->delimnumber_end->next = delimnumber;
	}
	handler_p->delimnumber_end = delimnumber;
}
static void delimnumber_free(StringSplitHandler *handler_p)
{
	DelimNumber *p;
	if (!handler_p) {
		return;
	}

	while(handler_p->delimnumber_head->next)
	{
		p = handler_p->delimnumber_head;
		handler_p->delimnumber_head = p->next;
		free(p);
	}
	handler_p->delimnumber_head = NULL;
	handler_p->delimnumber_end = NULL;
	handler_p->delimnumber_next = NULL;
}
static void delimnumber_next_value(StringSplitHandler *handler_p)
{
	if (!handler_p) {
		return;
	}

	if(handler_p->delimnumber_next->next == NULL)
		return;
	handler_p->delimnumber_next = handler_p->delimnumber_next->next;
}
static int get_delim_number(char* src_strings, char delim, StringSplitHandler *handler_p)
{
	int delim_number = 0, i = 0;
	if (!handler_p) {
		return -1;
	}

	delimnumber_expand(i, handler_p);
	while((*src_strings) && (*src_strings) != '\0')
	{
		if(*src_strings == delim)
		{
			delim_number++;
			delimnumber_expand(i + 1, handler_p);
		}
		i++;
		src_strings++;
	}
	return delim_number;
}
static int get_delims_number(char *src_strings, char *delims, StringSplitHandler *handler_p)
{
	int delims_length = 0;
	int delim_number = 0, i = 0;
	int j;
	if (delims == NULL || !handler_p || !src_strings)
		return 0;
	delims_length = strlen(delims);

	delimnumber_expand(i, handler_p);
	
	while((*src_strings) && (*src_strings) != '\0')
	{
		for(j = 0; j < delims_length; j++)
		{
			if(*src_strings == delims[j])
			{
				delim_number++;
				delimnumber_expand(i + 1, handler_p);
				break;
			}
		}
		i++;
		src_strings++;
	}
	return delim_number;
}
StringSplit* string_split_handle(char delim, char *src, StringSplitHandler *handler_p)
{
	int i;
	int delim_number;
	int sub_str_number;
	int length;
	int size;
	StringSplit* string_split;
	StringSplitItem* item;

	if (!handler_p) {
		return NULL;
	}

	delim_number = get_delim_number(src, delim, handler_p);
	sub_str_number = (handler_p->delimnumber_end->value - 1 == (strlen(src) - 1 )) ? delim_number : delim_number + 1;
	length = strlen(src);
	size = length - delim_number + 2 * sizeof(int) + 3 * sub_str_number * sizeof(int);
	string_split = (StringSplit *)malloc(sizeof(StringSplit));

	#ifdef RUN_DEBUG
		printf("delim_number:%d, sub_str_number:%d\n", delim_number, sub_str_number);
	#endif

	string_split->size = size;
	string_split->number = sub_str_number;
	string_split->items = (StringSplitItem **)calloc(sub_str_number, sizeof(StringSplitItem*));

	handler_p->delimnumber_next = handler_p->delimnumber_head;
	for (i = 0; i < sub_str_number; i++) {
		string_split->items[i] = (StringSplitItem*)malloc(sizeof(StringSplitItem));
		item = string_split->items[i];
		item->pos = handler_p->delimnumber_next->value;
		item->length = (i + 1 == sub_str_number) ? length - item->pos : handler_p->delimnumber_next->next->value - handler_p->delimnumber_next->value - 1;
		item->size = 3 * sizeof(int) + item->length;
		item->str = (char *)calloc((size_t)item->length + 1, sizeof(char));
		memset(item->str, 0, item->length + 1);
		memcpy(item->str, src + item->pos, item->length);
		#ifdef RUN_DEBUG
			printf("index:%d, pos:%d, length:%d, size:%d, str:%s\n", i, item->pos, item->length, item->size, item->str);
		#endif
		delimnumber_next_value(handler_p);
	}
	#ifdef RUN_DEBUG
		printf("Ending Split\n");
	#endif
	return  string_split;
}
StringSplit* string_delims_split_handle(char* delims, char *src, StringSplitHandler *handler_p)
{
	int delim_number;
	int i;
	int sub_str_number;
	int length;
	int size;
	StringSplit *string_split;
	StringSplitItem *item;

	if (!src || ! delims || !handler_p) {
		return NULL;
	}
	delim_number = get_delims_number(src, delims, handler_p);
	sub_str_number = (handler_p->delimnumber_end->value - 1 == (strlen(src) - 1 )) ? delim_number : delim_number + 1;
	length = strlen(src);
	size = length - delim_number + 2 * sizeof(int) + 3 * sub_str_number * sizeof(int);
	string_split = (StringSplit*)malloc(sizeof(StringSplit));

	#ifdef RUN_DEBUG
		printf("delim_number:%d, sub_str_number:%d\n", delim_number, sub_str_number);
	#endif

	string_split->size = size;
	string_split->number = sub_str_number;
	string_split->items = (StringSplitItem **)calloc(sub_str_number, sizeof(StringSplitItem*));
	handler_p->delimnumber_next = handler_p->delimnumber_head;
	for (i = 0; i < sub_str_number; i++) {
		string_split->items[i] = (StringSplitItem*)malloc(sizeof(StringSplitItem));
		item = string_split->items[i];
		item->pos = handler_p->delimnumber_next->value;
		item->length = (i + 1 == sub_str_number) ? length - item->pos : handler_p->delimnumber_next->next->value - handler_p->delimnumber_next->value - 1;
		item->size = 3 * sizeof(int) + item->length;
		item->str = (char *)calloc((size_t)item->length + 1, sizeof(char));
		memset(item->str, 0, item->length + 1);
		memcpy(item->str, src + item->pos, item->length);
		#ifdef RUN_DEBUG
			printf("index:%d, pos:%d, length:%d, size:%d, str:%s\n", i, item->pos, item->length, item->size, item->str);
		#endif
		delimnumber_next_value(handler_p);
	}
	#ifdef RUN_DEBUG
		printf("Ending Split\n");
	#endif
	return string_split;
}
void string_split_free(StringSplit *splits, StringSplitHandler *handler_p)
{
	int i;
	if (splits == NULL || !handler_p) {
		return;
	}
	if (splits->number > 0 && splits->items != NULL) {
		for (i = 0; i < splits->number; i++) {
			free(splits->items[i]->str);
			free(splits->items[i]);
		}
	}
	free(splits);
	delimnumber_free(handler_p);
}
void string_split_output_test(StringSplit *splits)
{
	int i;
	StringSplitItem *item;
	
	if (!splits) {
		return;
	}

	printf("splits: number:%d, size:%d \n", splits->number, splits->size);
	if (splits->items != NULL) {
		for (i = 0; i < splits->number; i++) {
			item = splits->items[i];
			if (item != NULL) {
				printf("output: index:%d, pos:%d, length:%d, size:%d, str:%s\n", i, item->pos, item->length, item->size, item->str);
			} else {
				printf("item:%d is NULL\n", i);
			}
		}
	} else{
		printf("splits->item == NULL\n");
	}
}
