#include "ctpool.h"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif (defined(__linux__)) || (defined(_AIX)) || (defined(__sun))
#include <unistd.h>
#define CTP_ON_LINUX
#elif (defined(__FreeBSD__)) || (defined(__NetBSD__)) \
      || (defined(__OpenBSD__)) || (defined(__bsdi__)) \
      || (defined(__DragonFly__)) \
      || ((defined(__APPLE__)) && (defined(__MACH__)))
#define CTP_ON_BSD
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#endif

#ifndef CTP_DEFAULT_THREADS_NUM
#define CTP_DEFAULT_THREADS_NUM 4U
#else
#if (CTP_DEFAULT_THREADS_NUM < 1)
#error Invalid CTP_DEFAULT_THREADS_NUM value
#endif
#endif

#ifndef CTP_MULTIPLY_QUEUE_FACTOR
#define CTP_MULTIPLY_QUEUE_FACTOR 8U
#else
#if (CTP_MULTIPLY_QUEUE_FACTOR < 1)
#error Invalid CTP_MULTIPLY_QUEUE_FACTOR value
#endif
#endif

#ifndef CTP_MIN_QUEUE_SIZE
#define CTP_MIN_QUEUE_SIZE 256U
#else
#if (CTP_MIN_QUEUE_SIZE < 1)
#error Invalid CTP_MIN_QUEUE_SIZE value
#endif
#endif

#define NON_PAUSED_VALUE (0U - 1U)


typedef unsigned int pu;

struct worker_t {
    pool_worker_t func;
    void* argument;
};

struct pool_t {
    struct worker_t* queue;
    pthread_t* threads;
    pthread_mutex_t mutex;
    sem_t semaphore;
    sem_t sem_add;
    pu threads_num;
    pu running;
    pu waiting;
    pu queue_size;
    pu queue_count;
    pu old_count;
    pu head;
    int block;
    int done;
};


static pu get_threads_num(void)
{
    pu cpus = CTP_DEFAULT_THREADS_NUM;

#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors > 0U) {
        cpus = (pu)si.dwNumberOfProcessors;
    }
#elif defined CTP_ON_LINUX

#if defined _SC_NPROCESSORS_ONLN
    const long v = sysconf(_SC_NPROCESSORS_ONLN);
    if (v > 0) {
        cpus = (pu)v;
    }
#endif

#elif defined CTP_ON_BSD
    int mib[2];
    size_t len = sizeof(int);
    int v;

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if ((sysctl(mib, 2U, &v, &len, NULL, 0U) == 0)
        && (v > 0))
    {
        cpus = (pu)v;
    }
#endif

    return cpus;
}

static void* run(void* arg)
{
    struct pool_t* const p = (struct pool_t*)arg;
    int must_sleep = 0;

    for (;;) {
        pthread_mutex_lock(&p->mutex);

        if (must_sleep != 0) {
            p->waiting--;
        }

        must_sleep = (p->queue_count == 0U);

        if (must_sleep == 0) {
            pool_worker_t func = p->queue[p->head].func;
            void* const argument = p->queue[p->head].argument;

            if (++p->head == p->queue_size) {
                p->head = 0U;
            }
            if (--p->queue_count == 0U) {
                p->head = 0U;
            }

            sem_post(&p->sem_add);
            pthread_mutex_unlock(&p->mutex);

            func(argument);
        }
        else {
            if (p->done == 0) {
                p->waiting++;
                pthread_mutex_unlock(&p->mutex);
                sem_wait(&p->semaphore);
            }
            else {
                break;
            }
        }
    }

    p->running--;

    pthread_mutex_unlock(&p->mutex);

    pthread_exit(NULL);
    return NULL;
}

static struct pool_t* free_resources(struct pool_t* p, int level)
{
    if ((level > 0) && (level < 6)) {
        if (level >= 2) {
            free(p->threads);
        }
        if (level >= 3) {
            free(p->queue);
        }
        if (level >= 4) {
            pthread_mutex_destroy(&p->mutex);
        }
        if (level >= 5) {
            sem_destroy(&p->semaphore);
        }

        free(p);
        p = NULL;
    }

    return p;
}

ctpool_t ctp_init(unsigned int threads_num, unsigned int queue_size, int block)
{
    int level = 0;

    struct pool_t* p = (struct pool_t*)malloc(sizeof(struct pool_t));
    if (p != NULL) {
        level++;

        p->threads_num = (threads_num > 0U) ? threads_num : get_threads_num();
        p->threads = (pthread_t*)malloc(sizeof(pthread_t)
                                        * (size_t)p->threads_num);
        if (p->threads != NULL) {
            level++;

            if (queue_size > 0U) {
                p->queue_size = queue_size;
            }
            else {
                p->queue_size = p->threads_num * CTP_MULTIPLY_QUEUE_FACTOR;
                if (p->queue_size < CTP_MIN_QUEUE_SIZE) {
                    p->queue_size = CTP_MIN_QUEUE_SIZE;
                }
            }

            p->queue = (struct worker_t*) malloc(sizeof(struct worker_t)
                                                 * (size_t)(p->queue_size));
            if (p->queue != NULL) {
                level++;

                if (pthread_mutex_init(&p->mutex, NULL) == 0) {
                    level++;

                    if (sem_init(&p->semaphore, 0, 0U) == 0) {
                        level++;

                        if (sem_init(&p->sem_add, 0, p->queue_size) == 0) {
                            level++;

                            p->running = 0U;
                            p->waiting = 0U;
                            p->queue_count = 0U;
                            p->old_count = NON_PAUSED_VALUE;
                            p->head = 0U;
                            p->block = block;
                            p->done = 0;
                        }
                    }
                }
            }
        }
    }

    p = free_resources(p, level);

    return p;
}

