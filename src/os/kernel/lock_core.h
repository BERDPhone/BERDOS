/**
 * @file piccolo_os_lock_core.h
 * @author Keith Standiford
 * @brief Piccolo OS overrides for Pi Pico SDK lock_core defaults
 * @version 1.0
 * @date 2022-08-29
 * 
 * @copyright Copyright (c) 2022 Keith Standiford
 * 
 * This file contains specific definitions and parameters to override default values in the Pi Pico SDK in order
 * to provide compatibility with the Piccolo_Os cooperative multi-tasking operating system originally
 * written by [Gary Sims](https://github.com/garyexplains). 
 * It should be included in CMakelists.txt files by adding the following line before the pico_sdk_init()
 * but after the project(...) line:
 * 
 * list(APPEND PICO_CONFIG_HEADER_FILES ${PROJECT_SOURCE_DIR}/piccolo_os_lock_core.h)
 */

#ifndef piccolo_os_lock_core
#define piccolo_os_lock_core

#include "pico.h"
/* force mutex for malloc even if multicore is not loaded */
#define PICO_USE_MALLOC_MUTEX 1
/* Protect divider from pre-emption */
#define PICO_DIVIDER_DISABLE_INTERRUPTS true


#ifndef lock_owner_id_t
/*! \brief  type to use to store the 'owner' of a lock.
 *  \ingroup lock_core
 * By default this is int8_t as it only needs to store the core number or -1, however it may be
 * overridden if a larger type is required (e.g. for an RTOS task id)
 * 
 * **For Piccolo OS, it will hold a task identifier (core number or task structure address)**
 */
#define lock_owner_id_t int32_t
#endif
#ifndef LOCK_INVALID_OWNER_ID
/*! \brief  marker value to use for a lock_owner_id_t which does not refer to any valid owner
 *  \ingroup lock_core
 */
#define LOCK_INVALID_OWNER_ID ((lock_owner_id_t)-1)
#endif
#ifndef lock_is_owner_id_valid
#define lock_is_owner_id_valid(id) ((id) != LOCK_INVALID_OWNER_ID)
#endif

#ifndef __ASSEMBLER__
#ifdef __cplusplus
extern "C" {
#endif

lock_owner_id_t piccolo_lock_get_owner_id();
void piccolo_lock_wait();
bool piccolo_lock_wait_until(absolute_time_t timeout_timestamp);
void piccolo_lock_yield();

#ifdef __cplusplus
}
#endif

#endif

/** @defgroup SDK The Piccolo OS interface to the SDK
 * 
 * `piccolo_os_lock_core.h` contains specific definitions and parameters to override default values in the Pi Pico SDK in order
 * to provide compatibility with the Piccolo_Os cooperative multi-tasking operating system originally
 * written by [Gary Sims](https://github.com/garyexplains). The associated functions are in `piccolo_os_lock_core.c`. 
 * 
 * This header should be included in CMakelists.txt files 
 * by adding the following line before the `pico_sdk_init()` but after the `project(...)` line:
 * 
 * `list(APPEND PICO_CONFIG_HEADER_FILES ${PROJECT_SOURCE_DIR}/piccolo_os_lock_core.h)`
 * 
 * @{
 */


#ifndef lock_get_caller_owner_id
/*! \brief  return the owner id for the caller. 
 *  \ingroup lock_core
 * By default this returns the calling core number, but may be overridden (e.g. to return an RTOS task id)
 * 
 * **For Piccolo OS, return the running task ID if there is one. Otherwise, return the core number.**
 */
#define lock_get_caller_owner_id() ((lock_owner_id_t) piccolo_lock_get_owner_id())
#endif


#ifndef lock_internal_spin_unlock_with_wait
/*! \brief   Atomically unlock the lock's spin lock, and wait for a notification.
 *  \ingroup lock_core
 *
 * _Atomic_ here refers to the fact that it should not be possible for a concurrent lock_internal_spin_unlock_with_notify
 * to insert itself between the spin unlock and this wait in a way that the wait does not see the notification (i.e. causing
 * a missed notification). In other words this method should always wake up in response to a lock_internal_spin_unlock_with_notify
 * for the same lock, which completes after this call starts.
 *
 * In an ideal implementation, this method would return exactly after the corresponding lock_internal_spin_unlock_with_notify
 * has subsequently been called on the same lock instance, however this method is free to return at _any_ point before that;
 * this macro is _always_ used in a loop which locks the spin lock, checks the internal locking primitive state and then
 * waits again if the calling thread should not proceed.
 *
 * By default this macro simply unlocks the spin lock, and then performs a WFE, but may be overridden
 * (e.g. to actually block the RTOS task).
 * 
 * **For Piccolo OS, if a valid task is running, yield. Then return.**
 *
 * \param lock the lock_core for the primitive which needs to block
 * \param save the uint32_t value that should be passed to spin_unlock when the spin lock is unlocked. (i.e. the `PRIMASK`
 *             state when the spin lock was acquire
 */


#define lock_internal_spin_unlock_with_wait(lock, save) ({spin_unlock((lock)->spin_lock, save);piccolo_lock_wait();})
#endif

#ifndef lock_internal_spin_unlock_with_best_effort_wait_or_timeout
/*! \brief   Atomically unlock the lock's spin lock, and wait for a notification or a timeout
 *  \ingroup lock_core
 *
 * _Atomic_ here refers to the fact that it should not be possible for a concurrent lock_internal_spin_unlock_with_notify
 * to insert itself between the spin unlock and this wait in a way that the wait does not see the notification (i.e. causing
 * a missed notification). In other words this method should always wake up in response to a lock_internal_spin_unlock_with_notify
 * for the same lock, which completes after this call starts.
 *
 * In an ideal implementation, this method would return exactly after the corresponding lock_internal_spin_unlock_with_notify
 * has subsequently been called on the same lock instance or the timeout has been reached, however this method is free to return
 * at _any_ point before that; this macro is _always_ used in a loop which locks the spin lock, checks the internal locking
 * primitive state and then waits again if the calling thread should not proceed.
 *
 * By default this simply unlocks the spin lock, and then calls  best_effort_wfe_or_timeout
 * but may be overridden (e.g. to actually block the RTOS task with a timeout).
 *
 *  **For Piccolo OS, if a valid task is running, yield. Then return the timeout status.**
 * 
 * \param lock the lock_core for the primitive which needs to block
 * \param save the uint32_t value that should be passed to spin_unlock when the spin lock is unlocked. (i.e. the PRIMASK
 *             state when the spin lock was acquire)
 * \param until the absolute_time_t value
 * \return true if the timeout has been reached
 */
#define lock_internal_spin_unlock_with_best_effort_wait_or_timeout(lock, save, until) ({ \
    spin_unlock((lock)->spin_lock,save); piccolo_lock_wait_until(until);                       \
})
#endif

#ifndef sync_internal_yield_until_before
/*! \brief   yield to other processing until some time before the requested time
 *  \ingroup lock_core
 *
 * This method is provided for cases where the caller has no useful work to do
 * until the specified time.
 *
 * By default this method does nothing, however it can be overridden (for example by an
 * RTOS which is able to block the current task until the scheduler tick before
 * the given time)
 * 
 * **For Piccolo OS, if a valid task is running, yield. Then return.**
 *
 * \param until the absolute_time_t value
 * 
 */
#define sync_internal_yield_until_before(until) (piccolo_lock_yield();)
#endif

/**@}**/

#endif