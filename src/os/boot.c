/*
 * Copyright (C) 2022 Keith Standiford
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "pico/malloc.h"
#include "hardware/structs/systick.h"
#include "hardware/exception.h"
#include "pico/multicore.h"
#include "kernel/lock_core.h"
#include "pico/sem.h"

#include "kernel/kernel.h"

#include <hagl_hal.h>
#include <hagl.h>

// const uint LED_PIN = 14;
// volatile extern uint32_t tickct;
// volatile extern int32_t sleep_time;
// volatile extern int32_t sleep_core;
// volatile extern int32_t sleep_ct;

//  semaphore_t talking_stick;

// /*
//  * This task blinks the LED. It also holds the semaphore talking_stick
//  * while the LED is off. This is used to gate the reporter task and keep it
//  * from printing.
//  */
// void blinker(void) {
//   gpio_init(LED_PIN);
//   gpio_set_dir(LED_PIN, GPIO_OUT);
//   while (true) {
//     gpio_put(LED_PIN, 1);
//     sem_release(&talking_stick);
//     piccolo_sleep(2000);
//     gpio_put(LED_PIN, 0);
//     sem_acquire_blocking(&talking_stick);
//     piccolo_sleep(2000);
//   }
// }


// int is_prime(unsigned int n)
// {
//   unsigned int p;
//   if (!(n & 1) || n < 2 ) return n == 2;
 
//   // comparing p*p <= n can overflow 
//   for (p = 3; p <= n/p; p += 2)
//     if (!(n % p)) return 0;
//   return 1;
// }

// /*  
//  * Prime number computing task. Just to burn CPU time
//  * Note that it never calls piccolo_yield()
//  */
// uint32_t primes[2], totalPrimes;
// piccolo_os_task_t * reporter;

// void find_primes(void) {
//   int p;

//   printf("task2: Created!\n");
//   while (1) {
//     for (p=5;p;p+=2) if(is_prime(p)==1) {
//     totalPrimes++;
//     primes[get_core_num()]++;
//     // every 4096 prime numbers, send the reporter a signal
//     if(!(totalPrimes & 0xFFF)) piccolo_send_signal(reporter);
//     }
//   }
// }
// /*
//  * Report on the progress of the prime number finder. Wait until he sends a signal
//  * Then get the "talking stick" semaphore from the LED blinker task. We can only 
//  * talk when the green light is on! Then print a report. We also report on
//  * how many tasks the garbage collector has reclaimed using a counter which
//  * is only there as a debug feature at the moment...
//  */
// extern uint32_t kills;
// void reporter_task(void){
//     printf("Reporter Started\n");
//     while(1) {
//         piccolo_get_signal_all_blocking();
//         if(!sem_acquire_timeout_ms(&talking_stick,10000)) printf("sem acquire timeout SHOULD NOT have failed\n");
       
//         printf("Total primes %d, cores 0/1 %d/%d kills %d recycled %d bytes\n",
//             totalPrimes,primes[0], primes[1], kills, kills*sizeof(piccolo_os_task_t));
//         sem_release(&talking_stick);
//     }
// }

// /*
//  * The next two tasks are created periodically by the stress_tester task
//  * only to quickly delete themselves. "z" dies immediatly whicle "sz" yields
//  * once first.
//  */
// void sz(){
//     piccolo_yield();
//     return;
// }

// void z(){
//     int a = 1;
//     return;
// }

// void stress_tester(void) {
//     /**
//      * Force a slew of task create and deletes by creating lots of tasks
//      * that die very quickly. Do this every few seconds. Note that we
//      * are perfectly safe creating multiple tasks running the same function
//      * since they all have seperate stacks. 
//      * 
//      */
//     while(1) {
//         piccolo_create_task(z);
//         piccolo_create_task(z);
//         piccolo_create_task(z);
//         piccolo_create_task(sz);
//         piccolo_create_task(sz);
//         piccolo_create_task(sz);
//         piccolo_create_task(z);
//         piccolo_create_task(sz);
//         piccolo_create_task(z);
//         piccolo_create_task(sz);
//         piccolo_create_task(sz);

//         piccolo_sleep(3000);
//     }
// }


