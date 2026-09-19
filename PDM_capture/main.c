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
#include "spi.h"
#include "pdm_usb_tx.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

/* IO mapping:
 * PA9 : TIM1_CH2  / AF2 - PDM clock out
 * PA10: SPI2_MOSI / AF0 - PDM data in
 * PB8 : SPI2_SCK  / AF4 - PDM clock in
 *
 * PDM microphone is a SPH0641LU4H-1
 * Notes:
 * - switch to normal mode (1.024MHz - 2.475MHz) before switching to Ultrasonic Mode (4.8MHz)
 * - Power-up Time:    <=50ms
 * - Mode-Change Time: <=10ms
 * => switch to 2.4MHz first, wait ~100ms, then switch to 4.8MHz
 */

static void pdmclk_setup(void) {
	/* TIM1 RCC already enabled in hw_init */

	/* Initially set prescaler to run timer at 48 MHz / 10 = 4.8 MHz */
	//TIM_PSC(TIM1) = (10-1);

	timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_continuous_mode(TIM1);

	TIM_ARR(TIM1)  = 1;
	TIM_CCR2(TIM1) = 1;

	// Enable CH2 Output
	timer_set_oc_mode(TIM1, TIM_OC2, TIM_OCM_PWM1);
	timer_enable_oc_output(TIM1, TIM_OC2);
	timer_enable_break_main_output(TIM1);

	// we will enable TIM1 later when we actually need it
	//timer_enable_counter(TIM1);

	gpio_set_af(GPIOA, GPIO_AF2, GPIO9);

	/* GPIO_OSPEED_HIGH    : val=2 -> Tr/Tf @10pF load according to STM32C0 DS: 4.0ns max
	 * GPIO_OSPEED_VERYHIGH: val=3 -> Tr/Tf @10pF load according to STM32C0 DS: 2.5ns max
	 * SPH0641LU4H-1 Clock Rise/Fall Time: 3ns max
	 * => We try GPIO_OSPEED_HIGH first and see how it goes. */
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_HIGH, GPIO9);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
}

enum {
	PDM_SLEEP		= 0,
	PDM_WAKEUP		= 1,
	PDM_US_STARTUP	= 2,
	PDM_US_RUNNING	= 3
};

int main(void) {
	uint32_t last_jiffies = 0, state = PDM_SLEEP;
	timeout_t waiting_to = 0;

	hw_init(); // see ../common-code/platform.c

	/* LEDs */
	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_LOW, GPIO2|GPIO1);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2|GPIO1);

	pdmclk_setup();
	spi2_setup();

	/* main loop */
	while(1) {

		/* sleep until >= 1ms has passed */
		do { __WFI(); } while(last_jiffies == _jiffies);
		last_jiffies = _jiffies;

		/* Nothing to do? -> go back to sleep */
		if (!pdm_request)
			continue;

		NVIC_DISABLE_IRQ(NVIC_USB_IRQ);

		// PDM stop / state reset request
		if (pdm_request & REQ_PDM_STOP) {
			/* Actual stop has been done in USB ISR already.
			 * Our state must be reset to sleep mode though. */
			state = PDM_SLEEP;
			pdm_request &= ~REQ_PDM_STOP; // clear stop flag
		}

		// PDM startup
		if (pdm_request & REQ_PDM_START) {
			if (state == PDM_SLEEP) {
				/* Initially set prescaler to run timer at 48 MHz / 10 = 4.8 MHz.
				 * -> 2.4MHz PDM clock */
				TIM_PSC(TIM1) = (10-1);
				TIM_CR1(TIM1) |= TIM_CR1_CEN; // Start PDM clkgen timer
				/* SPH0641LU4H-1 wake-up time: max. 15 ms */
				timeout_set(&waiting_to, MS_TO_TICKS(16));
				state++; // switch to next state
			}
			else if(timeout(&waiting_to)) {
				switch(state++) {
					case PDM_WAKEUP:
						/* Full Ultrasonic speed ahead */
						TIM_PSC(TIM1) = (5-1); // run timer at 48 MHz / 5 = 9.6 MHz
						/* SPH0641LU4H-1 mode-change time: max. 10 ms */
						timeout_set(&waiting_to, MS_TO_TICKS(11));
						break;
					case PDM_US_STARTUP:
						pdm_start();
						pdm_request &= ~REQ_PDM_START; // clear start flag
						break;
				}
			} // time for next state reached
		} // start request

		// go tickless again if nothing left to do
		if(!pdm_request)
			systick_counter_disable();

		NVIC_ENABLE_IRQ(NVIC_USB_IRQ);
	}
	return 0;
}
