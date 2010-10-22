/*
 *  pool.h
 *  dang
 *
 *  Created by Ellie on 19/10/10.
 *  Copyright 2010 Ellie. All rights reserved.
 *
 */

#ifndef POOL_INITIAL_SIZE
#define POOL_INITIAL_SIZE (16)
#endif

#ifndef POOL_H
#define POOL_H

#include <pthread.h>
#include <stdint.h>

#define POOL_TYPE(type)             struct type##_POOL
#define POOL_HANDLE(type)           uintptr_t
#define POOL_FLAG_INUSE             UINTPTR_MAX
#define POOL_OBJECT_FLAG_SHARED     0x80000000u
#define POOL_WRAPPER_STRUCT(type)   struct POOLED_##type

#define POOL_SINGLETON(type)        _##type##_POOL
#define POOL_OBJECT(type, handle)   POOL_SINGLETON(type).m_items[(handle) - 1].m_object
#define POOL_WRAPPER(type, handle)  POOL_SINGLETON(type).m_items[(handle) - 1]

#define POOL_ISINUSE(type, handle)          (POOL_WRAPPER(type, handle).m_next_free == POOL_FLAG_INUSE)
#define POOL_VALID_HANDLE(type, handle)     ((handle) > 0 && (handle) <= POOL_SINGLETON(type).m_allocated_count)

#define POOL_INIT(type)                         type##_POOL_INIT()
#define POOL_DESTROY(type)                      type##_POOL_DESTROY()
#define POOL_ALLOCATE(type, arg)                type##_POOL_ALLOCATE(arg)
#define POOL_INCREASE_REFCOUNT(type, handle)    type##_POOL_INCREASE_REFCOUNT(handle)
#define POOL_RELEASE(type, handle)              type##_POOL_RELEASE(handle)
#define POOL_LOCK(type, handle)                 type##_POOL_LOCK(handle)
#define POOL_UNLOCK(type, handle)               type##_POOL_UNLOCK(handle)

#define POOL_basic_init(p, a)   (0)
#define POOL_basic_destroy(p)   (0)

#define POOL_STRUCT(type)   struct type##_POOL {                                                \
    size_t              m_allocated_count;                                                      \
    size_t              m_count;                                                                \
    size_t              m_free_count;                                                           \
    POOL_WRAPPER_STRUCT(type)  *m_items;                                                        \
    POOL_HANDLE(type)   m_free_list_head;                                                       \
    pthread_mutex_t     m_free_list_mutex;                                                      \
}

#define POOL_DEFINITIONS(type, init, destroy)                                                   \
POOL_WRAPPER_STRUCT(type) {                                                                     \
    type    m_object;                                                                           \
    size_t  m_references;                                                                       \
    pthread_mutex_t *m_mutex;                                                                   \
    POOL_HANDLE(type)   m_next_free;                                                            \
};                                                                                              \
                                                                                                \
static POOL_TYPE(type) POOL_SINGLETON(type);                                                    \
                                                                                                \
