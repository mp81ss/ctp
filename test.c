/*
test free res
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <ctpool.h>

static pthread_mutex_t m;
static size_t fib_sum;
static unsigned int calculated;

static size_t fib(size_t n)
{
    return (n < 2) ? n : (fib(n - 1) + fib(n - 2));
}

static void* fib_worker(void* arg)
{
    fib_sum = fib((size_t)arg);
    return NULL;
}

static void* inc(void* arg)
{
    (void)arg;
    pthread_mutex_lock(&m);
    calculated++;
    pthread_mutex_unlock(&m);
    return NULL;
}

static void test1(void)
{
    ctpool_t pool;

    printf("Test1...");

    pool = ctp_init(3U, 3U, 0);
    if (pool != NULL) {
        assert(ctp_get_threads_num(pool) == 3U);
        assert(ctp_get_queue_size(pool) == 3U);
        assert(ctp_get_works_count(pool) == 0U);
        assert(ctp_get_status(pool) == 0);
        assert(ctp_get_load_factor(pool) == 0U);

        ctp_pause(pool);
        assert(ctp_get_status(pool) < 0);
        ctp_pause(pool);
        assert(ctp_get_status(pool) < 0);
        ctp_resume(pool);
        assert(ctp_get_status(pool) == 0);
        ctp_resume(pool);
        assert(ctp_get_status(pool) == 0);

        ctp_pause(pool);
        assert(ctp_add_work(pool, NULL, NULL) != 0);
        assert(ctp_add_work(pool, NULL, NULL) != 0);
        assert(ctp_add_work(pool, NULL, NULL) != 0);
        assert(ctp_add_work(pool, NULL, NULL) == 0);
        assert(ctp_get_works_count(pool) == 3U);
        ctp_clear_queue(pool);
        assert(ctp_get_works_count(pool) == 0U);

        ctp_pause(pool);
        ctp_finish(pool, NULL);

        puts("OK");
    }
    else {
        puts("FAILED");
    }
}

static void test2(void)
{
    ctpool_t pool;
    unsigned int i, total, rejected;

    printf("Test2...");

    pool = ctp_init(1U, 4U, -1);
    if (pool != NULL) {
        assert(ctp_get_threads_num(pool) > 0U);
        assert(ctp_get_queue_size(pool) == 4U);
        assert(ctp_get_works_count(pool) == 0U);
        assert(ctp_get_status(pool) == 0);
        assert(ctp_get_load_factor(pool) == 0U);

        total = 32U << 10U;
        rejected = 0U;
        for (i = 0U; i < total; i++) {
            const size_t r = (size_t)(rand() % 10);
            if (!ctp_add_work(pool, fib_worker, (void*)r)) rejected++;
        }
        assert(rejected == 0U);

        ctp_finish(pool, NULL);
        puts("OK");
    }
    else {
        puts("FAILED");
    }
}

static void test3(void)
{
    ctpool_t pool;
    unsigned int i, rejected, total;

    printf("Test3...");
    pool = ctp_init(0U, 0U, 0);
    if (pool != NULL) {
        assert(ctp_get_threads_num(pool) > 0U);
        assert(ctp_get_queue_size(pool) == 256U);
        assert(ctp_get_works_count(pool) == 0U);
        assert(ctp_get_status(pool) == 0);
        assert(ctp_get_load_factor(pool) == 0U);

        total = 1U << 20U;
        rejected = 0U;
        for (i = 0U; i < total; i++) {
            const size_t r = (size_t)(rand() % 10);
            if (!ctp_add_work(pool, fib_worker, (void*)r)) rejected++;
        }

        printf("Threads: %u, Works: %u, Rejected: %u (%.1f%%): ",
               ctp_get_threads_num(pool), total, rejected,
               ((float)rejected / (float)total) * 100.0f);

        ctp_finish(pool, NULL);
        puts("OK");
    }
    else {
        puts("FAILED");
    }
}

static void test4(void)
{
    ctpool_t pool;
    unsigned int i, total;

    printf("Test4...");
    pool = ctp_init(0U, 0U, 0);
    if (pool != NULL) {
        assert(ctp_get_threads_num(pool) > 0U);
        assert(ctp_get_queue_size(pool) == 256U);
        assert(ctp_get_works_count(pool) == 0U);
        assert(ctp_get_status(pool) == 0);
        assert(ctp_get_load_factor(pool) == 0U);

        total = 1U << 20U;
        for (i = 0U; i < total; i++) {
            const size_t r = (size_t)(rand() % 10);
            ctp_add_work(pool, fib_worker, (void*)r);
        }

        ctp_finish(pool, NULL);
        puts("OK");
    }
    else {
        puts("FAILED");
    }
}

static void test5(void)
{
    ctpool_t pool;
    unsigned int i, total, rejected;

    printf("Test5...");
    if (pthread_mutex_init(&m, NULL) == 0) {

        pool = ctp_init(0U, 1024U, -1);
        if (pool != NULL) {
            assert(ctp_get_threads_num(pool) > 0U);
            assert(ctp_get_queue_size(pool) == 1024U);
            assert(ctp_get_works_count(pool) == 0U);
            assert(ctp_get_status(pool) == 0);
            assert(ctp_get_load_factor(pool) == 0U);

            total = 2U << 20U;
            calculated = 0U;
            rejected = 0U;
            for (i = 0U; i < total; i++) {
                if (!ctp_add_work(pool, inc, NULL)) rejected++;
            }
            assert(rejected == 0U);

            ctp_finish(pool, NULL);

            assert(calculated == total);
            puts("OK");
        }

        pthread_mutex_destroy(&m);
    }
}

static void test6(void)
{
    ctpool_t pool;
    unsigned int i, total, rejected, spawned;

    printf("Test6...");
    if (pthread_mutex_init(&m, NULL) == 0) {

        pool = ctp_init(2U << 20U, 2U, -1);
        if (pool != NULL) {
            assert(ctp_get_threads_num(pool) > 0U);
            assert(ctp_get_queue_size(pool) == 2U);
            assert(ctp_get_works_count(pool) == 0U);
            assert(ctp_get_status(pool) == 0);
            assert(ctp_get_load_factor(pool) == 0U);

            total = 2U << 20U;
            calculated = 0U;
            rejected = 0U;
            for (i = 0U; i < total; i++) {
                if (!ctp_add_work(pool, inc, NULL)) rejected++;
            }

            ctp_finish(pool, &spawned);

            printf("Threads: %u, Spawned: %u, Works: %u, "
                   "Rejected: %u (%.1f%%): ", ctp_get_threads_num(pool),
                   spawned, total, rejected,
                   ((float)rejected / (float)total) * 100.0f);

            puts("OK");
        }

        pthread_mutex_destroy(&m);
    }
}

int main(void)
{
    srand((unsigned int)time(NULL));

    test1();
    test2();
    test3();
    test4();
    test5();
    test6();

    puts("\npool done");

    return 0;
}
