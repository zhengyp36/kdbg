ifeq ($(KERNELRELEASE),)

.PHONY: all clean

KBUILD_DIR := /lib/modules/$(shell uname -r)/build
TOP_DIR    := $(shell pwd)
EXT_OBJS   := $(patsubst %.c,%.o,$(wildcard extensions/*.c))

ifeq ($(target),)
MODULE = kdbg
else
MODULE = $(target)
endif

export TOP_DIR
export MODULE
export EXT_OBJS

all:
	$(MAKE) -C $(KBUILD_DIR) M=$(TOP_DIR) modules
	@echo "EXT_OBJS=$(EXT_OBJS)"

clean:
	$(MAKE) -C $(KBUILD_DIR) M=$(TOP_DIR) clean

else

obj-m          += $(MODULE).o
$(MODULE)-objs += frame/kdbg_sec_start.o
$(MODULE)-objs += frame/kdbg_drv.o
$(MODULE)-objs += frame/kdbg_lib.o
$(MODULE)-objs += frame/kdbg_main.o
$(MODULE)-objs += $(EXT_OBJS)
$(MODULE)-objs += frame/kdbg_sec_stop.o

ccflags-y  += -std=gnu99 -Wall -Werror
ccflags-y  += -Wno-declaration-after-statement
ccflags-y  += -I$(TOP_DIR)/../../inc
ccflags-y  += -D_KERNEL=1 -DKDBG_DEVNAME=\"$(MODULE)\"

endif
