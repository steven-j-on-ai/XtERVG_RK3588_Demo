#include "cbuf.h"

int cbuf_init(cbuf_t *c)
{
	int i = 0;

	if(!c) {
		fprintf(stderr, "[cbuf_init] the cbuf_t is NULL !\n");
		return -1;
	}

	if(pthread_mutex_init(&c->mutex, NULL)) {
		fprintf(stderr, "[cbuf_init] init fail ! mutex init fail !\n");
		return -2;
	}
	if(pthread_cond_init(&c->not_full, NULL)) {
		fprintf(stderr, "[cbuf_init] fail! cond not full init fail !\n");
		pthread_mutex_destroy(&c->mutex);
		return -3;
	}
	if(pthread_cond_init(&c->not_empty, NULL)) {
		fprintf(stderr, "[cbuf_init] fail ! cond not empty init fail !\n");
		pthread_cond_destroy(&c->not_full);
		pthread_mutex_destroy(&c->mutex);
		return -4;
	}

	for(; i < c->capacity; i++){
		c->idx[i] = -1;
	}
	c->size	= 0;
	c->next_in = 0;
	c->next_out = 0;
	c->capacity = C_BUF_MAX;
	c->inited = 1;
	fprintf(stderr, "[cbuf_init] success! p = %p\n", c);
	
	return 0;
}
void cbuf_destroy(cbuf_t *c)
{
	if(!c || !c->inited){
		fprintf(stderr, "[cbuf_destroy] the cbuf_t has been destroyed !\n");
		return;
	}

	c->inited = 0;
	pthread_cond_destroy(&c->not_empty);
	pthread_cond_destroy(&c->not_full);
	pthread_mutex_destroy(&c->mutex);

	fprintf(stderr, "[cbuf_destroy] success c = %p\n", c);
}
int cbuf_enqueue(cbuf_t *c, int idx)
{
	if (!c || !c->inited || (idx < 0 && idx != C_BUF_END)) {
		fprintf(stderr, "[cbuf_enqueue_new] return -1!!!, c = %p, c->inited = %d, idx = %d\n", c, c ? c->inited : 0, idx);
		return -1;
	}

	// fprintf(stderr, "[cbuf_enqueue] start ....................... idx = %d!!!\n", idx);
	while(pthread_mutex_lock(&c->mutex)){
		fprintf(stderr, "[cbuf_enqueue] c is locked !!!\n");
	}

	c->idx[c->next_in] = idx;
	c->next_in++;
	c->next_in %= c->capacity;
	c->size++;
	c->size = (c->size > c->capacity) ? c->capacity : c->size;

	pthread_cond_signal(&c->not_empty);
	pthread_mutex_unlock(&c->mutex);
	// fprintf(stderr, "[cbuf_enqueue] sucesss c->size = %d, c = %p, idx = %d!!!\n", c->size, c, idx);

	return 0;
}
int cbuf_dequeue(cbuf_t *c, int *idx_p)
{
	if (!idx_p || !c || !c->inited) {
		fprintf(stderr, "[cbuf_dequeue] idx_p = %p, c = %p, c->inited = %d, return -1\n", idx_p, c, c ? c->inited : 0);
		return -1;
	}

	// fprintf(stderr, "[cbuf_dequeue] start .......................!!!\n");
	pthread_mutex_lock(&c->mutex);
	while(cbuf_empty(c)){
		pthread_cond_wait(&c->not_empty, &c->mutex);
	}
	// if(c->idx[c->next_out] == -1){
	// 	c->size = 0;
	// 	pthread_mutex_unlock(&c->mutex);
	// 	return -2;
	// }
	
	*idx_p = c->idx[c->next_out];
	// c->idx[c->next_out] = -1;
	c->next_out++;
	c->next_out %= c->capacity;
	c->size--;
	c->size = (c->size < 0) ? 0 : c->size;
	pthread_cond_signal(&c->not_full);
	pthread_mutex_unlock(&c->mutex);
	// fprintf(stderr, "[cbuf_dequeue] sucesss c->size = %d, c = %p, *idx_p = %d!!!\n", c->size, c, *idx_p);
	 
	return 0;
}
int cbuf_full(cbuf_t  *c)
{
	return (c->size == c->capacity);
}
int cbuf_empty(cbuf_t *c)
{
	return (c->size == 0);
}
int cbuf_capacity(cbuf_t *c)
{
	return c->capacity;
}
