/**
 * @file piccolo_os_lock_core.c
 * @author Keith Standiford
 * @brief Piccolo OS overrides for Pi Pico SDK lock_core defaults
 * @version 1.0
 * @date 2022-08-29
 * 
 * @copyright Copyright (c) 2022 Keith Standiford
 * 
 * 
 */

#include "kernel.h"

extern piccolo_os_internals_t piccolo_ctx;

/** @defgroup SDK The Piccolo OS interface to the SDK
 * 
 * @{
 */

/*! \brief Get interrupt enable status. 
 *
 * \return Non zero (true) if interrupts are disabled
 */

__force_inline static uint32_t get_interrupts_disabled(void) {
    uint32_t status;
    __asm volatile ("mrs %0, PRIMASK" : "=r" (status)::);
    return status;
}


/**
 * @brief Get the ID (task struct address) of the running task
 * 
 * @return lock_owner_id_t Task ID to use as lock owner
 * 
 * If piccolo_os is *not* started (`piccolo_start()` hasn't yet been called), return the
 * core number. Otherwise, return the task ID of the running task. `Piccolo_init()`
 * sets these to the core number as well, since even if the scheduler is running
 * it *may not* have yet selected a task to run.
 */
lock_owner_id_t __time_critical_func(piccolo_lock_get_owner_id)(){
    lock_owner_id_t task;
    int interrupts;

    interrupts = save_and_disable_interrupts();
    asm volatile("": : :"memory");
    task = (lock_owner_id_t) piccolo_ctx.this_task[get_core_num()];
    asm volatile("": : :"memory");
    restore_interrupts(interrupts);
    return task;
}

/**
 * @brief If a valid task is running, yield. Then return
 * 
 * Remember the SDK caller checks anyway, so if we don't have a valid task running
 * we will just return (and we will spin in a loop with the SDK).
 * 
 */
void __time_critical_func(piccolo_lock_wait)(void) {
    if(piccolo_lock_get_owner_id()>1 && !get_interrupts_disabled()) piccolo_yield();
    return;
}

/**
 * @brief If a valid task is running, yield. Then return the timeout status.
 * 
 * @param timeout_timestamp 
 * @return true if the timeout has expired
 * @return false if the timeout has not expired
 * 
 * The SDK caller will keep trying to acquire the lock until it succeeds or the timeout expires. 
 * We will yield if a valid task is running until one or the other occurs.
 */
bool __time_critical_func(piccolo_lock_wait_until)(absolute_time_t timeout_timestamp){
    if(piccolo_lock_get_owner_id()>1 && !get_interrupts_disabled()) piccolo_yield();
    return time_reached(timeout_timestamp);
}

/**
 * @brief Default idle routine for the lock system. If a valid task is running, yield. Then return.
 * 
 */
void __time_critical_func(piccolo_lock_yield)(void) {
    if(piccolo_lock_get_owner_id()>1 && !get_interrupts_disabled()) piccolo_yield();
    return;
}

/**@}**/
