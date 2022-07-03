#ifndef KERNEL_H
#define KERNEL_H

void kernel_initalize();

void kernel_start();

uint8_t kernel_create_process(void (*pointer_to_task_function)(void), int priority);

bool kernel_kill_process_by_pointer(void (*pointer_to_task_function)(void));

bool kernel_kill_process_by_id(uint8_t task_id);

void list_all_tasks();

#endif