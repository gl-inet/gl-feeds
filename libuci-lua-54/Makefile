include $(TOPDIR)/rules.mk

PKG_NAME:=libuci-lua54
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/libuci-lua54
  SECTION=libs
  CATEGORY=Libraries
  DEPENDS:=+libuci +liblua5.4
  TITLE:=Lua5.4 plugin for UCI
endef

TARGET_CFLAGS += $(FPIC) -I$(STAGING_DIR)/usr/include/lua5.4
TARGET_LDFLAGS += -luci -llua5.4

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -shared \
		$(PKG_BUILD_DIR)/uci.c \
		-o $(PKG_BUILD_DIR)/uci.so
endef

define Package/libuci-lua54/install
	$(INSTALL_DIR) $(1)/usr/local/lib/lua/5.4
	$(CP) $(PKG_BUILD_DIR)/uci.so $(1)/usr/local/lib/lua/5.4
endef

$(eval $(call BuildPackage,libuci-lua54))
