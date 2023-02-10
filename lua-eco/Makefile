include $(TOPDIR)/rules.mk

PKG_NAME:=lua-eco
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/zhaojh329/lua-eco.git
PKG_SOURCE_VERSION:=0a9e161859826ec974ba055e5392cb4e7c70e062
PKG_MIRROR_HASH:=41cb750906b93518417ba7a3ce458ce7673bd0dcba7e9fcefd8f0a1d1074bcfe

PKG_MAINTAINER:=Jianhui Zhao <zhaojh329@gmail.com>
PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE

PKG_CONFIG_DEPENDS:= \
	LUA_ECO_OPENSSL \
	LUA_ECO_WOLFSSL \
	LUA_ECO_MBEDTLS

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/lua-eco
  TITLE:=A Lua interpreter with a built-in libev event loop
  SECTION:=lang
  CATEGORY:=Languages
  SUBMENU:=Lua
  URL:=https://github.com/zhaojh329/lua-eco
  DEPENDS:=+libev +liblua
endef

define Package/lua-eco/description
  Lua-eco is a Lua interpreter with a built-in libev event loop. It makes all Lua code
  running in Lua coroutines so code that does I/O can be suspended until data is ready.
  This allows you write code as if you're using blocking I/O, while still allowing code
  in other coroutines to run when you'd otherwise wait for I/O. It's kind of like Goroutines.
endef

define Package/lua-eco/Module
  TITLE:=$1 support for lua-eco
  SECTION:=lang
  CATEGORY:=Languages
  SUBMENU:=Lua
  URL:=https://github.com/zhaojh329/lua-eco
  DEPENDS:=+lua-eco $2
endef

Package/lua-eco-log=$(call Package/lua-eco/Module,log utils)
Package/lua-eco-sys=$(call Package/lua-eco/Module,system utils)
Package/lua-eco-file=$(call Package/lua-eco/Module,file utils)
Package/lua-eco-socket=$(call Package/lua-eco/Module,socket,+lua-eco-file +lua-eco-sys)
Package/lua-eco-dns=$(call Package/lua-eco/Module,dns,+lua-eco-socket +luabitop)
Package/lua-eco-ssl=$(call Package/lua-eco/Module,ssl,+libopenssl)
Package/lua-eco-ubus=$(call Package/lua-eco/Module,ubus,+libubus)
Package/lua-eco-termios=$(call Package/lua-eco/Module,termios)
Package/lua-eco-http=$(call Package/lua-eco/Module,http/https client/server,+lua-eco-dns +lua-eco-ssl)
Package/lua-eco-base64=$(call Package/lua-eco/Module,base64)
Package/lua-eco-mqtt=$(call Package/lua-eco/Module,mqtt,+lua-eco-socket +lua-eco-dns +lua-mosquitto)

CMAKE_OPTIONS += \
  -DPLATFORM="openwrt" \
  -DECO_UBUS_SUPPORT=O$(if $(CONFIG_PACKAGE_lua-eco-ubus),N,FF) \
  -DECO_SSL_SUPPORT=O$(if $(CONFIG_PACKAGE_lua-eco-ssl),N,FF)

ifneq ($(CONFIG_PACKAGE_lua-eco-ssl),)
  CMAKE_OPTIONS += -DUSE_OPENSSL=ON
endif

define Package/lua-eco/Module/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/eco/core
	[ -f $(PKG_INSTALL_DIR)/usr/lib/lua/eco/core/$2.so ] && \
		$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/lua/eco/core/$2.so $(1)/usr/lib/lua/eco/core || true
	[ -f $(PKG_INSTALL_DIR)/usr/lib/lua/eco/$2.so ] && \
		$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/lua/eco/$2.so $(1)/usr/lib/lua/eco || true
	[ -f $(PKG_INSTALL_DIR)/usr/lib/lua/eco/$2.lua ] && \
		$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/lua/eco/$2.lua $(1)/usr/lib/lua/eco || true
endef

define Package/lua-eco/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/eco $(1)/usr/bin
	$(call Package/lua-eco/Module/install,$(1),time)
	$(call Package/lua-eco/Module/install,$(1),buffer)
endef

Package/lua-eco-log/install=$(call Package/lua-eco/Module/install,$1,log)
Package/lua-eco-sys/install=$(call Package/lua-eco/Module/install,$1,sys)
Package/lua-eco-dns/install=$(call Package/lua-eco/Module/install,$1,dns)
Package/lua-eco-socket/install=$(call Package/lua-eco/Module/install,$1,socket)
Package/lua-eco-ssl/install=$(call Package/lua-eco/Module/install,$1,ssl)
Package/lua-eco-file/install=$(call Package/lua-eco/Module/install,$1,file)
Package/lua-eco-ubus/install=$(call Package/lua-eco/Module/install,$1,ubus)
Package/lua-eco-termios/install=$(call Package/lua-eco/Module/install,$1,termios)
Package/lua-eco-http/install=$(call Package/lua-eco/Module/install,$1,http)
Package/lua-eco-mqtt/install=$(call Package/lua-eco/Module/install,$1,mqtt)

define Package/lua-eco-base64/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/eco/encoding
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/lua/eco/encoding/base64.so $(1)/usr/lib/lua/eco/encoding
endef

$(eval $(call BuildPackage,lua-eco))
$(eval $(call BuildPackage,lua-eco-log))
$(eval $(call BuildPackage,lua-eco-sys))
$(eval $(call BuildPackage,lua-eco-dns))
$(eval $(call BuildPackage,lua-eco-socket))
$(eval $(call BuildPackage,lua-eco-ssl))
$(eval $(call BuildPackage,lua-eco-file))
$(eval $(call BuildPackage,lua-eco-ubus))
$(eval $(call BuildPackage,lua-eco-termios))
$(eval $(call BuildPackage,lua-eco-http))
$(eval $(call BuildPackage,lua-eco-base64))
$(eval $(call BuildPackage,lua-eco-mqtt))
