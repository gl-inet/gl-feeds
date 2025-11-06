include $(TOPDIR)/rules.mk

PKG_NAME:=lua-htmlparser
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL=https://github.com/msva/lua-htmlparser.git
PKG_MIRROR_HASH:=e5ebc91d6f84b37072d93440f071422fa194ca31d2b916324dbe3468c36c673a
PKG_SOURCE_DATE:=2023-04-06
PKG_SOURCE_VERSION:=5ce9a775a345cf458c0388d7288e246bb1b82bff

include $(INCLUDE_DIR)/package.mk

define Package/lua-htmlparser
  SUBMENU:=Lua
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=Parse HTML text into a tree of elements with selectors
  URL:=https://github.com/msva/lua-htmlparser.git
endef

Build/Compile=

define Package/lua-htmlparser/install
	$(INSTALL_DIR) $(1)/usr/local/lib/lua/5.4/htmlparser
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/src/htmlparser/*.lua $(1)/usr/local/lib/lua/5.4/htmlparser
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/src/htmlparser.lua $(1)/usr/local/lib/lua/5.4/
endef

$(eval $(call BuildPackage,lua-htmlparser))
