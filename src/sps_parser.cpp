#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "sps_parser.h"

#define MAX_SPS_SIZE 64

typedef struct {
    const uint8_t *data;
    int bit_pos;
    int size;
} BitStream;

void init_bitstream(BitStream *bs, const uint8_t *data, int size) 
{
    bs->data = data;
    bs->bit_pos = 0;
    bs->size = size;
}

int read_bit(BitStream *bs) 
{
    if (bs->bit_pos >= bs->size * 8)
        return -1;
    int byte_offset = bs->bit_pos / 8;
    int bit_offset = 7 - (bs->bit_pos % 8);
    bs->bit_pos++;
    return (bs->data[byte_offset] >> bit_offset) & 0x01;
}

uint32_t read_bits(BitStream *bs, int n) 
{
    uint32_t result = 0;
    for (int i = 0; i < n; i++) {
        result <<= 1;
        result |= read_bit(bs);
    }
    return result;
}

uint32_t read_ue(BitStream *bs) 
{
    int zero_count = 0;
    while (read_bit(bs) == 0) {
        zero_count++;
    }
    uint32_t value = (1 << zero_count) - 1 + read_bits(bs, zero_count);
    return value;
}

int read_se(BitStream *bs) 
{
    uint32_t value = read_ue(bs);
    int32_t signed_value = (value % 2 == 0) ? -(value / 2) : (value / 2);
    return signed_value;
}

int parse_sps(const uint8_t *sps, int sps_size, int *width, int *height) 
{
    BitStream bs;
    init_bitstream(&bs, sps, sps_size);

    // 跳过nal_unit_type (8 bits)
    read_bits(&bs, 8);

    // profile_idc, constraint_set_flags, level_idc
    uint8_t profile_idc = read_bits(&bs, 8);
    read_bits(&bs, 8);  // constraint_set_flags + reserved_zero
    read_bits(&bs, 8);  // level_idc

    // seq_parameter_set_id
    read_ue(&bs);

    // 判断是否需要解析某些扩展字段（基于profile_idc）
    if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 || profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc == 118 || profile_idc == 128) {
        uint32_t chroma_format_idc = read_ue(&bs);
        if (chroma_format_idc == 3) {
            read_bit(&bs);  // residual_colour_transform_flag
        }
        read_ue(&bs);  // bit_depth_luma_minus8
        read_ue(&bs);  // bit_depth_chroma_minus8
        read_bit(&bs);  // qpprime_y_zero_transform_bypass_flag
        uint32_t seq_scaling_matrix_present_flag = read_bit(&bs);
        if (seq_scaling_matrix_present_flag) {
            for (int i = 0; i < ((chroma_format_idc != 3) ? 8 : 12); i++) {
                uint32_t seq_scaling_list_present_flag = read_bit(&bs);
                if (seq_scaling_list_present_flag) {
                    // 忽略 scaling list
                    if (i < 6) {
                        // scaling_list(4x4)
                        read_bits(&bs, 64);  // 跳过数据
                    } else {
                        // scaling_list(8x8)
                        read_bits(&bs, 128);  // 跳过数据
                    }
                }
            }
        }
    }
    // log2_max_frame_num_minus4
    read_ue(&bs);

    // pic_order_cnt_type
    uint32_t pic_order_cnt_type = read_ue(&bs);
    if (pic_order_cnt_type == 0) {
        read_ue(&bs);  // log2_max_pic_order_cnt_lsb_minus4
    } else if (pic_order_cnt_type == 1) {
        read_bit(&bs);  // delta_pic_order_always_zero_flag
        read_se(&bs);   // offset_for_non_ref_pic
        read_se(&bs);   // offset_for_top_to_bottom_field
        uint32_t num_ref_frames_in_pic_order_cnt_cycle = read_ue(&bs);
        for (uint32_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
            read_se(&bs);  // offset_for_ref_frame
        }
    }

    // num_ref_frames
    read_ue(&bs);

    // gaps_in_frame_num_value_allowed_flag
    read_bit(&bs);

    // pic_width_in_mbs_minus1 和 pic_height_in_map_units_minus1
    uint32_t pic_width_in_mbs_minus1 = read_ue(&bs);
    uint32_t pic_height_in_map_units_minus1 = read_ue(&bs);

    // frame_mbs_only_flag
    int frame_mbs_only_flag = read_bit(&bs);
    if (!frame_mbs_only_flag) {
        read_bit(&bs);  // mb_adaptive_frame_field_flag
    }

    // direct_8x8_inference_flag
    read_bit(&bs);

    // frame_cropping_flag
    int frame_cropping_flag = read_bit(&bs);
    uint32_t frame_crop_left_offset = 0;
    uint32_t frame_crop_right_offset = 0;
    uint32_t frame_crop_top_offset = 0;
    uint32_t frame_crop_bottom_offset = 0;

    if (frame_cropping_flag) {
        frame_crop_left_offset = read_ue(&bs);
        frame_crop_right_offset = read_ue(&bs);
        frame_crop_top_offset = read_ue(&bs);
        frame_crop_bottom_offset = read_ue(&bs);
    }

    // 计算宽度和高度
    *width = (pic_width_in_mbs_minus1 + 1) * 16 - (frame_crop_left_offset + frame_crop_right_offset) * 2;
    *height = (pic_height_in_map_units_minus1 + 1) * 16 - (frame_crop_top_offset + frame_crop_bottom_offset) * 2;

    if (!frame_mbs_only_flag) {
        *height *= 2;  // 如果不是帧模式，还需乘以2
    }

    return 0;
}

