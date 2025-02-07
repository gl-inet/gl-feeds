include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qmi_wwan_q
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

#include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
PKG_VERSION:=1.0
endif

QCA_MC_SNOOPING_MAKE_OPTS:= \
        CROSS_COMPILE=$(KERNEL_CROSS) \
        ARCH=$(LINUX_KARCH) \
        KERNELPATH=$(LINUX_SRC_DIR) \
        KBUILDPATH=$(LINUX_DIR) \
        KERNELRELEASE=$(LINUX_RELEASE) \
        MDIR=$(PKG_BUILD_DIR)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qmi_wwan_q
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros, Inc.
  TITLE:=Quectel Linux USB QMI WWAN Driver 
  DEPENDS:= kmod-usb-net kmod-usb-wdm kmod-rmnet-core
  KCONFIG:=
  FILES:=$(PKG_BUILD_DIR)/qmi_wwan_q.ko
  AUTOLOAD:=$(call AutoLoad,53,qmi_wwan_q)
endef

define KernelPackage/qmi_wwan_q/description
  Quectel Linux USB QMI WWAN Driver 
endef

define Build/InstallDev
endef

#define Build/Prepare
#	mkdir -p $(PKG_BUILD_DIR)
#	$(CP) ${TOPDIR}/package/${PKG_NAME}/* $(PKG_BUILD_DIR)/
#endef

define Build/Compile
	$(MAKE) -C $(LINUX_DIR) M=$(PKG_BUILD_DIR) $(strip $(QCA_MC_SNOOPING_MAKE_OPTS)) \
		EXTRA_CFLAGS="-I$(STAGING_DIR)/usr/include/qca-nss-drv $(EXTRA_CFLAGS)"
endef

$(eval $(call KernelPackage,qmi_wwan_q))
