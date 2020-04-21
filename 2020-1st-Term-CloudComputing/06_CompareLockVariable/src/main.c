//
//  main.c
//  Assignment
//
//  Created by User on 23/03/2020.
//  Copyright © 2020 User. All rights reserved.
//

#include "kvs.h"

#include <time.h>

#define MINIMUM_KEY_COUNT 1000000
#define THREAD_COUNT 10

typedef void (*THREAD_FUNC)(void*);

struct SHARED_STATE_T {
    pthread_mutex_t mu;
    pthread_cond_t    cv;
    int total;
    int num_inited;
    int num_done;
    int start;
};

struct THREAD_STATE_T {
    pthread_t tid;
    int rand_seed;
    time_t start;
    time_t finish;
    int done;
    struct KVS_T* kvs;
};

struct THREAD_ARG_T {
    struct SHARED_STATE_T* ss;
    struct THREAD_STATE_T* ts;
    THREAD_FUNC tf;
};


long *keys;
long *values;

void *thread_main(void* arg);
void do_benchmark(struct KVS_T *kvs, int num_threads, THREAD_FUNC tf);
void search(void *arg);
void insert(void *arg);
//void delete(void *arg);
void initialize_keys(void);
void generate_random_keys(void);

int main() {
    long i;
    struct KVS_T *kvs = kv_create_db(4);
    if (kvs == 0) {
        perror("[ERROR] not allocated kvs:");
        exit(-1);
    }
    
    initialize_keys();
    //generate_random_keys();
    long key_count = MINIMUM_KEY_COUNT * (THREAD_COUNT -1);
    printf("[MAIN] inserting all required keys");
    for (i = 0; i < key_count; i++) {
        struct KVS_NODE_T *new_node = (struct KVS_NODE_T*) malloc(sizeof(struct KVS_NODE_T));
        if (new_node == 0) {
            perror("[ERROR] insert()");
            exit(-1);
        }
        init_list_head(&new_node->list);
        new_node->key = &keys[i];
        new_node->value = &values[i];
        kv_put(new_node, kvs);
        if ((i % 500000) == 0) printf(".");
    }
    puts("\n");
    for(i=0; i<60; i++) printf("=");
    puts("\n[MAIN] test Started!\n");
    do_benchmark(kvs, THREAD_COUNT, NULL);
    puts("\n[MAIN] test is done!");
    for(i=0; i<60; i++) printf("=");
    printf("\n");
    kv_destroy_db(kvs);
    return 0;
}


void *thread_main(void* arg) {
    struct THREAD_ARG_T* thread_arg = arg;
    struct SHARED_STATE_T* ss = thread_arg->ss;
    struct THREAD_STATE_T* ts = thread_arg->ts;
    
    pthread_mutex_lock(&ss->mu);
    ss->num_inited++;
    if (ss->num_inited >= ss->total) {
        pthread_cond_broadcast(&ss->cv);
    }
    while (!ss->start) {
        pthread_cond_wait(&ss->cv, &ss->mu);
    }
    pthread_mutex_unlock(&ss->mu);
    
    ts->start = time(NULL);
    (thread_arg->tf)((void *)ts);
    ts->finish = time(NULL);
    
    pthread_mutex_lock(&ss->mu);
    ss->num_done++;
    if (ss->num_done >= ss->total) {
        pthread_cond_broadcast(&ss->cv);
    }
    pthread_mutex_unlock(&ss->mu);
    
    return NULL;
}


