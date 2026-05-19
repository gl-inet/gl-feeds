#
# Copyright (C) 2021 CZ.NIC, z. s. p. o. (https://www.nic.cz/)
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# 嵌入式体积优化（-s -w + ts_include_cli），但不用官方完整 extra-small：
# gl-sdk4-tailscale 需要 useroutes/iptables/advertiseroutes 等，完整 extra-small 会业务不通。
# 生成 omit 列表（v1.92.5）；含 logtail（无需改 gl-sdk4 tailscale.init，校时后即可上传）：
#   GOOS= GOARCH= go run ./cmd/featuretags --min --add=osrouter,unixsocketidentity,debug,useroutes,iptables,advertiseroutes,advertiseexitnode,useexitnode,logtail
# 仍保留 ts_omit_netstack 以控体积；见 patches/003-netstack-omit-invertGSOChecksum.patch。
# 参考：https://tailscale.com/docs/how-to/set-up-small-tailscale
#

include $(TOPDIR)/rules.mk

PKG_NAME:=tailscale
PKG_VERSION:=1.92.5
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/tailscale/tailscale/tar.gz/v$(PKG_VERSION)?
PKG_HASH:=1e34d636521ebdb4c77fc80b8b5baa33dd414f727fd11b3f98068f560dae3151

PKG_MAINTAINER:=Zephyr Lykos <self@mochaa.ws>, \
		Sandro Jäckel <sandro.jaeckel@gmail.com>
PKG_LICENSE:=BSD-3-Clause
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/tailscale-$(PKG_VERSION)
PKG_BUILD_DEPENDS:=golang/host
PKG_BUILD_PARALLEL:=1
# 标准 OpenWrt 仅识别 PKG_USE_MIPS16；仅用 PKG_BUILD_FLAGS 时 CGO 仍会带 -mips16，
# 导致 runtime/cgo/gcc_mipsx.S 在汇编阶段报 invalid operands。
PKG_USE_MIPS16:=0
PKG_BUILD_FLAGS:=no-mips16

GO_PKG:=\
	tailscale.com/cmd/tailscaled

# -s -w：与官方 extra-small 一致，去掉符号表与 DWARF（与文档中 go build -ldflags="-s -w" 一致）
GO_PKG_LDFLAGS:=-s -w -X 'tailscale.com/version.longStamp=$(PKG_VERSION)-$(PKG_RELEASE) (OpenWrt)'
GO_PKG_LDFLAGS_X:=tailscale.com/version.shortStamp=$(PKG_VERSION)

# v1.92.5: router profile + logtail（单行；无 ts_omit_logtail）
GO_TS_ROUTER_OMIT:=ts_omit_ace,ts_omit_acme,ts_omit_appconnectors,ts_omit_aws,ts_omit_bakedroots,ts_omit_bird,ts_omit_cachenetmap,ts_omit_captiveportal,ts_omit_capture,ts_omit_cliconndiag,ts_omit_clientmetrics,ts_omit_clientupdate,ts_omit_cloud,ts_omit_completion,ts_omit_dbus,ts_omit_debugeventbus,ts_omit_debugportmapper,ts_omit_desktop_sessions,ts_omit_dns,ts_omit_doctor,ts_omit_drive,ts_omit_gro,ts_omit_health,ts_omit_hujsonconf,ts_omit_identityfederation,ts_omit_kube,ts_omit_lazywg,ts_omit_linkspeed,ts_omit_linuxdnsfight,ts_omit_listenrawdisco,ts_omit_netlog,ts_omit_netstack,ts_omit_networkmanager,ts_omit_oauthkey,ts_omit_outboundproxy,ts_omit_portlist,ts_omit_portmapper,ts_omit_posture,ts_omit_relayserver,ts_omit_resolved,ts_omit_sdnotify,ts_omit_serve,ts_omit_ssh,ts_omit_synology,ts_omit_syspolicy,ts_omit_systray,ts_omit_taildrop,ts_omit_tailnetlock,ts_omit_tap,ts_omit_tpm,ts_omit_useproxy,ts_omit_usermetrics,ts_omit_wakeonlan,ts_omit_webclient

GO_PKG_TAGS:=ts_include_cli,$(GO_TS_ROUTER_OMIT)

include $(INCLUDE_DIR)/package.mk
include ../../gl_feed_common/golang/golang-package.mk

define Package/tailscale
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=VPN
  TITLE:=Zero config VPN (embedded router build)
  URL:=https://tailscale.com
  DEPENDS:=$(GO_ARCH_DEPENDS) +ca-bundle +kmod-tun
  PROVIDES:=tailscaled
endef

define Package/tailscale/description
  It creates a secure network between your servers, computers,
  and cloud instances. Even when separated by firewalls or subnets.
  Router-oriented build: keeps routes/firewall/subnet-router features required by
  gl-sdk4-tailscale; smaller than stock tailscaled but not full extra-small.
endef

define Package/tailscale/conffiles
/etc/config/tailscale
/etc/tailscale/
endef

define Package/tailscale/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(GO_PKG_BUILD_BIN_DIR)/tailscaled $(1)/usr/sbin
	$(LN) tailscaled $(1)/usr/sbin/tailscale
endef

$(eval $(call BuildPackage,tailscale))
