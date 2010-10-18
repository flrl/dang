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

#define POOL_SINGLETON(type)    _##type##_POOL;

#define POOL_ITEM(type, handle) POOL_SINGLETON(type).m_items[(handle) - 1]

#define POOL_DEFINITIONS(type, isinuse, nextfree, lock, unlock)
static POOL_TYPE(type) POOL_SINGLETON(type);

static int type##_POOL_INIT(void) {
    if (NULL != (POOL_SINGLETON(type).m_items = calloc(POOL_INITIAL_SIZE, sizeof(type)))) {
        POOL_SINGLETON(type).m_allocated_count = POOL_SINGLETON(type).m_free_count = POOL_INITIAL_SIZE;
        POOL_SINGLETON(type).m_count = 0;
        if (0 == pthread_mutex_init(&POOL_SINGLETON(type).m_free_list_mutex, NULL)) {
            POOL_SINGLETON(type).m_free_list_head = 1;
            for (POOL_HANDLE(type) i = 2; i < POOL_SINGLETON(type).m_allocated_count - 1; i++) {
                nextfree(POOL_ITEM(type, i)) = i;
            }
            nextfree(POOL_ITEM(type, POOL_SINGLETON(type).m_allocated_count)) = 0;
            return 0;
        }
        else {
            free(POOL_SINGLETON(type).m_items);
            return -1;
        }
    }
    else {
        return -1;
    }
}

int scalar_pool_destroy(void) {
    if (0 == pthread_mutex_lock(&POOL_SINGLETON(type).m_free_list_mutex)) {
        free(POOL_SINGLETON(type).m_items);
        POOL_SINGLETON(type).m_allocated_count = POOL_SINGLETON(type).m_count = 0;
        pthread_mutex_destroy(&POOL_SINGLETON(type).m_free_list_mutex);
        // FIXME loop over and properly destroy all the currently defined pool items
        return 0;
    }
    else {
        return -1;
    }
}







#endif