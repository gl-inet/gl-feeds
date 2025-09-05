#
# Copyright (C) 2021 Jianhui Zhao <zhaojh329@gmail.com>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=ovpn-dco
PKG_VERSION:=0.2.20241216
PKG_RELEASE:=1

PKG_SOURCE:=v$(PKG_VERSION).tar.gz
PKG_SOURCE_URL=https://github.com/OpenVPN/ovpn-dco/archive/refs/tags/
PKG_HASH:=dfc30c86e686779635b31f433fc4908d

PKG_MAINTAINER:=Jianhui Zhao <zhaojh329@gmail.com>
PKG_LICENSE:=GPL-2.0-only


include $(INCLUDE_DIR)/package.mk

define KernelPackage/ovpn-dco-v2/aarch64
  DEPENDS+=+kmod-crypto-aead +kmod-crypto-gcm +kmod-crypto-chacha20poly1305
  KCONFIG+=CONFIG_ARM64_CRYPTO=y CONFIG_CRYPTO_SHA2_ARM64_CE=y CONFIG_CRYPTO_GHASH_ARM64_CE=y CONFIG_CRYPTO_AES_ARM64_CE_CCM=y CONFIG_CRYPTO_AES_ARM64_CE_BLK=y CONFIG_CRYPTO_SHA1_ARM64_CE=y CONFIG_CRYPTO_SHA512_ARM64=y CONFIG_CRYPTO_SHA512_ARM64_CE=y CONFIG_CRYPTO_CHACHA20_NEON=y CONFIG_CRYPTO_POLY1305_NEON=y \
	$(if $(CONFIG_CRYPTO_SHA3_ARM64),,CONFIG_CRYPTO_SHA3_ARM64=n) \
	$(if $(CONFIG_CRYPTO_SM3_ARM64_CE),,CONFIG_CRYPTO_SM3_ARM64_CE=n) \
	$(if $(CONFIG_CRYPTO_SM4_ARM64_CE),,CONFIG_CRYPTO_SM4_ARM64_CE=n) \
	$(if $(CONFIG_CRYPTO_AES_ARM64_NEON_BLK),,CONFIG_CRYPTO_AES_ARM64_NEON_BLK=n) \
	$(if $(CONFIG_CRYPTO_NHPOLY1305_NEON),,CONFIG_CRYPTO_NHPOLY1305_NEON=n) \
	$(if $(CONFIG_CRYPTO_AES_ARM64_BS),,CONFIG_CRYPTO_AES_ARM64_BS=n)
endef

define KernelPackage/ovpn-dco-v2
  $(info ARCH=$(ARCH))
  $(info BOARD=$(BOARD))
  $(info SUBTARGET=$(SUBTARGET))
  $(info CPU_TYPE=$(CPU_TYPE))
  $(info CPU_SUBTYPE=$(CPU_SUBTYPE))
  $(info TARGETID=$(TARGETID))
  $(info TARGETID_FULL=$(TARGETID_FULL))
  $(info profile = $(CONFIG_TARGET_PROFILE))
  SUBMENU:=Network Support
  TITLE:=OpenVPN data channel offload
  DEPENDS:=+kmod-udptunnel4 +IPV6:kmod-udptunnel6 +kmod-crypto-aead +kmod-crypto-gcm +kmod-crypto-chacha20poly1305
  KCONFIG:=CONFIG_CRYPTO_CHACHA20=y \
	CONFIG_CRYPTO_POLY1305=y
  FILES:=$(PKG_BUILD_DIR)/drivers/net/ovpn-dco/ovpn-dco-v2.ko
  AUTOLOAD:=$(call AutoLoad,30,ovpn-dco-v2)
endef

define KernelPackage/ovpn-dco-v2/description
  This module enhances the performance of the OpenVPN userspace software
  by offloading the data channel processing to kernelspace.
endef

NOSTDINC_FLAGS += \
	$(KERNEL_NOSTDINC_FLAGS) \
	-I$(PKG_BUILD_DIR)/include \
	-I$(PKG_BUILD_DIR)/compat-include \
	-include $(PKG_BUILD_DIR)/linux-compat.h

EXTRA_KCONFIG:= \
	CONFIG_OVPN_DCO_V2=m

PKG_EXTMOD_SUBDIRS = drivers/net/ovpn-dco

MAKE_OPTS:= \
	$(KERNEL_MAKE_FLAGS) \
	M="$(PKG_BUILD_DIR)/drivers/net/ovpn-dco" \
	NOSTDINC_FLAGS="$(NOSTDINC_FLAGS)" \
	$(EXTRA_KCONFIG)

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,ovpn-dco-v2))
