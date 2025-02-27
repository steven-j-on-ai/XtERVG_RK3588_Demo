#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "annotation_info.h"

#define NUM_MAX 20

ANNOTATION_ITEM_SET g_annotation_item_set = ANNOTATION_ITEM_SET_INITILIZER;

int init_annotation_item_arr(ANNOTATION_ITEM_SET *set_p)
{
	if (!set_p) {
		return -1;
	}
	memset(set_p, 0, sizeof(ANNOTATION_ITEM_SET));
	return 0;
}

int reset_annotation_item_arr(ANNOTATION_ITEM_SET *set_p)
{
	if (!set_p) {
		return -1;
	}
	set_p->annotion_item_len = 0;
	return 0;
}

int add_annotioan_item_to_set(ANNOTATION_ITEM_SET *set_p, ANNOTATION_ITEM *item_p)
{
	if (!set_p || !item_p) {
		return -1;
	}
	if (set_p->annotion_item_len >= ANNOTATION_ARR_LEN) {
		return -2;
	}
	set_p->annotation_item_arr[set_p->annotion_item_len] = *item_p;
	set_p->annotion_item_len++;
	return 0;
}

int encode_annotation_set_buff(const ANNOTATION_ITEM_SET *set_p, uint8_t *anno_buff)
{
	int i = 0, len = 0;
	uint8_t *p = anno_buff;
	uint32_t anno_ts = 0;
	uint32_t n_u32 = 0;
	uint16_t n_u16 = 0;

	if (!set_p || !anno_buff) {
		fprintf(stderr, "[encode_annotation_set_buff] failed 1\n");
		return 0;
	}
	if (!set_p->annotion_item_len) {
		fprintf(stderr, "[encode_annotation_set_buff] failed 2\n");
		return 0;
	}

	anno_ts = htonl(set_p->anno_ts);
	memcpy(p, (uint8_t *)&anno_ts, sizeof(uint32_t));
	len = sizeof(uint32_t);
	for (i = 0; i < set_p->annotion_item_len; i++) {
		n_u16 = htons(set_p->annotation_item_arr[i].id);
		memcpy(&p[len], (uint8_t *)&n_u16, sizeof(uint16_t));
		len += sizeof(uint16_t);

		n_u32 = htonl(set_p->annotation_item_arr[i].conf_level_1m);
		memcpy(&p[len], (uint8_t *)&n_u32, sizeof(uint32_t));
		len += sizeof(uint32_t);

		n_u32 = htonl(set_p->annotation_item_arr[i].xmin_1m);
		memcpy(&p[len], (uint8_t *)&n_u32, sizeof(uint32_t));
		len += sizeof(uint32_t);
	
		n_u32 = htonl(set_p->annotation_item_arr[i].ymin_1m);
		memcpy(&p[len], (uint8_t *)&n_u32, sizeof(uint32_t));
		len += sizeof(uint32_t);
		
		n_u32 = htonl(set_p->annotation_item_arr[i].xmax_1m);
		memcpy(&p[len], (uint8_t *)&n_u32, sizeof(uint32_t));
		len += sizeof(uint32_t);
		
		n_u32 = htonl(set_p->annotation_item_arr[i].ymax_1m);
		memcpy(&p[len], (uint8_t *)&n_u32, sizeof(uint32_t));
		len += sizeof(uint32_t);
	}

	return len;
}

int decode_annotation_buff(const uint8_t *anno_buff, int anno_buff_len, ANNOTATION_ITEM_SET *set_p)
{
	int i = 0;
	uint8_t *p = (uint8_t *)anno_buff;
	uint32_t *p_u32 = NULL;
	uint16_t *p_u16 = NULL;
	int len = 0, item = 0;

	if (!set_p || !anno_buff || anno_buff_len < 4) {
		fprintf(stderr, "[decode_annotation_buff] failed 1\n");
		return 0;
	}
	
	if ((anno_buff_len - 4) % sizeof(ANNOTATION_ITEM)) {
		fprintf(stderr, "[decode_annotation_buff] failed 2 \n");
		return 0;   
	}

	item = (anno_buff_len - 4) / sizeof(ANNOTATION_ITEM);

	p_u32 = (uint32_t *)&p[len];
	set_p->anno_ts = ntohl(*p_u32);
	len += sizeof(uint32_t);

	for (i = 0; i < item; i++) {
		p_u16 = (uint16_t *)&p[len];
		set_p->annotation_item_arr[i].id = ntohs(*p_u16);
		len += sizeof(uint16_t);

		p_u32 = (uint32_t *)&p[len];
		set_p->annotation_item_arr[i].conf_level_1m = ntohl(*p_u32);
		len += sizeof(uint32_t);

		p_u32 = (uint32_t *)&p[len];
		set_p->annotation_item_arr[i].xmin_1m = ntohl(*p_u32);
		len += sizeof(uint32_t);

		p_u32 = (uint32_t *)&p[len];
		set_p->annotation_item_arr[i].ymin_1m = ntohl(*p_u32);
		len += sizeof(uint32_t);

		p_u32 = (uint32_t *)&p[len];
		set_p->annotation_item_arr[i].xmax_1m = ntohl(*p_u32);
		len += sizeof(uint32_t);

		p_u32 = (uint32_t *)&p[len];
		set_p->annotation_item_arr[i].ymax_1m = ntohl(*p_u32);
		len += sizeof(uint32_t);
	}
	set_p->annotion_item_len = item;

	return item;  
}
