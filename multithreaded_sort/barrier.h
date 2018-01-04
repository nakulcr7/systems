#ifndef BARRIER_H
#define BARRIER_H

#include <pthread.h>

typedef struct barrier {
    pthread_mutex_t lock;
    pthread_cond_t condv;
    int   count;
    int   seen;
} barrier;

barrier* make_barrier(int nn);
void barrier_wait(barrier* bb);
void free_barrier(barrier* bb);


#endif

