/*
 * Copyright (c) 2016, Mathias Brossard <mathias@brossard.org>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file threadpool.c
 * @brief Threadpool implementation file
 */

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "threadpool.h"

typedef enum {
    immediate_shutdown = 1,
    graceful_shutdown  = 2
} threadpool_shutdown_t;

typedef struct task_node{
	struct task_node *prev;
	struct task_node *next;
	threadpool_task_t data;	
	unsigned int   task_no;
}task_node_t;


typedef struct task_list{
	task_node_t *head;//list head
	task_node_t *tail;//list tail
	task_node_t *free;//free node list 
	task_node_t *buf;
	int count;
	int fixed_size;
	unsigned int task_no_counter;
}task_list_t;
task_list_t* create_task_list(int size){
	if(size < 1 )return NULL;
	task_list_t *l = (task_list_t *)malloc(sizeof(task_list_t));
	l->fixed_size = size;
	l->count = 0;
	l->head= NULL;
	l->tail = NULL;
	l->task_no_counter = 1;//start from 1 to avoid  equal to thread cached counter at start 
	task_node_t *n = (task_node_t *)malloc(sizeof(task_node_t)*size);
	l->buf = n;
	l->free = n;
	//n[0]
	n[0].prev = NULL;
	if(size == 1){
		n[0].next = NULL;
		return l;
	}
	n[0].next = n + 1;

	//n[1 - size-2]
	for(int i = 1;i<size-1;i++){
		n[i].prev = n+ (i-1);
		n[i].next = n + (i+1);
	}
	//n[size -1]
	int last = size -1;
	n[last].prev = n+ (last -1);
	n[last].next = NULL;
	
	return l;
}
void destroy_task_list(task_list_t *list){

	list->count = 0;
	list->fixed_size = 0;
	free(list->buf);
	list = NULL;
}	
inline task_node_t *head(task_list_t *list){
	if(!list) return NULL;
	return list->head;
}

inline task_node_t *next(task_node_t *node){
	if(!node ) return NULL;
	return node->next;
}
int add(task_list_t *list,const threadpool_task_t *data){
	if(!list ) return -1;
	if(list->count >= list->fixed_size) return -1;
	task_node_t *n = list->free;
	if(!n) return -1;

	list->free = list->free->next;
	if(list->free) list->free->prev = NULL;

	memcpy(&n->data,data,sizeof(threadpool_task_t));
	n->task_no = list->task_no_counter++;
	n->prev = list->tail;
	n->next = NULL;

	if(list->tail){
		list->tail->next = n;
		list ->tail = list->tail->next;
	}else{
		list->head = n;
		list->tail = n;
	}
	
	list->count++;
	return 0;
	
}
void erase(task_list_t *list,task_node_t *node){
	if(!node) return;
	//take apart node form list
	task_node_t *prev = node->prev;
	task_node_t *next = node->next;
	if(prev) prev->next = next;
	if(next) next->prev = prev;
	if(list->head == node) list->head = next;
	if(list->tail == node) list->tail = prev;
	//push front to list->free
	
	if(list->free) list->free->prev = node;
	node->prev = NULL;
	node->next = list->free;
	list->free = node;
	//update count
	list->count--;
}
void erase(task_list_t *list,const threadpool_task_t *data){
	task_node_t *h = head(list);
	while(h){
		if((h->data.function == data->function) && (h->data.argument == data->argument)){
			task_node_t *t = h;
			h = next(h);
			erase(list,t);
		}else{
			h = next(h);
		}
	}
}


inline int count(task_list_t *list){
	return list->count;
}

inline int size(task_list_t *list){
	return list->fixed_size;
}

/**
 *  @struct threadpool
 *  @brief The threadpool struct
 *
 *  @var notify       Condition variable to notify worker threads.
 *  @var threads      Array containing worker threads ID.
 *  @var thread_count Number of threads
 *  @var queue        Array containing the task queue.
 *  @var queue_size   Size of the task queue.
 *  @var head         Index of the first element.
 *  @var tail         Index of the next element.
 *  @var count        Number of pending tasks
 *  @var shutdown     Flag indicating if the pool is shutting down
 *  @var started      Number of started threads
 */
struct threadpool_t {
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  task_list_t *task;
  task_list_t *running;
  int thread_count;
  int shutdown;
  int started;
  check_func *checker;
  void (*free)(void *);
};

/**
 * @function void *threadpool_thread(void *threadpool)
 * @brief the worker thread
 * @param threadpool the pool which own the thread
 */
static void *threadpool_thread(void *threadpool);

int threadpool_free(threadpool_t *pool);

threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{
    threadpool_t *pool;
    int i;
    (void) flags;

    if(thread_count <= 0 || thread_count > TP_MAX_THREADS || queue_size <= 0 || queue_size > TP_MAX_QUEUE) {
        return NULL;
    }

    if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
        goto err;
    }

    /* Initialize */
    pool->task = create_task_list(queue_size);
	pool->running = create_task_list(thread_count+1);
    pool->shutdown = pool->started = 0;

    /* Allocate thread and task queue */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);

    /* Initialize mutex and conditional variable first */
    if((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
       (pthread_cond_init(&(pool->notify), NULL) != 0) ||
       (pool->threads == NULL) ||
       (pool->task == NULL) ||
       (pool->running == NULL)) {
        goto err;
    }

    /* Start worker threads */
    for(i = 0; i < thread_count; i++) {
        if(pthread_create(&(pool->threads[i]), NULL,
                          threadpool_thread, (void*)pool) != 0) {
            threadpool_destroy(pool, 0);
            return NULL;
        }
        pool->thread_count++;
        pool->started++;
    }
	pool->checker = NULL;
    return pool;

 err:
    if(pool) {
        threadpool_free(pool);
    }
    return NULL;
}

