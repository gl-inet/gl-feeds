include $(TOPDIR)/rules.mk

PKG_NAME:=libuci-lua53
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/libuci-lua53
  SECTION=libs
  CATEGORY=Libraries
  DEPENDS:=+libuci +liblua5.3
  TITLE:=Lua5.3 plugin for UCI
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/lua5.3
TARGET_LDFLAGS += -luci

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -shared \
		$(PKG_BUILD_DIR)/uci.c \
		-o $(PKG_BUILD_DIR)/uci.so
endef

define Package/libuci-lua53/install
	$(INSTALL_DIR) $(1)/usr/local/lib/lua/5.3
	$(CP) $(PKG_BUILD_DIR)/uci.so $(1)/usr/local/lib/lua/5.3
endef

$(eval $(call BuildPackage,libuci-lua53))
