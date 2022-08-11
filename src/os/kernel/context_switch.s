.thumb
.syntax unified

@ Diagrams:
    @   # Process Stack
    @   ## Stacked upon Interrupt
    @   +------+
    @   | xPSR | stack_words[16] = 0x01000000
    @   |  PC  | stack_words[15] = pointer_to_task_function
    @   |  LR  | stack_words[14]
    @   |  R12 | stack_words[13]
    @   |  R3  | stack_words[12]
    @   |  R2  | stack_words[11]
    @   |  R1  | stack_words[10]
    @   |  R0  | stack_words[9]
    @   +------+  
    @   ## Stacked by Interrupt Handlers
    @   +------+ 
    @   |  LR  | stack_words[8] = 0xFFFFFFFD
    @   |  R7  | stack_words[7]
    @   |  R6  | stack_words[6]
    @   |  R5  | stack_words[5]
    @   |  R4  | stack_words[4]
    @   |  R11 | stack_words[3]
    @   |  R10 | stack_words[2]
    @   |  R9  | stack_words[1]
    @   |  R8  | stack_words[0]
    @   +------+ 
    @
    @   # Main Stack
    @   ## Stacked by 
    @   +------+ 
    @   |  LR  |
    @   |  R7  |
    @   |  R6  |
    @   |  R5  | 
    @   |  R4  |
    @   |  R12 | * Kernel xPSR
    @   |  R11 |
    @   |  R10 |
    @   |  R9  | 
    @   |  R8  |
    @   +------+ 

.type isr_svcall, %function
.global isr_svcall
isr_svcall:
    @ Pushing the Process State unto the Process Stack
    
    MRS R0, PSP
    SUBS R0, #4

    MOV R1, LR
    STR R1, [R0]

    SUBS R0, #16
    STMIA R0!, {R4-R7}

    MOV R4, R8
    MOV R5, R9
    MOV R6, R10
    MOV R7, R11
    SUBS R0, #32
    STMIA R0!, {R4-R7}
    SUBS R0, #16

    @ Popping the Kernel State off of the Main Stack
    POP {R1-R5}
    MOV R8, R1
    MOV R9, R2
    MOV R10, R3
    MOV R11, R4
    MOV R12, R5
    
    POP {R4-R7}
    
    MSR PSR_NZCVQ, IP
    
    POP {PC}

.type isr_systick, %function
.global isr_systick
isr_systick:
    @ Pushing the Process State unto the Process Stack
    MRS R0, PSP
    SUBS R0, #4

    MOV R1, LR
    STR R1, [R0]

    SUBS R0, #16
    STMIA R0!, {R4-R7}

    MOV R4, R8
    MOV R5, R9
    MOV R6, R11
    MOV R7, R12
    SUBS R0, #32
    STMIA R0!, {R4-R7}
    SUBS R0, #16

    @ Popping the Kernel State off of the Main Stack
    POP {R1-R5}
    MOV R8, R1
    MOV R9, R2
    MOV R10, R3
    MOV R11, R4
    MOV R12, R5
    
    POP {R4-R7}
    
    MSR PSR_NZCVQ, IP
    
    POP {PC}

.global __initialize_context_switch
__initialize_context_switch:
    @ Pushing the Kernel State unto the Main Stack
    MRS IP, PSR

    PUSH {R4-R7}

    MOV R4, R8
    MOV R5, R9
    MOV R6, R10
    MOV R7, R11
    PUSH {R4-R7}

    @ Popping the Process State off of the Process Stack
    LDMIA R0!, {R4-R7}
    MOV R8, R4
    MOV R9, R5
    MOV R10, R6
    MOV R11, R7

    LDMIA R0!, {R4-R7}
    
    LDMIA R0!, {R1}
    MOV LR, R1

    MSR PSP, R0

    @ Running Process
    BX LR


.global __initialize_process_stack
__initialize_process_stack:
    @ Pushing the Kernel State unto the Main Stack
    MRS IP, PSR

    PUSH {R4-R7}

    MOV R4, R8
    MOV R5, R9
    MOV R6, R10
    MOV R7, R11
    PUSH {R4-R7}

    @ Switch to Process Stack
    MSR PSP, R0
    
    MOVS R0, #0b00000000000000000000000000000011
    MSR CONTROL, R0
    ISB

    BL kernel_yield

.global kernel_yield
kernel_yield:
    @ Cause a Supervisor Call (SVCall) Interrupt
    NOP
    SVC 0
    NOP
    BX LR
