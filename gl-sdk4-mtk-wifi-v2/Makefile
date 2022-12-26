include $(TOPDIR)/rules.mk
include version.mk

PKG_NAME:=gl-sdk4-mtk-wifi-v2
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/gl-sdk4-mtk-wifi-v2
  SECTION:=base
  CATEGORY:=gl-sdk4
  TITLE:=MTK WiFi Configure for Gl.inet
  DEPENDS:=+lua +libuci-lua +libubox-lua +libubus-lua
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/gl-sdk4-mtk-wifi-v2/install
	$(INSTALL_DIR) $(1)/etc/config $(1)/lib/netifd/wireless $(1)/usr/sbin \
		$(1)/etc/init.d $(1)/usr/share $(1)/etc/hotplug.d/firmware \
		$(1)/sbin $(1)/usr/lib/lua
	$(INSTALL_CONF) ./files/wireless.config $(1)/etc/config/wireless
	$(INSTALL_CONF) ./files/wifi-features-mt798111 $(1)/usr/share
	$(INSTALL_CONF) ./files/wifi-features-mt798112 $(1)/usr/share
	$(INSTALL_BIN) ./files/mtk.sh $(1)/lib/netifd/wireless
	$(INSTALL_BIN) ./files/mtk-wifi-configurator.lua $(1)/usr/sbin/mtk-wifi-configurator
	$(INSTALL_BIN) ./files/mtk-wifi-configurator.init $(1)/etc/init.d/mtk-wifi-configurator
	$(INSTALL_DATA) ./files/mtk-wifi-e2p.hotplug $(1)/etc/hotplug.d/firmware/11-mtk-wifi-e2p
	$(INSTALL_BIN) ./files/l1dat.lua $(1)/sbin/l1dat
	$(INSTALL_BIN) ./files/smp.sh $(1)/sbin
	$(INSTALL_DATA) ./files/l1dat_parser.lua $(1)/usr/lib/lua
	$(CP) $(PKG_BUILD_DIR)/iwpriv.so $(1)/usr/lib/lua
endef

$(eval $(call BuildPackage,gl-sdk4-mtk-wifi-v2))
