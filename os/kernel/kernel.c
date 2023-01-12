#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "kernel.h"
#include "../boot/boot.h"

#include "pico/stdlib.h"
#include "hardware/structs/systick.h"
#include "hardware/sync.h"
#include "malloc.h"

// # DATA DECLARATIONS & FUNCTION PROTOTYPES
// ## FILE MANAGEMENT
index_node *__root_directory = NULL;

// ## MEMORY MANAGEMENT
unsigned int memory_utilization;
enum bitmap_partition bitmap[(PICO_SRAM_SIZE / BERDOS_PARTITION_SIZE)] = {UNALLOCATED};

// ## PROCESS MANAGEMENT
void __initialize_main_stack(unsigned int *MSP);
unsigned int *__context_switch(unsigned int *PSP);

unsigned int process_count;
enum schedulers scheduling_dicipline;

control_block *__head = NULL;
control_block *__tail = NULL;
control_block *__root = NULL;

static control_block *ready_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static control_block *job_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static control_block *device_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static unsigned int ready_queue_index;
static unsigned int job_queue_index;
static unsigned int device_queue_index;

// # FUNCTION DEFINITIONS
// ## DIAGNOSTICS
static void __print_diagnostics() {
	printf("******************************************************************\n");
	printf("DIAGNOSTICS:\n");
	printf("Bitmap: @0x%X\n",  memory_utilization);
	for (uint j = 0; j < (PICO_SRAM_SIZE / BERDOS_PARTITION_SIZE); j += 66) {
		for (uint i = j; i < j + 66; i++) {
			printf("%d", bitmap[i]);
		};
		printf("\n");
	};
	printf("Job Queue: ");
	for (uint i = 0; i < job_queue_index; i++) {
		printf("#%d ", job_queue[i]->id_number);
	};
	printf("\n");
	printf("Ready Queue: ");
	for (uint i = 0; i < ready_queue_index; i++) {
		printf("#%d ", ready_queue[i]->id_number);
	};
	printf("\n");
	printf("Device Queue: ");
	for (uint i = 0; i < device_queue_index; i++) {
		printf("#%d ", device_queue[i]->id_number);
	};
	printf("\n");
	printf("Process Table: @%d\n", process_count);
	control_block *process = __head;
	while (process != NULL) {
		printf(" * Process #%d @0x%X §%d @@0x%X\n", process->id_number, process->address_space, process->status, process);
		process = process->next_node;
	};
	printf("******************************************************************\n");
}

// ## MEMORY MANAGEMENT
// ### MEMORY -- ALLOCATION/DEALLOCATION FUNCTIONS
static void *__allocate_SRAM(size_t size, enum bitmap_partition data_type) {
	if (size < BERDOS_PARTITION_SIZE) size = BERDOS_PARTITION_SIZE;
	size += (size * (size % BERDOS_PARTITION_SIZE));

	for (uint i = 0; i < (PICO_SRAM_SIZE / BERDOS_PARTITION_SIZE); i++) {
		uint sum = 0;
		for (uint j = i; j < (i + (size / BERDOS_PARTITION_SIZE)); j++) {
			sum += bitmap[j];
		};
		if (sum == 0) {
			for (uint k = i; k < (i + (size / BERDOS_PARTITION_SIZE)); k++) {
				bitmap[k] = data_type;
			};
			memory_utilization += size;
			return (void *)((i * BERDOS_PARTITION_SIZE) + PICO_SRAM_BASE);
		};
	};
	return NULL;
}

static void *__deallocate_SRAM(void *pointer, size_t size) {
	if (pointer == NULL) return pointer;
	if (size < BERDOS_PARTITION_SIZE) size = BERDOS_PARTITION_SIZE;
	size += (size * (size % BERDOS_PARTITION_SIZE));

	unsigned int bitmap_index = ((uint)(pointer - PICO_SRAM_BASE) / BERDOS_PARTITION_SIZE);
	for (uint i = bitmap_index; i < (bitmap_index + (size / BERDOS_PARTITION_SIZE)); i++) {
		bitmap[i] = UNALLOCATED;
	};
	memory_utilization -= size;

	return NULL;
};

