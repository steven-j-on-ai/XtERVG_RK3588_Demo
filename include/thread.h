#ifndef __THREAD_H__
#define __THREAD_H__

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

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
};

typedef struct _mutex
{
	pthread_mutex_t mutex;
} mutex_t;
typedef struct _cond
{
	pthread_cond_t cond;
} cond_t;

extern int mutex_init(mutex_t *m);
extern int mutex_destroy(mutex_t *m);
extern int mutex_lock(mutex_t *m);
extern int mutex_unlock(mutex_t *m);
extern int mutex_try_lock(mutex_t *m);
extern int cond_init(cond_t *c);
extern int cond_destroy(cond_t *c);
extern int cond_signal(cond_t *c);
extern int cond_wait(cond_t *c, mutex_t *m);

#define thread_join(t, p) pthread_join(t, p)
#define thread_self() pthread_self()
#define thread_sigmask pthread_sigmask

#endif
