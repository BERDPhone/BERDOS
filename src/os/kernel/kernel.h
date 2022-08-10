#ifndef KERNEL_H
#define KERNEL_H
#define BDOS_STACK_SIZE 256

typedef enum status {
	TERMINATED,
	BLOCKED,
	READY,
	RUNNING,
} status;

typedef enum mode {
	KERNEL,
    USER
} mode;

/* This code initializes the "process" structure whose instances represent nodes of a linked list and a
process executed in the central processing unit (CPU). */
typedef struct process {
	void 				(*function_pointer)(void);
	double 				priority;
	uint				identification;
	enum 	status		status;
	struct 	process* 	next;
	int 				*stack_words;
	uint 				stack_size;
} process;

void kernel_initalize();

void kernel_start();

uint kernel_create_process(void (*pointer_to_task_function)(void), int necessity, mode running);

bool kernel_kill_process_by_pointer(void (*pointer_to_task_function)(void));

bool kernel_kill_process_by_id(uint task_id);

void list_all_tasks();

void kernel_start();

process *get_process_by_index(uint index);

process *get_process_by_id(uint task_id);

#endif