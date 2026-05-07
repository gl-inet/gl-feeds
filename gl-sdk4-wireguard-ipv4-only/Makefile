#
# Copyright (C) 2026 GL.iNet
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# gl-sdk4-wireguard-ipv4-only
#
# Per-server "IPv4-only" enforcement for the GL.iNet WireGuard backend.
# When `enable_ipv6` on a wireguard_server section is set to 0, this
# package strips IPv6 from the WG interface, peers, sysctl and firewall
# and keeps it that way against GUI regeneration, reboots and ifup
# events. When the option is unset or 1, behaviour is unchanged.
#

include $(TOPDIR)/rules.mk
include ./version.mk

PKG_NAME:=gl-sdk4-wireguard-ipv4-only
PKG_VERSION:=$(strip $(call findrev))
PKG_RELEASE:=1

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
 Adds per-server `enable_ipv6` handling to GL.iNet's WireGuard backend.
 When the toggle is disabled, the WG interface is forced into IPv4-only
 mode: IPv6 addresses are stripped from the kernel device and from the
 wireguard_server / network UCI configs, IPv6 entries are removed from
 each peer's allowed_ips/client_ip, an nftables (fw4) drop-in blocks
 IPv6 traffic on the WG interface, and hotplug + init.d hooks re-apply
 enforcement on every ifup or wireguard_server restart.
endef

define Package/gl-sdk4-wireguard-ipv4-only/conffiles
/etc/config/wireguard_server
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
