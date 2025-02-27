#ifndef __CBUF_H__
#define __CBUF_H__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// #define C_BUF_MAX 16
#define C_BUF_MAX 32
#define C_BUF_END -250

typedef struct _cbuf
{
	int size;					/* 当前缓冲区中存放的数据的个数 */
	int next_in;				/* 缓冲区中下一个保存数据的位置 */
	int next_out;				/* 从缓冲区中取出下一个数据的位置 */
	int capacity;				/* 这个缓冲区的可保存的数据的总个数 */

	pthread_mutex_t mutex;		/* Lock the structure */
	pthread_cond_t not_full;	/* Full -> not full condition */
	pthread_cond_t not_empty;	/* Empty -> not empty condition */

	int idx[C_BUF_MAX];		/* 缓冲区中保存数据块序号数组 */
	int inited; 
} cbuf_t;

#ifdef __cplusplus
extern "C" {
#endif

int cbuf_init(cbuf_t *c);				/* 初始化环形缓冲区 */
void cbuf_destroy(cbuf_t *c);			/* 销毁环形缓冲区 */
int cbuf_enqueue(cbuf_t *c, int idx);	/* 压入数据 */
int cbuf_dequeue(cbuf_t *c, int *idx_p);	/* 弹出数据 */
int cbuf_full(cbuf_t *c);				/* 判断缓冲区是否为满 */
int cbuf_empty(cbuf_t *c);				/* 判断缓冲区是否为空 */
int cbuf_capacity(cbuf_t *c);			/* 获取缓冲区可存放的元素的总个数 */

#ifdef __cplusplus
}
#endif

#endif
