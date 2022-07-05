#
# Copyright (C) 2019 Jianhui Zhao <zhaojh329@gmail.com>
#
# This is free software, licensed under the MIT.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include ./version.mk

PKG_NAME:=gl-sdk4-hw-info
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-hw-info
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=GL.iNet hardware information
  FILES:=$(PKG_BUILD_DIR)/gl-sdk4-hw-info.ko
  AUTOLOAD:=$(call AutoLoad,01,gl-sdk4-hw-info,1)
  KCONFIG:=
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

$(eval $(call KernelPackage,gl-sdk4-hw-info))
