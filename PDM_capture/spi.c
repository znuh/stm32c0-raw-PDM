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

#include <libopencm3/stm32/gpio.h>

#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/dmamux.h>

#include <libopencm3/cm3/nvic.h>

#include "spi.h"

#include <libopencm3/cm3/assert.h>

uint32_t pdm_ringbuf[PDM_RINGBUF_SIZE_U32];
volatile uint32_t dma_rollovers = 0;

void spi2_setup(void) {
	/* SPI2 + DMA RCCs already initialized in hw_init */

	/* Prepare DMA. */
	dma_set_priority(DMA1, DMA_CHANNEL1, DMA_CCR_PL_VERY_HIGH);

	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL1);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL1);
	dma_enable_circular_mode(DMA1, DMA_CHANNEL1);

	dma_set_memory_size(DMA1, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);
	dma_set_memory_address(DMA1, DMA_CHANNEL1, (uint32_t)pdm_ringbuf);

	dma_set_peripheral_size(DMA1, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
	dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (uint32_t)&SPI2_DR);

	dma_set_number_of_data(DMA1, DMA_CHANNEL1, PDM_RINGBUF_SIZE_U16);

	dmamux_set_dma_channel_request(DMAMUX1, DMA_CHANNEL1, DMAMUX_CxCR_DMAREQ_ID_SPI2_RX);

	dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL1);
	nvic_set_priority(NVIC_DMA1_CHANNEL1_IRQ, 0); // highest priority
	nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);

	/* Setup SPI2 in Slave Mode */
	//SPI_CR1(SPI2) = SPI_CR1_SSM | SPI_CR1_RXONLY;
	spi_set_data_size(SPI2, SPI_CR2_DS_16BIT);

	// enable RX DMA
	SPI_CR2(SPI2) |= SPI_CR2_RXDMAEN;

	/* PA10: SPI2_MOSI / AF0
	 * PB8 : SPI2_SCK  / AF4 */
	gpio_set_af(GPIOA, GPIO_AF0, GPIO10);
	gpio_set_af(GPIOB, GPIO_AF4, GPIO8);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO10);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8);

	DMA_CCR(DMA1, DMA_CHANNEL1) |= DMA_CCR_EN; // start DMA channel
	//SPI_CR1(SPI2) = SPI_CR1_SSM | SPI_CR1_RXONLY | SPI_CR1_SPE; // start SPI peripheral
}

void dma1_channel1_isr(void) {
	uint32_t flags = DMA_ISR(DMA1);

	if(flags&DMA_ISR_TEIF1) // error flag set
		cm3_assert_not_reached();

	DMA_IFCR(DMA1) = flags;
	dma_rollovers++;

	if(!(dma_rollovers&15))
		gpio_toggle(GPIOB, GPIO2); // toggle activity LED
}
