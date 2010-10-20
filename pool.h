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

#include <stdint.h>

#define POOL_TYPE(type)             struct type##_POOL
#define POOL_HANDLE(type)           uintptr_t
#define POOL_FLAG_INUSE             UINTPTR_MAX
#define POOL_WRAPPER_STRUCT(type)   struct POOLED_##type

#define POOL_STRUCT(type)   struct type##_POOL {                                                \
    size_t              m_allocated_count;                                                      \
    size_t              m_count;                                                                \
    size_t              m_free_count;                                                           \
    POOL_WRAPPER_STRUCT(type)  *m_items;                                                        \
    POOL_HANDLE(type)   m_free_list_head;                                                       \
    pthread_mutex_t     m_free_list_mutex;                                                      \
}

#define POOL_SINGLETON(type)        _##type##_POOL
#define POOL_OBJECT(type, handle)   POOL_SINGLETON(type).m_items[(handle) - 1].m_object
#define POOL_WRAPPER(type, handle)  POOL_SINGLETON(type).m_items[(handle) - 1]

#define POOL_ISINUSE(type, handle)          (POOL_WRAPPER(type, handle).m_free_ptr == POOL_FLAG_INUSE)
#define POOL_VALID_HANDLE(type, handle)     ((handle) > 0 && (handle) <= POOL_SINGLETON(type).m_allocated_count)

#define POOL_DEFINITIONS(type, init, initarg, destroy, lock, unlock)                            \
POOL_WRAPPER_STRUCT(type) {                                                                     \
    type    m_object;                                                                           \
    size_t  m_references;                                                                       \
    POOL_HANDLE(type)   m_free_ptr;                                                             \
};                                                                                              \
                                                                                                \
static POOL_TYPE(type) POOL_SINGLETON(type);                                                    \
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
                POOL_WRAPPER(type, i).m_free_ptr = i;                                           \
            }                                                                                   \
            POOL_WRAPPER(type, POOL_SINGLETON(type).m_allocated_count).m_free_ptr = 0;          \
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
            destroy(&POOL_OBJECT(type, i));                                                     \
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
static inline POOL_HANDLE(type) type##_POOL_ALLOCATE(initarg arg) {                             \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {                     \
        POOL_HANDLE(type) handle;                                                               \
                                                                                                \
        if (POOL_SINGLETON(type).m_free_count > 0) {                                            \
            assert(POOL_SINGLETON(type).m_free_list_head != 0);                                 \
            handle = POOL_SINGLETON(type).m_free_list_head;                                     \
            POOL_SINGLETON(type).m_free_list_head =                                             \
                POOL_WRAPPER(type, handle).m_free_ptr;                                          \
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
            if (tmp != NULL)  {                                                                 \
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
                POOL_WRAPPER(type, i).m_free_ptr = i;                                           \
            }                                                                                   \
            POOL_WRAPPER(type, POOL_SINGLETON(type).m_allocated_count).m_free_ptr = 0;          \
            POOL_SINGLETON(type).m_free_count = POOL_SINGLETON(type).m_allocated_count - handle;\
                                                                                                \
            pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);                      \
        }                                                                                       \
                                                                                                \
        init(&POOL_OBJECT(type, handle), arg);                                                  \
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
static inline int type##_POOL_INCREASE_REFCOUNT(POOL_HANDLE(type) handle) {                     \
    assert(POOL_VALID_HANDLE(type, handle));                                                    \
    assert(POOL_WRAPPER(type, handle).m_free_ptr == POOL_FLAG_INUSE);                           \
    assert(POOL_WRAPPER(type, handle).m_references > 0);                                        \
                                                                                                \
    if (0 == lock(&POOL_OBJECT(type,handle))) {                                                 \
        ++POOL_WRAPPER(type,handle).m_references;                                               \
        unlock(&POOL_OBJECT(type, handle));                                                     \
        return 0;                                                                               \
    }                                                                                           \
    else {                                                                                      \
        return -1;                                                                              \
    }                                                                                           \
}                                                                                               \
                                                                                                \
static inline void _##type##_POOL_ADD_TO_FREE_LIST(POOL_HANDLE(type) handle) {                  \
    assert(POOL_VALID_HANDLE(type, handle));                                                    \
                                                                                                \
    POOL_HANDLE(type) prev = handle;                                                            \
                                                                                                \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {                     \
        for ( ; prev > 0 && POOL_WRAPPER(type, prev).m_free_ptr != POOL_FLAG_INUSE; --prev) ;   \
                                                                                                \
        if (prev > 0) {                                                                         \
            POOL_WRAPPER(type, handle).m_free_ptr = POOL_WRAPPER(type, prev).m_free_ptr;        \
            POOL_WRAPPER(type, prev).m_free_ptr = handle;                                       \
        }                                                                                       \
        else {                                                                                  \
            POOL_WRAPPER(type, handle).m_free_ptr = POOL_SINGLETON(type).m_free_list_head;      \
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
    assert(POOL_WRAPPER(type, handle).m_free_ptr == POOL_FLAG_INUSE);                           \
    assert(POOL_WRAPPER(type, handle).m_references > 0);                                        \
                                                                                                \
    if (0 == lock(&POOL_OBJECT(type, handle))) {                                                \
        if (--POOL_WRAPPER(type, handle).m_references == 0) {                                   \
            destroy(&POOL_OBJECT(type, handle));                                                \
            _##type##_POOL_ADD_TO_FREE_LIST(handle);                                            \
            POOL_SINGLETON(type).m_count--;                                                     \
        }                                                                                       \
        unlock(&POOL_OBJECT(type, handle));                                                     \
        return 0;                                                                               \
    }                                                                                           \
    else {                                                                                      \
        return -1;                                                                              \
    }                                                                                           \
}


#define POOL_INIT(type)                         type##_POOL_INIT()
#define POOL_DESTROY(type)                      type##_POOL_DESTROY()
#define POOL_ALLOCATE(type, arg)                type##_POOL_ALLOCATE(arg)
#define POOL_INCREASE_REFCOUNT(type, handle)    type##_POOL_INCREASE_REFCOUNT(handle)
#define POOL_RELEASE(type, handle)              type##_POOL_RELEASE(handle)

#define POOL_basic_init(p, a)   (0)
#define POOL_basic_initarg      (void *)
#define POOL_basic_destroy(p)   (0)
#define POOL_basic_lock(p)      (0)
#define POOL_basic_unlock(p)    (0)

#endif
