#
# Copyright (C) 2019 Jianhui Zhao <zhaojh329@gmail.com>
#
# This is free software, licensed under the MIT.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include ./version.mk

PKG_NAME:=usb-serial-ch343
PKG_VERSION:=1
PKG_RELEASE:=2

include $(INCLUDE_DIR)/package.mk

define KernelPackage/usb-serial-ch343
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=usb-serial-ch343 linux serial driver
  DEPENDS:=+kmod-usb-core
  FILES:=$(PKG_BUILD_DIR)/ch343.ko
  AUTOLOAD:=$(call AutoLoad,10,ch343,1)
  $(call AddDepends/usb-serial)
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)
endef

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

define KernelPackage/usb-serial-ch343/install
endef

$(eval $(call KernelPackage,usb-serial-ch343))
