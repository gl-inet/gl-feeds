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

DNSCRYPT_VERSION:=2.4.0
DNSCRYPT_SOURCE:=dnscrypt-$(DNSCRYPT_VERSION).tar.gz
DNSCRYPT_URL:=https://codeload.github.com/ameshkov/dnscrypt/tar.gz/v$(DNSCRYPT_VERSION)?
DNSCRYPT_HASH:=a3b01bd53a169369a3c96f7aa60516fbb843af0f58cd2b866c38b52a4e4344c1

GO_MOD_PROXY?=https://proxy.golang.org,direct
GO_MOD_SUMDB?=sum.golang.org

PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR=$(BUILD_DIR)/$(if $(BUILD_VARIANT),$(PKG_NAME)-$(BUILD_VARIANT)/)$(PKG_NAME)-$(PKG_VERSION)
PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
PKG_USE_MIPS16:=0
PKG_BUILD_FLAGS:=no-mips16

GO_PKG:=github.com/AdguardTeam/dnsproxy
GO_PKG_LDFLAGS_X:=main.VersionString=$(PKG_VERSION)
GO_PKG_TAGS+=dnsproxy_trim_downstream

include $(INCLUDE_DIR)/package.mk
include ../golang/golang-package.mk

define Download/dnscrypt
	FILE:=$(DNSCRYPT_SOURCE)
	URL:=$(DNSCRYPT_URL)
	HASH:=$(DNSCRYPT_HASH)
endef
$(eval $(call Download,dnscrypt))

define Package/$(PKG_NAME)
	SECTION:=net
	CATEGORY:=gl-sdk4
	TITLE:=glinet SDK4 dnsproxy module
	URL:=https://github.com/AdguardTeam/dnsproxy
	DEPENDS:=$(GO_ARCH_DEPENDS)
endef

define Package/dnsproxy/description
  GL.iNet dnsproxy build with plain downstream DNS listeners and the currently
  used encrypted upstream protocols.  Unused encrypted downstream listeners are
  compiled out to reduce runtime memory usage.
endef

define Package/dnsproxy/config
config DNSPROXY_UPX
	bool "Compress dnsproxy binary with UPX"
	default n
	help
	  Compress the final Go binary before packaging.  This can reduce flash
	  usage significantly, but may increase anonymous RSS at runtime.
	  Requires a working upx-ucl or upx command in the build environment.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	rm -rf $(PKG_BUILD_DIR)/../dnscrypt
	mkdir -p $(PKG_BUILD_DIR)/../dnscrypt
	$(TAR) -xzf $(DL_DIR)/$(DNSCRYPT_SOURCE) -C $(PKG_BUILD_DIR)/../dnscrypt --strip-components=1
	cd $(PKG_BUILD_DIR)/../dnscrypt && patch -p1 < $(CURDIR)/patches-dnscrypt/0001-support-setting-upstream-query-mark-for-vpn-policy.patch
	cd $(PKG_BUILD_DIR) && \
		go mod edit -replace=github.com/ameshkov/dnscrypt/v2=../dnscrypt && \
		GOPROXY=$(GO_MOD_PROXY) GOSUMDB=$(GO_MOD_SUMDB) go mod tidy
endef

define Dnsproxy/UPX
	$(if $(filter y,$(CONFIG_DNSPROXY_UPX)), \
		UPX_BIN="$$$$(command -v upx-ucl || command -v upx)"; \
		if [ -z "$$$$UPX_BIN" ]; then \
			echo "dnsproxy: CONFIG_DNSPROXY_UPX requires upx-ucl or upx in PATH" >&2; \
			exit 1; \
		fi; \
		if ! "$$$$UPX_BIN" --version 2>&1 | grep -Eq "Ultimate Packer|UCL data compression"; then \
			echo "dnsproxy: $$$$UPX_BIN is not the UPX executable packer" >&2; \
			exit 1; \
		fi; \
		$(TARGET_CROSS)strip --strip-all "$(1)/usr/sbin/dnsproxy"; \
		"$$$$UPX_BIN" -9 "$(1)/usr/sbin/dnsproxy"; \
	)
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(GO_PKG_BUILD_BIN_DIR)/dnsproxy $(1)/usr/sbin/
	$(call Dnsproxy/UPX,$(1))
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/dnsproxy.init $(1)/etc/init.d/dnsproxy
	$(INSTALL_DIR) $(1)/etc/dnsproxy
	$(INSTALL_CONF) ./files/etc/dnsproxy.yaml $(1)/etc/dnsproxy/dnsproxy.yaml
	$(INSTALL_DIR) $(1)/etc/dnsproxy/provider
	$(INSTALL_DATA) ./files/provider/* $(1)/etc/dnsproxy/provider/
	$(INSTALL_DIR) $(1)/lib/upgrade/keep.d
	$(INSTALL_DATA) ./files/dnsproxy.upgrade $(1)/lib/upgrade/keep.d/dnsproxy
endef

define Package/dnsproxy/conffiles
/etc/dnsproxy/dnsproxy.yaml
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
