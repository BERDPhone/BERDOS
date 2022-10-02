
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include "hardware/structs/systick.h"
#include "hardware/sync.h"
#include "malloc.h"

#include "kernel.h"

process* __head = NULL;
uint process_count = 0;
uint i; // For Loops Index Variable


// # FUNCTIONS DECLARATIONS
// ### PROCESS -- SCHEUDLING ALGORITHS/SCHEDULERS
void __first_come_first_serve_scheduler(void);
void __round_robin_scheduler(void);

// ### PROCESS -- INTERNAL ALGORITHMS
void __disable_preemption(void);
void __enable_preemption(uint time_slice_duration);

// ### PROCESS -- CONTEXT SWITCHING
//uint *__piccolo_pre_switch(uint *R0);


// # FUNCTION DEFINITIONS
// ## PROCESS MANAGEMENT
// ### PROCESS -- ASSORTED ALGORITHMS
void __disable_preemption(void) {
    systick_hw->csr = 0;    // Update Systick Control Status Register to Disable Timer and IRQ 
    __dsb();                
    __isb();                

    // Clear Systick Exception Pending Bit
    hw_set_bits  ((io_rw_32 *)(PPB_BASE + M0PLUS_ICSR_OFFSET),M0PLUS_ICSR_PENDSTCLR_BITS);
}

void __enable_preemption(uint time_slice_duration) {
	__disable_preemption();

	systick_hw->rvr = (time_slice_duration) - 1UL;    // Set Systick Reload Value Register
    systick_hw->cvr = 0;   			// Set Systick Current Value Register; Clear Current Value
    systick_hw->csr = 0x03; 		// Set Systick Control Status Register; Enable IRQ and Clock
}

process *__get_process_by_id_number(uint node_id_number) {
	process *current_node = __head;
	while (current_node != NULL) {
		if (node_id_number == current_node->process_id_number) {
			return current_node;
		} current_node = current_node->next_node;
	} 
	return NULL;
}

uint __generate_new_id_number() {
	for (i = 0; i < process_count; i++) {
		bool match = false;
		process *current_node = __head;

		while (current_node != NULL) {
			if (current_node->process_id_number == i) {
				match = true;
			};
			current_node = current_node->next_node;
		};

		if (match == false) {
			return i;
		};
	};
	return i + 1;
}

// ### PROCESS -- STATE MANAGEMENT
uint create_process(uint *function_pointer) {
	if (process_count != BERDOS_PROCESS_LIMIT) {
		// Initializing the New Process's Node in the Process Table/Linked List
		process *new_node = malloc(sizeof(process));
		new_node->process_pointer = function_pointer;
		new_node->process_id_number = __generate_new_id_number();
		new_node->process_status = READY;
		new_node->process_stack_pointer = &new_node->process_stack[BERDOS_STACK_SIZE];
		new_node->next_node = NULL;

		// Initializing the New Process's Stack
		new_node->process_stack_pointer += -17;
		new_node->process_stack[8] = 0xFFFFFFFD; // EXC_RETURN in LR
		new_node->process_stack[15] = function_pointer; // Process Pointer in PC
		new_node->process_stack[16] = 0x01000000; // Thumb Bit in EPSR

		return new_node->process_id_number;
	} else {
		return -1;
	}
}

void execute_process(uint node_id_number) {
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == READY) {
		current_node->process_status = EXECUTING;
		current_node->process_stack_pointer = __piccolo_pre_switch(current_node->process_stack_pointer);
	};
}

void block_process(uint node_id_number) {
	process *current_node = __get_process_by_id_number(node_id_number);
	
	if (current_node->process_status == (EXECUTING || READY)) {
		current_node->process_status = BLOCKED;
	};
}

void ready_process(uint node_id_number) {
	process *current_node = __get_process_by_id_number(node_id_number);

	if (current_node->process_status == (EXECUTING || BLOCKED)) {
		current_node->process_status = READY;
	};
}

void terminate_process(uint node_id_number) {
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

// ### PROCESS -- SCHEUDLING ALGORITHS/SCHEDULERS
void __first_come_first_serve_scheduler(void) {
	__disable_preemption();

	while (true) {
		process *current_node = __head;

		for (i = 0; i < BERDOS_PROCESS_LIMIT ; i++) {
			if (current_node->process_status == READY) {
				execute_process(current_node->process_id_number);
			};
		};
	};
}

void __round_robin_scheduler(void) {
	__enable_preemption(BERDOS_TIME_SLICE);

	while (true) {
		process *current_node = __head;

		for (i = 0; i < BERDOS_PROCESS_LIMIT ; i++) {
			

			if (current_node->process_status == READY && current_node != NULL) {
				execute_process(current_node->process_id_number);
			};
		};
	};
}

// ## KERNEL OPERATION
// ### KERNEL -- START-UP
void kernel_initialize(void) {
}

void kernel_start(void) {
	while (true) {
		__first_come_first_serve_scheduler();
	}

}