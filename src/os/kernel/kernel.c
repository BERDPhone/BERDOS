/*
kernel.c manages the proccess and the safty of those proccess
i.e. prevent crashing of entire system from process accessing wrong memory or null.
*/

#include "pico/stdlib.h"
#include <stdio.h>
#include "kernel.h"
#include "malloc.h"
#include <string.h>

// Creation of linked list
/*
|-----------------|     |-----------------|    |-----------------|
|	head | next	  | ->  | new node | next | -> | new node | next | -> NULL
|-----------------|     |-----------------|    |-----------------|

kernel_create_process() replaces the NULL with the an additional object.
*/

/* This code initializes the "__task" structure whose instances represent nodes of a linked list and a
process executed in the central processing unit (CPU). */
typedef struct __task {
	void 			(*function_pointer)(void);
	int 			priority;
	uint			process_id;
	struct __task*	next;
} __task;

__task* __head = NULL;
uint process_id = 0;

void kernel_initalize() {
	// stdio_init_all();
	printf("Kernel: Kernal initaizing.\n");
}

void kernel_start() {
	// stdio_init_all();
	printf("Kernel: Kernal starting.\n");
	while (1) {
		__task *current_node = __head;
		while ( current_node != NULL) {
			current_node->function_pointer();
			current_node = current_node->next;
		}
	}
}

/* "kernel_create_process()" replaces the NULL node with an additional node in the "__task" linked list. 
The function returns the unsigned "process_id" integer of the created process. Each node represents a 
process. */
uint kernel_create_process(void (*pointer_to_task_function)(void), int priority) {
	printf("Kernel: Creating a kernel process.\n");

	/* The following five lines of code create a new node in the "__task" linked list, assign sufficent 
	memmory, and define the variables of the "__task" linked list/structure. */
	__task *new_node = malloc(sizeof(__task));
	new_node->function_pointer = pointer_to_task_function;
	new_node->priority = priority;
	new_node->process_id = process_id;
	new_node->next = NULL;

	/* If the "__head" instance of the "__task" linked list/structure equals NULL, the linked list contains 
	no processes or nodes, and the "new_node" structure redefines the "__head" structure. Otherwise, the 
	code locates the last node in the linked list/structure and adds the "new_node" strucutre on the end. */
	if(__head != NULL) {
		__task *last_node = __head;
		while(last_node->next != NULL) {
			last_node = last_node->next;
		}
		last_node->next = new_node;
	} else {
		__head = new_node;
	}
	process_id += 1;

	return process_id

}

void list_all_tasks() {
	printf("Within list_all_tasks \n");

	/* Print all the elements in the linked list */
	__task *current_node = __head;
	while ( current_node != NULL) {
		printf("Function pointer: ");
		unsigned char *cp = (unsigned char*)&current_node->function_pointer;
		for (int i=0;i<sizeof current_node->function_pointer; i++) {
			printf("[%08x]", cp[i]);
		}
		printf("\n");

		printf("Priority: %i \n", current_node->priority);
		printf("Process id: %i \n", current_node->process_id);
		current_node = current_node->next;
	}
}

bool kernel_kill_process_by_pointer(void (*pointer_to_task_function)(void)) {
	__task *current_node = __head;

	while ( current_node != NULL && current_node->next != NULL) {
		if (pointer_to_task_function == current_node->next->function_pointer) {
			__task *new_next = current_node->next->next;
			free(current_node->next);
			current_node->next = new_next;
			return true;
		}

		current_node = current_node->next;
	}

	return false;
}

/* The "kernel_kill_process_by_id()" function removes the 
*/
bool kernel_kill_process_by_id(uint task_id) {
	__task *current_node = __head;
	while ( current_node != NULL && current_node->next != NULL) {
		if (task_id == current_node->next->process_id) {
			__task *new_next = current_node->next->next;
			free(current_node->next);
			current_node->next = new_next;
			return true;
		}

		current_node = current_node->next;
	}

	return false;
}