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

export ksym_demo_enable
export spa_demo_enable
export trace_demo_enable
export update_zfs_depend

ifeq ($(spa_demo_enable),true)
DEPEND_INC_HEADERS += $(TOP_DIR)/extensions/zfs_depend.h
endif

all: $(DEPEND_INC_HEADERS)
	$(MAKE) -C $(KBUILD_DIR) M=$(TOP_DIR) modules

clean:
	$(MAKE) -C $(KBUILD_DIR) M=$(TOP_DIR) clean

$(TOP_DIR)/%.h: $(TOP_DIR)/%.in
	@if [ -f $@ -a "$(update_zfs_depend)" = "false" ]; then \
		echo "Skip generating $@"; \
	else \
		$(TOP_DIR)/../../scripts/ksym.py $<; \
	fi

else

obj-m          += $(MODULE).o
$(MODULE)-objs += frame/kdbg_sec_start.o
$(MODULE)-objs += frame/kdbg_drv.o
$(MODULE)-objs += frame/kdbg_lib.o
$(MODULE)-objs += frame/kdbg_main.o
$(MODULE)-objs += frame/kdbg_trace.o
$(MODULE)-objs += frame/kdbg_trace_demo.o
$(MODULE)-objs += $(EXT_OBJS)
$(MODULE)-objs += frame/kdbg_sec_stop.o

ccflags-y  += -std=gnu99 -Wall -Werror
ccflags-y  += -Wno-declaration-after-statement
ccflags-y  += -I$(TOP_DIR)/../../inc
ccflags-y  += -D_KERNEL=1 -DKDBG_DEVNAME=\"$(MODULE)\"

ifeq ($(ksym_demo_enable),true)
ccflags-y  += -DKSYM_DEMO_ENABLE
endif

ifeq ($(trace_demo_enable),true)
ccflags-y  += -DKDBG_TRACE_DEF_DEMO
ccflags-y  += -DKDBG_TRACE_IMPL_DEMO
ccflags-y  += -DKDBG_TRACE_IMPORT_DEMO
endif

ifeq ($(spa_demo_enable),true)
ccflags-y  += -DSPA_DEMO_ENABLE
endif

endif
