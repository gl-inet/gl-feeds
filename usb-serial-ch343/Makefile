#
# Copyright (C) 2019 Jianhui Zhao <zhaojh329@gmail.com>
#
# This is free software, licensed under the MIT.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include ./version.mk

PKG_NAME:=usb-serial-ch343
PKG_VERSION:=2024-4-8

PKG_SOURCE_VERSION:=05b4e1f442233a92a39295a66460f3e789c4e444
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/WCHSoftGroup/ch343ser_linux.git
PKG_MIRROR_HASH:=29acd2261dc75c923408a76427d0affa1e10cdeb54b09730bdd08e485d221b07

include $(INCLUDE_DIR)/package.mk

define KernelPackage/usb-serial-ch343
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=usb-serial-ch343 linux serial driver
  DEPENDS:=+kmod-usb-core
  FILES:=$(PKG_BUILD_DIR)/driver/ch343.ko
  AUTOLOAD:=$(call AutoLoad,10,ch343,1)
  $(call AddDepends/usb-serial)
endef

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS)/driver \
		modules
endef

define KernelPackage/usb-serial-ch343/install
endef

$(eval $(call KernelPackage,usb-serial-ch343))