// ### MEMORY -- HIERARCHY SWAPPING
static void *__swap_process_out(control_block *process) {
	if (process->status != SWAPPED_READY || process->status != SWAPPED_BLOCKED) {
		process->address_space = __deallocate_SRAM(process->address_space, sizeof(address_space));

		if (process->status == READY) {
			process->status = SWAPPED_READY;
		} else if (process->status == BLOCKED) {
			process->status = SWAPPED_BLOCKED;
		};

		return NULL;
	} else {
		return process->address_space;
	};
}

static void *__swap_process_in(control_block *process) {
	if (process->status == SWAPPED_READY || process->status == SWAPPED_BLOCKED) {
		process->address_space = __allocate_SRAM(sizeof(address_space), PROCESS_ADDRESS_SPACE);

		if (process->status == READY) {
			process->status = SWAPPED_READY;
		} else if (process->status == BLOCKED) {
			process->status = SWAPPED_BLOCKED;
		};

		return process->address_space;
	} else {
		return process->address_space;
	};
}

// ## PROCESS MANAGEMENT
// ### PROCESS -- SYSTICK FUNCTIONS
static void __disable_preemption(void) {
	systick_hw->csr = 0;	// Update Systick Control Status Register to Disable Timer and IRQ 
	__dsb();                
	__isb();                

	// Clear Systick Exception Pending Bit
	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_ICSR_OFFSET),M0PLUS_ICSR_PENDSTCLR_BITS);
}

static void __enable_preemption(unsigned int time_slice_duration) {
	__disable_preemption();

	systick_hw->rvr = (time_slice_duration) - 1UL;	// Set Systick Reload Value Register
	systick_hw->cvr = 0;							// Set Systick Current Value Register; Clear Current Value
	systick_hw->csr = 0x03;							// Set Systick Control Status Register; Enable IRQ and Clock
}

// ## FILE MANAGMENT
// ### FILE -- UTILITY FUNCTIONS
index_node *__parse_pathname(char *pathname) {
	char buffer[BERDOS_MAX_PATHNAME_SIZE + 1];
	strncpy(buffer, pathname, BERDOS_MAX_PATHNAME_SIZE + 1);

	char target[BERDOS_INODE_NAME_SIZE + 1];
	strncpy(target, strrchr(buffer, '/') + 1, strlen(target) + 1);

	char *token;
	token = strtok(buffer, "/");
	if (token == NULL) return NULL;

	index_node *inode = __root_directory;
	while (strncmp(target, inode->name, strlen(target)) != 0) {
		if (strncmp(token, inode->name, strlen(token)) != 0) {
			inode = inode->child_node;
			if (inode == NULL) return NULL;
			while (strncmp(inode->name, token, strlen(inode->name)) != 0) {
				inode = inode->sibling_node;
				if (inode == NULL) return NULL;;
			};
		} else {
			token = strtok(NULL, "/");
		};
	};
	return inode;
}

// ### FILE -- DIRECROTRY OPERATIONS
void __create_directory(char *directory_name, char *pathname) {
	index_node *new_inode = malloc(sizeof(index_node));
	if (new_inode == NULL) panic("ERROR: Malloc Failed\n");
	new_inode->pointer = new_inode;
	
	strncpy(new_inode->name, directory_name, BERDOS_INODE_NAME_SIZE);
	new_inode->name[BERDOS_INODE_NAME_SIZE] = '\0';
	new_inode->size = 0;
	new_inode->mode = DIRECTORY_NODE;

	printf("KERNEL: Creating Directory \"%s\" @ \"%s\"\n", new_inode->name, pathname);
	new_inode->child_node = NULL;
	new_inode->sibling_node = NULL;
	if (__root_directory != NULL) {
		index_node *parent_directory = __parse_pathname(pathname);
		if (parent_directory == NULL) panic("ERROR: Bad Pathname\n");
		if (parent_directory->child_node == NULL) {
			parent_directory->child_node = new_inode;
		} else {
			if (parent_directory->child_node->sibling_node == NULL) {
				parent_directory->child_node->sibling_node = new_inode;
			} else {
				new_inode->sibling_node = parent_directory->child_node->sibling_node;
				parent_directory->child_node->sibling_node = new_inode;
			};
		};
	} else {
		__root_directory = new_inode;
	};
}

