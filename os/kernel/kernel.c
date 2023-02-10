#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "pico/sync.h"
#include "pico/multicore.h"
#include "hardware/structs/systick.h"
#include "malloc.h"

// # DATA DECLARATIONS & FUNCTION PROTOTYPES
// ## DIAGNOSTICS
bool diagnostics_mode = 0;

// ## FILE MANAGEMENT
index_node *__root_directory = NULL;

// ## MEMORY MANAGEMENT
unsigned int memory_utilization;
enum bitmap_partition bitmap[BERDOS_PARTITION_COUNT] = {UNALLOCATED};

// ## PROCESS MANAGEMENT
extern void __initialize_main_stack(unsigned int *MSP);
extern unsigned int *__context_switch(unsigned int *PSP);
extern enum system_calls system_call_number;
extern unsigned int *hardware_frame_pointer;

unsigned int process_count;
enum schedulers scheduling_dicipline;

control_block *__head = NULL;
control_block *__tail = NULL;
control_block *__root = NULL;
void (*__boot_function)(unsigned int);

static control_block *ready_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static control_block *job_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static control_block *device_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static unsigned int ready_queue_index;
static unsigned int job_queue_index;
static unsigned int device_queue_index;

// ## MULTICORE SYNCRONIZATION
mutex_t os_lock;

