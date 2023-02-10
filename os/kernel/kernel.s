.thumb
.syntax unified

.type isr_svcall, %function
.global isr_svcall
isr_svcall:
    MRS R0, PSP
    LDR R1, [R0, #0x18]
    SUBS R1, #0x2
    LDRB R1, [R1]

    LDR R2, =system_call_number
    STR R1, [R2]

    LDR R2, =hardware_frame_pointer
    STR R0, [R2]

    B .L3

.type isr_systick, %function
.global isr_systick
isr_systick:
    MRS R0, PSP
.L3:
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

    POP {R1-R5}
    MOV R8, R1
    MOV R9, R2
    MOV R10, R3
    MOV R11, R4
    MOV R12, R5
    POP {R4-R7}       

    MSR PSR_NZCVQ, IP
    POP {PC}

.global __context_switch
__context_switch:
    MRS IP, PSR
    PUSH {R4-R7, LR}
    MOV R1, R8
    MOV R2, R9
    MOV R3, R10
    MOV R4, R11
    MOV R5, R12
    PUSH {R1-R5}    

    LDMIA R0!,{R4-R7}
    MOV R8, R4
    MOV R9, R5
    MOV R10, R6
    MOV R11, R7
    LDMIA R0!,{R4-R7}
    LDMIA R0!,{R1}
    MOV LR, R1
    MSR PSP, R0

    BX LR

.global __initialize_main_stack
__initialize_main_stack:
    MRS IP, PSR
    PUSH {R4-R7, LR}
    MOV R1, R8
    MOV R2, R9
    MOV R3, R10
    MOV R4, R11
    MOV R5, R12
    PUSH {R1-R5}    

    MSR PSP, R0
    MOVS R0, #2
    MSR CONTROL, R0

    ISB

.align 2
.global os_yield
os_yield:
    NOP
    SVC 0
    NOP
    BX LR

.align 2
.global os_exit
os_exit:
    NOP
    SVC 1
    NOP
    BX LR

.align 2
.global os_spawn
os_spawn:
    NOP
    SVC 2
    NOP
    BX LR

.align 2
.global os_mkdir
os_mkdir:
    NOP
    SVC 3
    NOP 
    BX LR

.align 2
.global os_rmdir
os_rmdir:
    NOP
    SVC 4
    NOP 
    BX LR

.align 2
.global os_create
os_create:
    NOP
    SVC 5
    NOP
    BX LR

.align 2
.global os_delete
os_delete:
    NOP
    SVC 6
    NOP
    BX LR

.align 2
.global os_read
os_read:
    NOP
    SVC 7
    NOP
    BX LR

.align 2
.global os_write
os_write:
    NOP
    SVC 8
    NOP
    BX LR

.align 2
.global os_open
os_open:
    NOP
    SVC 9
    NOP
    BX LR

.align 2
.global os_print
os_print:
    NOP
    SVC 10
    NOP
    BX LR

.data
.global system_call_number
system_call_number:
    .word system_call_number
.global hardware_frame_pointer
hardware_frame_pointer:
    .word hardware_frame_pointer