void do_benchmark(struct KVS_T *kvs, int num_threads, THREAD_FUNC tf) {
    int i, ret;
    long elapsed = 0, num_ops = 0;
    struct SHARED_STATE_T shared;
    struct THREAD_STATE_T* tss;
    struct THREAD_ARG_T* args;
    
    pthread_mutex_init(&shared.mu, NULL);
    pthread_cond_init(&shared.cv, NULL);
    shared.num_inited = 0;
    shared.num_done = 0;
    shared.start = 0;
    shared.total = num_threads;
    
    args = (struct THREAD_ARG_T *)calloc(num_threads, sizeof(struct THREAD_ARG_T));
    if (args == NULL) {
        perror("[ERROR] do_benchmark()");
        return;
    }
    
    tss = (struct THREAD_STATE_T*) calloc(num_threads, sizeof(struct THREAD_STATE_T));
    if (tss == NULL) {
        perror("[ERROR] do_benchmark()");
        free(args);
        return;
    }
    
    for (i = 0; i < num_threads; i++) {
        args[i].ss = &shared;
        args[i].ts = &tss[i];
        args[i].ts->kvs = kvs;
        args[i].ts->done = 0;
        args[i].ts->rand_seed = i;
        if (tf) {
            args[i].tf = tf;
        } else {
            args[i].tf = (i == num_threads -1) ? insert : search;
        }
        
        pthread_create(&args[i].ts->tid, NULL, thread_main, &args[i]);
    }
    
    pthread_mutex_lock(&shared.mu);
    while (shared.num_inited < num_threads) {
        pthread_cond_wait(&shared.cv, &shared.mu);
    }
    shared.start = 1;
    pthread_cond_broadcast(&shared.cv);
    while (shared.num_done < num_threads) {
        pthread_cond_wait(&shared.cv, &shared.mu);
    }
    pthread_mutex_unlock(&shared.mu);
    
    for (i = 0; i < num_threads; i++) {
        ret = pthread_join(args[i].ts->tid, NULL);
    }
    
    // results
    for (i = 0; i < num_threads; i++) {
        elapsed = difftime(tss[i].finish, tss[i].start);
        num_ops += tss[i].done;
    }
    
    printf("\n[MAIN] elapsed = %ld sec\n", elapsed);
    double ops = num_ops / elapsed;
    printf("[MAIN] ops = %lf\n", ops);
    
    // free
    free(tss);
    free(args);
}


void search(void* arg) {
    long i;
    struct THREAD_STATE_T* ts = arg;
    struct KVS_NODE_T* p_node;
    long start = MINIMUM_KEY_COUNT * ts->rand_seed;
    long end = MINIMUM_KEY_COUNT * (ts->rand_seed + 1);
    printf("[THREAD-%d] search(): range = [%ld, %ld]\n", (int)ts->tid, start, end);
    for (i = start; i < end; i++) {
        p_node = kv_get(&keys[i], ts->kvs);
        if (p_node) ts->done++;
        if ((ts->done % 200000) == 0) printf(".");
    }
}

void insert(void* arg) {
    long i;
    struct THREAD_STATE_T* ts = arg;
    long start = MINIMUM_KEY_COUNT * ts->rand_seed;
    long end = MINIMUM_KEY_COUNT * (ts->rand_seed + 1);
    printf("[THREAD-%d] insert(): range = [%ld, %ld]\n", (int)ts->tid, start, end);
    for (i = start; i < end; i++) {
        struct KVS_NODE_T *new_node = (struct KVS_NODE_T*) malloc(sizeof(struct KVS_NODE_T));
        if (new_node == 0) {
            perror("[ERROR] insert()");
            exit(-1);
        }
        init_list_head(&new_node->list);
        new_node->key = &keys[i];
        new_node->value = &values[i];
        kv_put(new_node, ts->kvs);
        ts->done++;
        if ((ts->done % 200000) == 0) printf(".");
    }
}


void initialize_keys() {
    int i;
    int key_count = MINIMUM_KEY_COUNT * THREAD_COUNT;
    if (!(keys = (long*) calloc(key_count, sizeof(long)))) {
        perror("[ERROR] initialize_keys()");
        exit(-1);
    }
    
    if (!(values = (long*) calloc(key_count, sizeof(long)))) {
        perror("[ERROR] initialize_keys()");
        exit(-1);
    }
    
    for (i=0; i<key_count; i++) {
        keys[i] = i+1;
        values[i] = 0xdeadbeef ^ keys[i];
    }
    puts("[MAIN] sequential keys was generated!");
}


void generate_random_keys() {
    int i;
    int key_count = MINIMUM_KEY_COUNT * THREAD_COUNT;
    srand(time(NULL));
    for (i=0; i<key_count; i++) {
        int pos = random() % key_count;
        long tmp = keys[i];
        keys[i] = keys[pos];
        keys[pos] = tmp;
    }
    puts("[MAIN] random keys was generated!");
}