// ### FILE -- FILE OPERATIONS
void *__create_file(char *file_name, size_t file_size, char *pathname) {
	index_node *new_inode = malloc(sizeof(index_node));
	if (new_inode == NULL) panic("ERROR: Malloc Failed\n");
	new_inode->pointer = __allocate_SRAM(file_size, TEXT_FILE);
	if (new_inode == NULL) panic("ERROR: Index Node Overflow\n");

	strncpy(new_inode->name, file_name, BERDOS_INODE_NAME_SIZE);
	new_inode->name[BERDOS_INODE_NAME_SIZE] = '\0';
	new_inode->size = file_size;
	new_inode->mode = FILE_NODE;

	printf("KERNEL: Creating File \"%s\" @ \"%s\"\n", new_inode->name, pathname);

	new_inode->child_node = NULL;
	new_inode->sibling_node = NULL;
	if (__root_directory != NULL) {
		index_node *directory = __parse_pathname(pathname);
		if (directory == NULL) panic("ERROR: Bad Pathname\n");
		if (directory->child_node == NULL) {
			directory->child_node = new_inode;
		} else {
			if (directory->child_node->sibling_node == NULL) {
				directory->child_node->sibling_node = new_inode;
			} else {
				index_node *youngest_sibling = directory->child_node;
				while (youngest_sibling->sibling_node != NULL) {
					youngest_sibling = youngest_sibling->sibling_node;
				};
				youngest_sibling->sibling_node = new_inode;
			};
		};
	} else {
		panic("ERROR: Null Root Directory\n");
	};
	return new_inode->pointer;
}

// ### PROCESS -- UTILITY FUNCTIONS
control_block *__get_process_by_id_number(unsigned int node_id_number) {
	control_block *process = __head;
	while (process != NULL) {
		if (process->id_number == node_id_number) {
			return process;
		};
		process = process->next_node;
	};
	return NULL;
}

static control_block *__get_executing_process(void) {
	control_block *process = __head;
	while (process != NULL) {
		if (process->status == EXECUTING) {
			return process;
		};
		process = process->next_node;
	};
	return NULL;
}

static control_block *__get_parent_process(control_block *child_process) {
	control_block *process = __head;
	while (process != NULL) {
		if (process->child_node == child_process) {
			return process;
		};
		if (process->sibling_node == child_process) {
			return __get_parent_process(process);
		};
		process = process->next_node;
	};
	return NULL;
}

// ### PROCESS -- STATE CHANGING
control_block *create_process(void (*function_pointer)(), unsigned int *starting_arguments, control_block *parent_process) {
	static unsigned int new_id_number;
	printf("KERNEL: Creating Process #%d \n", new_id_number);

	if (process_count >= BERDOS_PROCESS_LIMIT) panic("ERROR: Control Block Overflow\n");

	control_block *new_process = malloc(sizeof(control_block));
	if (new_process == NULL) panic("ERROR: Malloc Failed\n");
	new_process->address_space = __allocate_SRAM(sizeof(address_space), PROCESS_ADDRESS_SPACE);
	if (new_process->address_space == NULL) panic("ERROR: SRAM Allocation Failed\n");

	new_process->pointer = function_pointer;
	new_process->id_number = new_id_number;
	new_process->status = CREATED;

	new_process->address_space->heap_pointer = NULL;
	new_process->address_space->stack_pointer = new_process->address_space->stack + BERDOS_STACK_SIZE - 17; 
	*(new_process->address_space->stack_pointer + 8) = (unsigned int) 0xFFFFFFFD; // EXC_RETURN in LR
  	*(new_process->address_space->stack_pointer + 15) = (unsigned int) function_pointer; // Function Pointer in PC
  	*(new_process->address_space->stack_pointer + 16) = (unsigned int) 0x01000000; // Thumb Bit in EPSR
  	*(new_process->address_space->stack_pointer + 9) = (unsigned int) starting_arguments[0]; // Argument 0 in R0
  	*(new_process->address_space->stack_pointer + 10) = (unsigned int) starting_arguments[1]; // Argument 1 in R1
  	*(new_process->address_space->stack_pointer + 11) = (unsigned int) starting_arguments[2]; // Arugment 2 in R2
  	*(new_process->address_space->stack_pointer + 12) = (unsigned int) starting_arguments[3]; // Arugment 3 in R2

	if (__tail == NULL && __head == NULL) {
		new_process->next_node = NULL;
		new_process->previous_node = NULL;
		__head = new_process;
		__tail = new_process;
	} else {
		new_process->previous_node = __tail;
		new_process->previous_node->next_node = new_process;
		new_process->next_node = NULL;
		__tail = new_process;
	};

	new_process->child_node = NULL;
	new_process->sibling_node = NULL;
	if (__root != NULL) {
		if (parent_process->child_node == NULL) {
			parent_process->child_node = new_process;
		} else {
			if (parent_process->child_node->sibling_node == NULL) {
				parent_process->child_node->sibling_node = new_process;
			} else {
				new_process->sibling_node = parent_process->child_node->sibling_node;
				parent_process->child_node->sibling_node = new_process;
			};
		};
	} else {
		__root = new_process;
	};

	job_queue[job_queue_index] = new_process;
	job_queue_index++;

	process_count++;
	new_id_number++;

	return new_process;
}

