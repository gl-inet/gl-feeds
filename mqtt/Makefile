#
# Copyright (C) 2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mqtt
PKG_REFV:=2
PKG_VERSION:=1.3.1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/mqtt
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=MQTT common libary
  DEPENDS:=+libpthread +libopenssl
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Build/Configure
endef

#TARGET_CFLAGS += -ggdb3
MAKE_FLAGS += CFLAGS="$(TARGET_CFLAGS) -Wall"

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/
	$(CP) $(PKG_BUILD_DIR)/src/*.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/build/output/lib* $(1)/usr/lib/
	#$(CP) $(PKG_BUILD_DIR)/build/output/MQTTVersion $(1)/usr/lib/
endef

define Package/mqtt/install
	$(INSTALL_DIR) $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/usr/lib/
	$(CP) $(PKG_BUILD_DIR)/build/output/lib* $(1)/usr/lib/
	#$(CP) $(PKG_BUILD_DIR)/build/output/MQTTVersion $(1)/usr/lib/
endef

$(eval $(call BuildPackage,mqtt))

