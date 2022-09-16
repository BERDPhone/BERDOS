/**
 * @file piccolo_os.c
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
 * Piccolo OS is a simple cooperative multi-tasking OS originally written by Gary Simms 
 * as a teaching tool. Check it out here [piccolo_os_1.1](https://github.com/garyexplains/piccolo_os_v1.1)
 *     
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/multicore.h"
#include "hardware/structs/systick.h"
#include "hardware/exception.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/structs/mpu.h"
#include "pico/malloc.h"

#include "kernel.h"


uint32_t *__piccolo_os_create_task(uint32_t *stack,
                                       void (*start)(void), uint32_t);
void __piccolo_task_init(void);
uint32_t *__piccolo_pre_switch(uint32_t *stack); 
void __piccolo_task_init_stack(uint32_t *stack);
uint32_t *__piccolo_os_create_task(uint32_t *task_stack,
            void (*pointer_to_task_function)(void), uint32_t starting_argument);
int32_t __piccolo_send_signal(piccolo_os_task_t* task,bool block, uint32_t timeout_ms);
int32_t __piccolo_get_signal(bool block, uint32_t timeout_ms, bool get_all);
void __piccolo_garbage_man(void);
void __piccolo_idle( int32_t uSec);
void __piccolo_start_core1(void);


piccolo_os_internals_t piccolo_ctx;



/**
 * @brief Get the ID (task struct address) of the running task
 * 
 * @return piccolo_os_task_t* Task ID of the current task.
 * 
 * Return the task ID of the running task. Useful for telling callback routines
 * (or other tasks) where to send signals.
 * 
 * @note Will return the core number before `piccolo_start` is running on that core. But if you 
 * @note don't call this except inside a task, you will always get a valid task ID.
 */
piccolo_os_task_t* piccolo_get_task_id(){
    piccolo_os_task_t* task;
    int interrupts;

    // since we are called from the task environment, we must make sure we are
    // not preempted and switched to a different core during the task lookup.
    interrupts = save_and_disable_interrupts();
    asm volatile("": : :"memory");
    task = ( piccolo_os_task_t*) piccolo_ctx.this_task[get_core_num()]; // discard volitile status!
    asm volatile("": : :"memory");
    restore_interrupts(interrupts);
    return task;
}


/**
 * @brief Initialize user task stack for execution 
 * 
 * @param task_stack pointer to the END of the task stack (array).
 * @param pointer_to_task_function the function to execute.
 * @param starting_argument unsigned integer argument for function
 * 
 * @returns "Current" stack pointer for the task.
 * \ingroup Intern
 * We setup the initial stack frame with:
 *  - "software saved" LR set to PICCOLO_OS_THREAD_PSP so that exception return
 * works correctly. 
 *  - PC Set to the task starting point (function entry)
 *  - Exception frame LR set to `piccolo_end_task()` in case the task exits or returns
 * 
 * PICCOLO_OS_THREAD_PSP means: 
 *  - Return to Thread mode.
 *  - Exception return gets state from the process stack.
 *  - Execution uses PSP after return.
 * See also:
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0552a/Babefdjc.html
 * 
 * \note The starting arguement is placed in R0, but only used internally for the totally fake idle task.
 */
