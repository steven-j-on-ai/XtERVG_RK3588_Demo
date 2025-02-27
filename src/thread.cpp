#include "thread.h"

int mutex_init(mutex_t *m)
{
    int ret = OPER_OK;

    if((ret = pthread_mutex_init(&m->mutex, NULL)) != 0)
        ret = -THREAD_MUTEX_INIT_ERROR;

    return ret;
}
int mutex_destroy(mutex_t *m)
{
    int ret = OPER_OK;

    if((ret = pthread_mutex_destroy(&m->mutex)) != 0)
        ret = -MUTEX_DESTROY_ERROR;

    return ret;
}
int mutex_lock(mutex_t *m)
{
    int ret = OPER_OK;

    if((ret = pthread_mutex_lock(&m->mutex)) != 0)
        ret = -THREAD_MUTEX_LOCK_ERROR;

    return ret;
}
int mutex_try_lock(mutex_t *m)
{
    int        ret = OPER_OK;
    if((ret = pthread_mutex_trylock(&m->mutex)) != 0)
        ret = -THREAD_MUTEX_LOCK_ERROR;
    
    return ret;
}
int mutex_unlock(mutex_t *m)
{
    int ret = OPER_OK;

    if((ret = pthread_mutex_unlock(&m->mutex)) != 0)
        ret = -THREAD_MUTEX_UNLOCK_ERROR;
    
    return ret;
}
int cond_init(cond_t *c)
{
    int ret = OPER_OK;

    if((ret = pthread_cond_init(&c->cond, NULL)) != 0)
        ret = -THREAD_COND_INIT_ERROR;

    return ret;
}
int cond_destroy(cond_t *c)
{
    int ret = OPER_OK;

    if((ret = pthread_cond_destroy(&c->cond)) != 0)
        ret = -COND_DESTROY_ERROR;
    
    return ret;
}
int cond_signal(cond_t *c)
{
    int ret = OPER_OK;

    if((ret = pthread_cond_signal(&c->cond)) != 0)
        ret = -COND_SIGNAL_ERROR;
    
    return ret;
}
int cond_wait(cond_t *c, mutex_t *m)
{
    int ret = OPER_OK;

    if((ret = pthread_cond_wait(&c->cond, &m->mutex)) != 0)
        ret = -COND_WAIT_ERROR;    
    
    return ret;
}
