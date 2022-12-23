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
// ## PROCESS MANAGEMENT
// ### PROCESS -- FUNCTION DECLARATIONS
void __piccolo_task_init_stack(unsigned int *R0);
unsigned int *__piccolo_pre_switch(unsigned int *R0);

// ### PROCESS -- DOUBLY LINKED LIST
process *__head = NULL;
process *__tail = NULL;
process *__root = NULL;

// ### PROCESS -- GLOBAL DATA STRUCUTRES/VARAIBLES
unsigned int new_id_number;
unsigned int process_count;
enum schedulers scheduling_dicipline;

static unsigned int ready_queue[BERDOS_PROCESS_LIMIT];
static unsigned int job_queue[BERDOS_PROCESS_LIMIT];
static unsigned int device_queue[BERDOS_PROCESS_LIMIT];
static unsigned int ready_queue_index;
static unsigned int job_queue_index;
static unsigned int device_queue_index;

// # FUNCTION DECLARATIONS
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
process *__get_process_by_id_number(unsigned int node_id_number) {
	process *current_node = __head;
	while (current_node != NULL) {
		if (current_node->process_id_number == node_id_number) {
			return current_node;
		};
		current_node = current_node->next_node;
	};
	return NULL;
}

process *__get_executing_process(void) {
	process *current_node = __head;
	while (current_node != NULL) {
		if (current_node->process_status == EXECUTING) {
			return current_node;
		};
		current_node = current_node->next_node;
	};
	return NULL;
}

process *__get_parent_process(process *child_process) {
	process *current_node = __head;
	while (current_node != NULL) {
		if (current_node->child_node == child_process) {
			return current_node;
		};
		if (current_node->sibling_node == child_process) {
			return __get_parent_process(current_node);
		};
		current_node = current_node->next_node;
	};
	return NULL;
}

// ### PROCESS -- STATE CHANGING
unsigned int create_process(void (*function_pointer)(), unsigned int *starting_arguments, process *parent_node) {
	printf("KERNEL: Creating Process #%d \n", new_id_number);

	if (process_count >= BERDOS_PROCESS_LIMIT) {
		printf("ERROR!");
		__asm__("BKPT");
	};

	process *new_node = malloc(sizeof(process));
	new_node->process_pointer = function_pointer;
	new_node->process_id_number = new_id_number;
	new_node->process_status = READY;
	new_node->process_stack[(BERDOS_STACK_SIZE - 17) + 8] = (unsigned int) 0xFFFFFFFD; // EXC_RETURN in LR
  	new_node->process_stack[(BERDOS_STACK_SIZE - 17) + 15] = (unsigned int) function_pointer; // Function Pointer in PC
  	new_node->process_stack[(BERDOS_STACK_SIZE - 17) + 16] = (unsigned int) 0x01000000; // Thumb Bit in EPSR
  	new_node->process_stack[(BERDOS_STACK_SIZE - 17) + 9] = (unsigned int) starting_arguments[0]; // Argument 0 in R0
  	new_node->process_stack[(BERDOS_STACK_SIZE - 17) + 10] = (unsigned int) starting_arguments[1]; // Argument 1 in R1
  	new_node->process_stack[(BERDOS_STACK_SIZE - 17) + 11] = (unsigned int) starting_arguments[2]; // Arugment 2 in R2
  	new_node->process_stack[(BERDOS_STACK_SIZE - 17) + 12] = (unsigned int) starting_arguments[3]; // Arugment 3 in R2
	new_node->process_stack_pointer = new_node->process_stack + BERDOS_STACK_SIZE - 17;

	if (__tail == NULL && __head == NULL) {
		new_node->next_node = NULL;
		new_node->previous_node = NULL;
		__head = new_node;
		__tail = new_node;
	} else {
		new_node->previous_node = __tail;
		new_node->previous_node->next_node = new_node;
		new_node->next_node = NULL;
		__tail = new_node;
	};

	new_node->child_node = NULL;
	new_node->sibling_node = NULL;
	if (__root != NULL) {
		if (parent_node->child_node == NULL) {
			parent_node->child_node = new_node;
		} else {
			if (parent_node->child_node->sibling_node == NULL) {
				parent_node->child_node->sibling_node = new_node;
			} else {
				new_node->sibling_node = parent_node->child_node->sibling_node;
				parent_node->child_node->sibling_node = new_node;
			};
		};
	} else {
		__root = new_node;
	};

	job_queue[job_queue_index] = new_id_number;
	job_queue_index++;

	process_count++;
	new_id_number++;

	return new_node->process_id_number;
}

