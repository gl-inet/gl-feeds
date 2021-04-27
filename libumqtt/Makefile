#
# Copyright (C) 2014-2017 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libumqtt
PKG_VERSION:=1.1.0
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=v$(PKG_VERSION)
PKG_SOURCE_URL=https://github.com/zhaojh329/libumqtt.git
PKG_MIRROR_HASH:=7c6634f73538f2001943c312e1d1bd7f940a205a40ea074dbaa6a23c7575baed
CMAKE_INSTALL:=1

PKG_BUILD_DIR=$(BUILD_DIR)/$(PKG_NAME)-$(BUILD_VARIANT)/$(PKG_SOURCE_SUBDIR)

PKG_MAINTAINER:=Jianhui Zhao <zhaojh329@gmail.com>
PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/libumqtt/Default
  SECTION:=libs
  CATEGORY:=Libraries
  SUBMENU:=Networking
  TITLE:=libumqtt
  DEPENDS:=+libev $(2)
  VARIANT:=$(1)
  PROVIDES:=libumqtt
endef

define Package/libumqtt/Default/description
A Lightweight and fully asynchronous MQTT 3.1.1 client C library based on
libubox for Embedded Linux. Support QoS 0, 1 and 2. Support ssl.
endef

Package/libumqtt-openssl=$(call Package/libumqtt/Default,openssl,+PACKAGE_libumqtt-openssl:libopenssl)
Package/libumqtt-wolfssl=$(call Package/libumqtt/Default,wolfssl,+PACKAGE_libumqtt-wolfssl:libwolfssl)
Package/libumqtt-mbedtls=$(call Package/libumqtt/Default,mbedtls,+PACKAGE_libumqtt-mbedtls:libmbedtls +PACKAGE_libumqtt-mbedtls:zlib)
Package/libumqtt-nossl=$(call Package/libumqtt/Default,nossl)

Package/libumqtt-nossl/description = $(Package/libumqtt/Default/description)
Package/libumqtt-openssl/description = $(Package/libumqtt/Default/description)
Package/libumqtt-wolfssl/description = $(Package/libumqtt/Default/description)
Package/libumqtt-mbedtls/description = $(Package/libumqtt/Default/description)

ifeq ($(BUILD_VARIANT),openssl)
  CMAKE_OPTIONS += -DUSE_OPENSSL=ON
else ifeq ($(BUILD_VARIANT),wolfssl)
  CMAKE_OPTIONS += -DUSE_WOLFSSL=ON
else ifeq ($(BUILD_VARIANT),mbedtls)
  CMAKE_OPTIONS += -DUSE_MBEDTLS=ON
else
  CMAKE_OPTIONS += -DSSL_SUPPORT=OFF
endif

define Package/libumqtt-$(BUILD_VARIANT)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/libumqtt.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libumqtt-openssl))
$(eval $(call BuildPackage,libumqtt-mbedtls))
$(eval $(call BuildPackage,libumqtt-wolfssl))
$(eval $(call BuildPackage,libumqtt-nossl))
