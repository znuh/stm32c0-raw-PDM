/*
 * Copyright (C) 2026 Benedikt Heinz <Zn000h AT gmail.com>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this code.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "platform.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/syscfg.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <stdint.h>
#include <inttypes.h>

extern void usb_setup(void);

volatile uint64_t _jiffies = 0;

void sys_tick_handler(void) {
	_jiffies++;
}

/*
void sleep_ms(uint32_t ms) {
	timeout_t to;
	timeout_set(&to, MS_TO_TICKS(ms));
	timeout_sleep(&to);
}
*/

static void clocks_setup(void) {
	rcc_clock_setup_in_hsi48_out_48mhz();
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);

	rcc_periph_clock_enable(RCC_TIM1);

	rcc_periph_clock_enable(RCC_SPI2);
	rcc_periph_clock_enable(RCC_DMA1);
}

static void systick_setup(void) {
	systick_set_frequency(HZ, rcc_ahb_frequency);
	systick_clear();
	//systick_counter_enable();
	nvic_set_priority(NVIC_SYSTICK_IRQ, 255);  // lowest priority
	systick_interrupt_enable();
}

uint32_t __attribute__((section(".noinit"), used)) hardfault_dump[9];

static uint32_t __attribute__((section(".noinit"))) boot_magic;

static void __attribute__((constructor)) early_init(void) {
	if(boot_magic != BOOTLOADER_MAGIC)
		return;

	boot_magic = 0;
	__asm__ volatile ("CPSID I\n");

	uint32_t *vtab = (uint32_t *)0x1FFF0000;
	SCB_VTOR = (uint32_t)vtab;
	__asm__ volatile (
        "ldr r1, [%0]\n"
        "msr msp, r1\n"
        "ldr %0, [%0, #4]\n"
        "bx %0\n"
        : "+r" (vtab) : : "r1", "memory"
    );
}

void system_reset(uint32_t bl_magic) {
	usb_shutdown();
	boot_magic = bl_magic;
	__asm__ volatile ("dsb\n");
	SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ; /* trigger system reset via SCB */
	while(1){}
}

void hw_init(void) {
	/* do a clean reset if the bootloader was running before */
	if(SCB_VTOR)
		SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;

	clocks_setup();

	/* Enable Boot0 pin in Option Bytes? */
#if defined(STM32C0) && defined(BOOT0_PIN_ENABLE)
	/* Option Bytes are read directly from Flash because FLASH_OPTR register
	 * does not change its value until a POR occurs */
	if(FLASH_OPTION_BYTES & FLASH_OPTR_nBOOT_SEL)
		flash_program_option_bytes(FLASH_OPTION_BYTES & (~FLASH_OPTR_nBOOT_SEL));
#endif

	systick_setup();
	usb_setup();
}
