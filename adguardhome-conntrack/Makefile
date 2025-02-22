#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=adguardhome-conntrack
PKG_VERSION:=0.107.56
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/AdguardTeam/AdGuardHome/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=e4285c8d7611fe677e45dad469fbc0237ebeea57094148a059edd5aa32ab8fce

DNSPROXY_VERSION:=0.73.5
DNSPROXY_SOURCE:=dnsproxy-$(DNSPROXY_VERSION).tar.gz
DNSPROXY_URL:=https://codeload.github.com/AdguardTeam/dnsproxy/tar.gz/v$(DNSPROXY_VERSION)?
DNSPROXY_HASH:=fc5a7335048cc488906c540ad6562b4662aba8e3edcad77ebd1075ee3ed3792e

FRONTEND_FILE:=AdGuardHome_frontend-$(PKG_VERSION).tar.gz
FRONTEND_ORIG:=AdGuardHome_frontend.tar.gz
FRONTEND_URL:=https://github.com/AdguardTeam/AdGuardHome/releases/download/v$(PKG_VERSION)
FRONTEND_HASH:=f7d6308d17ed7ac97b62e4cb5bec79a647695ade94b91a8803a6dc0fdbac407f

PKG_BUILD_DIR:=$(BUILD_DIR)/AdGuardHome-$(PKG_VERSION)

PKG_LICENSE:=GPL-3.0-only
PKG_LICENSE_FILES:=LICENSE.txt
PKG_MAINTAINER:=Dobroslaw Kijowski <dobo90@gmail.com>

PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_BUILD_FLAGS:=no-mips16

GO_PKG:=github.com/AdguardTeam/AdGuardHome
GO_PKG_BUILD_PKG:=github.com/AdguardTeam/AdGuardHome

AGH_BUILD_TIME:=$(shell date -d @$(SOURCE_DATE_EPOCH) +%FT%TZ%z)
AGH_VERSION_PKG:=github.com/AdguardTeam/AdGuardHome/internal/version
GO_PKG_LDFLAGS_X:=$(AGH_VERSION_PKG).channel=release \
	$(AGH_VERSION_PKG).version=$(PKG_VERSION) \
	$(AGH_VERSION_PKG).buildtime=$(AGH_BUILD_TIME) \
	$(AGH_VERSION_PKG).goarm=$(GO_ARM) \
	$(AGH_VERSION_PKG).gomips=$(GO_MIPS)

include $(INCLUDE_DIR)/package.mk
include ../golang/golang-package.mk

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=Network
	TITLE:=Network-wide ads and trackers blocking DNS server
	URL:=https://github.com/AdguardTeam/AdGuardHome
	DEPENDS:=$(GO_ARCH_DEPENDS) +ca-bundle
endef

define Package/$(PKG_NAME)/description
Free and open source, powerful network-wide ads and trackers blocking DNS server.
endef

define Download/dnsproxy
	FILE:=$(DNSPROXY_SOURCE)
	URL:=$(DNSPROXY_URL)
	HASH:=$(DNSPROXY_HASH)
endef
$(eval $(call Download,dnsproxy))

define Download/frontend
	FILE:=$(FRONTEND_ORIG)
	URL:=$(FRONTEND_URL)
	HASH:=$(FRONTEND_HASH)
endef
$(eval $(call Download,frontend))

define Build/Prepare
	$(call Build/Prepare/Default)
	mv $(DL_DIR)/$(FRONTEND_ORIG) $(DL_DIR)/$(FRONTEND_FILE)
	tar -xzf $(DL_DIR)/$(FRONTEND_FILE) -C $(PKG_BUILD_DIR)
	rm -rf $(PKG_BUILD_DIR)/../dnsproxy
	mkdir -p $(PKG_BUILD_DIR)/../dnsproxy
	$(TAR) -xzf $(DL_DIR)/$(DNSPROXY_SOURCE) -C $(PKG_BUILD_DIR)/../dnsproxy --strip-components=1
	cd $(PKG_BUILD_DIR)/../dnsproxy && patch -p1 < $(CURDIR)/patches-dnsproxy/0001-upstream-dns-query-mark-sync-with-downstream-connect.patch
	cd $(PKG_BUILD_DIR) && \
		go mod edit -replace=github.com/AdguardTeam/dnsproxy=../dnsproxy && \
		go get github.com/florianl/go-conntrack@v0.4.0
endef

define Build/Compile
	$(call GoPackage/Build/Compile)
endef

define Package/$(PKG_NAME)/install
	$(call GoPackage/Package/Install/Bin,$(1))
endef

$(eval $(call GoBinPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)))