void terminate_process(control_block *process, control_block *parent_process){
	printf("KERNEL: Terminating Process #%d @0x%X §%d\n", process->id_number, process, process->status);

	process->status = TERMINATED;

	for (uint i = 0; i < BERDOS_PROCESS_LIMIT; i++) {
		if (job_queue[i] == process) {
			for (uint j = i; j < BERDOS_PROCESS_LIMIT; j++) {
				job_queue[j] = job_queue[j + 1];
			};
			job_queue[BERDOS_PROCESS_LIMIT - 1] = NULL;
			job_queue_index--;
		};
		if (ready_queue[i] == process) {
			for (uint j = i; j < BERDOS_PROCESS_LIMIT; j++) {
				ready_queue[j] = ready_queue[j + 1];
			};
			ready_queue[BERDOS_PROCESS_LIMIT - 1] = NULL;
			ready_queue_index--;
		};
		if (device_queue[i] == process) {
			for (uint j = i; j < BERDOS_PROCESS_LIMIT; j++) {
				device_queue[j] = device_queue[j + 1];
			};
			device_queue[BERDOS_PROCESS_LIMIT - 1] = NULL;
			device_queue_index--;
		};
	};

	process->previous_node->next_node = process->next_node;
	process->next_node->previous_node = process->previous_node;
	if (process == __head) {
		__head = process->next_node;
	};
	if (process == __tail) {
		__tail = process->previous_node;
	};

	if (parent_process->child_node == process) {
		parent_process->child_node = process->sibling_node;
	} else {
		control_block *previous_sibling = parent_process->child_node;
		if (previous_sibling->sibling_node != process) {
			previous_sibling = previous_sibling->sibling_node;
		};
		previous_sibling->sibling_node = process->sibling_node;
	};
	while (process->child_node != NULL) {
		terminate_process(process->child_node, process);
	};

	process->address_space = __deallocate_SRAM(process->address_space, sizeof(address_space));
	free(process);

	process_count--;
}

static void __execute_process(control_block *process) {
	printf("KERNEL: Executing Process #%d @0x%X §%d\n", process->id_number, process, process->status);
	
	if (process->status == READY) {
		process->status = EXECUTING;
		process->address_space->stack_pointer = __context_switch(process->address_space->stack_pointer);
	};
}

static void __block_process(control_block *process) {
	printf("KERNEL: Blocking Process #%d @0x%X §%d\n", process->id_number, process, process->status);
	
	if (process->status == EXECUTING || process->status == READY) {
		process->status = BLOCKED;

		for (uint i = 0; i < BERDOS_PROCESS_LIMIT; i++) {
			if (ready_queue[i] == process) {
				for (uint j = i; j < BERDOS_PROCESS_LIMIT; j++) {
					ready_queue[j] = ready_queue[j + 1];
				};
				ready_queue[BERDOS_PROCESS_LIMIT - 1] = 0;
				ready_queue_index--;
			};
		};
	};
}

static void __ready_process(control_block *process) {
	printf("KERNEL: Readying Process #%d @0x%X §%d\n", process->id_number, process, process->status);

	if (process->status == EXECUTING || process->status == BLOCKED || process->status == CREATED) {
		ready_queue[ready_queue_index] = process;
		ready_queue_index++;

		process->status = READY;
	};
}

// ### PROCESS -- SCHEDULING
static void __bubble_sort_queue(control_block *array[], unsigned int index, size_t member_offset) {
	for (uint i = 0; i < index - 1; i++) {
		for (uint j = 0; j < index - i - 1; j++) {
			if (((int *)(array[j]))[(member_offset / 4)] > ((int *)(array[j + 1]))[(member_offset / 4)]) {
				control_block *buffer;
				buffer = array[j];
				array[j] = array[j + 1];
				array[j + 1] = buffer;
			};
		};
	};
}

