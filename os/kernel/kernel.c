#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "kernel.h"
#include "../boot/boot.h"

#include "pico/stdlib.h"
#include "hardware/structs/systick.h"
#include "hardware/sync.h"
#include "malloc.h"

// # DATA & FUNCTION DECLARATIONS
// ## MEMORY MANAGMENT
unsigned int memory_utilization;

// ## PROCESS MANAGEMENT
// ### PROCESS -- FUNCTION DECLARATIONS
void __initialize_main_stack(unsigned int *MSP);
unsigned int *__context_switch(unsigned int *PSP);

// ### PROCESS -- GLOBAL DATA STRUCUTRES/VARAIBLES
control_block *__head = NULL;
control_block *__tail = NULL;
control_block *__root = NULL;

unsigned int process_count;
enum schedulers scheduling_dicipline;

static control_block *ready_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static control_block *job_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static control_block *device_queue[BERDOS_PROCESS_LIMIT] = {NULL};
static unsigned int ready_queue_index;
static unsigned int job_queue_index;
static unsigned int device_queue_index;

// # FUNCTION DEFINITIONS
// ## MEMORY MANAGMENT
void __swap_process_out(control_block *process) {
	if (process->status != SWAPPED_READY || process->status != SWAPPED_BLOCKED) {
		memory_utilization -= sizeof(address_space);

		if (process->status == READY) {
			process->status = SWAPPED_READY;
		} else if (process->status == BLOCKED) {
			process->status = SWAPPED_BLOCKED;
		};
	};
}

void *__swap_process_in(control_block *process) {
	if (process->status == SWAPPED_READY || process->status == SWAPPED_BLOCKED) {
		memory_utilization += sizeof(address_space);

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
void __disable_preemption(void) {
	systick_hw->csr = 0;    // Update Systick Control Status Register to Disable Timer and IRQ 
	__dsb();                
	__isb();                

	// Clear Systick Exception Pending Bit
	hw_set_bits  ((io_rw_32 *)(PPB_BASE + M0PLUS_ICSR_OFFSET),M0PLUS_ICSR_PENDSTCLR_BITS);
}

void __enable_preemption(unsigned int time_slice_duration) {
	__disable_preemption();

	systick_hw->rvr = (time_slice_duration) - 1UL;    // Set Systick Reload Value Register
	systick_hw->cvr = 0;   			// Set Systick Current Value Register; Clear Current Value
	systick_hw->csr = 0x03; 		// Set Systick Control Status Register; Enable IRQ and Clock
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

control_block *__get_executing_process(void) {
	control_block *process = __head;
	while (process != NULL) {
		if (process->status == EXECUTING) {
			return process;
		};
		process = process->next_node;
	};
	return NULL;
}

control_block *__get_parent_process(control_block *child_process) {
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
unsigned int create_process(void (*function_pointer)(), unsigned int *starting_arguments, control_block *parent_process) {
	static unsigned int new_id_number;

	printf("KERNEL: Creating Process #%d \n", new_id_number);

	if (process_count >= BERDOS_PROCESS_LIMIT) {
		printf("ERROR!");
		__asm__("BKPT");
	};

	control_block *new_process = malloc(sizeof(control_block));
	new_process->address_space = malloc(sizeof(address_space));
	new_process->process_pointer = function_pointer;
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

	return new_process->id_number;
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

	free(process);
	free(process->address_space);

	process_count--;
}

void __execute_process(control_block *process) {
	printf("KERNEL: Executing Process #%d @0x%X §%d\n", process->id_number, process, process->status);
	
	if (process->status == READY) {
		process->status = EXECUTING;
		process->address_space->stack_pointer = __context_switch(process->address_space->stack_pointer);
	} else {
		printf("ERROR!");
		__asm__("BKPT");
	};
}

void __block_process(control_block *process) {
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
	} else {
		printf("ERROR!");
		__asm__("BKPT");
	};
}

void __ready_process(control_block *process) {
	printf("KERNEL: Readying Process #%d @0x%X §%d\n", process->id_number, process, process->status);

	if (process->status == EXECUTING || process->status == BLOCKED || process->status == CREATED) {
		ready_queue[ready_queue_index] = process;
		ready_queue_index++;

		process->status = READY;
	} else {
		printf("ERROR!");
		__asm__("BKPT");
	};
}

// ### PROCESS -- SCHEDULING
void __bubble_sort_queue(control_block *array[], unsigned int index, size_t member_offset) {
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

void __dispatcher(control_block *process) {
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
			printf("UNEXPECTED PROCESS->STATUS\n");
			__asm__("BKPT");
	};
}

void __short_term_scheduler() {
	printf("KERNEL: Short-Term Scheduling\n");
	for (uint i = 0; i < ready_queue_index; i++) {
		__dispatcher(ready_queue[i]);
	};
}

void __medium_term_scheduler() {
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
			//__bubble_sort_ready_queue(offsetof(control_block, scheduling_parameter));
			break;
		case SHORTEST_REMAINING_TIME_NEXT:
			__enable_preemption(BERDOS_TIME_SLICE);
			//__bubble_sort_ready_queue(offsetof(control_block, scheduling_parameter));
			break;
	};

	for (uint i = 0; memory_utilization > PICO_SRAM_SIZE && i < device_queue_index; i++) {
		__swap_process_out(device_queue[i]);
	};

	for (uint i = ready_queue_index; memory_utilization > PICO_SRAM_SIZE && i >= 0; i--) {
		__swap_process_out(ready_queue[i]);
	};

	for (uint i = 0; i <= ready_queue_index; i++) {
		ready_queue[i]->address_space = __swap_process_in(ready_queue[i]);
	};
}

void __long_term_scheduler(void) {
	printf("KERNEL: Long-Term Scheduling\n");
	if (process_count == 0) {
		printf("ERROR!\n");
		__asm__("BKPT");
	};

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
	scheduling_dicipline = BERDOS_DEFAULT_SCHEDULER;

	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET), M0PLUS_SHPR2_BITS);
  	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET), M0PLUS_SHPR3_BITS);

  	unsigned int dummy[32];
  	__initialize_main_stack(&dummy[32]);

  	__head = NULL;
	__tail = NULL;
	__root = NULL;

  	ready_queue_index = 0;
	job_queue_index = 0;
	device_queue_index = 0;

	memory_utilization = 0;
}

void kernel_start(void) {
	while (true) {
		__long_term_scheduler();
		__medium_term_scheduler();
		__short_term_scheduler();
	};
}

// ### KERNEL - SYSTEM CALL HANDLERS
void __exit(void) {
	control_block *caller_process = __get_executing_process();
	terminate_process(caller_process, __get_parent_process(caller_process));
	return;
}

unsigned int __spawn(void (*function_pointer)(), unsigned int *starting_arguments) {
	return create_process(function_pointer, starting_arguments, __get_executing_process());
}