uint32_t *__piccolo_os_create_task(uint32_t *task_stack,
            void (*pointer_to_task_function)(void), uint32_t starting_argument) {

  /* This task_stack frame needs to mimic would be saved by hardware and by the
   * software (in isr_svcall) */

  /*
        Exception frame saved by the hardware onto task_stack:
        +------+
        | xPSR | task_stack[16] 0x01000000 i.e. PSR Thumb bit
        |  PC  | task_stack[15] pointer_to_task_function
        |  LR  | task_stack[14]
        |  R12 | task_stack[13]
        |  R3  | task_stack[12]
        |  R2  | task_stack[11]
        |  R1  | task_stack[10]
        |  R0  | task_stack[9]
        +------+
        Registers saved by the software (isr_svcall):
        +------+
        |  LR  | task_stack[8]  (THREAD_PSP i.e. 0xFFFFFFFD)
        |  R7  | task_stack[7]
        |  R6  | task_stack[6]
        |  R5  | task_stack[5]
        |  R4  | task_stack[4]
        |  R11 | task_stack[3]
        |  R10 | task_stack[2]
        |  R9  | task_stack[1]
        |  R8  | task_stack[0]
        +------+
        */
//       printf(" make stack %d\n", task_stack);
  task_stack = (uint32_t *) (((uint32_t) task_stack)&~((uint32_t) 7));    // 8 byte align initial stack frame...
  task_stack +=  - 17; /* End of task_stack, minus what we are about to push */
  task_stack[8] = (unsigned int) PICCOLO_OS_THREAD_PSP;
  task_stack[15] = (unsigned int) pointer_to_task_function;
  task_stack[14] = (unsigned int) (uint32_t) piccolo_end_task; // in case the task returns!
  task_stack[16] = (unsigned int) 0x01000000; /* PSR Thumb bit */
  task_stack[9] = starting_argument;    // in R0 (for idle, really...)

  return task_stack;
}

/**
 * @brief Create a new task and initialize it's stack.
 * 
 * @param pointer_to_task_function The task function to call initially
 * @return Task identifier (Pointer ti task structure) or 0 if create failed
 * 
 * Allocates a new task and initializes its stack to the start of the given function.
 * Inserts the task at the end of the scheduler task list.
 * Can be called to create a new task while the scheduler is running. 
 * (In other words, a running task can create another task at runtime.)
 * 
 */
piccolo_os_task_t* piccolo_create_task(void (*pointer_to_task_function)(void)) {
    piccolo_os_task_t* task, *last_task;

    // allocate the space for the task
    task = (piccolo_os_task_t*) malloc(sizeof (piccolo_os_task_t));
    if(task == NULL) return task;   // fails
    
    task->task_flags = 0;  // Mark Task as runnable, and not running
    task->wakeup = get_absolute_time();
    task->signal_in = task->signal_out = 0;
    task->signal_limit = PICCOLO_OS_MAX_SIGNAL;
//    printf("Make task %d ",task->stack);
    task->stack_ptr =
        __piccolo_os_create_task(task->stack + PICCOLO_OS_STACK_SIZE, pointer_to_task_function,0);
    
    // Lock the task scheduler structure to insert in task list
    uint32_t lock_value = spin_lock_blocking(piccolo_ctx.piccolo_lock);

    // Bail out the scheduler in the odd case that all the tasks were previously deleted
    if(piccolo_ctx.current_task == NULL) piccolo_ctx.current_task = task;

    task->next_task = NULL;
    if(piccolo_ctx.task_list_head) {
        // there is already an existing task. Just tack the new one on the end
        piccolo_ctx.task_list_tail->next_task = task;   // fwd ptr of prev end of list
        task->prev_task = piccolo_ctx.task_list_tail;   // new task prev points to old list end
        piccolo_ctx.task_list_tail = task;
    }
    else {
        // There was no task in existence. Initialize both ends of the list
        piccolo_ctx.task_list_head = task;  // the first task created.
        piccolo_ctx.task_list_tail = task;
        task->prev_task = NULL;
    }    
    // and unlock and reenable interrupts
    spin_unlock(piccolo_ctx.piccolo_lock,lock_value);

  return task;
}

/**
 * @brief Ends the current task, never returns
 * 
 * Marks the current task as dead (ZOMBIE) and yields, so the scheduler can remove it.
 * (The scheduler must do this, since we cannot free the memory for a task
 * while it is running!). The scheduler will immediately remove a ZOMBIE task
 * from the scheduler chain, add it to the zombies list and signal the garbage
 * collector to return the task space to free memory.
 * 
 * @note A task that executes a `return` will also be ended.
 */

void piccolo_end_task(void){
    piccolo_os_task_t * task;
    uint32_t lock = spin_lock_blocking(piccolo_ctx.piccolo_lock);
    task = piccolo_get_task_id();
    task->task_flags |= PICCOLO_TASK_ZOMBIE;    // marked for death...
    spin_unlock(piccolo_ctx.piccolo_lock,lock);
    while(1) piccolo_yield();
    return;                 // just to turn of doxygen warning!
}

