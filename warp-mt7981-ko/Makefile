include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=warp-mt7981-ko
PKG_VERSION:=20221209-3e6ae1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/warp-mt7981-ko
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  TITLE:=MT7981 warp driver with ko
  DEPENDS+=+kmod-mediatek_hnat
  FILES:=./files/mtk_warp.ko
  AUTOLOAD:=$(call AutoLoad,60,mtk_warp,1)
endef

Build/Compile=

define KernelPackage/warp-mt7981-ko/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) ./files/7981_WOCPU0_RAM_CODE_release.bin $(1)/lib/firmware
endef

$(eval $(call KernelPackage,warp-mt7981-ko))
