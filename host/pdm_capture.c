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

#include <libusb-1.0/libusb.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define VID 				0xffff
#define PID 				0x3015
#define INTERFACE 			0
#define ENDPOINT 			0x85

#define BUFSZ				(1024)

#define SIMPLE_REQ		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE)

enum {
	REQ_PDM_STOP	=  1,
	REQ_PDM_START	=  2,
	REQ_DFU_BOOTLDR	= 0x55
};

static int done = 0;
static uint64_t total_rcvd = 0;

static void user_abort(int v) {
	done=1;
}

static void alarm_cb(int sig) {
	static uint64_t last_rcvd = 0;
    alarm(1);
    uint64_t delta = total_rcvd - last_rcvd;
    last_rcvd = total_rcvd;
    delta /= 1000>>3; // >>3 because 8 Bits per Byte ;)
    fprintf(stderr,"\rsrate: %"PRIu64" kS/s", delta);
}

void print_usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  -b                     Invoke DFU bootloader\n"
        "  -N                     Disable stdout output\n"
        "  -S                     Don't show stats\n"
        "  -h                     Show this help message\n",
        progname
    );
}

#define N_RXBUFS	16
#define RXBUF_SZ	8192

typedef struct rxbuf_s {
	uint8_t buf[RXBUF_SZ];
} rxbuf_t;

int transferred, stdout_en = 1, show_stats = 1;

static void usb_transfer_cb(struct libusb_transfer *transfer) {
	rxbuf_t *rxb = transfer->user_data;

	assert(transfer->status == LIBUSB_TRANSFER_COMPLETED);

	int len = transfer->actual_length;
	total_rcvd += len;
	for(uint16_t *buf16 = (void*)rxb->buf, n = len>>1; n; n--, buf16++)
		*buf16 = __builtin_bswap16(*buf16);
	if(stdout_en) {
		int res = write(1, rxb->buf, len);
		if (res != len)
			done=1;
	}

	transfer->buffer = rxb->buf;
	transfer->length = RXBUF_SZ;
	libusb_submit_transfer(transfer);
}


int main(int argc, char **argv) {
	uint64_t buf[BUFSZ / sizeof(uint64_t)];
    libusb_context *ctx = NULL;
    libusb_device_handle *handle = NULL;
    int res, dfu_req = 0;

	for (int opt; (opt = getopt(argc, argv, "bNSh")) != -1;) {
        switch (opt) {
			case 'b':
				dfu_req = 1;
				break;
			case 'N':
				stdout_en = 0;
				break;
			case 'S':
				show_stats = 0;
				break;
            case 'h':
            default:
                print_usage(argv[0]);
                return (opt == 'h') ? 0 : 1;
        }
    }

    res = libusb_init(&ctx);
    if (res < 0) {
        fprintf(stderr, "libusb_init error\n");
        return 1;
    }

    handle = libusb_open_device_with_vid_pid(ctx, VID, PID);
    if (!handle) {
        fprintf(stderr, "Device not found\n");
        libusb_exit(ctx);
        return 1;
    }
/*
    if (libusb_kernel_driver_active(handle, INTERFACE)) {
        libusb_detach_kernel_driver(handle, INTERFACE);
    }
*/
    res = libusb_claim_interface(handle, INTERFACE);
    if (res < 0) {
        fprintf(stderr, "Failed to claim interface\n");
        libusb_close(handle);
        libusb_exit(ctx);
        return 1;
    }

	if (dfu_req) {
		libusb_control_transfer(handle, SIMPLE_REQ, REQ_DFU_BOOTLDR, 0, 0, NULL, 0, 100);
		goto finished;
	}

	signal(SIGINT, user_abort);
	if(show_stats) {
		signal(SIGALRM, alarm_cb);
		alarm(1);
	}

	libusb_control_transfer(handle, SIMPLE_REQ, REQ_PDM_STOP, 0, 0, NULL, 0, 100);

	/* flush old data */
	do {
		res = libusb_bulk_transfer(handle, ENDPOINT, (void*)buf, BUFSZ, &transferred, 1000);
	} while(!res && transferred);

	rxbuf_t *rxbuf = malloc(sizeof(rxbuf_t) * N_RXBUFS);
	for(int i=0; i<N_RXBUFS; i++) {
		rxbuf_t *rxb = rxbuf+i;

		// submit initial async bulk transfer
		struct libusb_transfer *transfer = libusb_alloc_transfer(0);
		assert(transfer);
		libusb_fill_bulk_transfer(transfer, handle, ENDPOINT, rxb->buf, RXBUF_SZ, usb_transfer_cb, rxb, 10000);
		res = libusb_submit_transfer(transfer);
		assert(!(res<0));
	}

	res = libusb_control_transfer(handle, SIMPLE_REQ, REQ_PDM_START, 0, 0, NULL, 0, 100);
	assert(!res);

    while(!done)
		libusb_handle_events_completed(NULL, &done);
	fputs("\n", stderr);

	libusb_control_transfer(handle, SIMPLE_REQ, REQ_PDM_STOP, 0, 0, NULL, 0, 100);

finished:
    libusb_release_interface(handle, INTERFACE);
    libusb_close(handle);
    libusb_exit(ctx);
    return 0;
}