/**
 * @brief sleeps for a specified number of milliseconds
 * 
 * @param sleep_time_ms number of milliseconds to sleep;
 * 
 * The scheduler marks the task as blocked and suspends its execution 
 * until the delay has expired.
 */
void piccolo_sleep(uint32_t sleep_time_ms) {
    piccolo_os_task_t *task;
    
    piccolo_sleep_until( make_timeout_time_ms(sleep_time_ms));
    return;
}

/**
 * @brief sleeps until an absolute time.
 * 
 * @param until absolute time to wake up
 * 
 * Set the wakeup time for the task and mark it as sleeping for the scheduler. 
 * Then suspend execution until the scheduler wakes up the task.
 * 
 */

/* Figure out which core we are and get the task we are running. Then set the wakeup 
 * time and the sleeping and blocked flags.
 * Finally, yield.
 * 
 * We ARE the running task. If we get preempted here, we will still be running
 * when restored. The scheduler will not alter our flag values while we run.
 * So we are thread and core safe.
 */
void piccolo_sleep_until(absolute_time_t until) {
    piccolo_os_task_t *task;

    if(time_reached(until)) return;

    task = piccolo_get_task_id();
    task->wakeup = until;
    task->task_flags |= PICCOLO_TASK_SLEEPING;
    piccolo_yield();  
}
/**
 * @brief Helper for send signal functions
 * 
 * @param task pointer to task to send to
 * @param block true if blocking on send
 * @param timeout_ms non zero if timeout enabled on blocking
 * @return 1 if signal sent. <0 if no room, or timeout occurred on blocking
 * \ingroup Intern
 * Send a signal to the designated task. If there is space the signal is sent.
 * If there is no room for the signal, return the error unless blocking was requested.
 * If blocking is necessary, start a timeout as well, if one was requested.
 * 
 * @note Since multiple senders are allowed, we must grab the spinlock. 
 */
int32_t __piccolo_send_signal(piccolo_os_task_t* task,bool block, uint32_t timeout_ms){
    uint32_t lock, inptr, flags; 
    bool we_blocked = false;
    bool not_done = true;
    int32_t result = 1;
    piccolo_os_task_t* owntask;

    owntask = piccolo_get_task_id();
    do {
        lock = spin_lock_blocking(piccolo_ctx.piccolo_lock);
        inptr = (uint32_t) task->signal_in + 1;         // increment in pointer
        if ( inptr == task->signal_limit) inptr = 0;    // modulo limit
        if( inptr == task->signal_out) {                // in+1 == out means FULL. Oh dear...
            result = -1;
            if(block && !we_blocked) {          // we will try blocking unless we already tried
                we_blocked = true;

                // set time out and blocking flags
                owntask->wakeup = delayed_by_ms(get_absolute_time(),timeout_ms);
                owntask->task_flags |= (
                    ((timeout_ms)? PICCOLO_TASK_SLEEPING:0) | PICCOLO_TASK_SEND_SIGNAL_BLOCKED);
                flags = owntask->task_flags;
                owntask->task_sending_to = task;

                // clear the lock and yield with flags set
                spin_unlock(piccolo_ctx.piccolo_lock,lock);
                piccolo_yield();

                // Repeat the loop one more time
                continue;
            }
        } else {

            // We have room for a signal!
            task->signal_in = inptr;        // signal is sent
            result = 1;
        }
        not_done = false;
        spin_unlock(piccolo_ctx.piccolo_lock,lock);
    } while (not_done);


    return result;
}
/**
 * @brief Send a signal to the specified task
 * 
 * @param toTask pointer to task to send to
 * @return 1 if signal sent. <0 if no room
 * 
 * Send a signal to the designated task. If there is space the signal is sent.
 * If there is no room for the signal, return the error. 
 * 
 */
inline int32_t piccolo_send_signal(piccolo_os_task_t* toTask) {
    return __piccolo_send_signal(toTask,false, 0);
};
/**
 * @brief Send a signal to the specified task. If the signal channel is full, block until it is not.
 * 
 * @param toTask pointer to task to send to
 * @return 1 to indicate success.
 * 
 * Send a signal to the designated task. If there is space the signal is sent.
 * If there is no room for the signal, block until space is available.
 */
