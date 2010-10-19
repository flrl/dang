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

#define POOL_TYPE(type)     
struct type##_POOL

#define POOL_HANDLE(type)   uintptr_t

#define POOL_STRUCT(type)   
struct type##_POOL {
    size_t m_allocated_count;
    size_t m_count;
    size_t m_free_count;
    type   *m_items;
    POOL_HANDLE(type)   m_free_list_head;
    pthread_mutex_t     m_free_list_mutex;
}


#define POOL_INIT(type, ...)
// etc


#define POOL_basic_init     (0)
// etc

#define POOL_SINGLETON(type)    _##type##_POOL

#define POOL_ITEM(type, handle) POOL_SINGLETON(type).m_items[(handle) - 1]

#define POOL_DEFINITIONS(type, isinuse, init, initarg, destroy, nextfree, lock, unlock)  \
static POOL_TYPE(type) POOL_SINGLETON(type);                                             \
                                                                                         \
static inline int type##_POOL_INIT(void) {                                               \
    if (    NULL !=                                                                      \
            (POOL_SINGLETON(type).m_items = calloc(POOL_INITIAL_SIZE, sizeof(type)))) {  \
        POOL_SINGLETON(type).m_allocated_count = POOL_INITIAL_SIZE;                      \
        POOL_SINGLETON(type).m_free_count = POOL_INITIAL_SIZE;                           \
        POOL_SINGLETON(type).m_count = 0;                                                \
        if (0 == pthread_mutex_init(&POOL_SINGLETON(type).m_free_list_mutex, NULL)) {    \
            POOL_SINGLETON(type).m_free_list_head = 1;                                   \
            for (   POOL_HANDLE(type) i = 2;                                             \
                    i < POOL_SINGLETON(type).m_allocated_count - 1;                      \
                    i++) {                                                               \
                nextfree(&POOL_ITEM(type, i)) = i;                                       \
            }                                                                            \
            nextfree(&POOL_ITEM(type, POOL_SINGLETON(type).m_allocated_count)) = 0;      \
            return 0;                                                                    \
        }                                                                                \
        else {                                                                           \
            free(POOL_SINGLETON(type).m_items);                                          \
            return -1;                                                                   \
        }                                                                                \
    }                                                                                    \
    else {                                                                               \
        return -1;                                                                       \
    }                                                                                    \
}                                                                                        \
                                                                                         \
static inline int type##_POOL_DESTROY(void) {                                            \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {              \
        /* FIXME loop over and properly destroy all the currently defined pool items */  \
        free(POOL_SINGLETON(type).m_items);                                              \
        POOL_SINGLETON(type).m_allocated_count = POOL_SINGLETON(type).m_count = 0;       \
        pthread_mutex_destroy(&POOL_SINGLETON(type).m_free_list_mutex);                  \
        return 0;                                                                        \
    }                                                                                    \
    else {                                                                               \
        return -1;                                                                       \
    }                                                                                    \
}                                                                                        \
                                                                                         \
