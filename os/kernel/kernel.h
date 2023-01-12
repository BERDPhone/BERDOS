#ifndef KERNEL_H
#define KERNEL_H

#define PICO_SRAM_SIZE 0x42000
#define PICO_SRAM_BASE 0x20000000

#define BERDOS_PARTITION_SIZE 1024
#define BERDOS_TIME_SLICE 10000
#define BERDOS_PROCESS_LIMIT 226

#define BERDOS_TEXT_SIZE 255
#define BERDOS_DATA_SIZE 255
#define BERDOS_HEAP_SIZE 256
#define BERDOS_STACK_SIZE 256

#define BERDOS_INODE_NAME_SIZE 15
#define BERDOS_MAX_PATHNAME_SIZE 31

#define BERDOS_DEFAULT_SCHEDULER 0 // 0 = FIRST_COME_FIRST_SERVED, 1 = ROUND_ROBIN, 2 = SHORTEST_JOB_NEXT, 3 = SHORTEST_REMAINING_TIME_NEXT

// # DATA DECLARATIONS
// ## FILE SYSTEM
typedef enum index_node_modes {
	DIRECTORY_NODE,
	FILE_NODE,
} index_node_modes;

typedef struct index_node {
	void 								*pointer;
	char 								name[BERDOS_INODE_NAME_SIZE + 1];
	unsigned int 						size;
	enum 			index_node_modes	mode;
	struct 			index_node 			*child_node;
	struct 			index_node			*sibling_node;
} index_node;

// ## MEMORY MANAGEMENT
// ### MEMORY -- DATA STRUCTURES
typedef enum bitmap_partition {
	UNALLOCATED = 0,
	OPERATING_SYSTEM_DATA,
	PROCESS_ADDRESS_SPACE,
	TEXT_FILE,
} bitmap_partition;

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
	unsigned int text[BERDOS_TEXT_SIZE];
	unsigned int data[BERDOS_DATA_SIZE];
	unsigned int heap[BERDOS_HEAP_SIZE];
	unsigned int stack[BERDOS_STACK_SIZE];
	unsigned int *heap_pointer;
	unsigned int *stack_pointer;
} address_space;

typedef struct control_block {
	void								(*pointer)(void);
	unsigned int 						id_number;
	enum 			states 				status;
	struct 			address_space 		*address_space;
	struct 			control_block		*next_node;
	struct    		control_block		*previous_node;
	struct 			control_block 		*child_node;
	struct 			control_block 		*sibling_node;
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
control_block *create_process(void (*function_pointer)(), unsigned int *starting_arguments, control_block *parent_node);
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
void os_mkdir(char *directory_name, char *pathname);
void *os_create(char *file_name, size_t file_size, char *pathname);

#endif