/*
 * This file is based on the cdcacm.c file of the libopencm3 project:
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 * 
 * Changes Copyright (C) 2025, 2026 Benedikt Heinz <hunz@mailbox.org>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/crs.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/stm32/desig.h>

#include <libopencm3/usb/usbd.h>

#include <libopencm3/cm3/nvic.h>

#include <libopencmsis/core_cm3.h>

#include <libopencm3/cm3/assert.h>

#include <string.h> // needed for memcmp & memcpy

#include "platform.h"

#include "pdm_usb_tx.h"

static char serial_nr[16] = "0";

static const char *usb_strings[] = {
	"znu",
	"PDM_capture",
	serial_nr,
};

usbd_device *usb_dev = NULL;

/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[128];

static const struct usb_device_descriptor dev = {
  .bLength = USB_DT_DEVICE_SIZE,
  .bDescriptorType = USB_DT_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = 0xFF,
  .bDeviceSubClass = 0,
  .bDeviceProtocol = 0,
  .bMaxPacketSize0 = 64,
  .idVendor = 0xffff,
  .idProduct = 0x3015,
  .bcdDevice = 0x0200,
  .iManufacturer = 1,
  .iProduct = 2,
  .iSerialNumber = 3,
  .bNumConfigurations = 1,
};

enum usb_interfaces {
	PDM_DATA_IFNUM = 0,
	N_USB_INTERFACES
};

static const struct usb_endpoint_descriptor pdm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = PDM_DATA_EP,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = PDM_DATA_CHUNKSIZE,
	.bInterval = 1,
}};

const struct usb_interface_descriptor pdm_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = PDM_DATA_IFNUM,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = 0xFF,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
	.endpoint = pdm_endp,
};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = &pdm_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = N_USB_INTERFACES,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = (50/2),

	.interface = ifaces,
};

static void usb_set_config(usbd_device *usbd_dev, uint16_t wValue) {
	/* PDM Data */
	usbd_ep_setup(usbd_dev, PDM_DATA_EP, USB_ENDPOINT_ATTR_BULK, PDM_DATA_CHUNKSIZE, pdm_data_sent);
	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				pdm_control_request);
	(void)wValue;
}

void usb_isr(void) {
	if(usb_dev)
		usbd_poll(usb_dev);
}

void usb_setup(void);

void usb_setup(void) {

	desig_get_unique_id_as_dfu(serial_nr);

#if defined(STM32F0)
/* for PLL USB clock source an external HSE and PLL output of 48MHz is necessary */
#ifdef USBCLK_USE_PLL
	rcc_set_usbclk_source(RCC_PLL);
#else
	/* default to HSI48 w/ CRS unless user sets USBCLK_USE_PLL */
	rcc_set_usbclk_source(RCC_HSI48);
	crs_autotrim_usb_enable();

#endif
#elif defined(STM32C0)
	rcc_set_usbclk_source(RCC_HSIUSB48);
	crs_autotrim_usb_enable();
#else
#	error "STM32 family not supported by this code"
#endif

	usb_dev = usbd_init(&st_usbfs_usb_driver, &dev, &config, usb_strings,
						sizeof(usb_strings)/sizeof(char *),
						usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usb_dev, usb_set_config);

	nvic_set_priority(NVIC_USB_IRQ, 64); // second highest priority
	nvic_enable_irq(NVIC_USB_IRQ);
}

void usb_shutdown(void) {
	nvic_disable_irq(NVIC_USB_IRQ);
	usbd_disconnect(usb_dev, true);
	usb_dev = NULL;
	delay_ms(10);
}