// void spinner2(void){
//     piccolo_os_task_t * task;
//     task = piccolo_get_task_id();
//     while(!piccolo_get_signal()) piccolo_yield();   // spin until we get a signal
//     piccolo_get_signal_blocking();                  // hang until we get a second one
// //    printf(" signal hang2 task %8X in %d out %d limit %d\n",task,task->signal_in,task->signal_out,task->signal_limit);
//     piccolo_get_signal_blocking();                  // hang until we get a third one
// //    printf(" semaphore hang task %8X in %d out %d limit %d\n",task,task->signal_in,task->signal_out,task->signal_limit);
//     sem_acquire_blocking(&talking_stick);           // then hang on the semaphore
// //    printf(" semaphore exit task %8X in %d out %d limit %d\n",task,task->signal_in,task->signal_out,task->signal_limit);
//     return;                                         // and exit
// }
// void spinner(){
//     int i,yielding=1,blocking=1, semaphore;
//     absolute_time_t start;
//     uint64_t time;
//     piccolo_os_task_t *spin1, *spin2;
// #define loops 1000
//     piccolo_sleep(10);
//     start = get_absolute_time();
//     for(i=0;i<loops;i++) piccolo_yield();
//     time = absolute_time_diff_us(start,get_absolute_time());
//     printf("\n");
//     printf("Yielding:%3d  Blocking:%3d Takes %6lld nanoseconds\n",yielding,blocking,time);

//     spin1 = piccolo_create_task(spinner2);
//     yielding++;
//     piccolo_sleep(10);
//     start = get_absolute_time();
//     for(i=0;i<loops;i++) piccolo_yield();
//     time = absolute_time_diff_us(start,get_absolute_time());
//     printf("Yielding:%3d  Blocking:%3d Takes %6lld nanoseconds\n",yielding,blocking,time);

//     spin2 = piccolo_create_task(spinner2);
//     yielding++;
//     piccolo_sleep(10);
//     start = get_absolute_time();
//     for(i=0;i<loops;i++) piccolo_yield();
//     time = absolute_time_diff_us(start,get_absolute_time());
//     printf("Yielding:%3d  Blocking:%3d Takes %6lld nanoseconds\n",yielding,blocking,time);

//     piccolo_send_signal(spin1);
//     piccolo_send_signal(spin2);     // send them both to signal blocking
//     yielding -=2;
//     blocking +=2;
//     piccolo_sleep(2000);
//     start = get_absolute_time();
//     for(i=0;i<loops;i++) piccolo_yield();
//     time = absolute_time_diff_us(start,get_absolute_time());
//     printf("Yielding:%3d  Blocking:%3d Takes %6lld nanoseconds\n",yielding,blocking,time);

//     sem_try_acquire(&talking_stick);           // Eat the permit that should be there
//     piccolo_send_signal(spin1);
//     piccolo_send_signal(spin2);     // send them both to semaphores
//     yielding = 1;
//     blocking = 1;
//     semaphore = 2;
//     piccolo_sleep(2000);
//     start = get_absolute_time();
//     for(i=0;i<loops;i++) piccolo_yield();
//     time = absolute_time_diff_us(start,get_absolute_time());
//     printf("Yielding:%3d  Blocking:%3d On Semaphore:%3d Takes %6lld nanoseconds\n",yielding,blocking,semaphore, time);

//     sem_release(&talking_stick);    // one of them will die
//     semaphore--;
//     piccolo_sleep(2000);
//     start = get_absolute_time();
//     for(i=0;i<loops;i++) piccolo_yield();
//     time = absolute_time_diff_us(start,get_absolute_time());
//     printf("Yielding:%3d  Blocking:%3d On Semaphore:%3d Takes %6lld nanoseconds\n",yielding,blocking,semaphore, time);

//     sem_release(&talking_stick);    // one of them will die
//     piccolo_sleep(20);
//     sem_release(&talking_stick);    // replace the permit

//     //start the LED blinker
//     piccolo_create_task(blinker);
//     printf("\nStart the prime finder, his reporter and the stress tester, and then depart!\n");
//     piccolo_create_task(stress_tester);
//     reporter = piccolo_create_task(reporter_task);
//     piccolo_create_task(find_primes);
// }


// int main() {
//     stdio_init_all();
//     piccolo_init();


//     sleep_ms(5000);  // pause to give user a chance to start putty

//     printf("Hello World!\n");

//     // initialize the semaphore 
//     sem_init(&talking_stick,1,1);

//     // test a few things for timing first
//     // the spinner task will start everything else ...
//     piccolo_create_task(spinner);

//     printf("PICCOLO OS Demo Starting...\n");
//     // and begin!
//     piccolo_start();

//   return 0; /* Never gonna happen */
// }

static hagl_backend_t *display;

int main() {
    display = hagl_init();

    while (1) {
        hagl_clear(display);

        for (uint16_t i = 1; i < 10; i++) {
            int16_t x0 = rand() % DISPLAY_WIDTH;
            int16_t y0 = rand() % DISPLAY_HEIGHT;
            int16_t x1 = rand() % DISPLAY_WIDTH;
            int16_t y1 = rand() % DISPLAY_HEIGHT;
            color_t color = rand() % 0xffff;

            hagl_fill_rectangle(display, x0, y0, x1, y1, color);
        }

        hagl_flush(display);

    }

    hagl_close(display);

    return 0;
}