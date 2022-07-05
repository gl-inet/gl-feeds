#
# Copyright (C) 2019 Jianhui Zhao <zhaojh329@gmail.com>
#
# This is free software, licensed under the MIT.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include ./version.mk

PKG_NAME:=gl-sdk4-tertf
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gl-sdk4-tertf
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  DEPENDS:=+kmod-br-netfilter +kmod-arptables +(TARGET_ipq807x||TARGET_ipq):kmod-ebtables
  TITLE:=Bandwidth Monitor for oui
  FILES:=$(PKG_BUILD_DIR)/gl-sdk4-tertf.ko
  AUTOLOAD:=$(call AutoLoad,30,gl-sdk4-tertf,1)
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

define KernelPackage/gl-sdk4-tertf/install
	$(INSTALL_DIR) $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_CONF) ./files/gl-tertf.config $(1)/etc/config/gl-tertf
	$(INSTALL_BIN) ./files/gl-tertf.init $(1)/etc/init.d/gl-tertf
endef

$(eval $(call KernelPackage,gl-sdk4-tertf))