static int add_last(struct pool_t* p, pu* p_count)
{
    const int ok = (p->block != 0) && (p->old_count == NON_PAUSED_VALUE);

    if (ok != 0) {
        int owned = 0;

        do {
            pthread_mutex_unlock(&p->mutex);

            sem_wait(&p->sem_add);

            pthread_mutex_lock(&p->mutex);

            if (*p_count == p->queue_size) {
                sem_post(&p->sem_add);
            }
            else {
                owned--;
            }

        } while (owned == 0);
    }

    return ok;
}

int ctp_add_work(ctpool_t pool, pool_worker_t func, void* argument)
{
    struct pool_t* const p = (struct pool_t*)pool;
    int added = -1;

    pthread_mutex_lock(&p->mutex);

    if (p->done == 0) {
        pu* const p_count = (p->old_count == NON_PAUSED_VALUE) ?
            &p->queue_count : &p->old_count;

        if (*p_count == p->queue_size) {
            added = add_last(p, p_count);
        }
        else {
            sem_wait(&p->sem_add);
        }

        if (added != 0) {
            pu index = p->head + *p_count;
            if (index >= p->queue_size) {
                index -= p->queue_size;
            }
            p->queue[index].func = func;
            p->queue[index].argument = argument;
            *p_count = *p_count + 1U;

            if (p->waiting > 0U) {
                sem_post(&p->semaphore);
            }
            else {
                if (p->running < p->threads_num) {
                    added = pthread_create(&p->threads[p->running],
                                           NULL, run, p);
                    if ((added == 0) || (p->running > 0U)) {
                        if (added == 0) {
                            p->running++;
                        }
                        added = -1;
                    }
                    else {
                        *p_count = *p_count - 1U;
                        added = 0;
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&p->mutex);

    return added;
}

void ctp_pause(ctpool_t pool)
{
    struct pool_t* const p = (struct pool_t*)pool;
    pthread_mutex_lock(&p->mutex);
    if (p->old_count == NON_PAUSED_VALUE) {
        p->old_count = p->queue_count;
        p->queue_count = 0U;
    }
    pthread_mutex_unlock(&p->mutex);
}

void ctp_resume(ctpool_t pool)
{
    struct pool_t* const p = (struct pool_t*)pool;
    pthread_mutex_lock(&p->mutex);
    if (p->old_count != NON_PAUSED_VALUE) {
        pu i;

        p->queue_count = p->old_count;
        p->old_count = NON_PAUSED_VALUE;

        for (i = 0U; i < p->waiting; i++) {
            sem_post(&p->semaphore);
        }
    }
    pthread_mutex_unlock(&p->mutex);
}

void ctp_clear_queue(ctpool_t pool)
{
    struct pool_t* const p = (struct pool_t*)pool;
    pthread_mutex_lock(&p->mutex);
    if (p->old_count != NON_PAUSED_VALUE) {
        p->old_count = 0U;
    }
    else {
        p->queue_count = 0U;
    }
    p->head = 0U;
    pthread_mutex_unlock(&p->mutex);
}

void ctp_finish(ctpool_t pool, unsigned int* spawned)
{
    struct pool_t* const p = (struct pool_t*)pool;

    pthread_mutex_lock(&p->mutex);

    if (p->done == 0) {
        const pu running = p->running;
        pu i;

        p->done--;

        if (p->old_count != NON_PAUSED_VALUE) {
            p->queue_count = p->old_count;
            p->old_count = NON_PAUSED_VALUE;
        }

        for (i = 0U; i < p->waiting; i++) {
            sem_post(&p->semaphore);
        }

        if (spawned != NULL) {
            *spawned = running;
        }

        pthread_mutex_unlock(&p->mutex);

        for (i = 0U; i < running; i++) {
            pthread_join(p->threads[i], NULL);
        }

        sem_destroy(&p->sem_add);
        sem_destroy(&p->semaphore);
        free(p->queue);
        free(p->threads);
        pthread_mutex_destroy(&p->mutex);
        free(p);
    }
    else {
        pthread_mutex_unlock(&p->mutex);
    }
}

int ctp_get_status(const ctpool_t pool)
{
    const struct pool_t* const p = (const struct pool_t*)pool;
    int status = (p->old_count == NON_PAUSED_VALUE) ? 1 : -1;
    if ((status == 1) && (p->waiting == p->running)) {
        status = 0;
    }
    return status;
}

unsigned int ctp_get_threads_num(const ctpool_t pool)
{
    const struct pool_t* const p = (const struct pool_t*)pool;
    return p->threads_num;
}

unsigned int ctp_get_works_count(const ctpool_t pool)
{
    const struct pool_t* const p = (const struct pool_t*)pool;
    return (p->old_count == NON_PAUSED_VALUE) ? p->queue_count : p->old_count;
}

unsigned int ctp_get_queue_size(const ctpool_t pool)
{
    const struct pool_t* const p = (const struct pool_t*)pool;
    return p->queue_size;
}

unsigned int ctp_get_load_factor(const ctpool_t pool)
{
    const struct pool_t* const p = (const struct pool_t*)pool;
    const pu count = (p->old_count == NON_PAUSED_VALUE) ?
        p->queue_count : p->old_count;
    const float sum = (float)(p->running + count);
    const float k = ((sum * 100.0f) / (float)p->threads_num) + 0.5f;
    return (unsigned int)k;
}
