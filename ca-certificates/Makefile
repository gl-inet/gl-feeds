#
# Copyright (C) 2006-2017 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=ca-certificates
PKG_VERSION:=20260601
PKG_RELEASE:=1
PKG_MAINTAINER:=

PKG_LICENSE:=GPL-2.0-or-later MPL-2.0
PKG_LICENSE_FILES:=debian/copyright

PKG_SOURCE:=$(PKG_NAME)_$(PKG_VERSION).tar.xz
PKG_SOURCE_URL:=http://ftp.debian.org/debian/pool/main/c/ca-certificates
PKG_HASH:=7ab6301f7f34eef90a4d278647c260bc0762e0e14561f4649854cf4b0d4bea21
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

TAR_OPTIONS+= --strip-components 1
TAR_CMD=$(HOST_TAR) -C $(1) $(TAR_OPTIONS)

# ca-certs is deprecated and new packages should use ca-certificates-any if
# they don't need a specific package
define Package/ca-certificates
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=System CA certificates
  PKGARCH:=all
  PROVIDES:=ca-certs
endef

define Package/ca-bundle
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=System CA certificates as a bundle
  PKGARCH:=all
  PROVIDES:=ca-certs ca-certificates-any
endef

CA_CERTIFICATES_SIMPLE_CERTS:= \
	Amazon_Root_CA_1.crt \
	GTS_Root_R4.crt \
	Go_Daddy_Root_Certificate_Authority_-_G2.crt \
	ISRG_Root_X1.crt \
	ISRG_Root_X2.crt \
	USERTrust_RSA_Certification_Authority.crt

define Build/Install
	mkdir -p \
		$(PKG_INSTALL_DIR)/usr/sbin \
		$(PKG_INSTALL_DIR)/usr/share/ca-certificates
	$(call Build/Install/Default,)
endef

define Package/ca-certificates/install
	$(INSTALL_DIR) $(1)/etc/ssl/certs
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/share/ca-certificates/*/*.crt $(1)/etc/ssl/certs/
	cat $(addprefix $(PKG_INSTALL_DIR)/usr/share/ca-certificates/*/,$(CA_CERTIFICATES_SIMPLE_CERTS)) >$(1)/etc/ssl/certs/ca-certificates-simple.crt

	for CERTFILE in `ls -1 $(1)/etc/ssl/certs`; do \
		HASH=`openssl x509 -hash -noout -in $(1)/etc/ssl/certs/$$$$CERTFILE` ; \
		SUFFIX=0 ; \
		while [ -h "$(1)/etc/ssl/certs/$$$$HASH.$$$$SUFFIX" ]; do \
			let "SUFFIX += 1" ; \
		done ; \
		$(LN) "$$$$CERTFILE" "$(1)/etc/ssl/certs/$$$$HASH.$$$$SUFFIX" ; \
	done
endef

define Package/ca-bundle/install
	$(INSTALL_DIR) $(1)/etc/ssl/certs
	cat $(PKG_INSTALL_DIR)/usr/share/ca-certificates/*/*.crt >$(1)/etc/ssl/certs/ca-certificates.crt
	$(LN) /etc/ssl/certs/ca-certificates.crt $(1)/etc/ssl/cert.pem
endef
$(eval $(call BuildPackage,ca-bundle))
$(eval $(call BuildPackage,ca-certificates))
