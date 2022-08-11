.thumb
.syntax unified

.text

.type __systick_initialize, %function
.global __systick_initialize
__systick_initialize:
	@ Programing SysTick Reload Value Register
	MOVS R0, #0xE000E014
	MOVS R1, #0b00000000111111111111111111111111
	STR R1, [R0]

	@ Resetting SysTick Current Value Register
	MOVS R0, #0xE000E018
	MOVS R1, #0b00000000000000000000000000000000
	STR R1, [R0]

	@ Programing SysTick Control & Status Register
	MOVS R0, #0xE000E010
	MOVS R1, #0b00000000000000000000000000000111
	STR R1, [R0]
	
	BX LR