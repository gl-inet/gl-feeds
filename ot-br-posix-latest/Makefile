#
#  Copyright (c) 2020, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

include $(TOPDIR)/rules.mk
include version.mk

PKG_NAME := ot-br-posix-latest
PKG_LICENSE := BSD-3-Clause
PKG_LICENSE_FILES := LICENSE
PKG_SOURCE_VERSION:=2ed96e74f99610d98209c3857b886e683c9f2a56
PKG_VERSION:=2023-7-12-r4.3.1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/lancersky/ot-br-posix.git
PKG_MIRROR_HASH:=0c066fb9a20784504a651d5fa1511a30375a91a1829f81a0fb3473af1b7dda1d
PKG_RELEASE:=$(strip $(call findrev))
PKG_USE_MIPS16:=0

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

PKG_CONFIG_DEPENDS:= \
	CONFIG_OTBR_WEB \
	CONFIG_OTBR_TTY_DRIVER_CH343 \
	CONFIG_OTBR_TTY_DRIVER_CDC_ACM

TARGET_CFLAGS += \
	-DOPENTHREAD_POSIX_CONFIG_DAEMON_SOCKET_BASENAME=\\\"/var/run/openthread-%s\\\" \
	-DOPENTHREAD_CONFIG_COMMISSIONER_MAX_JOINER_ENTRIES=100 \
	-DOPENTHREAD_CONFIG_PLATFORM_RADIO_SPINEL_RX_FRAME_BUFFER_SIZE=10240 \
	-DOPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS=8192 \
	-DOPENTHREAD_CONFIG_MLE_MAX_CHILDREN=100 \
	-DOPENTHREAD_CONFIG_TMF_ADDRESS_CACHE_ENTRIES=256 \
	-DOPENTHREAD_CONFIG_TCP_ENABLE=0 \
	-DOPENTHREAD_CONFIG_OPENWRT_EXTENSION=1

define Package/ot-br-posix-latest
	SECTION := base
	CATEGORY := Network
	TITLE := OpenThread Border Router
	DEPENDS := +libstdcpp +libjson-c +libubus +libblobmsg-json +libreadline +libuci +mdnsresponder +libnetfilter-queue \
			   +kmod-ipt-nat6 +kmod-tun +iptables-mod-extra +ipset +kmod-usb-serial \
			   +OTBR_WEB:jsoncpp +OTBR_WEB:boost +OTBR_WEB:boost-filesystem +OTBR_WEB:boost-system
	MENU:=1
endef

define Package/ot-br-posix-latest/config
menu "Configuration"
	depends on PACKAGE_$(1)

config OTBR_WEB
	bool "Enable OTBR_WEB"
	default n

choice
	prompt "USB to UART Converter Driver Select"
	default OTBR_TTY_DRIVER_CH343

	config OTBR_TTY_DRIVER_CH343
		bool "CH343"
		select PACKAGE_kmod-usb-serial-ch343

	config OTBR_TTY_DRIVER_CDC_ACM
		bool "CDC_ACM"
		select PACKAGE_kmod-usb-acm
endchoice
endmenu
endef

define Package/ot-br-posix-latest/conffiles
/etc/openthread/
endef

define Package/ot-br-posix-latest/description
    A Thread border router for POSIX-based platforms.
endef

CMAKE_OPTIONS += \
	-DBUILD_TESTING=OFF \
	-DOT_PACKAGE_NAME=${PKG_NAME} \
	-DOT_PACKAGE_VERSION=${PKG_VERSION} \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_FULL_DATADIR=/usr/share \
	-DOT_POSIX_SETTINGS_PATH=\"/etc/openthread\" \
	-DOT_THREAD_VERSION=1.3 \
	-DOTBR_MDNS=mDNSResponder \
	-DOT_READLINE=readline \
	-DOTBR_BACKBONE_ROUTER=ON \
	-DOTBR_BORDER_ROUTING=ON \
	-DOTBR_DUA_ROUTING=ON \
	-DOTBR_OPENWRT=ON \
	-DOTBR_REST=ON \
	-DOT_FIREWALL=ON \
	-DOTBR_SRP_ADVERTISING_PROXY=ON \
	-DOTBR_DNSSD_DISCOVERY_PROXY=ON \
	-DOT_DNS_CLIENT=ON \
	-DOT_LOG_LEVEL=INFO \
	-DOT_RCP_RESTORATION_MAX_COUNT=5 \
	-DOT_REFERENCE_DEVICE=ON \
	-DOTBR_NAT64=ON \
	-DOT_DIAGNOSTIC=OFF

ifdef CONFIG_OTBR_WEB
	CMAKE_OPTIONS += -DOTBR_WEB=ON -DOTBR_REST=ON
endif

ifeq ($(CONFIG_TARGET_ath79_nand_DEVICE_glinet_gl-s200-nor-nand),y)
	CMAKE_OPTIONS += -DOTBR_VENDOR_NAME="GL.iNET Inc." -DOTBR_PRODUCT_NAME="S200"
endif

define Package/ot-br-posix-latest/install
	$(INSTALL_DIR) $(1)/etc/openthread
	$(FIND) files/ -name "*.gbl" -exec $(CP) {} $(1)/etc/openthread \;

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/* $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tools/pskc $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tools/steering-data $(1)/usr/sbin

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) files/etc/init.d/otbr-agent $(1)/etc/init.d/otbr-agent
	$(INSTALL_BIN) files/etc/init.d/otbr-firewall $(1)/etc/init.d/otbr-firewall

	$(INSTALL_DIR) $(1)/etc/config
	$(CP) files/etc/config/otbr $(1)/etc/config

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/etc/uci-defaults/* $(1)/etc/uci-defaults
	
ifdef CONFIG_OTBR_WEB
	# Nest Thread Web
	$(CP) files/etc/init.d/otbr-web $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/usr/share/otbr-web
	$(CP) $(PKG_INSTALL_DIR)/usr/share/otbr-web/* $(1)/usr/share/otbr-web/
endif
	# LUCI Thread Web
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller/admin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/openwrt/controller/thread.lua $(1)/usr/lib/lua/luci/controller/admin

	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view
	$(CP) $(PKG_BUILD_DIR)/src/openwrt/view/admin_thread $(1)/usr/lib/lua/luci/view

	$(INSTALL_DIR) $(1)/www/luci-static/resources
	$(CP) $(PKG_BUILD_DIR)/src/openwrt/handle_error.js $(1)/www/luci-static/resources
endef

$(eval $(call BuildPackage,ot-br-posix-latest))
