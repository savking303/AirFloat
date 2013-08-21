//
//  mutex_posix.c
//  AirFloat
//
//  Copyright (c) 2013, Kristian Trenskow All rights reserved.
//
//  Redistribution and use in source and binary forms, with or
//  without modification, are permitted provided that the following
//  conditions are met:
//
//  Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//  Redistributions in binary form must reproduce the above
//  copyright notice, this list of conditions and the following
//  disclaimer in the documentation and/or other materials provided
//  with the distribution. THIS SOFTWARE IS PROVIDED BY THE
//  COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
//  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
//  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
//  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
//  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if (__APPLE__)

#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#include "debug.h"

#include "obj.h"

#include "mutex.h"

#define MAX_NAME_LENGTH 250

struct mutex_t {
    pthread_mutexattr_t attr;
    pthread_mutex_t mutex;
    uint32_t lock_count;
#ifdef DEBUG
    char name[MAX_NAME_LENGTH];
#endif
};

struct mutex_t* _mutex_create(int type) {
    
    struct mutex_t* m = (struct mutex_t*)obj_create(sizeof(struct mutex_t));
    
    pthread_mutexattr_init(&m->attr);
    pthread_mutexattr_settype(&m->attr, type);
    
    pthread_mutex_init(&m->mutex, &m->attr);
    
    return m;
    
}

struct mutex_t* mutex_create() {
    
    return _mutex_create(PTHREAD_MUTEX_DEFAULT);
    
}

struct mutex_t* mutex_create_recursive() {
    
    return _mutex_create(PTHREAD_MUTEX_RECURSIVE);
    
}

void _mutex_destroy(void* o) {
    
    struct mutex_t* m = (struct mutex_t*)o;
    
    pthread_mutex_destroy(&m->mutex);
    pthread_mutexattr_destroy(&m->attr);
    
}

struct mutex_t* mutex_retain(struct mutex_t* m) {
    
    return obj_retain(m);
    
}

struct mutex_t* mutex_release(struct mutex_t* m) {
    
    return obj_release(m, _mutex_destroy);
    
}

bool mutex_trylock(struct mutex_t* m) {
    
    return (pthread_mutex_trylock(&m->mutex) == 0);
    
}

void mutex_lock(struct mutex_t* m) {
    
    if (m != NULL) {
        mutex_retain(m); /* Self retain while locked. */
        pthread_mutex_lock(&m->mutex);
        m->lock_count++;
#ifdef DEBUG
        pthread_getname_np(pthread_self(), m->name, MAX_NAME_LENGTH);
#endif
    }
    
}

void mutex_unlock(struct mutex_t* m) {
    
    if (m != NULL) {
#ifdef DEBUG
        assert(m->lock_count > 0);
        
        m->name[0] = '\0';
#endif
        m->lock_count--;
        pthread_mutex_unlock(&m->mutex);
        mutex_release(m); /* Release self */
    }
    
}

pthread_mutex_t* mutex_pthread(struct mutex_t* m) {
    
    return &m->mutex;
    
}

#endif