inline int32_t piccolo_send_signal_blocking(piccolo_os_task_t* toTask) {
    return __piccolo_send_signal(toTask,true, 0);
};
/**
 * @brief Send a signal to a specified task. If the channel is full, block with a timeout until it is not.
 * 
 * @param toTask pointer to task to send to
 * @param timeout_ms maximum time in ms to wait if the channel is full.
 * @return 1 if signal sent. <0 if a timeout occurred while waiting for space in the channel.
 * 
 * Send a signal to the designated task. If there is space the signal is sent.
 * If there is no room for the signal, wait until the timeout expires or space is available.
 * 
 */
inline int32_t piccolo_send_signal_blocking_timeout(piccolo_os_task_t* toTask,uint32_t timeout_ms) {
    return __piccolo_send_signal(toTask,true, timeout_ms);
};

/**
 * @brief Helper for get signal functions
 * 
 * @param block true if blocking on send
 * @param timeout_ms non zero if timeout enabled on blocking
 * @param get_all if true, get ALL signal available. Otherwise just get one. 
 * @return Number of signals received. Can be zero on timeout or non-blocking
 * \ingroup Intern
 * Get a signal for the current task. Return the number received.
 * If there are no signals available, return zero unless blocking was requested.
 * If blocking is necessary, start a timeout as well if one was requested.
 * 
 * @note With only ONE receiver, we do not have to lock anything. 
 */
int32_t __piccolo_get_signal(bool block, uint32_t timeout_ms, bool get_all){
    uint32_t outptr, inptr; 
    bool we_blocked = false;
    bool not_done = true;
    int32_t result;
    piccolo_os_task_t * task;

    task = piccolo_get_task_id();
    do {
        outptr = (uint32_t) task->signal_out;         
        if( outptr == task->signal_in) {                // in == out means empty. Oh dear...
            result = 0;
            if(block && !we_blocked) {          // we will try blocking unless we already tried
                we_blocked = true;

                // set time out and blocking flags
                task->wakeup = delayed_by_ms(get_absolute_time(),timeout_ms);
                task->task_flags |= ( 
                    PICCOLO_TASK_GET_SIGNAL_BLOCKED | ((timeout_ms)? PICCOLO_TASK_SLEEPING:0));
                // yield with flags set. This will block
                piccolo_yield(); 

                // Repeat the loop one more time
                continue;
            }
        } else {

            // We have a signal!
            if(get_all) {
                inptr = (uint32_t) task->signal_in;     // snapshot in pointer to avoid changes
                result = inptr - outptr;
                if(result < 0) result += task->signal_limit;        // fix wraparound
                task->signal_out = inptr;                 // empty signal count
            } else {
                result = 1;
                outptr += 1;
                if(outptr == task->signal_limit) outptr = 0;  // increment out pointer mod limit
                task->signal_out = outptr;                      // and update task values
            }
        }
        not_done = false;
    } while (not_done);

    return result;
}

/**
 * @brief Attempt to get a signal
 * 
 * @return 1 if signal received, 0 if none were available
 * 
 */
// We optimize this one since it doesn't block or timeout
inline int32_t piccolo_get_signal() {
    piccolo_os_task_t* task;
    uint32_t i;

    task = piccolo_get_task_id();
    // in == out is empty
    if(task->signal_in == task->signal_out) return 0;
    // increment out, but never set it >= limit ...
    if((i=task->signal_out++) >= task->signal_limit) i = 0;
    task->signal_out = i;
    // return success
    return 1;
}

/**
 * @brief Get a signal. If none are available, block until one arrives.
 * 
 * @return 1 for number of signals received.
 * 
 */
inline int32_t piccolo_get_signal_blocking() {
    return __piccolo_get_signal(true, 0, false);
}
/**
 * @brief Attempt to get a signal. If none were available, block with a timeout until one arrives.
 * 
 * @param timeout_ms non zero if timeout enabled on blocking
 * @return Number of signals received. Will be zero is timeout occurred.
 * 
 */
