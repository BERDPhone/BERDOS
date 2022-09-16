/**
 * @file piccolo_os.h
 * @author Keith Standiford
 * @brief Piccolo OS Plus
 * @version 1.01
 * @date 2022-08-15
 * 
 *
 * @copyright Copyright (C) 2022 Keith Standiford
 * All rights reserved.
 * 
 * Portions copyright (C) 2021 Gary Sims
 * Portions copyright (C) 2017 Scott Nelson
 * Portions copyright (C) 2015-2018 National Cheng Kung University, Taiwan
 * Portions copyright (C) 2014-2017 Chris Stones
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Piccolo OS is a simple cooperative multi-tasking OS originally written by 
 * [Gary Sims](https://github.com/garyexplains) as a teaching tool. 
 */
#ifndef PICCOLO_OS_H
#define PICCOLO_OS_H
#include "hardware/sync.h"
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif


/** @defgroup Intern The Piccolo Plus Internals
 * 
 * @{
 */

/**
 * @brief Size of a task stack in 32 bit words. 
 * @note Must be **even**, for exception frame stack alignment!
 * 
 */
#define PICCOLO_OS_STACK_SIZE 1024

/** Exception return behavior value **/
#define PICCOLO_OS_THREAD_PSP 0xFFFFFFFD

/**
 * @brief The OS time slice, in microseconds 
 * 
    Setting time slice to zero will disable Systick and preemptive scheduling!
*/
#define PICCOLO_OS_TIME_SLICE 1000
/**
 * @brief The maximum time that the scheduler will sleep in the idle task (in usec).
 * 
 * This will influence the latency for tasks waking up for signals, 
 * for example. Setting this to zero will disable the idle task.
*/
#define PICCOLO_OS_MAX_IDLE 700

/**
 * @brief If true, scheduler will not idle if tasks are blocking for signals.
 * 
 * If set to false, the scheduler will run the idle task for the minimum of
 * PICCOLO_OS_MAX_IDLE or the smallest time remaining for any task with a timeout
 * running. This will improve power consumption but potentially delay the response
 * of any task waiting for a signal. This may make good sense for applications without
 * serious response time concerns.
 * 
 */
#define PICCOLO_OS_NO_IDLE_FOR_SIGNALS true

/**
 * @brief Enable/disable multi-core scheduling
 * 
**/
#define PICCOLO_OS_MULTICORE true

/**
 * @brief Signal channel size. (max is INT32_MAX)
 * 
 * The number of signals which can be queued in a signal
 * channel (+1). It can be set smaller for testing or special purposes.
 * @remark Remember that signals do not use any memory, so there
 * is no penalty for a large number here.
 */
#define PICCOLO_OS_MAX_SIGNAL 10

/** Piccolo spin lock to use **/
#define PICCOLO_SPIN_LOCK_ID PICO_SPINLOCK_ID_OS1

/**
 * @brief Piccolo OS task data structure
 * 
 */
// \cond force_doxygen_to_list
typedef /*\endcond**/
struct piccolo_os_task_t {
    volatile uint32_t task_flags;               /**< Task Status **/
    struct piccolo_os_task_t *next_task;        /**< next task in scheduler chain **/
    struct piccolo_os_task_t *prev_task;        /**< previous task in scheduler chain **/
    struct piccolo_os_task_t *task_sending_to;  /**< task that this one if blocked trying to signal **/
    absolute_time_t wakeup;                     /**< end of sleep time or timeout **/
    volatile uint32_t signal_in;                /**< input values for the task's input signal channel **/
    volatile uint32_t signal_out;               /**< output values for the task's input signal channel **/
    uint32_t signal_limit;                      /**< maximum (-1) number of signals the task can queue **/
    uint32_t *stack_ptr;                        /**< the task stack pointer **/
    uint32_t __attribute__((aligned(8))) stack[PICCOLO_OS_STACK_SIZE];       /**< the task stack space **/
}  piccolo_os_task_t;

/**
 * @brief Piccolo OS internal data structure
 * 
 */

