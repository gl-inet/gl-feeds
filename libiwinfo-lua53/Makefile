include $(TOPDIR)/rules.mk

PKG_NAME:=libiwinfo-lua53
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/libiwinfo-lua53
  SUBMENU:=Lua
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=libiwinfo Lua5.3 binding
  DEPENDS:=+libiwinfo +liblua5.3
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/lua5.3
TARGET_LDFLAGS += -liwinfo -fPIC

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -shared \
		$(PKG_BUILD_DIR)/iwinfo.c \
		-o $(PKG_BUILD_DIR)/iwinfo.so
 endef

define Package/libiwinfo-lua53/install
	$(INSTALL_DIR) $(1)/usr/local/lib/lua/5.3
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/iwinfo.so $(1)/usr/local/lib/lua/5.3/iwinfo.so
endef

$(eval $(call BuildPackage,libiwinfo-lua53))