inline int32_t piccolo_get_signal_blocking_timeout(uint32_t timeout_ms){
    return __piccolo_get_signal(true, timeout_ms, false);
}

/**
 * @brief Get all the signals available
 * 
 * @return Number of signals received, 0 if none were available
 * 
 * Empties the signal channel if signals were available.
 * 
 */
inline int32_t piccolo_get_signal_all(){
    return __piccolo_get_signal(false, 0, true);
}

/**
 * @brief Get all the signals available. If none were available, block until one arrives.
 * 
 * @return Number of signals received
 * 
 * Empties the signal channel.
 * 
 */
inline int32_t piccolo_get_signal_all_blocking(){
    return __piccolo_get_signal(true, 0, true);
}
/**
 * @brief Get all the signals available. If none were available, block with a timeout until one arrives.
 * 
 * @param timeout_ms Time in ms to wait for a signal to arrive
 * 
 * @return Number of signals received, 0 if timeout occurred.
 * 
 * Empties the signal channel.
 * 
 */

inline int32_t piccolo_get_signal_all_blocking_timeout(uint32_t timeout_ms){
    return __piccolo_get_signal(true, timeout_ms, true);
}

uint32_t kills = 0;
/**
 * @brief Task to delete dead tasks.
 * 
 * \ingroup Intern
 * The task is created during the scheduler start-up on core 0.
 * When it runs, it tries to free the space for all the dead tasks on 
 * the zombies list. (The scheduler cannot call free, because it
 * is an interrupt handler and would break the mutex that protects
 * free and malloc). 
 * 
 */
void __piccolo_garbage_man(void) {
    uint32_t lock;
    piccolo_os_task_t *temp;
    while (1) { 
        piccolo_get_signal_all_blocking();  // wait for dead bodies to appear ...
//        piccolo_yield();
        while (piccolo_ctx.zombies) {
            lock = spin_lock_blocking(piccolo_ctx.piccolo_lock);
            // we could have been fooled by the other core or preemption
            // so check things again while we have the lock
            if(temp = (piccolo_os_task_t *) piccolo_ctx.zombies) piccolo_ctx.zombies = temp->next_task;
            spin_unlock(piccolo_ctx.piccolo_lock,lock);
            if(temp) free(temp);
            kills++;
        }
    }
}

/**
 * @brief Switch the scheduler to handler mode
 * 
 * \ingroup Intern
 * After a reset, the processor is in thread mode. 
 * Switch to handler mode, so that the Piccolo OS kernel runs in handler mode,
 * and to ensure the correct return from an exception/interrupt later
 * when switching to a task. We make a dummy stack and arrange to route the
 * service call exception back to ourselves!
 */
void __piccolo_task_init(void) {
  uint32_t dummy[48];
  __piccolo_task_init_stack((uint32_t *)((uint32_t)(dummy + 48) & ~((uint32_t) 7)));  // Create phony 8 byte aligned stack & SVC back to ourselves
}

extern void __isr_SVCALL(void);
/**
 * @brief Initialize the piccolo run time environment
 * 
 * Set the scheduler task list to empty and set up the context switching, 
 * interrupt handlers, interrupt priorities and interlocks (spinlocks) for the scheduler.
 * 
 * @note Also called internally on Core 1 when multi-core is enabled. On core1 only the interrupt
 * priorities are set, and all the "one time" stuff is skipped.
 * 
 */
void piccolo_init() {

    if(!get_core_num()) { // if we ARE core 0

        // Initialize the scheduler task lists and last running task
        piccolo_ctx.this_task[0] = 0;
        piccolo_ctx.this_task[1] = (piccolo_os_task_t *) 1;
        piccolo_ctx.task_list_head = NULL;
        piccolo_ctx.task_list_tail = NULL;
        piccolo_ctx.current_task = NULL;
        piccolo_ctx.zombies = NULL;

        // claim the spinlock, initialize it and save it's instance
        spin_lock_claim(PICCOLO_SPIN_LOCK_ID);
        piccolo_ctx.piccolo_lock = spin_lock_init(PICCOLO_SPIN_LOCK_ID);

        // Install the exception handlers for Systick and SVC
        exception_set_exclusive_handler(SYSTICK_EXCEPTION,&__isr_SVCALL);
        exception_set_exclusive_handler(SVCALL_EXCEPTION,&__isr_SVCALL);
    }

    // things from here on happen on ALL cores...

    /*
    * set interrupt priority for SVC, PENDSV and Systick to 'all bits on'
    * for LOWEST interrupt priority. We do not want ANY of them to preempt
    * any other irq or each other. Remember that the vector table is the same 
    * for both cores, but interrupt enable and priorities ARE NOT!
    * Also, Systick is different for each core.
    */

    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET),M0PLUS_SHPR2_BITS);
    hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET),M0PLUS_SHPR3_BITS);
}

