#include <stdio.h>
#include <string.h>

#include "frame_cir_buff.h"

FRAME_CIR_BUFF g_frame_cir_buff;

int frame_cir_buff_init(FRAME_CIR_BUFF *c)
{
    int ret = OPER_OK;
    int i = 0;

    if (!c) {
        return OPER_ERR;
    }
    
    frame_cir_buff_destroy(c);
    if ((ret = mutex_init(&c->mutex)) != OPER_OK) {
    	fprintf(stderr, "[frame_cir_buff_init] init fail ! mutex init fail !\n");
        return ret;
    }
    if ((ret = cond_init(&c->not_full)) != OPER_OK) {
    	fprintf(stderr, "[frame_cir_buff_init] init fail! cond not full init fail !\n");
        mutex_destroy(&c->mutex);
        return ret;
    }
    if ((ret = cond_init(&c->not_empty)) != OPER_OK) {
    	fprintf(stderr, "[frame_cir_buff_init] init fail ! cond not empty init fail !\n");
        cond_destroy(&c->not_full);
        mutex_destroy(&c->mutex);
        return ret;
    }
    c->is_inited = 1;
    c->size = 0;
    c->next_in = 0;
    c->next_out = 0;
    c->capacity = FRAME_INFO_CIRQ_MAX;

    for (i = 0; i < c->capacity; i++) {
    	c->frame_info_arr[i].seqno = 0;
    	c->frame_info_arr[i].timestamp = 0;
    }

    fprintf(stderr, "[frame_cir_buff_init] init success !\n");

    return ret;	
}
void frame_cir_buff_destroy(FRAME_CIR_BUFF *c)
{
    if (!c || !c->is_inited) {
        return;
    }

    c->is_inited = 0;
    cond_destroy(&c->not_empty);
    cond_destroy(&c->not_full);
    mutex_destroy(&c->mutex);
    c->size = 0;
    fprintf(stderr, "[frame_cir_buff_destroy] destroy success \n");	
}
int frame_cir_buff_enqueue(FRAME_CIR_BUFF *c, FRAME_INFO *info_p)
{
    int ret = OPER_OK;
    
    if (!c || !info_p) {
        return -1;
    }	
    if ((ret = mutex_lock(&c->mutex)) != OPER_OK) {
        return ret;
    }

    memcpy(&c->frame_info_arr[c->next_in], info_p, sizeof(FRAME_INFO));
    c->next_in++;
    c->next_in %= c->capacity;
    c->size++;
    c->size = (c->size > c->capacity) ? c->capacity:c->size;

    cond_signal(&c->not_empty);
    mutex_unlock(&c->mutex);
    
    return ret;
}
int frame_cir_buff_dequeue(FRAME_CIR_BUFF *c, FRAME_INFO *info_p)
{
    int ret = OPER_OK;

    if (!c || !info_p) {
        return -1;
    }
    if ((ret = mutex_lock(&c->mutex)) != OPER_OK) {
        return ret;
    }

    while (frame_cir_buff_empty(c)) {
        fprintf(stderr, "==--->frame_cir_buff is empty!!!\n");
        cond_wait(&c->not_empty, &c->mutex);
    }
    memcpy(info_p, &c->frame_info_arr[c->next_out], sizeof(FRAME_INFO));
    c->next_out++;
    c->next_out %= c->capacity;
    c->size--;
    c->size = (c->size < 0) ? 0 : c->size;
    cond_signal(&c->not_full);
    mutex_unlock(&c->mutex);

    return OPER_OK;
}
int frame_cir_buff_full(FRAME_CIR_BUFF    *c) 
{
	return (c->size == c->capacity) ? 1 : 0;
}
int frame_cir_buff_empty(FRAME_CIR_BUFF *c)
{
	return (c->size == 0) ? 1 : 0;
}
int frame_cir_buff_capacity(FRAME_CIR_BUFF *c)
{
	return c->capacity;
}
