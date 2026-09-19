#ifndef PDM_USB_TX_H
#define PDM_USB_TX_H

#include <libopencm3/usb/usbd.h>

/* USB endpoint defs */
#define PDM_DATA_EP			0x85
#define PDM_DATA_CHUNKSIZE	64		// chunksize in bytes

extern volatile uint32_t pdm_request;
void pdm_start(void);

enum {
	REQ_PDM_STOP	=  1,
	REQ_PDM_START	=  2,
	REQ_DFU_BOOTLDR	= 0x55
};

void pdm_data_sent(usbd_device *usbd_dev, uint8_t ep);

enum usbd_request_return_codes pdm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req));

#endif // PDM_USB_TX_H
