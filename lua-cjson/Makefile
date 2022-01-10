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

HOST_BUILD_DEPENDS:=lua/host

include $(INCLUDE_DIR)/host-build.mk
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/lua-cjson
  SUBMENU:=Lua
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=Lua CJSON parser
  URL:=https://github.com/mpx/lua-cjson
  DEPENDS:= +lua
endef

define Package/lua-cjson/description
  Lua CJSON is a fast JSON encoding/parsing module for Lua.
endef

CMAKE_OPTIONS += \
	-DUSE_LUA=ON

CMAKE_HOST_OPTIONS += \
	-DLUA_MATH_LIBRARY=m

define Package/lua-cjson/install
	$(INSTALL_DIR) $(1)/usr/lib/lua
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cjson.so $(1)/usr/lib/lua/

	$(INSTALL_DIR) $(1)/usr/lib/lua/cjson
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lua/cjson/util.lua $(1)/usr/lib/lua/cjson
endef

$(eval $(call HostBuild))
$(eval $(call BuildPackage,lua-cjson))
