include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mt-wifi-mt7981-ko
PKG_VERSION:=20220907-8b55f5
PKG_RELEASE:=7

include $(INCLUDE_DIR)/package.mk

define KernelPackage/mt-wifi-mt7981-ko
  CATEGORY:=MTK Properties
  SUBMENU:=Drivers
  TITLE:=MT7981 WiFi driver with ko
  DEPENDS+=+kmod-mediatek_hnat
  FILES:=./files/mt_wifi.ko ./files/mtk_warp_proxy.ko
  AUTOLOAD:=$(call AutoProbe,mt_wifi mtk_warp_proxy)
endef

Build/Compile=

define KernelPackage/mt-wifi-mt7981-ko/install
	$(INSTALL_DIR) $(1)/lib/firmware $(1)/etc
	$(INSTALL_DATA) ./files/MT7981_ePAeLNA_EEPROM.bin $(1)/lib/firmware
	$(INSTALL_DATA) ./files/MT7981_iPAiLNA_EEPROM.bin $(1)/lib/firmware
	$(CP) ./files/wireless $(1)/etc
endef

$(eval $(call KernelPackage,mt-wifi-mt7981-ko))
