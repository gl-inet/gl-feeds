# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dnsproxy
PKG_VERSION:=0.75.5
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=gl-sdk4
	TITLE:= glinet SDK4 dnsproxy module
	DEPENDS:= +libuci-lua
endef

define Package/dnsproxy/description
  A lightweight DNS proxy for GL-iNet routers.
endef

define Build/Compile
  true
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/etc/dnsproxy.init $(1)/etc/init.d/dnsproxy
	$(INSTALL_DIR) $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/dnsproxy $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/dnsproxy/
	$(INSTALL_CONF) ./files/etc/dnsproxy.yaml $(1)/etc/dnsproxy/dnsproxy.yaml
	$(INSTALL_DIR) $(1)/etc/dnsproxy/provider
	$(INSTALL_DATA) ./files/provider/* $(1)/etc/dnsproxy/provider/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
