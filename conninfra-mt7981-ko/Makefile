include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=conninfra-mt7981-ko
PKG_VERSION:=20220907-bbf588
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/conninfra-mt7981-ko
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  TITLE:=Conninfra driver with ko
  FILES:=./files/conninfra.ko
  AUTOLOAD:=$(call AutoLoad,10,conninfra,1)
endef

Build/Compile=

$(eval $(call KernelPackage,conninfra-mt7981-ko))
