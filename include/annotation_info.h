#ifndef _ANNOTATION_INFO_H
#define _ANNOTATION_INFO_H

#include <stdint.h>

#ifdef __cplusplus
    extern "C" {
#endif

#define C_NAME_LEN 256
#define ANNOTATION_ARR_LEN 1024
#define MILLION_BASE 1000000

typedef struct _annotation_item_t {
    uint16_t id;
    uint32_t conf_level_1m;
    uint32_t xmin_1m;
    uint32_t ymin_1m;
    uint32_t xmax_1m;
    uint32_t ymax_1m;
} __attribute__ ((packed)) ANNOTATION_ITEM;

#define ANNOTATION_ITEM_INITILIZER { \
    .id = 0, \
    .conf_level_1m = 0, \
    .xmin_1m = 0, \
    .ymin_1m = 0, \
    .xmax_1m = 0, \
    .ymax_1m = 0 \
}

typedef struct _ANNOTATION_ITEM_SET {
    uint32_t anno_ts;
    int annotion_item_len;
    ANNOTATION_ITEM annotation_item_arr[ANNOTATION_ARR_LEN];
} ANNOTATION_ITEM_SET;

#define ANNOTATION_ITEM_SET_INITILIZER { \
    .anno_ts = 0, \
    .annotion_item_len = 0, \
    .annotation_item_arr = {ANNOTATION_ITEM_INITILIZER} \
}

extern ANNOTATION_ITEM_SET g_annotation_item_set;

int init_annotation_item_arr(ANNOTATION_ITEM_SET *set_p);
int reset_annotation_item_arr(ANNOTATION_ITEM_SET *set_p);
int add_annotioan_item_to_set(ANNOTATION_ITEM_SET *set_p, ANNOTATION_ITEM *item_p);
int encode_annotation_set_buff(const ANNOTATION_ITEM_SET *set_p, uint8_t *anno_buff);
int decode_annotation_buff(const uint8_t *anno_buff, int anno_buff_len, ANNOTATION_ITEM_SET *set_p);
int recursive_annotation_item(ANNOTATION_ITEM_SET *set_p);

#ifdef __cplusplus
    }
#endif

#endif
