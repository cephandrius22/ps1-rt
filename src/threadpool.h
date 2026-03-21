/*
 * threadpool.h — Fixed-size thread pool for parallel rendering.
 *
 * Workers sleep on a condition variable between frames. The main thread
 * posts a job (function pointer + per-worker args), wakes all workers,
 * and waits on a barrier for completion. Designed for fork-join
 * parallelism where all workers run the same function on different data.
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdbool.h>

#define THREADPOOL_MAX_WORKERS 16

typedef void (*threadpool_func)(void *arg);

typedef struct {
    int id;                       /* worker index (0..worker_count-1) */
    struct ThreadPool *pool;
} WorkerInfo;

typedef struct ThreadPool {
    pthread_t threads[THREADPOOL_MAX_WORKERS];
    WorkerInfo worker_info[THREADPOOL_MAX_WORKERS];
    int worker_count;

    /* Job dispatch */
    threadpool_func func;
    void *args[THREADPOOL_MAX_WORKERS]; /* one arg per worker */

    /* Synchronization */
    pthread_mutex_t mutex;
    pthread_cond_t  wake_cond;    /* main → workers: new job ready */
    pthread_cond_t  done_cond;    /* workers → main: all done */
    int generation;               /* incremented each dispatch */
    int jobs_finished;            /* workers that have finished */
    bool shutdown;
} ThreadPool;

/* Create pool with `count` worker threads. Returns 0 on success. */
int  threadpool_create(ThreadPool *pool, int count);

/* Dispatch: run `func(args[i])` on worker i, block until all finish. */
void threadpool_dispatch(ThreadPool *pool, threadpool_func func, void *args[], int count);

/* Shut down all threads and clean up. */
void threadpool_destroy(ThreadPool *pool);

/* Returns number of available CPU cores (sysctl on macOS). */
int  threadpool_cpu_count(void);

#endif