struct {
  piccolo_os_task_t* task_list_head;   /**< pointer to the first task in scheduler list **/
  piccolo_os_task_t* task_list_tail;   /**< pointer to the last task in scheduler list **/
  volatile piccolo_os_task_t* this_task[2];     /**< `this_task[i]` points to task being run on core `i`. **/
  volatile piccolo_os_task_t *current_task;     /**< last task started on either core for round-robin scheduling **/
  piccolo_os_task_t *zombies;          /**< (singly linked) list of dead tasks for garbage collection **/
   piccolo_os_task_t *garbage_man;              /**< Garbage collector task (so schedulers can signal him) **/
  spin_lock_t *piccolo_lock;                    /**< spin lock instance **/
} typedef piccolo_os_internals_t;

// Define Task Flag values
enum piccolo_task_flag_values
{
    PICCOLO_TASK_RUNNING    = 0x1,      ///< Task is running
    PICCOLO_TASK_ZOMBIE     = 0x2,      ///< Task has ended and should be removed by the scheduler
    PICCOLO_TASK_SLEEPING   = 0x4,      ///< Task has a timeout running
    PICCOLO_TASK_GET_SIGNAL_BLOCKED     = 0x8,  ///< Task blocked getting signal
    PICCOLO_TASK_SEND_SIGNAL_BLOCKED    = 0x10, ///< Task block sending signal
    PICCOLO_TASK_BLOCKING = (PICCOLO_TASK_SLEEPING | PICCOLO_TASK_GET_SIGNAL_BLOCKED | PICCOLO_TASK_SEND_SIGNAL_BLOCKED) \
                                        ///<Task blocked for some reason
};
/**@}**/
/** @defgroup Cinter The Piccolo OS Plus APIs
 * 
 * Here are the Piccolo_Plus APIs, grouped by family function
 * 
 * @{
 */
/** @name The initializers
 * 
 * These are literally the beginning and the end.
 * 
 * `piccolo_init()` is called before any other Piccolo function. 
 * `piccolo_start()` begins the piccolo scheduler execution, and never returns. 
 */

///@{

void piccolo_init();
void piccolo_start();
///@}
/** @name Task creation and execution control
 * 
 * These are the functions for creating and ending tasks, and controlling their execution.
 * Once a task is running, it can yield the processor to the next task voluntarily. It can also 
 * suspend execution (sleep) for a time and allow other tasks to execute.
 * 
 * @note To send a signal to a task, the sender must have the pointer to the task returned by `piccolo_create_task()`.
 * 
  */

///@{
piccolo_os_task_t* piccolo_create_task(void (*pointer_to_task_function)(void));
void piccolo_end_task();



/**
 * @brief Yields the processor to another task.
 * 
 * The scheduler will switch to the next ready task in a "round robin" manner.
 * 
 */
__force_inline static void piccolo_yield(void) {
    __asm volatile ("nop" );
    __asm volatile ("svc 0");
    __asm volatile ("nop" );
    return;
}

/**
 * @brief Same as \ref piccolo_yield
 * 
 */
void piccolo_syscall(void);
void piccolo_sleep(uint32_t sleep_time_ms);
void piccolo_sleep_until(absolute_time_t until);
///@}

/** @name Task Signals
 * 
 * Each task has a built in signal channel which can be used for inter-task synchronization. A channel can hold 
 * a specified number of signals. 
 * (The maximum is a \#define value which can be as large as 2^31). Signals are only events which contain no data
 * and do not occupy any memory. Signals can be sent to a task by another task, or by interrupt service handlers
 * or timer or alarm callback routines. (Though only tasks ought to try blocking!)
 */

///@{

piccolo_os_task_t* piccolo_get_task_id();
int32_t piccolo_send_signal(piccolo_os_task_t* toTask);
int32_t piccolo_send_signal_blocking(piccolo_os_task_t* toTask);
int32_t piccolo_send_signal_blocking_timeout(piccolo_os_task_t* toTask,uint32_t timeout_ms);
int32_t piccolo_get_signal();
int32_t piccolo_get_signal_blocking();
int32_t piccolo_get_signal_blocking_timeout(uint32_t timeout_ms);
int32_t piccolo_get_signal_all();
int32_t piccolo_get_signal_all_blocking();
int32_t piccolo_get_signal_all_blocking_timeout(uint32_t timeout_ms);

///@}
/**@}**/


#ifdef __cplusplus
}
#endif


#endif