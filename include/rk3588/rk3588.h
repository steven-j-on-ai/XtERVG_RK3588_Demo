/****************************************************************************
*
*    Copyright (c) 2017 - 2022 by Rockchip Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Rockchip Corporation. This is proprietary information owned by
*    Rockchip Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Rockchip Corporation.
*
*****************************************************************************/


#ifndef _RK3588_H
#define _RK3588_H

#include "rknn_api.h"
#include "mpp_decoder.h"
#include "mpp_encoder.h"
#include "postprocess.h"

#define MODEL_FILE "/usr/local/xt/models/yolov5s-640-640.rknn"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    rknn_context rknn_ctx;
    rknn_input_output_num io_num;
    rknn_tensor_attr *input_attrs;
    rknn_tensor_attr *output_attrs;
    int model_channel;
    int model_width;
    int model_height;
    FILE *out_fp;
    MppDecoder *decoder;
    MppEncoder *encoder;
    int is_rate;
} rknn_app_context_t;

int init_rk3588(rknn_app_context_t *ctx, const char *model_name, int video_type, int is_rate);
int deinit_rk3588(rknn_app_context_t *ctx);

#ifdef __cplusplus
} //extern "C"
#endif

#endif  //_RK3588_H
