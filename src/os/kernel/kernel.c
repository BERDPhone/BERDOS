/* The file "kernel.c" contains the operating system's kernel. The kernel manages (1) processes/threads, 
(2) memory, (3) input and output, and (4) files. In so doing the kernel (1) facillitates software-hardware 
interactions and (2) controls all hardware */

#include "pico/stdlib.h"
#include <stdio.h>
#include "kernel.h"
#include "malloc.h"
#include <string.h>


unsigned int *__initialize_context_switch(void (*pointer_to_task_function)(void)) {
	return 0;
}

process* __head = NULL;
uint process_count = 0;

void kerenl_initialize() {
	__systick_initialize();
}

void kernel_start() {
	__round_robin_scheduler();
}

void __round_robin_scheduler(void) {
	while (true) {
 		process *current_node = __head;
 		while (current_node != NULL) {
 			kernel_execute_process(current_node->identification);
			current_node = current_node->next;
 		}
 	}
}

uint kernel_create_process(void (*pointer_to_process)(void)) {
	uint new_identification = __compute_id();
	uint new_priority = __compute_priority(new_identification);

	process *new_node = malloc(sizeof(process));
	new_node->function_pointer = pointer_to_process;
	new_node->priority = new_priority;
	new_node->identification = new_identification;
	new_node->status = READY;
	new_node->next = NULL;

	new_node->stack_words += BDOS_STACK_SIZE - 17;
	new_node->stack_words[8] = 0xFFFFFFFD; // EXC_RETURN in LR
	new_node->stack_words[15] = (unsigned int) pointer_to_process; // Process Pointer in PC
	new_node->stack_words[16] = 0x01000000; // Thumb Bit in EPSR
	new_node->stack_words = __initialize_context_switch(new_node->stack_words);
}

void kernel_execute_process(uint node_identification) {
	process *current_node = get_process_by_id(node_identification);
	current_node->status = RUNNING;

	__initialize_context_switch(current_node->function_pointer);
}

void kernel_hault_process(uint node_identification) {
	process *current_node = get_process_by_id(node_identification);
	current_node->status = READY;
}

void kernel_block_process(uint node_identification) {
	process *current_node = get_process_by_id(node_identification);
	current_node->status = BLOCKED;
}

void kernel_unblock_process(uint node_identification) {
	process *current_node = get_process_by_id(node_identification);
	current_node->status = READY;
}

bool kernel_terminate_process_by_id(uint node_identification) {
	process *current_node = __head;

	while (current_node != NULL && current_node->next != NULL) {
		if (node_identification == current_node->next->identification) {
			process *new_next = current_node->next->next;
			free(current_node->next);
			current_node->next = new_next;
			return true;
		}
		current_node->status = TERMINATED;
		current_node = current_node->next;
	}
	return false;
}

bool kernel_terminate_process_by_pointer(void (*pointer_to_task_function)(void)) {
	process *current_node = __head;

	while (current_node != NULL && current_node->next != NULL) {
		if (pointer_to_task_function == current_node->next->function_pointer) {
			process *new_next = current_node->next->next;
			free(current_node->next);
			current_node->next = new_next;
			return true;
		}
		current_node->status = TERMINATED;
		current_node = current_node->next;
	}
	return false;
}

process *get_process_by_id(uint task_id) {
	process *current_node = __head;
	while (current_node != NULL) {
		if (task_id == current_node->identification) {
			return current_node;
		}
		current_node = current_node->next;
	}
	process_count -= 1;
	return NULL;
}

uint __compute_identification() {
	uint i;

	for (i = 0; i < process_count; i++) {
		bool is_match = false;
		process *current_node = __head;

		while (current_node != NULL) {
			if (current_node->identification == i) {
				is_match = true;
			}
			current_node = current_node->next;
		}

		if (is_match == false) {
			return i;
		}
	}
	return i + 1;
}

uint __compute_prioirty(uint node_identification) {

}

void list_all_tasks() {
	printf("Within list_all_tasks \n");

	/* Print all the elements in the linked list */
	process *current_node = __head;
	while ( current_node != NULL) {
		printf("Function pointer: ");
		unsigned char *cp = (unsigned char*)&current_node->function_pointer;
		for (int i=0;i<sizeof current_node->function_pointer; i++) {
			printf("[%08x]", cp[i]);
		}
		printf("\n");

		printf("Priority: %i \n", current_node->priority);
		printf("Process ID: %i \n", current_node->identification);
		current_node = current_node->next;
	}
}