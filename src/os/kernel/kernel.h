#ifndef KERNEL_H
#define KERNEL_H
#define BERDOS_STACK_SIZE 256
#define BERDOS_TIME_SLICE 999
#define BERDOS_PROCESS_LIMIT 5

// # DATA DECLARATIONS
// ## PROCESS MANAGEMENT
// ### PROCESS -- DATA STRUCTURES
typedef enum states {
	TERMINATED,
	READY,
	BLOCKED,
	EXECUTING,
} states;

typedef struct process {
void				(*process_pointer)(void);
uint 				process_stack[BERDOS_STACK_SIZE]; // Must be word-aligned.
uint 				process_id_number;
enum 	states 		process_status;
uint				*process_stack_pointer;
struct process		*next_node;
} process;


// # FUNCTION DECLARATIONS
// ## PROCESS MANAGEMENT
// ### PROCESS -- STATE MANAGEMENT
uint create_process(void (*function_pointer)(void));
void execute_process(uint node_id_number);
void block_process(uint node_id_number);
void ready_process(uint node_id_number);
void terminate_process(uint node_id_number);

// ## KERNEL OPERATION
// ### KERNEL -- START-UP
void kernel_initizalize(void);
void kernel_start(void);

#endif