/**
 * @brief Internal Idle "task" used by the scheduler to sleep the core
 * 
 * @param uSec the number of microseconds to sleep
 * \ingroup Intern
 * Enter sleep mode and then "yield" back to the scheduler. Entry and parameter passing
 * is set up in a dummy stack frame before switching context.
 * 
 * \note Can be running on **both** cores with different sleep times
 * 
 */
__attribute__ ((noinline)) void __piccolo_idle( int32_t uSec)  {
    do {
        if(uSec>= 0) sleep_us(uSec);
        piccolo_yield();            // This should never return!
    } while (1);
}

/**
 * @brief Core 1 code to initialize and immediately start the piccolo scheduler
 * \ingroup Intern
 * 
 * Remember that the `piccolo_init()` checks the core number and does not
 * setup the data structures or the interrupt vector table if it is not core 0.
 * `piccolo_start()` takes similar precautions.
 * The scheduler should run fine on both cores at once.
 * 
 */

void __piccolo_start_core1(void) {
//    printf("Hello from Core %d\n",get_core_num());
    piccolo_init();
    piccolo_start();
    return;   // never happens
}


/**
 * @brief Start the Piccolo Task scheduler
 * 
 * @returns Never!
 * 
 * Start the second processor if multi-core mode is enabled.
 * Switch to handler mode and begin the round robin scheduler. 
 * 
 * The scheduler starts with the task after the last task run by either core
 * and looks for a task which is not running (on the other core) and not blocked. Along the way it checks
 * if sleeping tasks or tasks blocked for signaling should be unblocked. The first task found ready to run, gets run
 * with the preemption timer reset and armed if preemption is enabled. After the task runs the
 * scheduler checks if it has ended. (Marked as a zombie.) If so, the task is
 * removed from the scheduler's task list and sent to the garbage collector to free the task's memory.
 * 
 * If no task is ready to run an idle task will be started to sleep for the minimum of \ref PICCOLO_OS_MAX_IDLE
 * or the smallest time remaining of any timeout. Sleep is the Pico sleep
 * routine, so the core goes to sleep for power reduction. If \ref PICCOLO_OS_MAX_IDLE is set to zero, 
 * idle will not run. 
 * 
 * If \ref PICCOLO_OS_NO_IDLE_FOR_SIGNALS is true (the default), the idle task *will not* be run if *any* task is blocked
 * waiting to send or receive a signal in order to minimize system response time.
 * Setting \ref PICCOLO_OS_NO_IDLE_FOR_SIGNALS to false will improve power consumption but potentially delay the response
 * of any task waiting for a signal. This may make good sense for low power applications without
 * serious response time concerns.
 * 
 * @note Runs on **both** cores if multi-core is enabled
 * 
 */
