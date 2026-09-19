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
#include "pdm_usb_tx.h"
#include "spi.h"

#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>

#include <libopencm3/cm3/systick.h>

#include <libopencm3/cm3/assert.h>

extern usbd_device *usb_dev;

static void pdm_stop(void);

volatile uint32_t pdm_request = 0;

/* 64 Bytes = 16x uint32 words */
#define PDM_DATA_CHUNKSIZE_U32	(PDM_DATA_CHUNKSIZE / sizeof(uint32_t))
#define PDM_DATA_CHUNKSIZE_U16	(PDM_DATA_CHUNKSIZE / sizeof(uint16_t))

static uint32_t usb_rollovers = 0;
static uint32_t tx_cndtr = 0, tx_idx = 0;

void pdm_data_sent(usbd_device *usbd_dev, uint8_t ep) {
	if(!SPI2_ACTIVE())
		return;

	/* Wait until we have data to send.
	 *
	 * There are three cases:
	 *
	 * 1) Simple case, no DMA ringbuf rollover:
	 *    DMA_CNDTR eventually reaches a value <= tx_cndtr
	 *
	 * 2) Anticipated DMA ringbuf rollover:
	 *    To account for the upcoming rigbuf rollover, usb_rollovers was incremented
	 *    prior to the dma_rollover to prevent dma_rollovers from being > usb_rollovers
	 *    and triggering a "ready to send" immediately when the dma_rollover happens.
	 *    Eventually, after the dma_rollover, DMA_CNDTR reaches a value <= tx_cndtr
	 *    and we can send the next chunk.
	 *
	 * 3) Unanticipated DMA ringbuf rollover:
	 *    This should only happen when the USB reads lag further behind the DMA ringbuf writes.
	 *    Next USB read is *before* last chunk of the ringbuffer, but DMA already
	 *    did the ringbuf rollover.
	 *    So usb_rollovers was *not* incremented before the DMA rollover, therefore
	 *    dma_rollovers is > usb_rollovers and we can send the next chunk.
	 *    This condition is met until the USB reads eventually reach the last chunk,
	 *    so usb_rollovers is incremented and catches up with dma_rollovers. */
	while(DMA_CNDTR(DMA1, DMA_CHANNEL1) > tx_cndtr && !(dma_rollovers > usb_rollovers)) {}

	/* Send ASAP. To faciliate this, we check for ringbuf overflows only after the chunk
	 * has already been written to the USB endpoint. */
	int res = usbd_ep_write_packet(usbd_dev, ep, pdm_ringbuf + tx_idx, PDM_DATA_CHUNKSIZE);
	// if there is still old data in the endpoint buffer we stop everything
	if(res != PDM_DATA_CHUNKSIZE)
		goto stop;

	/* Check for ringbuffer overflows
	 * TBD: verify dma_rollover vs. DMA_CNDTR timing */
	if(dma_rollovers > usb_rollovers) {
		uint32_t read_ofs  = tx_idx<<1; // U32 -> U16
		uint32_t write_ofs = PDM_RINGBUF_SIZE_U16 - DMA_CNDTR(DMA1, DMA_CHANNEL1);
		if((write_ofs >= read_ofs) || ((dma_rollovers-usb_rollovers)>1))
			goto stop;
	}

	// set tx_idx to next chunk
	tx_idx += PDM_DATA_CHUNKSIZE_U32;
	tx_idx &= PDM_RINGBUF_SIZE_U32-1;

	if (!tx_idx) {
		/* The next chunk at tx_idx=0 requires a ringbuf rollover to happen before we can sent it.
		 * dma_rollovers will be incremented once this happens, even when the chunk isn't complete yet.
		 * To prevent a premature sending of the chunk when dma_rollovers is incremented, we anticipate
		 * this by incrementing usb_rollovers already, thereby "muting"
		 * the !(dma_rollovers > usb_rollovers) condition in the waiting loop above. */
		usb_rollovers++;
		tx_cndtr = PDM_RINGBUF_SIZE_U16; // reset tx_cndtr to start of ringbuf
	}
	tx_cndtr -= PDM_DATA_CHUNKSIZE_U16; // advance tx_cndtr one chunk

	return;

stop:
	pdm_stop();
	gpio_set(GPIOB, GPIO1); // set overflow LED
}

/* MUST be called from usb_isr context */
static void pdm_stop(void) {
	/* First we stop the timer generating the PDM clock.
	 * This shuts down the PDM microphone and the clock to SPI2. */
	TIM_CR1(TIM1) &= ~TIM_CR1_CEN;

	/* Then we shutdown SPI2. */
	SPI_CR1(SPI2) = SPI_CR1_SSM | SPI_CR1_RXONLY;

	/* Clear activity LED */
	gpio_clear(GPIOB, GPIO2);

	/* Send PDM stopped signal to main loop */
	pdm_request = REQ_PDM_STOP;
}

/* MUST be called with NVIC_USB_IRQ disabled */
void pdm_start(void) {

	gpio_clear(GPIOB, GPIO1); // clear overflow LED

	// reset rollovers first
	dma_rollovers = 0;

	// get current ringbuffer position
	uint32_t ofs = PDM_RINGBUF_SIZE_U16 - DMA_CNDTR(DMA1, DMA_CHANNEL1);

	/* Snap offset for next tx_idx to start of next chunk */
	ofs += PDM_DATA_CHUNKSIZE_U16;
	ofs &= ~(PDM_DATA_CHUNKSIZE_U16-1);
	ofs &= PDM_RINGBUF_SIZE_U16-1;			// respect end of ringbuffer
	uint32_t new_tx_idx = ofs>>1;			// U16 -> U32
	usb_rollovers = (new_tx_idx < tx_idx);	// expect a rollover?
	tx_idx = new_tx_idx;

	ofs += PDM_DATA_CHUNKSIZE_U16;
	ofs &= PDM_RINGBUF_SIZE_U16-1; // respect end of ringbuffer
	tx_cndtr = PDM_RINGBUF_SIZE_U16 - ofs;

	/* Reenable SPI2 */
	SPI_CR1(SPI2) = SPI_CR1_SSM | SPI_CR1_RXONLY | SPI_CR1_SPE;

	/* Send 1st chunk of data ASAP */
	pdm_data_sent(usb_dev, PDM_DATA_EP);
}

enum usbd_request_return_codes pdm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req)) {

	(void)usbd_dev;
	(void)complete;
	(void)buf;
	(void)len;

	uint32_t prev_requests = pdm_request;

	switch(req->bRequest) {

		case REQ_PDM_STOP:
			pdm_stop();
			break;

		case REQ_PDM_START:
			if (!SPI2_ACTIVE())
				pdm_request |= REQ_PDM_START;
			break;

		case REQ_DFU_BOOTLDR:
			system_reset(BOOTLOADER_MAGIC);
			break;

		default:
			return USBD_REQ_NOTSUPP;
	}

	// reenable systicks to reactivate main loop
	if (pdm_request != prev_requests)
		systick_counter_enable();

	return USBD_REQ_HANDLED;
}
