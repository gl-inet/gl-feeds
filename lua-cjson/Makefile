#
# Copyright (C) 2015 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=lua-cjson
PKG_VERSION:=2.1.0
PKG_RELEASE:=1
PKG_MAINTAINER:=Dirk Chang <dirk@kooiot.com>
PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/openresty/lua-cjson.git
PKG_MIRROR_HASH:=892c3e70a9675d6ba4d1ae47a8dbc4d791664593d506f4d211ca08440b6b62a1
PKG_SOURCE_DATE:=2021-10-25
PKG_SOURCE_VERSION:=0f3dda34a35d93b92aba39a6c7b30d1d23918812

include $(INCLUDE_DIR)/package.mk

define Package/lua-cjson/default
  SUBMENU:=Lua
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=Lua CJSON parser
  URL:=https://github.com/mpx/lua-cjson
endef

define Package/lua-cjson
  $(Package/lua-cjson/default)
  DEPENDS+=+liblua
  VARIANT:=lua-51
endef

define Package/lua-cjson-lua5.3
  $(Package/lua-cjson/default)
  DEPENDS+=+liblua5.3
  VARIANT:=lua-53
endef

TARGET_CFLAGS += $(FPIC) -std=gnu99

ifeq ($(BUILD_VARIANT),lua-53)
  TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/lua5.3
endif

TARGET_LDFLAGS += -lm

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -shared \
		$(PKG_BUILD_DIR)/fpconv.c \
    $(PKG_BUILD_DIR)/lua_cjson.c \
    $(PKG_BUILD_DIR)/strbuf.c \
		-o $(PKG_BUILD_DIR)/cjson.so
endef

define Package/lua-cjson/install
	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cjson.so $(1)/usr/lib/lua/

	$(INSTALL_DIR) $(1)/usr/lib/lua/cjson
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lua/cjson/util.lua $(1)/usr/lib/lua/cjson
endef

define Package/lua-cjson-lua5.3/install
	$(INSTALL_DIR) $(1)/usr/local/lib/lua/5.3/cjson
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cjson.so $(1)/usr/local/lib/lua/5.3
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lua/cjson/util.lua $(1)/usr/local/lib/lua/5.3/cjson
endef

$(eval $(call BuildPackage,lua-cjson))
$(eval $(call BuildPackage,lua-cjson-lua5.3))
