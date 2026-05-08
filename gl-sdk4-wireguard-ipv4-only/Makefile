#
# Copyright (C) 2026 GL.iNet
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include ./version.mk

PKG_NAME:=gl-sdk4-wireguard-ipv4-only
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=6

PKG_LICENSE:=GPL-2.0
PKG_MAINTAINER:=GL.iNet <support@gl-inet.com>

include $(INCLUDE_DIR)/package.mk

define Package/gl-sdk4-wireguard-ipv4-only
  CATEGORY:=gl-sdk4
  SUBMENU:=VPN
  TITLE:=GL.iNet WireGuard IPv4-only enforcement
  DEPENDS:=+wireguard-tools
  PKGARCH:=all
endef

define Package/gl-sdk4-wireguard-ipv4-only/description
 Keys IPv4-only WireGuard enforcement off the existing global IPv6
 toggle on /#/ipv6, no new GUI control. When global IPv6 is disabled
 it strips IPv6 from the kernel device and from wireguard_server /
 network UCI (saving the originals so they can be restored), drops
 IPv6 traffic on the WG iface via fw4 (nftables) or fw3 (ip6tables),
 and re-applies on every ifup, wireguard_server reload, or netdev
 add. Removing the package fully restores the prior config.
endef

define Build/Compile
endef

define Package/gl-sdk4-wireguard-ipv4-only/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/usr/sbin/wg-noipv6 $(1)/usr/sbin/wg-noipv6

	$(INSTALL_DIR) $(1)/usr/lib/wg-noipv6
	$(INSTALL_DATA) ./files/usr/lib/wg-noipv6/functions.sh $(1)/usr/lib/wg-noipv6/functions.sh

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/wg-noipv6 $(1)/etc/init.d/wg-noipv6

	$(INSTALL_DIR) $(1)/etc/hotplug.d/iface
	$(INSTALL_BIN) ./files/etc/hotplug.d/iface/99-wg-noipv6 $(1)/etc/hotplug.d/iface/99-wg-noipv6

	$(INSTALL_DIR) $(1)/etc/hotplug.d/net
	$(INSTALL_BIN) ./files/etc/hotplug.d/net/99-wg-noipv6 $(1)/etc/hotplug.d/net/99-wg-noipv6

	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/etc/uci-defaults/80-wg-noipv6 $(1)/etc/uci-defaults/80-wg-noipv6

	$(INSTALL_DIR) $(1)/usr/lib/oui-httpd/rpc
	$(INSTALL_DATA) ./files/usr/lib/oui-httpd/rpc/wireguard_ipv4_only $(1)/usr/lib/oui-httpd/rpc/wireguard_ipv4_only
endef

define Package/gl-sdk4-wireguard-ipv4-only/postinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	/etc/init.d/wg-noipv6 enable >/dev/null 2>&1 || true
	/etc/init.d/wg-noipv6 start >/dev/null 2>&1 || true
fi
exit 0
endef

define Package/gl-sdk4-wireguard-ipv4-only/prerm
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
	/etc/init.d/wg-noipv6 stop >/dev/null 2>&1 || true
	/etc/init.d/wg-noipv6 disable >/dev/null 2>&1 || true
	/usr/sbin/wg-noipv6 clear-all >/dev/null 2>&1 || true
fi
exit 0
endef

$(eval $(call BuildPackage,gl-sdk4-wireguard-ipv4-only))
