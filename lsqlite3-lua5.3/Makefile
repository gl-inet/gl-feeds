#
# Copyright (C) 2006-2016 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=lsqlite3-lua5.3
PKG_VERSION:=0.9.5
PKG_RELEASE:=1

PKG_SOURCE:=lsqlite3_fsl09y.zip
PKG_SOURCE_URL:=http://lua.sqlite.org/index.cgi/zip/
PKG_HASH:=d38402aa7640055d260c1246c36e6d6d31b425a25a805431f13695694466b722

PKG_LICENSE:=MIT
PKG_MAINTAINER:=Oskari Rauta <oskari.rauta@gmail.com>

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)/$(basename $(PKG_SOURCE))

include $(INCLUDE_DIR)/package.mk

define Package/lsqlite3-lua5.3
  SUBMENU:=Lua
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=Lua wrapper for the SQLite3 library
  URL:=http://lua.sqlite.org
  DEPENDS:=+libsqlite3 +liblua5.3
endef

TARGET_CFLAGS += $(FPIC) -std=gnu99
TARGET_CPPFLAGS += -DLUA_USE_LINUX
TARGET_LDFLAGS += -lsqlite3 -lpthread

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/lua5.3

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-c $(PKG_BUILD_DIR)/lsqlite3.c \
		-o $(PKG_BUILD_DIR)/lsqlite3.o \
		-DLSQLITE_VERSION=\"$(PKG_VERSION)\"
	$(TARGET_CC) $(TARGET_LDFLAGS) -shared \
		$(PKG_BUILD_DIR)/lsqlite3.o \
		-o $(PKG_BUILD_DIR)/lsqlite3.so
endef

define Package/lsqlite3-lua5.3/install
	$(INSTALL_DIR) $(1)/usr/local/lib/lua/5.3
	$(CP) $(PKG_BUILD_DIR)/*.so $(1)/usr/local/lib/lua/5.3
endef

$(eval $(call BuildPackage,lsqlite3-lua5.3))
