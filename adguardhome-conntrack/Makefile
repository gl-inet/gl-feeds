#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=adguardhome-conntrack
PKG_VERSION:=0.107.73
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/AdguardTeam/AdGuardHome/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=f5f2c97dc1425ba1445d0c4050911e6924f153bd64926f27c677087586f9a4fb

DNSPROXY_VERSION:=0.78.2
DNSPROXY_SOURCE:=dnsproxy-$(DNSPROXY_VERSION).tar.gz
DNSPROXY_URL:=https://codeload.github.com/AdguardTeam/dnsproxy/tar.gz/v$(DNSPROXY_VERSION)?
DNSPROXY_HASH:=dabb78173f518da7c2a5394cf52e3f6cdd09d9cb48f3dafa59f5b5788459bf75

DNSCRYPT_VERSION:=2.4.0
DNSCRYPT_SOURCE:=dnscrypt-$(DNSCRYPT_VERSION).tar.gz
DNSCRYPT_URL:=https://codeload.github.com/ameshkov/dnscrypt/tar.gz/v$(DNSCRYPT_VERSION)?
DNSCRYPT_HASH:=a3b01bd53a169369a3c96f7aa60516fbb843af0f58cd2b866c38b52a4e4344c1

FRONTEND_FILE:=AdGuardHome_frontend-$(PKG_VERSION).tar.gz
FRONTEND_ORIG:=AdGuardHome_frontend.tar.gz
FRONTEND_URL:=https://github.com/AdguardTeam/AdGuardHome/releases/download/v$(PKG_VERSION)
FRONTEND_HASH:=a69c540fe6959575330bdea150e31eee0c0c204985bf50e3522ce6d292e3c64e

GO_MOD_PROXY?=https://proxy.golang.org,direct
GO_MOD_SUMDB?=sum.golang.org

PKG_BUILD_DIR:=$(BUILD_DIR)/AdGuardHome-$(PKG_VERSION)

PKG_LICENSE:=GPL-3.0-only
PKG_LICENSE_FILES:=LICENSE.txt
PKG_MAINTAINER:=Dobroslaw Kijowski <dobo90@gmail.com>

ifeq ($(CONFIG_TARGET_ath79_nand_DEVICE_glinet_gl-e750),)
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
endif

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

define Download/dnscrypt
	FILE:=$(DNSCRYPT_SOURCE)
	URL:=$(DNSCRYPT_URL)
	HASH:=$(DNSCRYPT_HASH)
endef
$(eval $(call Download,dnscrypt))

define Download/frontend
	FILE:=$(FRONTEND_ORIG)
	URL:=$(FRONTEND_URL)
	HASH:=$(FRONTEND_HASH)
endef
$(eval $(call Download,frontend))

define Build/Prepare
	$(call Build/Prepare/Default)
	if [ ! -f $(DL_DIR)/$(FRONTEND_FILE) ] && [ -f $(DL_DIR)/$(FRONTEND_ORIG) ]; then \
		cp $(DL_DIR)/$(FRONTEND_ORIG) $(DL_DIR)/$(FRONTEND_FILE); \
	fi
	tar -xzf $(DL_DIR)/$(FRONTEND_FILE) -C $(PKG_BUILD_DIR)
	rm -rf $(PKG_BUILD_DIR)/../dnsproxy
	mkdir -p $(PKG_BUILD_DIR)/../dnsproxy
	$(TAR) -xzf $(DL_DIR)/$(DNSPROXY_SOURCE) -C $(PKG_BUILD_DIR)/../dnsproxy --strip-components=1
	cd $(PKG_BUILD_DIR)/../dnsproxy && patch -p1 < $(CURDIR)/patches-dnsproxy/0001-upstream-dns-query-mark-sync-with-downstream-connect.patch
	cd $(PKG_BUILD_DIR)/../dnsproxy && \
		GOPROXY=$(GO_MOD_PROXY) GOSUMDB=$(GO_MOD_SUMDB) go get github.com/florianl/go-conntrack@v0.4.0 && \
		GOPROXY=$(GO_MOD_PROXY) GOSUMDB=$(GO_MOD_SUMDB) go mod tidy
	rm -rf $(PKG_BUILD_DIR)/../dnscrypt
	mkdir -p $(PKG_BUILD_DIR)/../dnscrypt
	$(TAR) -xzf $(DL_DIR)/$(DNSCRYPT_SOURCE) -C $(PKG_BUILD_DIR)/../dnscrypt --strip-components=1
	cd $(PKG_BUILD_DIR)/../dnscrypt && patch -p1 < $(CURDIR)/patches-dnscrypt/0001-support-setting-upstream-query-mark-for-vpn-policy.patch
	cd $(PKG_BUILD_DIR) && \
		go mod edit -replace=github.com/AdguardTeam/dnsproxy=../dnsproxy && \
		go mod edit -replace=github.com/ameshkov/dnscrypt/v2=../dnscrypt && \
		GOPROXY=$(GO_MOD_PROXY) GOSUMDB=$(GO_MOD_SUMDB) go get github.com/florianl/go-conntrack@v0.4.0 && \
		GOPROXY=$(GO_MOD_PROXY) GOSUMDB=$(GO_MOD_SUMDB) go mod tidy
endef

define Build/Compile
	$(call GoPackage/Build/Compile)
endef

define Package/$(PKG_NAME)/install
	$(call GoPackage/Package/Install/Bin,$(1))
endef

define Package/$(PKG_NAME)/conffiles
	/etc/AdGuardHome/config.yaml
endef

$(eval $(call GoBinPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)))