static inline int type##_POOL_LOCK(POOL_HANDLE(type) handle) {                                  \
    assert(POOL_VALID_HANDLE(type, handle));                                                    \
    assert(POOL_WRAPPER(type, handle).m_next_free == POOL_FLAG_INUSE);                          \
                                                                                                \
    if (POOL_WRAPPER(type, handle).m_mutex != NULL) {                                           \
        return pthread_mutex_lock(POOL_WRAPPER(type, handle).m_mutex);                          \
    }                                                                                           \
    else {                                                                                      \
        return 0;                                                                               \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline int type##_POOL_UNLOCK(POOL_HANDLE(type) handle) {                                \
    assert(POOL_VALID_HANDLE(type, handle));                                                    \
    assert(POOL_WRAPPER(type, handle).m_next_free == POOL_FLAG_INUSE);                          \
                                                                                                \
    if (POOL_WRAPPER(type, handle).m_mutex != NULL) {                                           \
        return pthread_mutex_unlock(POOL_WRAPPER(type, handle).m_mutex);                        \
    }                                                                                           \
    else {                                                                                      \
        return 0;                                                                               \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline int type##_POOL_INIT(void) {                                                      \
    if (    NULL != (POOL_SINGLETON(type).m_items =                                             \
                calloc(POOL_INITIAL_SIZE, sizeof(POOL_WRAPPER_STRUCT(type))))) {                \
        POOL_SINGLETON(type).m_allocated_count = POOL_INITIAL_SIZE;                             \
        POOL_SINGLETON(type).m_free_count = POOL_INITIAL_SIZE;                                  \
        POOL_SINGLETON(type).m_count = 0;                                                       \
        if (0 == pthread_mutex_init(&POOL_SINGLETON(type).m_free_list_mutex, NULL)) {           \
            POOL_SINGLETON(type).m_free_list_head = 1;                                          \
            for (POOL_HANDLE(type) i = 2; i < POOL_SINGLETON(type).m_allocated_count - 1; i++) {\
                POOL_WRAPPER(type, i).m_next_free = i;                                          \
            }                                                                                   \
            POOL_WRAPPER(type, POOL_SINGLETON(type).m_allocated_count).m_next_free = 0;         \
            return 0;                                                                           \
        }                                                                                       \
        else {                                                                                  \
            free(POOL_SINGLETON(type).m_items);                                                 \
            return -1;                                                                          \
        }                                                                                       \
    }                                                                                           \
    else {                                                                                      \
        return -1;                                                                              \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline int type##_POOL_DESTROY(void) {                                                   \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {                     \
        for (POOL_HANDLE(type) i = 1; i <= POOL_SINGLETON(type).m_allocated_count; i++) {       \
            if (0 == POOL_LOCK(type, i)) {                                                      \
                destroy(&POOL_OBJECT(type, i));                                                 \
                if (POOL_WRAPPER(type, i).m_mutex != NULL) {                                    \
                    pthread_mutex_t *tmp = POOL_WRAPPER(type, i).m_mutex;                       \
                    POOL_WRAPPER(type, i).m_mutex = NULL;                                       \
                    pthread_mutex_destroy(tmp);                                                 \
                    free(tmp);                                                                  \
                }                                                                               \
            }                                                                                   \
        }                                                                                       \
        free(POOL_SINGLETON(type).m_items);                                                     \
        POOL_SINGLETON(type).m_allocated_count = POOL_SINGLETON(type).m_count = 0;              \
        pthread_mutex_destroy(&POOL_SINGLETON(type).m_free_list_mutex);                         \
        return 0;                                                                               \
    }                                                                                           \
    else {                                                                                      \
        return -1;                                                                              \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline POOL_HANDLE(type) type##_POOL_ALLOCATE(uint32_t flags) {                          \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {                     \
        POOL_HANDLE(type) handle;                                                               \
                                                                                                \
        if (POOL_SINGLETON(type).m_free_count > 0) {                                            \
            assert(POOL_SINGLETON(type).m_free_list_head != 0);                                 \
            handle = POOL_SINGLETON(type).m_free_list_head;                                     \
            POOL_SINGLETON(type).m_free_list_head =                                             \
                POOL_WRAPPER(type, handle).m_next_free;                                         \
            POOL_SINGLETON(type).m_free_count--;                                                \
                                                                                                \
            pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);                      \
        }                                                                                       \
        else {                                                                                  \
            handle = POOL_SINGLETON(type).m_allocated_count + 1;                                \
                                                                                                \
            size_t new_size = 2 * POOL_SINGLETON(type).m_allocated_count;                       \
            POOL_WRAPPER_STRUCT(type) *tmp =                                                    \
                calloc(new_size, sizeof(POOL_WRAPPER_STRUCT(type)));                            \
            if (tmp == NULL)  {                                                                 \
                pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);                  \
                return 0;                                                                       \
            }                                                                                   \
            memcpy( tmp,                                                                        \
                    POOL_SINGLETON(type).m_items,                                               \
                    POOL_SINGLETON(type).m_allocated_count * sizeof(type));                     \
            free(POOL_SINGLETON(type).m_items);                                                 \
            POOL_SINGLETON(type).m_items = tmp;                                                 \
            POOL_SINGLETON(type).m_allocated_count = new_size;                                  \
                                                                                                \
            POOL_SINGLETON(type).m_free_list_head = handle + 1;                                 \
            for (   POOL_HANDLE(type) i = POOL_SINGLETON(type).m_free_list_head;                \
                    i < new_size - 1;                                                           \
                    i++) {                                                                      \
                POOL_WRAPPER(type, i).m_next_free = i;                                          \
            }                                                                                   \
            POOL_WRAPPER(type, POOL_SINGLETON(type).m_allocated_count).m_next_free = 0;         \
            POOL_SINGLETON(type).m_free_count = POOL_SINGLETON(type).m_allocated_count - handle;\
                                                                                                \
            pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);                      \
        }                                                                                       \
                                                                                                \
        init(&POOL_OBJECT(type, handle));                                                       \
                                                                                                \
        if (flags & POOL_OBJECT_FLAG_SHARED) {                                                  \
            POOL_WRAPPER(type, handle).m_mutex = calloc(1, sizeof(pthread_mutex_t));            \
            assert(POOL_WRAPPER(type, handle).m_mutex != NULL);                                 \
            pthread_mutex_init(POOL_WRAPPER(type, handle).m_mutex, NULL);                       \
        }                                                                                       \
                                                                                                \
        POOL_SINGLETON(type).m_count++;                                                         \
                                                                                                \
        return handle;                                                                          \
    }                                                                                           \
    else {                                                                                      \
        return 0;                                                                               \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline POOL_HANDLE(type) type##_POOL_INCREASE_REFCOUNT(POOL_HANDLE(type) handle) {       \
    assert(POOL_VALID_HANDLE(type, handle));                                                    \
    assert(POOL_WRAPPER(type, handle).m_next_free == POOL_FLAG_INUSE);                          \
    assert(POOL_WRAPPER(type, handle).m_references > 0);                                        \
                                                                                                \
    if (0 == POOL_LOCK(type, handle)) {                                                         \
        ++POOL_WRAPPER(type,handle).m_references;                                               \
        POOL_UNLOCK(type, handle);                                                              \
        return handle;                                                                          \
    }                                                                                           \
    else {                                                                                      \
        return 0;                                                                               \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline void _##type##_POOL_ADD_TO_FREE_LIST(POOL_HANDLE(type) handle) {                  \
    assert(POOL_VALID_HANDLE(type, handle));                                                    \
                                                                                                \
    POOL_HANDLE(type) prev = handle;                                                            \
                                                                                                \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {                     \
        for ( ; prev > 0 && POOL_WRAPPER(type, prev).m_next_free != POOL_FLAG_INUSE; --prev) ;  \
                                                                                                \
        if (prev > 0) {                                                                         \
            POOL_WRAPPER(type, handle).m_next_free = POOL_WRAPPER(type, prev).m_next_free;      \
            POOL_WRAPPER(type, prev).m_next_free = handle;                                      \
        }                                                                                       \
        else {                                                                                  \
            POOL_WRAPPER(type, handle).m_next_free = POOL_SINGLETON(type).m_free_list_head;     \
            POOL_SINGLETON(type).m_free_list_head = handle;                                     \
        }                                                                                       \
                                                                                                \
        ++POOL_SINGLETON(type).m_free_count;                                                    \
                                                                                                \
        pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);                          \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline int type##_POOL_RELEASE(POOL_HANDLE(type) handle) {                               \
    assert(POOL_VALID_HANDLE(type, handle));                                                    \
    assert(POOL_WRAPPER(type, handle).m_next_free == POOL_FLAG_INUSE);                          \
    assert(POOL_WRAPPER(type, handle).m_references > 0);                                        \
                                                                                                \
    if (0 == POOL_LOCK(type, handle)) {                                                         \
        if (--POOL_WRAPPER(type, handle).m_references == 0) {                                   \
            destroy(&POOL_OBJECT(type, handle));                                                \
            if (POOL_WRAPPER(type, handle).m_mutex != NULL) {                                   \
                pthread_mutex_t *tmp = POOL_WRAPPER(type, handle).m_mutex;                      \
                POOL_WRAPPER(type, handle).m_mutex = NULL;                                      \
                pthread_mutex_destroy(tmp);                                                     \
                free(tmp);                                                                      \
            }                                                                                   \
            _##type##_POOL_ADD_TO_FREE_LIST(handle);                                            \
            POOL_SINGLETON(type).m_count--;                                                     \
        }                                                                                       \
        POOL_UNLOCK(type, handle);                                                              \
        return 0;                                                                               \
    }                                                                                           \
    else {                                                                                      \
        return -1;                                                                              \
    }                                                                                           \
}


#endif