void __time_critical_func(piccolo_start)() {
    
    uint32_t lock_value;
    piccolo_os_task_t  *last_task, *current_task = NULL, *temp;
    enum piccolo_task_flag_values current_flags;
    uint32_t minimum_wait;
    int64_t time_to_wait;
    bool idle;
    #define Idle_Stack_Size 256
    uint32_t Idle_Stack[Idle_Stack_Size];   // a dummy stack for the idle task

    /*
     * If we are core 0, do some one time initialization,
     * then launch core 1 if we are in multi-core mode
     */
    if(!get_core_num()) {
        piccolo_ctx.garbage_man = piccolo_create_task(__piccolo_garbage_man); // create the garbage collector
        // just in case malloc didn't panic, we should if there was no space
        if(piccolo_ctx.garbage_man==NULL) panic("Piccolo cannot create garbage collector task!\n");
        piccolo_ctx.garbage_man->signal_limit = INT32_MAX;
        // Initalize the last task run to the tail of the chain after task creation
        piccolo_ctx.current_task = piccolo_ctx.task_list_tail; 
        
#if PICCOLO_OS_MULTICORE
        multicore_launch_core1( __piccolo_start_core1); // if we ARE core 0
#endif
    }
    /*
     * Now make the context switch to handler mode.
     */
    __piccolo_task_init();  //Switch to handler mode

 
    // And now begin the round robin scheduler...
  while (1) {

    /*
     * We get to the top of the scheduler loop three ways...
     * 
     * 1) Initial entry. (Which can't have conflicts...)
     * 2) SVC call returned from previous task directly. (Task yielded, we are in the main loop)
     * 3) Systick fired which "acts" like SVC. (Task is preempted.)
     * 
     * SVC and Systick preemption are asynchronous and *could both* occur. We want to make sure that only
     * one happens, otherwise we could schedule twice and skip a task (at best). If they try to occur on top
     * of each other, SVC will alway eventually win because it had a lower exception number.
     * So it is sufficient to clear any pending systick pending flag. 
     * 
     */
    systick_hw->csr = 0;    //Disable timer and IRQ   
    __dsb();                // make sure systick is disabled
    __isb();                // and it is really off

    // clear the systick pending bit if it got set
    hw_set_bits  ((io_rw_32 *)(PPB_BASE + M0PLUS_ICSR_OFFSET),M0PLUS_ICSR_PENDSTCLR_BITS);

    /*
     * Before manipulating the scheduler variables to find a task to run, lock out the other core!
     * 
     * lock_value will contain interrupt status.
     */
    idle = true;    // assume there is nothing to do...
    minimum_wait = PICCOLO_OS_MAX_IDLE;
    do {
        lock_value = spin_lock_blocking(piccolo_ctx.piccolo_lock);


        // Note that piccolo_ctx.current_task is volatile, but since we have the lock, current_task is NOT
        current_task = (piccolo_os_task_t *) piccolo_ctx.current_task;  // last task to run (NULL is bad!)
        last_task = current_task;                                       // save starting point of our search
        // Note that there may NOT be any tasks. None were created or all have ended is possible

        // get the next task to run
        // And we should check for timeouts, signal waits etc.
        do {
            if (current_task != NULL) current_task =  current_task->next_task;     // next task in list
            if (current_task == NULL) current_task =  piccolo_ctx.task_list_head; // reset to list head
            if (current_task == NULL) break;    // STILL no tasks found. Idle until?

            // Is the task running? (Presumably on a different core!)
            current_flags = current_task->task_flags;
            if (current_flags & PICCOLO_TASK_RUNNING) continue; // the other core is running it, skip it

            if(current_flags & PICCOLO_TASK_BLOCKING) {  // Is any blocking flag set?
                //  Is there a task timer running?
                if(current_flags & PICCOLO_TASK_SLEEPING) {
                    time_to_wait = absolute_time_diff_us(get_absolute_time(),current_task->wakeup);
                    if(time_to_wait <=0) { // Has timer hit now?
                        // Time to wake up. clear sleeping and blocked
                        current_flags = 0;
                        current_task->task_flags = current_flags;
                    }
                    else {
                        // keep track of the shortest time left for any task
                        if(time_to_wait < minimum_wait) minimum_wait = time_to_wait;
                    }
                }

                //  Is it blocked waiting for a signal?
                if(current_flags & PICCOLO_TASK_GET_SIGNAL_BLOCKED) {
#if PICCOLO_OS_NO_IDLE_FOR_SIGNALS
                    minimum_wait = 0;
#endif
                    // is there data?
                    if(current_task->signal_in != current_task->signal_out) { //in=out => empty
                        // YES, clear block flags and run it
                        current_flags = 0;
                        current_task->task_flags = current_flags;
                    }
                } else {

                    //  Or is it blocked waiting to send a signal? (Can't be both)
                    if(current_flags & PICCOLO_TASK_SEND_SIGNAL_BLOCKED) {
#if PICCOLO_OS_NO_IDLE_FOR_SIGNALS
                        minimum_wait = 0;
#endif
                        // is there room for data?
                        if(((current_task->task_sending_to->signal_in+1 == 
                            current_task->task_sending_to->signal_limit)?0:current_task->task_sending_to->signal_in+1 )
                            != current_task->task_sending_to->signal_out) { //((in+1)%limit == out)=>full
                            // Yes, clear blocks and run it
                            current_flags = 0;
                            current_task->task_flags = current_flags;
                        }
                    }
                }
            }

            //  It the task ready to run?
            if (!(current_flags)) {
                /*
                 * We found a ready task not already running. Mark it running
                 * Set idle to false, and break the search loop
                 */
                current_task->task_flags = PICCOLO_TASK_RUNNING;
                piccolo_ctx.current_task = current_task;    // start other schedulers looking in right spot
                piccolo_ctx.this_task[get_core_num()] = current_task;   // so we can find who we are at run time
                idle = false;
                break;
            }

        } while (current_task != last_task);
        // and unlock and reenable interrupts
        spin_unlock(piccolo_ctx.piccolo_lock,lock_value);

        /*
         * If we could not find a task, idle is set. If idle sleeping is enabled (PICCOLO_OS_MAX_IDLE not zero),
         * go to low power mode by starting a task in Thread (user) mode which will
         * actually sleep for PICCOLO_OS_MAX_IDLE, or the minimum timeout time pending.
         * Remember that a timer or an IRQ can unblock a task by signaling. This implies that the 
         * system may not respond for one PICCOLO_OS_MAX_IDLE slice time.
         */
        if(!idle) break;
        else if( minimum_wait) {
            __piccolo_pre_switch(__piccolo_os_create_task(
                    (Idle_Stack + Idle_Stack_Size),(void (*)(void)) __piccolo_idle,(uint32_t) minimum_wait));
        }
    } while (idle);

    /*
     * There is a task to run. Reset the systick timer, for preemption and then run the task
     * 
     */
    systick_hw->rvr = PICCOLO_OS_TIME_SLICE; // set for interval
    // NOTE: setting Time Slice to 0 will disable Systick and turn off preemptive scheduling!
    systick_hw->cvr = 0;    // reset the current counter
    __dsb();                // make sure systick is set
    __isb();                // and it is really ready
    systick_hw->csr = 3;    // Enable systick timer and IRQ, select 1 usec clock    

    // At long last, run the task...

    current_task->stack_ptr =
        __piccolo_pre_switch(current_task->stack_ptr);

    /*
     * The task is preempted or yielded. Since we ran it, we own it, so here
     * is where we mark it not running, and any other such stuff...
     * 
     */


    /*
     * Did the currently running task end? If so, remove it from the scheduler chain,
     * add it to the zombie list and wake up the garbage collector. If not, turn off the running flag.
     * 
     */
    if(current_task->task_flags & PICCOLO_TASK_ZOMBIE) {
        lock_value = spin_lock_blocking(piccolo_ctx.piccolo_lock);
        if(current_task->prev_task) 
            current_task->prev_task->next_task = current_task->next_task;
        else
            piccolo_ctx.task_list_head = current_task->next_task;
        if(current_task->next_task)
            current_task->next_task->prev_task = current_task->prev_task;
        else
            piccolo_ctx.task_list_tail = current_task->prev_task;
        if(current_task == piccolo_ctx.current_task) {
            if(!(piccolo_ctx.current_task = current_task->next_task)) piccolo_ctx.current_task = piccolo_ctx.task_list_head;
            }

        current_task->next_task = (piccolo_os_task_t *) piccolo_ctx.zombies;
        piccolo_ctx.zombies = current_task;
        spin_unlock(piccolo_ctx.piccolo_lock,lock_value);
        piccolo_send_signal(piccolo_ctx.garbage_man);

    }
    else current_task->task_flags &= ~PICCOLO_TASK_RUNNING;

  }
  return; // shuts doxygen up about no return...
}
