include $(TOPDIR)/rules.mk

PKG_NAME:=libiwinfo-lua54
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/libiwinfo-lua54
  SUBMENU:=Lua
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=libiwinfo Lua5.4 binding
  DEPENDS:=+libiwinfo +liblua5.4
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/lua5.4
TARGET_LDFLAGS += -liwinfo -llua5.4 -fPIC

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -shared \
		$(PKG_BUILD_DIR)/iwinfo.c \
		-o $(PKG_BUILD_DIR)/iwinfo.so
endef

define Package/libiwinfo-lua54/install
	$(INSTALL_DIR) $(1)/usr/local/lib/lua/5.4
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/iwinfo.so $(1)/usr/local/lib/lua/5.4/iwinfo.so
endef

$(eval $(call BuildPackage,libiwinfo-lua54))