static inline POOL_HANDLE(type) type##_POOL_ALLOCATE(initarg arg) {                      \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {              \
        POOL_HANDLE(type) handle;                                                        \
                                                                                         \
        if (POOL_SINGLETON(type).m_free_count > 0) {                                     \
            /* allocate a new one from the free list */                                  \
            assert(POOL_SINGLETON(type).m_free_list_head != 0);                          \
            handle = POOL_SINGLETON(type).m_free_list_head;                              \
            POOL_SINGLETON(type).m_free_list_head = nextfree(&POOL_ITEM(handle));        \
            POOL_SINGLETON(type).m_free_count--;                                         \
                                                                                         \
            pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);               \
        }                                                                                \
        else {                                                                           \
            /* grow the pool and allocate a new one from the increased free list */      \
            handle = POOL_SINGLETON(type).m_allocated_count + 1;                         \
                                                                                         \
            size_t new_size = 2 * POOL_SINGLETON(type).m_allocated_count;                \
            type *tmp = calloc(new_size, sizeof(type));                                  \
            if (tmp != NULL)  {                                                          \
                pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);           \
                return 0;                                                                \
            }                                                                            \
            memcpy( tmp,                                                                 \
                    POOL_SINGLETON(type).m_items,                                        \
                    POOL_SINGLETON(type).m_allocated_count * sizeof(type));              \
            free(POOL_SINGLETON(type).m_items);                                          \
            POOL_SINGLETON(type).m_items = tmp;                                          \
            POOL_SINGLETON(type).m_allocated_count = new_size;                           \
                                                                                         \
            POOL_SINGLETON(type).m_free_list_head = handle + 1;                          \
            for (   POOL_HANDLE(type) i = POOL_SINGLETON(type).m_free_list_head;         \
                    i < new_size - 1;                                                    \
                    i++) {                                                               \
                nextfree(&POOL_ITEM(type, i)) = i;                                       \
            }                                                                            \
            nextfree(&POOL_ITEM(type, POOL_SINGLETON(type).m_allocated_count)) = 0;      \
            POOL_SINGLETON(type).m_free_count =                                          \
                POOLED_SINGLETON(type).m_allocated_count - handle;                       \
                                                                                         \
            pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);               \
        }                                                                                \
                                                                                         \
        init(&POOL_ITEM(type, handle), arg);                                             \
                                                                                         \
        POOL_SINGLETON(type).m_count++;                                                  \
                                                                                         \
        return handle;                                                                   \
    }                                                                                    \
    else {                                                                               \
        return 0;                                                                        \
    }                                                                                    \
}                                                                                        \
                                                                                         \
                                                                                         \
static inline int type##_INCREASE_REFCOUNT(POOL_HANDLE(type) handle) {                   \
    assert(handle != 0);                                                                 \
    assert(handle <= POOL_SINGLETON(type).m_allocated_count);                            \
    assert(isinuse(&POOL_ITEM(type, handle)));                                           \
    assert(POOL_ITEM(type, handle).m_references > 0);                                    \
                                                                                         \
    if (0 == lock(handle)) {                                                             \
        ++POOL_ITEM(handle).m_references;                                                \
        unlock(handle);                                                                  \
        return 0;                                                                        \
    }                                                                                    \
    else {                                                                               \
        return -1;                                                                       \
    }                                                                                    \
}                                                                                        \
                                                                                         \
static inline void _##type##_POOL_ADD_TO_FREE_LIST(POOL_HANDLE(type) handle) {           \
    assert(handle != 0);                                                                 \
    assert(handle <= POOL_SINGLETON(type).m_allocated_count);                            \
                                                                                         \
    POOL_HANDLE(type) prev = handle;                                                     \
                                                                                         \
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {              \
        for ( ; prev > 0 && ! isinuse(&POOL_ITEM(type, prev)); --prev) ;                 \
                                                                                         \
        if (prev > 0) {                                                                  \
            nextfree(&POOL_ITEM(type, handle)) = nextfree(&POOL_ITEM(type, prev));       \
            nextfree(&POOL_ITEM(type, prev)) = handle;                                   \
        }                                                                                \
        else {                                                                           \
            nextfree(&POOL_ITEM(type, handle)) = POOL_SINGLETON(type).m_free_list_head;  \
            POOL_SINGLETON(type).m_free_list_head = handle;                              \
        }                                                                                \
                                                                                         \
        ++POOL_SINGLETON(type).m_free_count;                                             \
                                                                                         \
        pthread_mutex_unlock(&POOL_SINGLETON(type).m_free_list_mutex);                   \
    }                                                                                    \
}                                                                                        \
                                                                                         \
static inline int type##_RELEASE(POOL_HANDLE(type) handle) {                             \
    assert(handle != 0);                                                                 \
    assert(handle <= POOL_SINGLETON(type).m_allocated_count);                            \
    assert(isinuse(&POOL_ITEM(type, handle)));                                           \
    assert(POOL_ITEM(type, handle).m_references > 0);                                    \
                                                                                         \
    if (0 == lock(handle)) {                                                             \
        if (--POOL_ITEM(type, handle).m_references == 0) {                               \
            destroy(&POOL_ITEM(type, handle));                                           \
            _##type##_POOL_ADD_TO_FREE_LIST(handle);                                     \
            POOL_SINGLETON(type).m_count--;                                              \
        }                                                                                \
        unlock(handle);                                                                  \
        return 0;                                                                        \
    }                                                                                    \
    else {                                                                               \
        return -1;                                                                       \
    }                                                                                    \
}                                                                                        \



#endif
