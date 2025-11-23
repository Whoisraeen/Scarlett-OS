#ifndef _SCARLETTOS_THREAD_H
#define _SCARLETTOS_THREAD_H

#include <scarlettos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t sc_thread_t;

typedef struct {
    uint32_t stack_size;
    uint32_t priority;
    uint32_t flags;
} sc_thread_attr_t;

// Thread creation and management
int sc_thread_create(sc_thread_t *thread, const sc_thread_attr_t *attr, void *(*start_routine)(void *), void *arg);
void sc_thread_exit(void *retval);
int sc_thread_join(sc_thread_t thread, void **retval);
sc_thread_t sc_thread_self(void);
int sc_thread_yield(void);
int sc_thread_sleep(uint32_t ms);

// Synchronization
typedef struct {
    uint32_t lock;
} sc_mutex_t;

int sc_mutex_init(sc_mutex_t *mutex);
int sc_mutex_lock(sc_mutex_t *mutex);
int sc_mutex_trylock(sc_mutex_t *mutex);
int sc_mutex_unlock(sc_mutex_t *mutex);
int sc_mutex_destroy(sc_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif // _SCARLETTOS_THREAD_H