void terminate_process(unsigned int node_id_number, process *parent_node){
	printf("KERNEL: Terminating Process #%d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);

	current_node->process_status = TERMINATED;

	uint i;
	uint j;
	for (i = 0; i < BERDOS_PROCESS_LIMIT; i++) {
		if (job_queue[i] == node_id_number) {
			for (j = i; j < BERDOS_PROCESS_LIMIT; j++) {
				job_queue[j] = job_queue[j + 1];
			};
			job_queue[BERDOS_PROCESS_LIMIT - 1] = 0;
			job_queue_index--;
		};
		if (ready_queue[i] == node_id_number) {
			for (j = i; j < BERDOS_PROCESS_LIMIT; j++) {
				ready_queue[j] = ready_queue[j + 1];
			};
			ready_queue[BERDOS_PROCESS_LIMIT - 1] = 0;
			ready_queue_index--;
		};
		if (device_queue[i] == node_id_number) {
			for (j = i; j < BERDOS_PROCESS_LIMIT; j++) {
				device_queue[j] = device_queue[j + 1];
			};
			device_queue[BERDOS_PROCESS_LIMIT - 1] = 0;
			device_queue_index--;
		};
	};

	current_node->previous_node->next_node = current_node->next_node;
	current_node->next_node->previous_node = current_node->previous_node;
	if (current_node == __head) {
		__head = current_node->next_node;
	};
	if (current_node == __tail) {
		__tail = current_node->previous_node;
	};

	if (parent_node->child_node == current_node) {
		parent_node->child_node = current_node->sibling_node;
	} else {
		process *previous_sibling = parent_node->child_node;
		if (previous_sibling->sibling_node != current_node) {
			previous_sibling = previous_sibling->sibling_node;
		};
		previous_sibling->sibling_node = current_node->sibling_node;
	};
	while (current_node->child_node != NULL) {
		terminate_process(current_node->child_node->process_id_number, current_node);
	};

	free(current_node);

	process_count--;
}

void __execute_process(unsigned int node_id_number) {
	printf("KERNEL: Executing Process #%d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == EXECUTING || current_node->process_status == READY) {
		current_node->process_status = EXECUTING;
		current_node->process_stack_pointer = __piccolo_pre_switch(current_node->process_stack_pointer);
	} else {
		printf("ERROR!");
		__asm__("BKPT");
	};
}

void __block_process(unsigned int node_id_number) {
	printf("KERNEL: Blocking Process #%d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == EXECUTING || current_node->process_status == READY) {
		current_node->process_status = BLOCKED;
	} else {
		printf("ERROR!");
		__asm__("BKPT");
	};
}

void __ready_process(unsigned int node_id_number) {
	printf("KERNEL: Readying Process #%d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);

	if (current_node->process_status == EXECUTING || current_node->process_status == BLOCKED) {
		current_node->process_status = READY;
	} else {
		printf("ERROR!");
		__asm__("BKPT");
	};
}

// ### PROCESS -- SCHEDULING
void __first_come_first_served_scheduler(void) {
	__disable_preemption();

	uint i;
	process *current_node = __head;
	while (current_node != NULL) {
		for (i = 0; i < process_count; i++) {
			if (current_node->process_status == READY) {
				ready_queue[i] = current_node->process_id_number;
				ready_queue_index++;
			};
			current_node = current_node->next_node;
		};
	};
}

void __round_robin_scheduler(void) {
	__first_come_first_served_scheduler();

	__enable_preemption(BERDOS_TIME_SLICE);
}

void __shortest_job_next_scheduler(void) {
	__first_come_first_served_scheduler();

	//__bubble_sort(ready_queue);

	__disable_preemption();
}

void __shortest_job_remaining_scheduler(void) {
	__shortest_job_next_scheduler();

	__enable_preemption(BERDOS_TIME_SLICE);
}

void __long_term_scheduler(void) {
	if (process_count == 0) {
		printf("ERROR!");
		__asm__("BKPT");
	};
}

void __short_term_scheduler(schedulers selected_dicipline) {
	switch (selected_dicipline) {
		case FIRST_COME_FIRST_SERVED:
			__first_come_first_served_scheduler();
			break;
		case ROUND_ROBIN:
			__round_robin_scheduler();
			break;
		case SHORTEST_JOB_NEXT:
			__shortest_job_next_scheduler();
			break;
		case SHORTEST_REMAINING_TIME_NEXT:
			__shortest_job_remaining_scheduler();
			break;
	};
}

void __dispatcher(void) {
	uint i;
	for (i = 0; i < process_count; i++) {
		unsigned int node_id_number = ready_queue[i];
		__execute_process(node_id_number);
		if (__get_process_by_id_number(node_id_number) != NULL) {
			__ready_process(node_id_number);
		} else {
			return;
		};
	};
}

// ## KERNEL OPERATION
// ### KERNEL - START-UP
void kernel_initizalize(void) {
	unsigned int process_count = 0;
	unsigned int new_id_number = 0;
	scheduling_dicipline = BERDOS_DEFAULT_SCHEDULER;

	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR2_OFFSET), M0PLUS_SHPR2_BITS);
  	hw_set_bits((io_rw_32 *)(PPB_BASE + M0PLUS_SHPR3_OFFSET), M0PLUS_SHPR3_BITS);

  	unsigned int dummy[32];
  	__piccolo_task_init_stack(&dummy[32]);

  	unsigned int job_queue[] = {0};
  	unsigned int ready_queue[] = {0};
  	unsigned int device_queue[] = {0};

  	ready_queue_index = 0;
	job_queue_index = 0;
	device_queue_index = 0;
}

void kernel_start(void) {
	while (true) {
		__long_term_scheduler();
		__short_term_scheduler(scheduling_dicipline);
		__dispatcher();
	};
}

// ### KERNEL - SYSTEM CALL HANDLERS
void __exit(void) {
	process *caller_process = __get_executing_process();
	terminate_process(caller_process->process_id_number, __get_parent_process(caller_process));
	return;
}

unsigned int __fork(void (*function_pointer)(), unsigned int *starting_arguments) {
	return create_process(function_pointer, starting_arguments, __get_executing_process());
}