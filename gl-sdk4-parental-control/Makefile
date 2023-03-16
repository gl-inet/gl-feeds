
include $(TOPDIR)/rules.mk
include ./version.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gl-sdk4-parental-control
PKG_VERSION:=4.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/$(PKG_NAME)
  SECTION:=base
  CATEGORY:=gl-sdk4
  SUBMENU:=Kernel modules
  TITLE:=glinet parental control
  FILES:=$(PKG_BUILD_DIR)/parental_control.ko 
  DEPENDS:=+kmod-ipt-conntrack
endef

KERNEL_MAKE_FLAGS?= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)"

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)"

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

define KernelPackage/$(PKG_NAME)/conffiles
/etc/parental_control/app_feature.cfg
endef

 
define KernelPackage/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin  $(1)/etc/init.d $(1)/lib/functions
	$(INSTALL_BIN) ./files/pc_schedule.sh $(1)/usr/bin/pc_schedule
	$(INSTALL_BIN) ./files/parental_control.init $(1)/etc/init.d/parental_control
	$(INSTALL_BIN) ./files/parental_control.sh $(1)/lib/functions

	$(INSTALL_DIR) $(1)/usr/lib/oui-httpd/rpc $(1)/etc/parental_control  $(1)/etc/config
	$(CP) ./files/parental_control.lua $(1)/usr/lib/oui-httpd/rpc/parental-control
	$(CP) ./files/app_feature.cfg $(1)/etc/parental_control/app_feature.cfg
	$(CP) ./files/parental_control.config $(1)/etc/config/parental_control
endef

$(eval $(call KernelPackage,$(PKG_NAME)))

