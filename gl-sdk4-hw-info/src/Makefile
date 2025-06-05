include $(TOPDIR)/.config

ifeq ($(CONFIG_TARGET_sdx75),y)
PRJ_PATH ?= $(shell pwd)

KERNEL_SRC ?= ./*.c
endif
gl-sdk4-hw-info-objs := proc.o factory_data.o iso3166.o main.o
obj-m := gl-sdk4-hw-info.o
ifeq ($(CONFIG_TARGET_sdx75),y)
ccflags-y += -DCONFIG_TARGET_sdx75

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PRJ_PATH) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PRJ_PATH) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PRJ_PATH) clean
endif
