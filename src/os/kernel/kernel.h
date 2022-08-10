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

typedef struct process {
	void 				(*function_pointer)(void);
	double 	            priority;
	uint				identification;
	enum 	status		status;
	int 				*stack_words;
	struct 	process* 	next;
} process;

void kernel_initalize();

void kernel_start();

void round_robin_scheduler(void);

uint kernel_create_process(void (*pointer_to_task_function)(void));

void kernel_execute_process(uint node_identification);

void kernel_hault_process(uint node_identification);

void kernel_unblock_process(uint node_identification);

void kernel_block_process(uint node_identification);

bool kernel_terminate_process_by_id(uint node_identification);

bool kernel_terminate_process_by_pointer(void (*pointer_to_task_function)(void));

void list_all_tasks();

process *get_process_by_id(uint task_id);

#endif