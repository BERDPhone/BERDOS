/* kernel.c manages the proccess and the safty of those proccess
i.e. prevent crashing of entire system from process accessing wrong memory or null. */

#include "pico/stdlib.h"
#include <stdio.h>
#include "kernel.h"
#include "malloc.h"
#include <string.h>

process* __head = NULL;
uint process_count = 0;

uint __compute_id(void);
uint __priority_scheduler(void);

void kernel_initalize() {
	// stdio_init_all();
	printf("Kernel: Kernal initaizing.\n");
}

// void kernel_start() {
// 	// stdio_init_all();
// 	printf("Kernel: Kernal starting.\n");
// 	while (1) {
// 		process *current_node = __head;
// 		while ( current_node != NULL) {
// 			current_node->function_pointer();
// 			current_node = current_node->next;
// 		}
// 	}
// }

void kernel_start() {
	uint run_id = __priority_scheduler();
	process *running_node = get_process_by_id(run_id);
	
	running_node->status = RUNNING;
	running_node->function_pointer();
}

uint __priority_scheduler(void) {
	int i;
	uint king_prioirty = 0;
	
	process *current_node = __head;
	uint king_id = current_node->identification;
	
	while (current_node != NULL) {
		for (uint i = 0; i < process_count; i++) {
			uint current_priority = current_node->priority;

			if (current_priority > king_prioirty) {
				king_prioirty = current_priority;
				king_id = current_node->identification;
			}
		}
		current_node = current_node->next;
	}
	return king_id;
}

double __compute_priority(uint current_node_id) { 
	
	for (uint i = 0; i < process_count; i++) {
		double current_node_prioirty;
	}




}

/* "kernel_create_process()" replaces the NULL node with an additional node in the "process" linked list. 
The function returns the unsigned "id" integer of the created process. Each node represents a 
process. */
uint kernel_create_process(void (*pointer_to_task_function)(void), int necessity, mode running) {
	printf("Kernel: Creating a kernel process.\n");

	uint new_id = __compute_id();
	uint new_priority = __compute_priority(new_id);
	/* The following five lines of code create a new node in the "process" linked list, assign sufficent 
	memmory, and define the variables of the "process" linked list/structure. */
	process *new_node = malloc(sizeof(process));
	new_node->function_pointer = pointer_to_task_function;
	new_node->priority = new_priority;
	new_node->identification = new_id;
	new_node->status = READY;
	new_node->next = NULL;

	/* If the "__head" instance of the "process" linked list/structure equals NULL, the linked list contains 
	no processes or nodes, and the "new_node" structure redefines the "__head" structure. Otherwise, the 
	code locates the last node in the linked list/structure and adds the "new_node" strucutre on the end. */
	if(__head != NULL) {
		process *last_node = __head;
		while(last_node->next != NULL) {
			last_node = last_node->next;
		}
	
		last_node->next = new_node;
	} else {
		__head = new_node;
	}
	process_count += 1;
	return new_id;
}

uint __compute_id(void) {
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

bool kernel_kill_process_by_pointer(void (*pointer_to_task_function)(void)) {
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

bool kernel_kill_process_by_id(uint task_id) {
	process *current_node = __head;
	while (current_node != NULL && current_node->next != NULL) {
		if (task_id == current_node->next->identification) {
			process *new_next = current_node->next->next;
			free(current_node->next);
			current_node->next = new_next;
			return true;
		}
		current_node->status = TERMINATED;
		current_node = current_node->next;
	}
	process_count -= 1;
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
	return false;
}

process *get_process_by_index(uint index) {
	process *current_node = __head;

	for (int i = 0; i < index; i++) {
		if (current_node != NULL) {
			current_node = current_node->next;
		} else {
			return NULL;
		}
	}
	return current_node;
}