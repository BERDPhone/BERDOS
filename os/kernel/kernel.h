#ifndef KERNEL_H
#define KERNEL_H

#define PICO_SRAM_SIZE 246000

#define BERDOS_STACK_SIZE 64
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
	SWAPPED_READY,
	SWAPPED_BLOCKED,
} states;

typedef struct address_space {
	unsigned int text[BERDOS_STACK_SIZE];
	unsigned int data[BERDOS_STACK_SIZE];
	unsigned int heap[BERDOS_STACK_SIZE];
	unsigned int stack[BERDOS_STACK_SIZE];
	unsigned int *heap_pointer;
	unsigned int *stack_pointer;
} address_space;

typedef struct control_block {
void									(*process_pointer)(void);
unsigned int 							id_number;
enum 			states 					status;
struct 			address_space 			*address_space;
struct 			control_block			*next_node;
struct    		control_block 			*previous_node;
struct 			control_block 			*child_node;
struct 			control_block 			*sibling_node;
} control_block;

typedef enum schedulers {
	FIRST_COME_FIRST_SERVED = 0,
	ROUND_ROBIN,
	SHORTEST_JOB_NEXT,
	SHORTEST_REMAINING_TIME_NEXT,
} schedulers;

// # FUNCTION DECLARATIONS
// ## PROCESS MANAGEMENT
// ### PROCESS -- STATE MANAGEMENT
unsigned int create_process(void (*function_pointer)(), unsigned int *starting_arguments, control_block *parent_node);
void terminate_process(control_block *process, control_block *parent_node);

control_block *__get_process_by_id_number(unsigned int node_id_number);


// ## KERNEL OPERATION
// ### KERNEL -- START-UP
void kernel_initizalize(void);
void kernel_start(void);

// ## KERNEL -- SYSTEM CALLS
void os_yield(void);
void os_exit(void);
unsigned int os_spawn(void (*function_pointer)(), unsigned int *starting_arguments);


#endif