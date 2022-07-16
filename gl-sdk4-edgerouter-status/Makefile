#
#
# Copyright (C) 2017 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-edgerouter-status
PKG_VERSION:=4.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk


define KernelPackage/gl-sdk4-edgerouter-status
  CATEGORY:=gl-inet
  SUBMENU:=Kernel modules
  TITLE:=Status Monitor for edgerouter
  FILES:=$(PKG_BUILD_DIR)/edgerouter-status.ko
  AUTOLOAD:=$(call AutoLoad,30,edgerouter-status,1)
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

define Package/gl-sdk4-edgerouter-status/install
	echo "install done"
endef

$(eval $(call KernelPackage,gl-sdk4-edgerouter-status))