// # FUNCTION DEFINITIONS
// ## DIAGNOSTICS
void print_diagnostics() {
	printf("******************************************************************\n");
	printf("DIAGNOSTICS: &%d\n", get_core_num());
	printf("Bitmap: @0x%X\n",  memory_utilization);
	for (uint j = 0; j < BERDOS_PARTITION_COUNT; j += 66) {
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

	void *malloc_pointer = malloc(size);
	if (malloc_pointer == NULL) return NULL;

	unsigned int bitmap_index = ((uint)(malloc_pointer - PICO_SRAM_BASE) / BERDOS_PARTITION_SIZE);

	bitmap[bitmap_index] = FLAG;
	for (uint i = bitmap_index + 1; i < bitmap_index + (size / BERDOS_PARTITION_SIZE); i++) {
		if (bitmap[i] == MIXED) bitmap[i]++;
		if (bitmap[i] == UNALLOCATED) bitmap[i] = data_type;	
	};

	memory_utilization += size;
	return malloc_pointer;
}

static void *__deallocate_SRAM(void *pointer) {
	unsigned int bitmap_index = ((uint)(pointer - PICO_SRAM_BASE) / BERDOS_PARTITION_SIZE);
	if (bitmap[bitmap_index] != FLAG) return NULL;

	free(pointer);

	if (bitmap[bitmap_index] <= MIXED) {bitmap[bitmap_index] = UNALLOCATED;} else {bitmap[bitmap_index]--;};
	for (uint i = bitmap_index + 1; bitmap[i] != FLAG && bitmap[i] != UNALLOCATED; i++) {
		if (bitmap[i] <= MIXED) {bitmap[i] = UNALLOCATED;} else {bitmap[i]--;};
		memory_utilization -= BERDOS_PARTITION_SIZE;
	};

	return NULL;
};

// ### MEMORY -- HIERARCHY SWAPPING
static void *__swap_process_out(control_block *process) {
	if (process->status != SWAPPED_READY || process->status != SWAPPED_BLOCKED) {
		process->address_space = __deallocate_SRAM(process->address_space);

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

// ## FILE MANAGMENT
// ### FILE -- UTILITY FUNCTIONS
index_node *__parse_pathname(char *pathname) {
	char buffer[BERDOS_MAX_PATHNAME_SIZE + 1];
	strncpy(buffer, pathname, BERDOS_MAX_PATHNAME_SIZE + 1);

	char *token, *token_save;
	token = strtok_r(buffer, "/", &token_save);
	token = strtok_r(NULL, "/", &token_save);
	
	index_node *inode = __root_directory;
	while (token != NULL) {
		if (inode->mode == FILE_INODE) return NULL;
		inode = inode->child_node;
		while (strncmp(inode->name, token, strlen(inode->name)) != 0) {
			inode = inode->sibling_node;
			if (inode == NULL) return NULL;
		};
		token = strtok_r(NULL, "/", &token_save);
	};
	return inode;
}

// ### FILE -- FILE OPERATIONS
void *__create_file(char *file_name, size_t file_size, index_node *parent_directory) {
	index_node *new_inode = (index_node *) __allocate_SRAM(sizeof(index_node), OPERATING_SYSTEM_DATA);
	if (new_inode == NULL) panic("ERROR: SRAM Allocation Failed\n");
	new_inode->file_pointer = (char *) __allocate_SRAM(file_size, TEXT_FILE);
	if (new_inode->file_pointer == NULL) panic("ERROR: SRAM Allocation Faile\n");

	strncpy(new_inode->name, file_name, BERDOS_INODE_NAME_SIZE);
	new_inode->name[BERDOS_INODE_NAME_SIZE] = '\0';
	new_inode->size = file_size;
	new_inode->mode = FILE_INODE;

	if (diagnostics_mode) printf("KERNEL: Creating File \"%s\" @0x%X @ \"%s\"\n", new_inode->name, new_inode->file_pointer, parent_directory->name);

	new_inode->parent_node = parent_directory;
	new_inode->child_node = NULL;
	new_inode->sibling_node = NULL;
	if (__root_directory != NULL) {
		if (parent_directory->child_node == NULL) {
			parent_directory->child_node = new_inode;
		} else {
			if (parent_directory->child_node->sibling_node == NULL) {
				parent_directory->child_node->sibling_node = new_inode;
			} else {
				index_node *youngest_sibling = parent_directory->child_node;
				while (youngest_sibling->sibling_node != NULL) {
					youngest_sibling = youngest_sibling->sibling_node;
				};
				youngest_sibling->sibling_node = new_inode;
			};
		};
	} else {
		panic("ERROR: Null Root Directory\n");
	};
}

void __delete_file(index_node *file, index_node *parent_directory) {
	if (diagnostics_mode) printf("KERNEL: Deleting File \"%s\"\n", file->name);

	if (file->mode == DIRECTORY_INODE) panic("ERROR: Pathname to Directory not File");

	if (parent_directory->child_node == file) {
		parent_directory->child_node = file->sibling_node;
	} else {
		index_node *previous_sibling = parent_directory->child_node;
		while (previous_sibling->sibling_node != file) {
			previous_sibling = previous_sibling->sibling_node;
			if (previous_sibling == NULL) panic("ERROR: Bad Directory\n");
		};
		previous_sibling->sibling_node = file->sibling_node;
	};

	file->file_pointer = __deallocate_SRAM(file->file_pointer);
	file = __deallocate_SRAM(file);
}

// ### FILE -- DIRECROTRY OPERATIONS
void __create_directory(char *directory_name, index_node *parent_directory) {
	index_node *new_inode = (index_node *) __allocate_SRAM(sizeof(index_node), OPERATING_SYSTEM_DATA);
	if (new_inode == NULL) panic("ERROR: SRAM Allocation Failed\n");
	
	strncpy(new_inode->name, directory_name, BERDOS_INODE_NAME_SIZE);
	new_inode->name[BERDOS_INODE_NAME_SIZE] = '\0';
	new_inode->mode = DIRECTORY_INODE;
	new_inode->file_pointer = NULL;

	if (diagnostics_mode) printf("KERNEL: Creating Directory \"%s\" @ \"%s\"\n", new_inode->name, parent_directory->name);

	new_inode->parent_node = parent_directory;
	new_inode->child_node = NULL;
	new_inode->sibling_node = NULL;
	if (__root_directory != NULL) {
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

void __delete_directory(index_node *directory, index_node *parent_directory) {
	if (diagnostics_mode) printf("KERNEL: Deleting Dirrectory \"%s\"\n", directory->name);

	if (directory->mode == FILE_INODE) panic("ERROR: Pathname to File not Directory\n");

	if (parent_directory->child_node == directory) {
		parent_directory->child_node = directory->sibling_node;
	} else {
		index_node *previous_sibling = parent_directory->child_node;
		while (previous_sibling->sibling_node != directory) {
			previous_sibling = previous_sibling->sibling_node;
			if (previous_sibling == NULL) panic("ERROR: Bad Directory\n");
		};
		previous_sibling->sibling_node = directory->sibling_node;
	};

	while (directory->child_node != NULL) {
		if (directory->child_node->mode == FILE_INODE) {
			__delete_file(directory->child_node, directory);
		} else {
			__delete_directory(directory, parent_directory);
		};
	};

	directory = __deallocate_SRAM(directory);
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

// ### PROCESS -- UTILITY FUNCTIONS
static control_block *__get_process_by_id_number(unsigned int node_id_number) {
	control_block *process = __head;
	while (process != NULL) {
		if (process->id_number == node_id_number) {
			return process;
		};
		process = process->next_node;
	};
	return NULL;
}

// ### PROCESS -- STATE CHANGING
control_block *__create_process(void (*function_pointer)(), void *arguments, control_block *parent_process) {
	unsigned int *starting_arguments = arguments;
	static unsigned int new_id_number;

	if (diagnostics_mode) printf("KERNEL: Creating Process #%d \n", new_id_number);

	if (process_count >= BERDOS_PROCESS_LIMIT) panic("ERROR: Control Block Overflow\n");

	control_block *new_process = (control_block *) __allocate_SRAM(sizeof(control_block), OPERATING_SYSTEM_DATA);
	if (new_process == NULL) panic("ERROR: SRAM Allocation Failed\n");
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

	new_process->parent_node = parent_process;
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

void __terminate_process(control_block *process){
	if (diagnostics_mode) printf("KERNEL: Terminating Process #%d @0x%X §%d\n", process->id_number, process, process->status);

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

	if (process->parent_node->child_node == process) {
		process->parent_node->child_node = process->sibling_node;
	} else {
		control_block *previous_sibling = process->parent_node->child_node;
		while (previous_sibling->sibling_node != process) {
			previous_sibling = previous_sibling->sibling_node;
			if (previous_sibling == NULL) panic("ERROR: Unexpected Process Hierarchy\n");
		};
		previous_sibling->sibling_node = process->sibling_node;
	};
	while (process->child_node != NULL) {
		__terminate_process(process->child_node);
	};

	process->address_space = __deallocate_SRAM(process->address_space);
	process = __deallocate_SRAM(process);

	process_count--;
}

static void __execute_process(control_block *process) {
	if (diagnostics_mode) printf("KERNEL: Executing Process #%d @0x%X §%d &%d\n", process->id_number, process, process->status, get_core_num());
	
	if (process->status == READY) {
		process->status = EXECUTING;

		mutex_exit(&os_lock);
		process->address_space->stack_pointer = __context_switch(process->address_space->stack_pointer);
		mutex_enter_blocking(&os_lock);
	};
}

static void __block_process(control_block *process) {
	if (diagnostics_mode) printf("KERNEL: Blocking Process #%d @0x%X §%d\n", process->id_number, process, process->status);
	
	if (process->status == READY) {
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
	if (diagnostics_mode) printf("KERNEL: Readying Process #%d @0x%X §%d\n", process->id_number, process, process->status);

	if (process->status == BLOCKED || process->status == CREATED) {
		ready_queue[ready_queue_index] = process;
		ready_queue_index++;

		process->status = READY;
	};
}

// ## KERNEL OPERATION
// ### KERNEL - ROOT PROCESS
void __idle(void) {
	while (true) {os_yield();};
}

// ### KERNEL - SYSTEM CALL HANDLERS
void __exit(control_block *caller_process) {
	__terminate_process(caller_process);
}

unsigned int __spawn(void (*function_pointer)(), unsigned int *starting_arguments, control_block *caller_process) {
	return __create_process(function_pointer, starting_arguments, caller_process)->id_number;
}

void __mkdir(char *pathname, char *directory_name) {
	index_node *parent_directory = __parse_pathname(pathname);
	if (parent_directory == NULL) panic("ERROR: Bad Pathname\n");
	__create_directory(directory_name, parent_directory);
}

void __rmdir(char *pathname) {
	index_node *directory = __parse_pathname(pathname);
	if (directory == NULL) panic("ERROR: Bad Pathname\n");
	__delete_directory(directory, directory->parent_node);
}

void __create(char *file_name, size_t file_size, char *pathname) {
	index_node *parent_directory = __parse_pathname(pathname);
	if (parent_directory == NULL) panic("ERROR: Bad Pathname\n");
	__create_file(file_name, file_size, parent_directory);
}

void __delete(char *pathname) {
	index_node *file = __parse_pathname(pathname);
	if (file == NULL) panic("ERROR: Bad Pathname\n");
	__delete_file(file, file->parent_node);
}

index_node *__open(char *pathname) {
	index_node *inode = __parse_pathname(pathname);
	if (inode == NULL) return NULL;
	return inode;
}

void __read(char *pathname, char *buffer, size_t count) {
	index_node *file = __parse_pathname(pathname);
	if (file == NULL) panic("ERROR: Bad Pathname\n");

	if (diagnostics_mode) printf("KERNEL: Reading File \"%s\"\n", file->name);
	strncpy(buffer, file->file_pointer, count);
}

void __write(char *pathname, const char *buffer, size_t count) {
	index_node *file = __parse_pathname(pathname);
	if (file == NULL) panic("ERROR: Bad Pathname\n");
	if (count > file->size) panic("ERROR: Buffer Underflow\n");

	if (diagnostics_mode) printf("KERNEL: Writing to File \"%s\"\n", file->name);
	strncpy(file->file_pointer, buffer, count);
}

void __system_call_handler(control_block *caller_process) {
	unsigned int result = hardware_frame_pointer[0];

	switch (system_call_number) {
		case YIELD: break;
		case EXIT: __exit(caller_process); break;
		case SPAWN: result = (uint) __spawn((void *)hardware_frame_pointer[0], (uint *)hardware_frame_pointer[1], caller_process); break;
		case MKDIR: __mkdir((char *)hardware_frame_pointer[0], (char *)hardware_frame_pointer[1]); break;
		case RMDIR: __rmdir((char *)hardware_frame_pointer[0]); break;
		case CREATE: __create((char *)hardware_frame_pointer[0], (size_t) hardware_frame_pointer[1], (char *)hardware_frame_pointer[2]); break;
		case DELETE: __delete((char *)hardware_frame_pointer[0]); break;
		case READ: __read((char *)hardware_frame_pointer[0], (char *)hardware_frame_pointer[1], (size_t) hardware_frame_pointer[2]); break;
		case WRITE: __write((char *)hardware_frame_pointer[0], (char *)hardware_frame_pointer[1], (size_t) hardware_frame_pointer[2]); break;
		case OPEN: result = (uint) __open((char *)hardware_frame_pointer[0]); break;
		default: break;
	};
	
	hardware_frame_pointer[0] = result;
	system_call_number = 0;
}

// ### KERNEL -- SCHEDULING
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

	if (system_call_number != 0) __system_call_handler(process);

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

static void __short_term_scheduler(void) {
	if (diagnostics_mode) printf("KERNEL: Short-Term Scheduling\n");
	for (uint i = 0; i < ready_queue_index; i++) {
		if (ready_queue[i]->status == READY) __dispatcher(ready_queue[i]);

		if (ready_queue_index == 1) {
			mutex_exit(&os_lock);
			__asm__("NOP");
			__asm__("NOP");
			mutex_enter_blocking(&os_lock);
		};
	};
}

static void __medium_term_scheduler(void) {
	if (diagnostics_mode) printf("KERNEL: Medium-Term Scheduling\n");
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
	if (diagnostics_mode) printf("KERNEL: Long-Term Scheduling\n");
	if (process_count == 0) {__create_process(__idle, NULL, NULL);};
	if (process_count == 1) {unsigned int arguments[4] = {diagnostics_mode}; __create_process(__boot_function, arguments, __root);};

	control_block *process = __head;
	while (process != NULL) {
		if (process->status == CREATED) {
			__ready_process(process);
		};
		process = process->next_node;
	};
}

// ### KERNEL - START-UP
void kernel_initizalize(void (*boot)(unsigned int)) {
	if (get_core_num() == 0) {
		printf("> Diagnostic Mode? (Y/N)\n");
		char c = getchar();
		if (c == 'y' || c == 'Y') diagnostics_mode = 1;
	
		process_count = 0;
		memory_utilization = 0;
		scheduling_dicipline = BERDOS_DEFAULT_SCHEDULER;
	
	  	for (uint i = 0; i < (0x2A20 / BERDOS_PARTITION_SIZE) + 1; i++) bitmap[i] = OPERATING_SYSTEM_DATA;
	  	for (uint i = 0; i < (0x1F40 / BERDOS_PARTITION_SIZE) + 1; i++) bitmap[BERDOS_PARTITION_COUNT - 1 - i] = OPERATING_SYSTEM_DATA;
	
	  	__head = NULL;
		__tail = NULL;
		__root = NULL;
		__boot_function = boot;
	
	  	ready_queue_index = 0;
		job_queue_index = 0;
		device_queue_index = 0;

		mutex_init(&os_lock);
		
		unsigned int arguments[4] = {diagnostics_mode};
		__create_directory(":)", NULL);
		__create_process(__idle, NULL, NULL);
		__create_process(__boot_function, arguments, __root);
	};

	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET), M0PLUS_SHPR2_BITS);
	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET), M0PLUS_SHPR3_BITS);

	unsigned int dummy[32];
	__initialize_main_stack(&dummy[32]);
}

void __kernel_start_core1(void) {
	kernel_initizalize(NULL);
	kernel_start();
}

void kernel_start(void) {
#if BERDOS_MULTICORE
	if (get_core_num() == 0) multicore_launch_core1(&__kernel_start_core1);
#endif

	mutex_enter_blocking(&os_lock);
	while (true) {
		if (diagnostics_mode) print_diagnostics();
		__long_term_scheduler();
		__medium_term_scheduler();
		__short_term_scheduler();
	};
}
