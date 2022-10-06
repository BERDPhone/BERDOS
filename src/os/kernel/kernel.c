#include "kernel.h"

#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include "hardware/structs/systick.h"
#include "hardware/sync.h"
#include "malloc.h"

void __piccolo_task_init_stack(unsigned int *R0);
unsigned int *__piccolo_pre_switch(unsigned int *R0);
void blink1(void);
void blink2(void);

process *__head = NULL;
unsigned int process_count;
unsigned int ready_queue[BERDOS_PROCESS_LIMIT];
enum schedulers scheduling_dicipline;

unsigned int *__create_process_stack(unsigned int *process_stack, void (*function_pointer)(void)) {
		process_stack += BERDOS_STACK_SIZE - 17; /* End of task_stack, minus what we are about to push */
  	process_stack[8] = (unsigned int) 0xFFFFFFFD; // EXC_RETURN in LR
  	process_stack[15] = (unsigned int) function_pointer; // Function Pointer in PC
  	process_stack[16] = (unsigned int) 0x01000000; // Thumb Bit in EPSR

  	return process_stack;
}

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
		if (node_id_number == current_node->process_id_number) {
			return current_node;
		} current_node = current_node->next_node;
	} 
	return NULL;
}

unsigned int create_process(void (*function_pointer)(void)) {
	if (process_count < BERDOS_PROCESS_LIMIT) {
		// Initializing the New Process's Node in the Process Table/Linked List
		process *new_node = malloc(sizeof(process));
		new_node->process_pointer = function_pointer;
		new_node->process_id_number = 0;
		new_node->process_status = READY;
		new_node->next_node = NULL;
		new_node->process_stack_pointer = __create_process_stack(new_node->process_stack, function_pointer);

		process_count++;

		return new_node->process_id_number;
	} else {
		return -1;
	};
}

void execute_process(unsigned int node_id_number) {
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == READY) {
		current_node->process_status = EXECUTING;
		current_node->process_stack_pointer = __piccolo_pre_switch(current_node->process_stack_pointer);
	};
}

void block_process(unsigned int node_id_number) {
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == (EXECUTING || READY)) {
		current_node->process_status = BLOCKED;
	};
}

void ready_process(unsigned int node_id_number) {
	process *current_node = __get_process_by_id_number(node_id_number);

	if (current_node->process_status == (EXECUTING || BLOCKED)) {
		current_node->process_status = READY;
	};
}

void terminate_process(unsigned int node_id_number){
	process *current_node = __head;

	if (node_id_number == current_node->next_node->process_id_number) {
		process *new_next_node = current_node->next_node->next_node;
		free(current_node->next_node);
		current_node->next_node = new_next_node;
	};

	if (true) {
		current_node->process_status = TERMINATED;
	};
}

void __first_come_first_served_scheduler(void) {
	__disable_preemption();

	unsigned int j_start = 0;
	unsigned int j;
	unsigned int i;

	for (i = 0; i > BERDOS_PROCESS_LIMIT; i++) {
		for (j = j_start; j > BERDOS_PROCESS_LIMIT; j++) {
			process *current_node = __get_process_by_id_number(j);
			if (current_node->process_status = READY && current_node != NULL) {
				j_start = j;
				ready_queue[i] = j;
			};
		};
	};
}

void __round_robin_scheduler(void) {
		__enable_preemption(BERDOS_TIME_SLICE);
		
}

void __shortest_job_next_scheduler(void) {
		__disable_preemption();
		
}

void __shortest_job_remaining_scheduler(void) {
		__enable_preemption(BERDOS_TIME_SLICE);
}

void __scheduler(schedulers schedulering_dicipline) {
	// do stuff to ready queue

	switch (scheduling_dicipline) {
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

void __dispatcher(unsigned int node_id_number) {
	unsigned int dummy[32];
  __piccolo_task_init_stack(&dummy[32]);
  blink2();
	execute_process(node_id_number);
}

void kernel_initizalize(void) {
	enum schedulers scheduling_dicipline = BERDOS_DEFAULT_SCHEDULER;
	process *__head = NULL;
	unsigned int process_count = 0;
}

void kernel_start(void) {
	while (true) {
		__scheduler(scheduling_dicipline);
		__dispatcher(ready_queue[0]);
	}
}
