#ifndef KERNEL_H
#define KERNEL_H

#define BERDOS_STACK_SIZE 256
#define BERDOS_TIME_SLICE 10000
#define BERDOS_PROCESS_LIMIT 5
#define BERDOS_DEFAULT_SCHEDULER 0 // 0 = FIRST_COME_FIRST_SERVED, 1 = ROUND_ROBIN

// # DATA DECLARATIONS
// ## PROCESS MANAGEMENT
// ### PROCESS -- DATA STRUCTURES
typedef enum states {
	TERMINATED = 0,
	READY,
	BLOCKED,
	EXECUTING,
	CREATED,
} states;

typedef struct process {
void									(*process_pointer)(void);
unsigned int 							process_stack[BERDOS_STACK_SIZE]; // Must be word-aligned.
unsigned int 							process_id_number;
enum 			states 					process_status;
unsigned int							*process_stack_pointer;
struct 			process					*next_node;
struct    		process 				*previous_node;
struct 			process 				*child_node;
struct 			process 				*sibling_node;
} process;

typedef enum schedulers {
	FIRST_COME_FIRST_SERVED = 0,
	ROUND_ROBIN,
	SHORTEST_JOB_NEXT,
	SHORTEST_REMAINING_TIME_NEXT,
} schedulers;

// # FUNCTION DECLARATIONS
// ## PROCESS MANAGEMENT
// ### PROCESS -- STATE MANAGEMENT
unsigned int create_process(void (*function_pointer)(), unsigned int *starting_arguments, process *parent_node);
void terminate_process(unsigned int node_id_number, process *parent_node);

process *__get_process_by_id_number(unsigned int node_id_number);


// ## KERNEL OPERATION
// ### KERNEL -- START-UP
void kernel_initizalize(void);
void kernel_start(void);

// ## KERNEL -- SYSTEM CALLS
void os_yield(void);
void os_exit(void);
unsigned int os_fork(void (*function_pointer)(), unsigned int *starting_arguments);

#endif