int threadpool_add(threadpool_t *pool, void (*function)(void *),
                   void *argument, void (*free)(void *) )
{
    int err = 0;
	int ret = 0;

    if(pool == NULL || function == NULL) {
        return threadpool_invalid;
    }

    if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return threadpool_lock_failure;
    }

    do {
        /* Are we full ? */
        if(count(pool->task) == size(pool->task)) {
            err = threadpool_queue_full;
            break;
        }

        /* Are we shutting down ? */
        if(pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }

        /* Add task to queue */
        threadpool_task_t t ;
		t.function = function;
		t.argument = argument;
		t.freer    = free;
		ret = add(pool->task,&t);
		

        /* pthread_cond_broadcast */
        if(pthread_cond_signal(&(pool->notify)) != 0) {
            err = threadpool_lock_failure;
            break;
        }
    } while(0);

    if(pthread_mutex_unlock(&pool->lock) != 0) {
        err = threadpool_lock_failure;
    }
	if(ret < 0) err= threadpool_queue_full;
    return err;
}

int threadpool_destroy(threadpool_t *pool, int flags)
{
    int i, err = 0;

    if(pool == NULL) {
        return threadpool_invalid;
    }

    if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return threadpool_lock_failure;
    }

    do {
        /* Already shutting down */
        if(pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }

        pool->shutdown = (flags & threadpool_graceful) ?
            graceful_shutdown : immediate_shutdown;

        /* Wake up all worker threads */
        if((pthread_cond_broadcast(&(pool->notify)) != 0) ||
           (pthread_mutex_unlock(&(pool->lock)) != 0)) {
            err = threadpool_lock_failure;
            break;
        }

        /* Join all worker thread */
        for(i = 0; i < pool->thread_count; i++) {
            if(pthread_join(pool->threads[i], NULL) != 0) {
                err = threadpool_thread_failure;
            }
        }
    } while(0);

    /* Only if everything went well do we deallocate the pool */
    if(!err) {
        threadpool_free(pool);
    }
    return err;
}

int threadpool_free(threadpool_t *pool)
{
    if(pool == NULL || pool->started > 0) {
        return -1;
    }

    /* Did we manage to allocate ? */
    if(pool->threads) {
        free(pool->threads);
        destroy_task_list(pool->task);
		destroy_task_list(pool->running);
 
        /* Because we allocate pool->threads after initializing the
           mutex and condition variable, we're sure they're
           initialized. Let's lock the mutex just in case. */
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
    }
    free(pool);    
    return 0;
}


static void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task = {0};
	int wait_flag = 0;	
	static __thread  unsigned int task_no = 0;
	
    for(;;) {
        /* Lock must be taken to wait on conditional variable */
        pthread_mutex_lock(&(pool->lock));

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        while(( wait_flag || !count(pool->task))&& (!pool->shutdown)) {
			wait_flag = 0;//just wait once time
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        if((pool->shutdown == immediate_shutdown) ||
           ((pool->shutdown == graceful_shutdown) &&
            (count(pool->task) == 0))) {
            break;
        }

        /* Grab our task */
		wait_flag = 0;
		do{
			check_func *f = pool->checker;
			task_node_t *tn = head(pool->task);//task node
			if(!f){//case 1 . does not need to check
				if(tn){
					task = tn->data;
					erase(pool->task,tn);
				}else{
					task.function = NULL;
					task.argument = NULL;
				}
				break;//go to WORK:
			}
			//case 2.1 check 
			if(task.function){//erase last running task 
				
				erase(pool->running,&task);
				if(task.argument&& task.freer) {
					(*(task.freer))(task.argument);
				}
				task.function = NULL;
				task.argument = NULL;
				task.freer    = NULL;
			}
			while(tn && task_no != UINT_MAX && tn->task_no < task_no+1){
				tn = next(tn);
			}
			
			
			while(tn){
				task_node_t *rn = head(pool->running);//running node
				while(rn && (*f)(&rn->data,&tn->data)){//could concurrent
					rn = next(rn);
				}

				if(!rn){//case 2.1.1.reach end :this task could concurrency with all running task
					add(pool->running,&tn->data);
					task = tn->data;
					erase(pool->task,tn);
					break;//goto WORK:
				}
				//case 2.1.2 .not reach end ,which means than there is an running task prevent tn->task to run 
				task_no = tn->task_no;//update task_no
				tn = next(tn);
				
			}
			
		}while(0);

       
//WORK:
		/* Unlock */
        pthread_mutex_unlock(&(pool->lock));

        /* Get to work */
		if(task.function){
			(*(task.function))(task.argument);
		}else{
			wait_flag = 1;
		}
        
    }

    pool->started--;

    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return(NULL);
}

int threadpool_tasknum(threadpool_t *pool){
	if(pool == NULL) {
        return threadpool_invalid;
    }
	int task_num = -1;
    if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return threadpool_lock_failure;
    }
	task_num = count(pool->task);
	
	if(pthread_mutex_lock(&(pool->lock)) != 0) {
        return threadpool_lock_failure;
    }
	return task_num;
}

int threadpool_set_concurrence_check(threadpool_t *pool,check_func *checker){
	pool->checker = checker;
	return 0;
}


