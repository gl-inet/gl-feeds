#
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=leds-et6326
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define KernelPackage/leds-et6326
  SUBMENU:=LED modules
  TITLE:= ET6326 LED support
  DEPENDS:=+kmod-i2c-core
  FILES:=$(PKG_BUILD_DIR)/leds-et6326.ko
  AUTOLOAD:=$(call AutoLoad,41,leds-et6326,1)
  KCONFIG:=CONFIG_LEDS_CLASS
endef

define KernelPackage/leds-et6326/description
  LED driver for ET6326 RGB LED.
endef

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,leds-et6326))
