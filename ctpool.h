/**
 * @file      ctpool.h
 * @brief     Header of CTP (C-Thread-Pool)
 * @date      Sat Apr 10 17:29:14 2021
 * @author    Michele Pes
 * @copyright BSD-3-Clause
 */

#ifndef CTPOOL_H_
#define CTPOOL_H_

typedef void* ctpool_t;

/**
 * @typedef pool_worker_t
 * The signature of the functions to run, the same to be passed to pthreads
 */
typedef void* (*pool_worker_t)(void*);

/**
 * @brief Initialize a new pool
 * @details Description
 * @param[in] threads_num The number of threads that can be spawned. If you pass
 *            zero, ctp will try to guess your cores number.
 *            See ctp_get_threads_num()
 * @param[in] queue_size The size of queue. If you pass zero, ctp will calculate
 *            the queue size according to three parameters. For details, see
 *            ctp_get_queue_size()
 * @param[in] block Non-zero if ctp_add_work() will block. Passing zero, if the
 *            queue is full, ctp_add_work() will fail
 * @return NULL on error, non NULL if pool is properly initialized
 */
ctpool_t ctp_init(unsigned int threads_num, unsigned int queue_size, int block);

/**
 * @brief Add passed work to pool
 * @param[in] pool The pool that will process this work
 * @param[in] func The work function to run
 * @param[in] argument The argument to pass to \a func
 * @return Non-zero if work is added, zero if not. The function will fail when
 *         the queue is full and thread is paused or cannot block 
 * @note A paused pool can receive new works
 */
int ctp_add_work(ctpool_t pool, pool_worker_t func, void* argument);

/**
 * @brief Pause a pool
 * @param[in] pool The pool to pause
 * @note Works can normally added to a paused pool. A paused pool can be
 *        regulary finished. In this case it will be resumed and terminated.
 *        Calling this function on a paused pool has no effect
 * @sa ctp_resume()
 */
void ctp_pause(ctpool_t pool);

/**
 * @brief Resume a paused pool
 * @param[in] pool The pool to resume
 * @note If you call this function on a \b non-paused pool, nothing is done
 * @sa ctp_pause()
 */
void ctp_resume(ctpool_t pool);

/**
 * @brief Clear the current queue. Works in progress won't be affected
 * @param[in] pool The pool to clear
 * @note A typical usage is when you want to immediately abort the pool.
 *        Since current works \b cannot be stopped (see ctp_pause()), you can
 *        call this function before calling ctp_finish()
 */
void ctp_clear_queue(ctpool_t pool);

/**
 * @brief Close and destroy a pool, <b>after all works</b> are done
 * @details Essentially, after calling this function all other functions cannot
 *          be called, and the pool will destroy itself when all works are done
 * @param[in] pool The pool to terminate
 * @param[out] spawned If not NULL, will receive the number of threads \b really
 *             spawned. The value is in range [0 .. ctp_get_threads_num()]
 * @note To terminate ignoring current queue, call ctp_clear_queue() before
 *        calling this function. Please note that current works will not be
 *        affected in any way. This function can be called on a paused pool.
 *        In this case, pool will be resumed and terminated
 */
void ctp_finish(ctpool_t pool, unsigned int* spawned);

/**
 * @brief Return the current pool status
 * @details The status can be paused, idle, running
 * @param[in] pool The pool to query
 * @return A negative value if paused, zero if idle or positive if running
 * @note If pool is paused, the returned value is reliable. Please do not rely
 *        on idle/running status since situation changes continuosly
 */
int ctp_get_status(const ctpool_t pool);

/**
 * @brief Query the number of threads in pool
 * @param[in] pool The pool to query
 * @return The \b maximum number that can be spawned on high load
 * @note This function is useful if you passed zero as first parameter of
 *        ctp_init(), to know if ctp correctly guess hw cores number. Otherwise,
 *        this function returns the passed value
 */
unsigned int ctp_get_threads_num(const ctpool_t pool);

/**
 * @brief Query the number of works \b currently enqueued
 * @param[in] pool The pool to query
 * @return A value in range [0..ctp_get_queue_size()]
 */
unsigned int ctp_get_works_count(const ctpool_t pool);

/**
 * @brief Query the queue size
 * @param[in] pool The pool to query
 * @return This function is useful if you passed zero as second  parameter of
 *         ctp_init(). Otherwise, this function returns the passed value.
 * @note If pool was initialized with 0, ctp calculate the queue size this way:
 *        <i>max(CTP_MIN_QUEUE_SIZE, threads_num*CTP_MULTIPLY_QUEUE_FACTOR)</i>.
 *        Note that both CTP_MIN_QUEUE_SIZE and CTP_MULTIPLY_QUEUE_FACTOR can be
 *        configured at compile time of the pool. The default value for
 *        CTP_MIN_QUEUE_SIZE is \b 256. The default value for
 *        CTP_MULTIPLY_QUEUE_FACTOR is \b 8
 */
unsigned int ctp_get_queue_size(const ctpool_t pool);

/**
 * @brief Calculate a percentage of \b current load factor
 * @param[in] pool The pool to query
 * @return A percentage of the current load factor
 */
unsigned int ctp_get_load_factor(const ctpool_t pool);

#endif /* CTPOOL_H_ */
