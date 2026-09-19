PROJECT = PDM_capture
BUILD_DIR = bin

GIT_COMMIT  := "$(shell git describe --abbrev=8 --dirty --always --tags)"
GIT_BRANCH  := "$(shell git branch --show-current)"
GIT_REMOTE  := "$(shell git config --get remote.origin.url)"
GIT_VERSION := "$(GIT_REMOTE) $(GIT_BRANCH) $(GIT_COMMIT)"

SHARED_DIR = ../common-code
CFILES = main.c spi.c pdm_usb_tx.c
CFILES += platform.c stm32_usb.c
AFILES += lowlevel.S
CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"

# NOTE: edit for other STM32 parts
#DEVICE=stm32f042k6t6
DEVICE=stm32c071kbt6

# You shouldn't have to edit anything below here.
VPATH += $(SHARED_DIR)
INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
OPENCM3_DIR=../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

all: $(PROJECT).bin
	$(PREFIX)size $(PROJECT).elf

load: $(PROJECT).bin
	(cd ../utils && make && ./pdm_capture -b && sleep 1) || true
	dfu-util -a 0 -s 0x08000000:leave -D $(PROJECT).bin
