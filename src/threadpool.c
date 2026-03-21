#include "threadpool.h"
#include <stdlib.h>
#include <unistd.h>

static void *worker_thread(void *arg) {
    WorkerInfo *info = arg;
    ThreadPool *pool = info->pool;
    int my_id = info->id;
    int my_gen = 0;

    pthread_mutex_lock(&pool->mutex);
    for (;;) {
        /* Wait until a new generation is posted or shutdown */
        while (pool->generation == my_gen && !pool->shutdown)
            pthread_cond_wait(&pool->wake_cond, &pool->mutex);

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }

        /* Latch this generation so we don't re-enter the loop */
        my_gen = pool->generation;

        /* Grab our pre-assigned arg */
        threadpool_func func = pool->func;
        void *my_arg = pool->args[my_id];
        pthread_mutex_unlock(&pool->mutex);

        /* Do the work (no lock held) */
        func(my_arg);

        /* Signal completion */
        pthread_mutex_lock(&pool->mutex);
        pool->jobs_finished++;
        if (pool->jobs_finished == pool->worker_count)
            pthread_cond_signal(&pool->done_cond);
    }
}

int threadpool_create(ThreadPool *pool, int count) {
    if (count <= 0 || count > THREADPOOL_MAX_WORKERS)
        return -1;

    pool->worker_count = count;
    pool->func = NULL;
    pool->generation = 0;
    pool->jobs_finished = 0;
    pool->shutdown = false;

    for (int i = 0; i < THREADPOOL_MAX_WORKERS; i++)
        pool->args[i] = NULL;

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->wake_cond, NULL);
    pthread_cond_init(&pool->done_cond, NULL);

    for (int i = 0; i < count; i++) {
        pool->worker_info[i].id = i;
        pool->worker_info[i].pool = pool;
        if (pthread_create(&pool->threads[i], NULL, worker_thread, &pool->worker_info[i]) != 0) {
            /* Clean up threads already created */
            pool->shutdown = true;
            pthread_cond_broadcast(&pool->wake_cond);
            for (int j = 0; j < i; j++)
                pthread_join(pool->threads[j], NULL);
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->wake_cond);
            pthread_cond_destroy(&pool->done_cond);
            return -1;
        }
    }

    return 0;
}

void threadpool_dispatch(ThreadPool *pool, threadpool_func func, void *args[], int count) {
    if (count <= 0) return;

    pthread_mutex_lock(&pool->mutex);
    pool->func = func;
    pool->jobs_finished = 0;
    for (int i = 0; i < count; i++)
        pool->args[i] = args[i];
    pool->generation++;
    pthread_cond_broadcast(&pool->wake_cond);

    /* Wait for all workers to finish */
    while (pool->jobs_finished < count)
        pthread_cond_wait(&pool->done_cond, &pool->mutex);

    pthread_mutex_unlock(&pool->mutex);
}

void threadpool_destroy(ThreadPool *pool) {
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->wake_cond);
    pthread_mutex_unlock(&pool->mutex);

    for (int i = 0; i < pool->worker_count; i++)
        pthread_join(pool->threads[i], NULL);

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->wake_cond);
    pthread_cond_destroy(&pool->done_cond);
}

int threadpool_cpu_count(void) {
    int count = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1) count = 1;
    if (count > THREADPOOL_MAX_WORKERS) count = THREADPOOL_MAX_WORKERS;
    return count;
}
