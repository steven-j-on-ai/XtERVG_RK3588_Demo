#ifndef _FRAME_INFO_CIRQ
#define _FRAME_INFO_CIRQ

#include <stdint.h>

#include "thread.h"

#ifdef __cplusplus
    extern "C" {
#endif

#define FRAME_INFO_CIRQ_MAX 16

typedef struct _frame_info_t {
	uint32_t seqno;
	uint32_t timestamp;
} FRAME_INFO;

typedef struct _frame_cir_buff
{
    int size;
    int next_in;
    int next_out;
    int capacity;
    mutex_t mutex;
    cond_t not_full;
    cond_t not_empty;
    FRAME_INFO frame_info_arr[FRAME_INFO_CIRQ_MAX];
    int is_inited;
} FRAME_CIR_BUFF;

extern FRAME_CIR_BUFF g_frame_cir_buff;

extern int frame_cir_buff_init(FRAME_CIR_BUFF *c);
extern void frame_cir_buff_destroy(FRAME_CIR_BUFF *c);
extern int frame_cir_buff_enqueue(FRAME_CIR_BUFF *c, FRAME_INFO *info_p);
extern int frame_cir_buff_dequeue(FRAME_CIR_BUFF *c, FRAME_INFO *info_p);
extern int frame_cir_buff_full(FRAME_CIR_BUFF *c);
extern int frame_cir_buff_empty(FRAME_CIR_BUFF *c);
extern int frame_cir_buff_capacity(FRAME_CIR_BUFF *c);

#ifdef __cplusplus
    }
#endif

#endif
