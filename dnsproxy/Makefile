# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dnsproxy
PKG_VERSION:=0.79.0
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/AdguardTeam/dnsproxy/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=fd96e9694ab9b93c5f85a551920128f31e63a2deaeab9c8aa0e7579566feef32

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/dnsproxy-$(PKG_VERSION)
PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0
PKG_BUILD_FLAGS:=no-mips16

GO_PKG:=github.com/AdguardTeam/dnsproxy
GO_PKG_LDFLAGS_X:=main.VersionString=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include ../golang/golang-package.mk

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=gl-sdk4
	TITLE:=glinet SDK4 dnsproxy module
	URL:=https://github.com/AdguardTeam/dnsproxy
	DEPENDS:=$(GO_ARCH_DEPENDS)
endef

define Package/dnsproxy/description
  A simple DNS proxy server that supports all existing DNS protocols
  including DNS-over-TLS, DNS-over-HTTPS, DNSCrypt, and DNS-over-QUIC.
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(GO_PKG_BUILD_BIN_DIR)/dnsproxy $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/dnsproxy.init $(1)/etc/init.d/dnsproxy
	$(INSTALL_DIR) $(1)/etc/dnsproxy
	$(INSTALL_CONF) ./files/etc/dnsproxy.yaml $(1)/etc/dnsproxy/dnsproxy.yaml
	$(INSTALL_DIR) $(1)/etc/dnsproxy/provider
	$(INSTALL_DATA) ./files/provider/* $(1)/etc/dnsproxy/provider/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
