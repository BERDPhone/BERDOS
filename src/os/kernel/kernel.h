#ifndef KERNEL_H
#define KERNEL_H

void kernel_initalize();

void kernel_start();

void kernel_create_process(void (*pointer_to_task_function)(void));

void kernel_kill_process(void (*pointer_to_task_function)(void));

#endif