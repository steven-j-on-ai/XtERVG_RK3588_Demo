#ifndef _SPS_PARSER_H
#define _SPS_PARSER_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int parse_sps(const uint8_t *sps, int sps_size, int *width, int *height);

#endif
