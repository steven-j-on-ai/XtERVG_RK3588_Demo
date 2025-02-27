/*
 * @Author: wq<779508400@qq.com>
 * @Date: 2021-11-03 09:47:52
 * @LastEditTime: 2021-11-03 11:15:49
 * @LastEditors: Please set LastEditors
 * @Description: string slit for delims
 * @FilePath: /string_split/src/string_split.h
 */

#ifndef _STRING_SPLIT_H_
#define _STRING_SPLIT_H_

#include <stdlib.h>
#include <stdio.h>

struct _DelimNumber
{
	int value;
	struct _DelimNumber* next;
};
typedef struct _DelimNumber DelimNumber;

struct  _StringSplitHandler
{
	DelimNumber *delimnumber_head;
	DelimNumber *delimnumber_end;
	DelimNumber *delimnumber_next;	
};
typedef struct _StringSplitHandler StringSplitHandler;

struct _StringSplitItem
{
	int pos;
	int size;
	int length;
	char *str;
};
typedef struct _StringSplitItem StringSplitItem;

struct _StringSplit
{
	int number;
	int size;
	StringSplitItem** items;
};
typedef struct _StringSplit StringSplit;


int init_string_split_handler(StringSplitHandler *handler_p);

/**
 * @name: 字符串分割处理
 * @msg: 
 * @param {char} delim 分隔符
 * @param {char} *src 字符串输入源
 * @return {*} 分隔符结构体
 */
StringSplit* string_split_handle(char delim, char *src, StringSplitHandler *handler_p);

/**
 * @brief 
 * 
 * @param delims 
 * @param src 
 * @return StringSplit* 
 */
StringSplit* string_delims_split_handle(char* delims, char *src, StringSplitHandler *handler_p);
/**
 * @name: 字符串释放
 * @msg: 
 * @param {StringSplit} *splits StringSplit指针
 * @return {*}
 */
void string_split_free(StringSplit *splits, StringSplitHandler *handler_p);
/**
 * @description: 输出字符串打印测试
 * @param {*} 输出字符串指针
 * @return {*}
 */
void string_split_output_test(StringSplit *splits);

#endif //_STRING_SPLIT_H_