static void __dispatcher(control_block *process) {
	__execute_process(process);

	switch (process->status) {
		case EXECUTING:
			process->status = READY;
			break;
		case BLOCKED:
			process->status = BLOCKED;
			break;
		case TERMINATED:
			process->status = TERMINATED;
			break;
		default:
			panic("ERROR: Unexpected Process Status\n");
	};
}

static void __short_term_scheduler() {
	printf("KERNEL: Short-Term Scheduling\n");
	for (uint i = 0; i < ready_queue_index; i++) {
		__dispatcher(ready_queue[i]);
	};
}

static void __medium_term_scheduler() {
	printf("KERNEL: Medium-Term Scheduling\n");
	switch (scheduling_dicipline) {
		case FIRST_COME_FIRST_SERVED:
			__disable_preemption();
			__bubble_sort_queue(ready_queue, ready_queue_index, offsetof(control_block, id_number));
			break;
		case ROUND_ROBIN:
			__enable_preemption(BERDOS_TIME_SLICE);
			__bubble_sort_queue(ready_queue, ready_queue_index, offsetof(control_block, id_number));
			break;
		case SHORTEST_JOB_NEXT:
			__disable_preemption();
			break;
		case SHORTEST_REMAINING_TIME_NEXT:
			__enable_preemption(BERDOS_TIME_SLICE);
			break;
	};

	for (uint i = 0; i <= ready_queue_index; i++) {
		ready_queue[i]->address_space = __swap_process_in(ready_queue[i]);

		for (uint j = 0; ready_queue[j]->address_space == NULL && j < device_queue_index; j++) {
			device_queue[j]->address_space = __swap_process_out(device_queue[j]);
			ready_queue[i]->address_space = __swap_process_in(ready_queue[i]);
		};

		for (uint j = ready_queue_index; ready_queue[i]->address_space == NULL && j >= 0; j--) {
			ready_queue[j]->address_space = __swap_process_out(ready_queue[j]);
			ready_queue[i]->address_space = __swap_process_in(ready_queue[i]);
		};
	};
}

static void __long_term_scheduler(void) {
	printf("KERNEL: Long-Term Scheduling\n");
	if (process_count == 0) panic("ERROR:\n");

	control_block *process = __head;
	while (process != NULL) {
		if (process->status == CREATED) {
			__ready_process(process);
		};
		process = process->next_node;
	};
}

// ## KERNEL OPERATION
// ### KERNEL - START-UP
void kernel_initizalize(void) {
	process_count = 0;
	memory_utilization = 0;
	scheduling_dicipline = BERDOS_DEFAULT_SCHEDULER;

	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET), M0PLUS_SHPR2_BITS);
  	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET), M0PLUS_SHPR3_BITS);

  	unsigned int dummy[32];
  	__initialize_main_stack(&dummy[32]);

  	__create_directory(":)", NULL);

  	void *os_heap = __allocate_SRAM((BERDOS_PARTITION_SIZE * 16), OPERATING_SYSTEM_DATA);
	void *free = __allocate_SRAM((BERDOS_PARTITION_SIZE * 240), PROCESS_ADDRESS_SPACE);
	void *os_stack = __allocate_SRAM((BERDOS_PARTITION_SIZE * 8), OPERATING_SYSTEM_DATA);
	__deallocate_SRAM(free, (BERDOS_PARTITION_SIZE * 240));

  	__head = NULL;
	__tail = NULL;
	__root = NULL;

  	ready_queue_index = 0;
	job_queue_index = 0;
	device_queue_index = 0;
}

void kernel_start(void) {
	while (true) {
		__print_diagnostics();
		__long_term_scheduler();
		__medium_term_scheduler();
		__short_term_scheduler();
	};
}

// ### KERNEL - SYSTEM CALL HANDLERS
void __exit(void) {
	control_block *caller_process = __get_executing_process();
	terminate_process(caller_process, __get_parent_process(caller_process));
}

unsigned int __spawn(void (*function_pointer)(), unsigned int *starting_arguments) {
	return create_process(function_pointer, starting_arguments, __get_executing_process())->id_number;
}

void __mkdir(char *directory_name, char *pathname) {
	__create_directory(directory_name, pathname);
}

void *__create(char *file_name, size_t file_size, char *pathname) {
	return __create_file(file_name, file_size, pathname);
}
