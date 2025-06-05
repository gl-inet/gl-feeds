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
  DEPENDS:=+uboot-envtools
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

ifeq ($(CONFIG_TARGET_sdx75),y)
MAKE_OPTS := $(filter-out -fno-caller-saves,$(MAKE_OPTS))
endif

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

define KernelPackage/gl-sdk4-hw-info/install
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_DATA) ./files/gl-hw-env.default $(1)/etc/uci-defaults/00-gl-hw-env
ifeq ($(CONFIG_TARGET_sdx75),y)
	$(INSTALL_DIR) $(1)/lib/modules/$(UNAME_VERSION)
	$(SIGN_KEY) $(PKG_BUILD_DIR)/gl-sdk4-hw-info.ko
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gl-sdk4-hw-info.ko $(1)/lib/modules/$(UNAME_VERSION)
endif
endef

$(eval $(call KernelPackage,gl-sdk4-hw-info))
