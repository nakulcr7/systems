#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>

#include "float_vec.h"
#include "barrier.h"
#include "utils.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int
compare_floats(const void* aap, const void* bbp)
{
    float aa = *((float*) aap);
    float bb = *((float*) bbp);

    if (aa < bb) {
        return -1;
    }
    else if (aa > bb) {
        return 1;
    }
    else {
        return 0;
    }
}

void
qsort_floats(floats* xs)
{
    qsort(xs->data, xs->size, sizeof(float), compare_floats);
}

floats*
sample(floats* input, int P)
{
    long size = input->size;
    float* data = input->data;
    int kk = P - 1;
    floats* raw = make_floats(kk * 3);

    for (int ii = 0; ii < raw->size; ++ii) {
        long jj = random() % size;
        raw->data[ii] = data[jj];
    }

    qsort_floats(raw);

    floats* samps = make_floats(kk + 2);
    samps->data[0] = 0;
    samps->data[kk + 1] = INFINITY;

    for (int ii = 1; ii < (kk + 1); ++ii) {
        int jj = ii * 3 - 2;
        samps->data[ii] = raw->data[jj];
    }

    free_floats(raw);

    return samps;
}

void
sort_worker(int pnum, floats* input, const char* output, int P, floats* samps, long* sizes, barrier* bb)
{
    floats* xs = make_floats(0);
    float* data = input->data;
    long size = input->size;

    for (long ii = 0; ii < size; ++ii) {
        float xx = data[ii];

        if (xx >= samps->data[pnum] && xx < samps->data[pnum + 1]) {
            floats_push(xs, data[ii]);
        }
    }

    printf("%d: start %.04f, count %ld\n", pnum, samps->data[pnum], xs->size);

    sizes[pnum] = xs->size;

    qsort_floats(xs);

    barrier_wait(bb);

    long i0 = 0;

    for (int ii = 0; ii < pnum; ++ii) {
        i0 += sizes[ii];
    }

    int rv;
    int fd = open(output, O_WRONLY);
    check_rv(fd);

    rv = lseek(fd, sizeof(long) + i0 * sizeof(float), SEEK_SET);
    check_rv(rv);

    for (long ii = 0; ii < xs->size; ++ii) {
        rv = write(fd, &(xs->data[ii]), sizeof(float));
        check_rv(rv);
    }

    rv = close(fd);
    check_rv(rv);

    free_floats(xs);
}

typedef struct worker_params {
    int pnum;
    floats* input;
    const char* output;
    int P;
    floats* samps;
    long* sizes;
    barrier* bb;
} worker_params;

void*
sort_worker_wrap(void* data)
{
    worker_params* wp = (worker_params*) data;
    sort_worker(wp->pnum, wp->input, wp->output, wp->P, wp->samps, wp->sizes, wp->bb);
    free(data);
    return 0;
}

void
run_sort_workers(floats* input, const char* output, int P, floats* samps, long* sizes, barrier* bb)
{
    pthread_t kids[P];
    int rv;

    for (int ii = 0; ii < P; ++ii) {
        worker_params* wp = malloc(sizeof(worker_params));
        wp->pnum = ii;
        wp->input = input;
        wp->output = output;
        wp->P = P;
        wp->samps = samps;
        wp->sizes = sizes;
        wp->bb = bb;
        rv = pthread_create(&(kids[ii]), 0, sort_worker_wrap, wp);
        assert(rv == 0);
    }

    for (int ii = 0; ii < P; ++ii) {
        rv = pthread_join(kids[ii], 0);
        assert(rv == 0);
    }
}

void
sample_sort(floats* input, const char* output, int P, long* sizes, barrier* bb)
{
    floats* samps = sample(input, P);
    run_sort_workers(input, output, P, samps, sizes, bb);
    free_floats(samps);
}

int
main(int argc, char* argv[])
{
    if (argc != 4) {
        printf("Usage:\n");
        printf("\t%s P input.dat output.dat\n", argv[0]);
        return 1;
    }

    const int P = atoi(argv[1]);
    const char* fname = argv[2];
    const char* oname = argv[3];

    seed_rng();

    int rv;

    int fd = open(fname, O_RDONLY);
    check_rv(fd);

    long count;
    rv = read(fd, &count, sizeof(long));
    check_rv(rv);

    int ofd = open(oname, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    check_rv(ofd);

    rv = ftruncate(ofd, sizeof(long) + count*sizeof(float));
    check_rv(rv);

    rv = write(ofd, &count, sizeof(long));
    check_rv(rv);

    floats* input = make_floats(0);

    for (long ii = 0; ii < count; ++ii) {
        float x;
        rv = read(fd, &x, sizeof(float));
        check_rv(rv);
        floats_push(input, x);
    }

    rv = close(fd);
    check_rv(rv);

    barrier* bb = make_barrier(P);

    long* sizes = malloc(P * sizeof(long));
    sample_sort(input, oname, P, sizes, bb);
    free(sizes);

    free_barrier(bb);
    free_floats(input);

    return 0;
}

