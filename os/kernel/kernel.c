#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "kernel.h"

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

// ### PROCESS -- GLOBAL DATA STRUCUTRES/VARAIBLES
unsigned int new_id_number;
unsigned int process_count;
enum schedulers scheduling_dicipline;
unsigned int ready_queue[BERDOS_PROCESS_LIMIT];

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

// ### PROCESS -- STATE CHANGING
unsigned int *__create_process_stack(unsigned int *process_stack, void (*function_pointer)(void), uint parameter_count, int *starting_arguments) {
	uint i;
	process_stack += BERDOS_STACK_SIZE - 17; /* End of task_stack, minus what we are about to push */
  	process_stack[8] = (unsigned int) 0xFFFFFFFD; // EXC_RETURN in LR
  	process_stack[15] = (unsigned int) function_pointer; // Function Pointer in PC
  	process_stack[16] = (unsigned int) 0x01000000; // Thumb Bit in EPSR
  	for (i = 0; i < parameter_count; i++) {
  		process_stack[9] = (unsigned int) starting_arguments[0]; // Argument 0 in R0
  		process_stack[10] = (unsigned int) starting_arguments[1]; // Argument 1 in R1
  		process_stack[11] = (unsigned int) starting_arguments[2]; // Arugment 2 in R2
  		process_stack[12] = (unsigned int) starting_arguments[3]; // Arugment 3 in R2
  	};

  	return process_stack;
}

unsigned int create_process(void (*function_pointer)(), unsigned int parameter_count, ...) {
	printf("KERNEL: Creating Process %d \n", new_id_number);
	
	if (parameter_count > 4) {
		int parameter_count = 4; 
		printf("WARNING: Too Many Arguments \n");
	};
	int starting_arguments[parameter_count];

	uint i;
	va_list args;
	va_start(args, parameter_count);
	for (i = 0; i <= parameter_count - 1; i++) {
		starting_arguments[i] = va_arg(args, int);
	};
	va_end(args);

	process *new_node = malloc(sizeof(process));
	new_node->process_pointer = function_pointer;
	new_node->process_id_number = new_id_number;
	new_node->process_status = READY;
	new_node->process_stack_pointer = __create_process_stack(new_node->process_stack, function_pointer, parameter_count, starting_arguments);

	if (__tail == NULL && __head == NULL) {
		new_node->next_node = NULL;
		__tail = new_node;
		new_node->previous_node = NULL;
		__head = new_node;
	} else {
		new_node->previous_node = __tail;
		new_node->previous_node->next_node = new_node;
		new_node->next_node = NULL;
		__tail = new_node;
	};

	process_count++;
	new_id_number++;

	return new_node->process_id_number;
}

void execute_process(unsigned int node_id_number) {
	printf("KERNEL: Executing Process #%d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == (READY || EXECUTING)) {
		//current_node->process_status = EXECUTING;
		current_node->process_stack_pointer = __piccolo_pre_switch(current_node->process_stack_pointer);
	};
}

void block_process(unsigned int node_id_number) {
	printf("KERNEL: Blocking Process %d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == (EXECUTING || READY)) {
		current_node->process_status = BLOCKED;
	};
}

void ready_process(unsigned int node_id_number) {
	printf("KERNEL: Readying Process %d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);

	if (current_node->process_status == (EXECUTING || BLOCKED)) {
		current_node->process_status = READY;
	};
}

void terminate_process(unsigned int node_id_number){
	printf("KERNEL: Terminating Process %d \n", node_id_number);
	process *current_node = __get_process_by_id_number(node_id_number);

	if (true) {
		current_node->process_status = TERMINATED;
	};

	current_node->previous_node->next_node = current_node->next_node;
	current_node->next_node->previous_node = current_node->previous_node;
	if (current_node == __head) {
		__head = current_node->next_node;
	};
	if (current_node == __tail) {
		__tail = current_node->previous_node;
	};

	process_count--;
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
			};
			current_node = current_node->next_node;
		};
	};
}

void __round_robin_scheduler(void) {
	__enable_preemption(BERDOS_TIME_SLICE);	

	uint i;
	process *current_node = __head;
	while (current_node != NULL) {
		for (i = 0; i < process_count; i++) {
			if (current_node->process_status == READY) {
				ready_queue[i] = current_node->process_id_number;
			};
			current_node = current_node->next_node;
		};
	};
}

void __shortest_job_next_scheduler(void) {
	//__disable_preemption();
		
}

void __shortest_job_remaining_scheduler(void) {
	//__enable_preemption(BERDOS_TIME_SLICE);
}

void __long_term_scheduler(void) {}

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
			execute_process(node_id_number);
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
}

void kernel_start(void) {
	uint i;
	while (true) {
		//__long_term_scheduler();
		__short_term_scheduler(scheduling_dicipline);
		__dispatcher();
	};
}