// Author: Nat Tuck
// CS3650 starter code

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "barrier.h"

barrier*
make_barrier(int nn)
{
    int rv;
    barrier* bb = malloc(sizeof(barrier));
    assert(bb != 0);

    rv = pthread_mutex_init(&(bb->lock), 0);
    assert(rv == 0);

    rv = pthread_cond_init(&(bb->condv), 0);
    assert(rv == 0);

    bb->count = nn;
    bb->seen  = 0;
    return bb;
}

void
barrier_wait(barrier* bb)
{
    pthread_mutex_lock(&(bb->lock));

    bb->seen += 1;

    if (bb->seen >= bb->count) {
        pthread_cond_broadcast(&(bb->condv));
    }
    else {
        while (bb->seen < bb->count) {
            pthread_cond_wait(&(bb->condv), &(bb->lock));
        }
    }

    pthread_mutex_unlock(&(bb->lock));
}

void
free_barrier(barrier* bb)
{
    free(bb);
}

