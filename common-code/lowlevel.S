.syntax unified
.thumb

/* WARNING: all cycle-based timings are for a Cortex-M0+ with code execution from SRAM.
 * Other Cortexes (e.g. M0) have different instruction timings!
 * Execution from Flash instead of SRAM can introduce additional delays
 * due to flash read waitstates.
 */  

.section .ramtext, "ax", %progbits
.global delay_loop
.func delay_loop
.type delay_loop, %function

// bcs: delay=0 -> 0 iterations, delay=1 -> 1 iteration , delay=2 -> 2 iterations, ...
// bhi: delay=0 -> 0 iterations, delay=1 -> 0 iterations, delay=2 -> 1 iteration, ...

delay_loop:
	subs r0,#1           /* 1 cycle  */
	bcs delay_loop       /* 2 if taken, 1 if not taken */
    bx lr                /* 2 cycles */
.endfunc

.size delay_loop, .-delay_loop

.section .text.hard_fault_handler, "ax", %progbits
.global hard_fault_handler
.func hard_fault_handler
.type hard_fault_handler, %function

hard_fault_handler:
    mrs   r0, msp
    ldr   r1, =hardfault_dump

    ldm   r0!, {r2, r3, r4, r5}   /* Read r0, r1, r2, r3 from stack */
    stm   r1!, {r2, r3, r4, r5}

    ldm   r0!, {r2, r3, r4, r5}   /* Read r12, lr, pc, xpsr from stack */
    stm   r1!, {r2, r3, r4, r5}

    ldr   r2, =0xdeadbeef
    str   r2, [r1]

    /* trigger System Reset via SCB_AIRCR */
    ldr   r3, =0xE000ED0C         /* SCB->AIRCR addr */
    ldr   r4, =0x05FA0004         /* VECTKEY (0x05FA0000) | SYSRESETREQ (0x00000004) */
    str   r4, [r3]

    dsb
hf_wait_sys_reset:
    b     hf_wait_sys_reset
.endfunc

.size hard_fault_handler, .-hard_fault_handler
