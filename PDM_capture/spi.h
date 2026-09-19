#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <libopencm3/stm32/spi.h>

/* 16kB is ~27.3ms worth of buffering time */
#define PDM_RINGBUF_SIZE_BYTES	(4096*4) // *MUST* be a power of two
#define PDM_RINGBUF_SIZE_U16	(PDM_RINGBUF_SIZE_BYTES / sizeof(uint16_t))
#define PDM_RINGBUF_SIZE_U32	(PDM_RINGBUF_SIZE_BYTES / sizeof(uint32_t))

extern volatile uint32_t dma_rollovers;
extern uint32_t pdm_ringbuf[PDM_RINGBUF_SIZE_U32];

#define SPI2_ACTIVE()	(SPI_CR1(SPI2) & SPI_CR1_SPE)

void spi2_setup(void);

#endif // SPI_H
