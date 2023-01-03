.thumb
.syntax unified

.local __svcall_0
__svcall_0:
    B .L3

.local __svcall_1
__svcall_1:
    PUSH {R4}
    MOV R4, LR
    PUSH {R4}
    MOV R4, R0
    PUSH {R0-R3}

    LDR R0, [R4]

    BL __exit

    POP {R0-R3}
    POP {R4}
    MOV LR, R4
    POP {R4}

    B .L3

.local __svcall_2
__svcall_2:
    PUSH {R4}
    MOV R4, LR
    PUSH {R4}
    MOV R4, R0
    PUSH {R0-R3}

    LDR R0, [R4]
    LDR R1, [R4, #0x04]

    BL __fork

    STR R0, [R4]

    POP {R0-R3}
    POP {R4}
    MOV LR, R4
    POP {R4}

    B .L3

.type isr_svcall, %function
.global isr_svcall
isr_svcall:
.L0:
    MOVS R0, #0b100
    MOV R1, LR
    TST R0, R1
    BEQ .L1

    MRS R0, PSP
    B .L2
.L1:
    MRS R0, MSP
    B .L2
.L2:
    LDR R1, [R0, #0x18]
    SUBS R1, #0x2
    LDRB R1, [R1]

    CMP R1, #0
    BEQ __svcall_0
    CMP R1, #1
    BEQ __svcall_1
    CMP R1, #2
    BEQ __svcall_2
.L3:
    B .L4

.type isr_systick, %function
.global isr_systick
isr_systick:
    MRS R0, PSP
.global svc_return
.svc_return:
.L4:
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
.global os_fork
os_fork:
    NOP
    SVC 2
    NOP
    BX LR
