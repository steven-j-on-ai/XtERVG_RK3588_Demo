#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
/*
typedef unsigned char           byte;
typedef unsigned short          word;
typedef unsigned int            dword;
typedef int                     bool;
typedef signed char             int8_t;
typedef unsigned char           uint8_t;
typedef signed short int        int16_t;
typedef unsigned short int      uint16_t;
*/
typedef signed int              int32_t;
/*
typedef unsigned int            uint32_t;
typedef unsigned long long      uint64_t;
typedef long long               int64_t;
typedef int8_t                  int8;
typedef uint8_t                 uint8;
typedef int16_t                 int16;
typedef uint16_t                uint16;
typedef int32_t                 int32;
typedef uint32_t                uint32;
typedef int64_t                 int64;
typedef uint64_t                uint64;
typedef unsigned char           uchar_t;
typedef uint32_t                wchar_t;
typedef uint32_t                size_t;
typedef uint32_t                addr_t;
typedef int32_t                 pid_t;
*/

#define CBUF_MAX  256

//#define RECV_CIRBUF_MAX  256	//256 -> 2048 -> 256 -> 512 -> 1024 -> 2048 -> 256
//#define RECV_CIRBUF_MAX  512	//256 -> 2048 -> 256 -> 512 -> 1024 -> 2048 -> 256
// #define RECV_CIRBUF_MAX  2048 * 2	//256 -> 2048 -> 256 -> 512 -> 1024 -> 2048 -> 256
#define RECV_CIRBUF_MAX  512 * 2	//256 -> 2048 -> 256 -> 512 -> 1024 -> 2048 -> 256

enum{
OPER_OK,
OPER_ERR,
THREAD_MUTEX_INIT_ERROR,
MUTEX_DESTROY_ERROR,
THREAD_MUTEX_LOCK_ERROR,
THREAD_MUTEX_UNLOCK_ERROR,
THREAD_COND_INIT_ERROR,
COND_DESTROY_ERROR,
COND_SIGNAL_ERROR,
COND_WAIT_ERROR
// OPER_EXCEEDS_MAX_SEQ
};

#define OPER_EXCEEDS_MAX_SEQ -10

#endif	//TYPES_H


