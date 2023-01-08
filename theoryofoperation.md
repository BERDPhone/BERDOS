# Theory of Operation

As an operating system, BERDOS creates abstractions and shares hardware resources, providing a software platform for the user. BERDOS includes a kernel, shell, and debugger. The kernel manages hardware space and time through process, memory, and file management. The shell provides a command-line interface, and the debugger gives diagnostic information about the system.

## The Kernel

### Process Managment

To abstract away the processor and facilitate time sharing, BERDOS relies on processes. As abstractions of executing code, processes represent the current state of the processor, id est the values stored in its registers called a context. The operating system switches between these contexts or processes to create the illusion of multiprogramming and running multiple programs simultaneously. 

#### Process Implementation

The operating system implements the process abstraction and stores data about active processes in a data structure called the process table; each process gets an entry in the process table, called a process control block, containing the information necessary to manage the process. BERDOS implements process control blocks with the `control_block` struct strung together in a double, non-circular linked list with the `*next_node` and `*previous_node` members. Furthermore, BERDOS defines the `__head` and `__tail` variables pointing to the beginning and end of the list.

The kernel gives each process a state. BERDOS recognizes five statuses specified by the `states` enumerated constant type and `status` member of the `control_block` struct. BERDOS handles processes by updating and checking their state, and the operating system does so by calling the `__terminate_process()`, `__create_process()`, `__execute_process()`, `__block_process()`, and `__ready_process()`.
* BERDOS assigns the `TERMINATED` state to processes while terminating them in the `__terminate_process()` function. After updating the state of the process passed to it, `__terminate_process()` first removes the process from the queues, second removes the process from the process table linked list, and third, deallocates the memory allocated to the process.
* BERDOS assigns the `CREATED` state when it creates a process in the `__create_process()` function, so the `CREATED` state means the process can but has not run. `__create_process()` first allocates memory for the process control block and address space, second initializes the process control block and process context, third adds the process to the linked list, and fourth places the process on the job queue.
* BERDOS assigns the `EXECUTING` state to the processes currently executing on the CPU. `__exectue_process()` updates the state of the process passed to it if and only if it has the `READY` state before calling the `__context_switch()` function. Since `__execute_process()` ultimately calls `__context_switch()`, it updates the stack pointer of the process passed to it.
* BERDOS assigns the `BLOCKED` state to unrunnable processes, excluding them from scheduling. If the process passed to it has the `READY` or `EXECUTING` states, `__block_process()` changes the process's state to `BLOCKED` and removes the process from the ready queue so that the scheduler does not context switch to a process waiting for an I/O update.
* BERDOS assigns the `READY` state to runnable processes that the scheduler may execute. If the process passed to it has the `EXECUTING` or `CREATED` or `BLOCKED` states, `__ready_proces()` changes the process's state to `READY` and puts the process on the ready queue. The scheduler only schedules `READY`, so the processor only executes `READY` processes.

#### Context Switching

Only one process runs on one CPU at a time. However, context switching -- saving a program's CPU context and loading in another CPU context -- interrupts the currently executing code and redirects the processor. BERDOS context switches with the `__context_switch()` function. `__context_switch()` swaps from the kernel to the process stored on its process stack. The operating system takes back control of the system when an interrupt occurs, so the interrupt service routines (ISRs) -- `isr_systick()` and `isr_svcall()` -- context switch from a process back to the kernel. While context switching, BERDOS also updates the processor's mode. Since the operating system has access to the entire system, whereas processes do not, processes run in unprivileged thread mode, and BERDOS runs in privileged handler mode. 

`__context_switch()` takes the stack pointer of the process stacked to since the last thing pushed to a process's stack by the ISRs and context switches to that process. First, the `__context_switch()` pushes the kernel context unto the main stack, creating the main stack frame, and second unpacks the process stack frame from the stack pointer passed to it. `__context_switch` pushes the entire kernel context except for the scratch registers, R0-R3 and R12, in the same order the ISRs restore the process by branching to the EXE_RETURN, returns to the interrupted process, and puts the processor in thread mode. 
```
+------+ * MAIN STACK
|  LR  |
|  R7  |
|  R6  |
|  R5  |
|  R4  |
|  R12 | == xPSR
|  R11 |
|  R10 |
|  R9  |
|  R8  | <- MSP
+------+ * Kernel Frame
```

`isr_systick()` and `isr_svcall()` context switch back to the kernel from the interrupted process and return the processor to handler mode after handling its exception. When an exception occurs, the CPU enters handler mode, creates a hardware stack frame, and calls the appropriate ISR listed in the Raspberry Pi Pico SDK's vector table. The ISR first pushes the interrupted process unto the process's stack, creating the software frame, and second, it pops the kernel frame off of the main stack. Because the processor saves the kernel state while it is technically executing `__context_switch()`, the function also returns the updated process stack pointer. Together the ISR and hardware save the entire process unto the process's stack.
```
+------+ * PROCESS STACK
| xPSR |
|  PC  |
|  LR  |
|  R12 |
|  R3  |
|  R2  |
|  R1  |
|  R0  |
+------+ * Hardware Frame
|  LR  | <- EXE_RETURN
|  R7  |
|  R6  |
|  R5  |
|  R4  |
|  R11 |
|  R10 |
|  R9  |
|  R8  | <- PSP
+------+ * Software Frame
```

#### Scheduling

An operating system's scheduler decides which process to run on a CPU before context switching to it. BERDOS implements scheduling policy through the `__long_term_scheduler()` `__medium_term_scheduler()`, and `__short_term_scheduler`.


### Memory Managment


### File Managment


### Kernel Operation




## The Shell




## The Debugger



