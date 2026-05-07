#!/bin/sh
# shellcheck shell=ash
#
# /usr/lib/wg-noipv6/functions.sh
#
# Shared POSIX-sh helpers for the gl-sdk4-wireguard-ipv4-only package.
#
# Sourced by:
#   /usr/sbin/wg-noipv6
#   /etc/init.d/wg-noipv6
#   /etc/hotplug.d/iface/99-wg-noipv6
#   /etc/hotplug.d/net/99-wg-noipv6
#
# Layout: behaviour mirrors the upstream reference script
# (configure-wireguard-ipv4-only.sh by Blackout Secure) but is
# wired into GL.iNet's wireguard_server UCI config so the GUI
# `enable_ipv6` toggle drives state directly.

WG_NOIPV6_TAG="wg-noipv6"
WG_NOIPV6_SYSCTL_DIR="/etc/sysctl.d"
WG_NOIPV6_NFT_DIR="/etc/nftables.d"
WG_NOIPV6_FW3_DIR="/etc/wg-noipv6/fw3"
WG_NOIPV6_STATE_DIR="/var/run/wg-noipv6"
WG_NOIPV6_DEFAULT_DEV="wgserver"

_wg_log() {
	logger -t "$WG_NOIPV6_TAG" "$*" 2>/dev/null || true
}

# Validate an interface name before interpolating it into firewall /
# sysctl drop-ins. Reject anything that isn't a normal Linux netdev
# name to avoid command injection through user-controlled UCI values.
wg_iface_is_valid() {
	case "$1" in
		''|*[!A-Za-z0-9_-]*|[!A-Za-z]*) return 1 ;;
	esac
	return 0
}

# Echo "fw4" if nftables/fw4 is available, otherwise "fw3".
wg_detect_fw_backend() {
	if [ -x /sbin/fw4 ] || command -v nft >/dev/null 2>&1; then
		printf 'fw4\n'
	else
		printf 'fw3\n'
	fi
}

# List wireguard_server "servers" sections (GL.iNet layout).
wg_list_glinet_servers() {
	[ -f /etc/config/wireguard_server ] || return 0
	uci -q show wireguard_server 2>/dev/null | awk -F= '
		/=.?servers.?$/ {
			n = split($1, parts, ".")
			if (n >= 2) print parts[2]
		}
	' | sort -u
}

# List all peers sections in /etc/config/wireguard_server.
wg_list_glinet_peers() {
	[ -f /etc/config/wireguard_server ] || return 0
	uci -q show wireguard_server 2>/dev/null | awk -F= '
		/=.?peers.?$/ {
			n = split($1, parts, ".")
			if (n >= 2) print parts[2]
		}
	'
}

# List netifd-managed wireguard interfaces.
wg_list_netifd_ifaces() {
	[ -f /etc/config/network ] || return 0
	uci -q show network 2>/dev/null | awk -F= '
		/\.proto=.?wireguard.?$/ {
			n = split($1, parts, ".")
			if (n >= 2) print parts[2]
		}
	' | sort -u
}

# Echo the kernel device name for the given GL.iNet server section.
# Falls back to $WG_NOIPV6_DEFAULT_DEV ("wgserver"), which is what
# GL.iNet firmware uses by default.
wg_glinet_kernel_dev() {
	_srv="$1"
	[ -n "$_srv" ] || { printf '%s\n' "$WG_NOIPV6_DEFAULT_DEV"; return; }
	_d="$(uci -q get "wireguard_server.${_srv}.ifname" 2>/dev/null)"
	[ -n "$_d" ] || _d="$(uci -q get "wireguard_server.${_srv}.device" 2>/dev/null)"
	[ -n "$_d" ] || _d="$WG_NOIPV6_DEFAULT_DEV"
	printf '%s\n' "$_d"
}

# Echo the kernel device name for the given netifd wireguard section.
wg_netifd_kernel_dev() {
	_iface="$1"
	[ -n "$_iface" ] || return 0
	_d="$(uci -q get "network.${_iface}.ifname" 2>/dev/null)"
	[ -n "$_d" ] || _d="$_iface"
	printf '%s\n' "$_d"
}

# Read the per-server enable_ipv6 toggle. Defaults to 1 (IPv6 allowed)
# so existing GL.iNet behaviour is preserved when the option is unset.
wg_glinet_enable_ipv6() {
	_srv="$1"
	_v="$(uci -q get "wireguard_server.${_srv}.enable_ipv6" 2>/dev/null)"
	case "$_v" in
		0|no|off|false|disabled) printf '0\n' ;;
		*) printf '1\n' ;;
	esac
}

# Read per-section enable_ipv6 toggle for netifd wireguard ifaces.
# Same defaulting rule as the glinet version.
wg_netifd_enable_ipv6() {
	_iface="$1"
	_v="$(uci -q get "network.${_iface}.enable_ipv6" 2>/dev/null)"
	case "$_v" in
		0|no|off|false|disabled) printf '0\n' ;;
		*) printf '1\n' ;;
	esac
}

# Strip non-link-local IPv6 addresses from a live kernel device.
# Echoes the count of addresses removed.
wg_strip_live_ipv6() {
	_dev="$1"
	[ -n "$_dev" ] || { printf '0\n'; return 0; }
	[ -d "/sys/class/net/$_dev" ] || { printf '0\n'; return 0; }
	_n=0
	for _a in $(ip -6 addr show dev "$_dev" 2>/dev/null \
			| awk '/inet6/ && $2 !~ /^fe80/ {print $2}'); do
		ip -6 addr del "$_a" dev "$_dev" 2>/dev/null && _n=$((_n+1))
	done
	printf '%s\n' "$_n"
}

# Drop ":"-bearing entries from a comma-separated list. Used for the
# GL.iNet peer client_ip / allowed_ips fields which carry both the
# IPv4 and IPv6 sides as a single CSV string.
wg_strip_v6_from_csv() {
	printf '%s' "$1" | awk -v RS=',' '{
		gsub(/^[ \t]+|[ \t]+$/, "", $0)
		if (length($0) == 0) next
		if (index($0, ":") > 0) next
		if (out == "") out = $0
		else            out = out "," $0
	} END { print out }'
}

# Strip IPv6 entries from netifd network.<iface>.addresses list.
wg_strip_netifd_ipv6_addrs() {
	_iface="$1"
	_addrs=""
	for _a in $(uci -q get "network.${_iface}.addresses" 2>/dev/null); do
		case "$_a" in *:*) ;; *) _addrs="$_addrs $_a" ;; esac
	done
	uci -q delete "network.${_iface}.addresses" 2>/dev/null || true
	for _a in $_addrs; do
		uci -q add_list "network.${_iface}.addresses=$_a" 2>/dev/null || true
	done
}

# Reconcile the live kernel WG peer state to drop IPv6 entries from
# each peer's AllowedIPs. Required because wireguard_server may not
# re-push peer programming after a UCI rewrite + restart -- existing
# sessions keep their old AllowedIPs in the kernel until the peer
# re-handshakes. Echoes the count of peers updated.
wg_reconcile_kernel_peers_ipv4() {
	_dev="$1"
	[ -n "$_dev" ] || { printf '0\n'; return 0; }
	[ -d "/sys/class/net/$_dev" ] || { printf '0\n'; return 0; }
	command -v wg >/dev/null 2>&1 || { printf '0\n'; return 0; }
	_tmp="/tmp/.wg_noipv6_peers.$$"
	wg show "$_dev" allowed-ips >"$_tmp" 2>/dev/null || {
		rm -f "$_tmp"; printf '0\n'; return 0
	}
	_updated=0
	# Each line: "<pubkey>\t<ip1> <ip2> ..." or "<pubkey>\t(none)"
	while IFS="$(printf '\t')" read -r _pub _ips; do
		[ -n "$_pub" ] || continue
		case "$_ips" in '(none)'|'') continue ;; esac
		_new=""
		_had_v6=0
		for _ip in $_ips; do
			case "$_ip" in
				*:*) _had_v6=1 ;;
				'') ;;
				*)  _new="${_new:+$_new,}$_ip" ;;
			esac
		done
		[ "$_had_v6" -eq 1 ] || continue
		if [ -z "$_new" ]; then
			# Peer is IPv6-only -- refuse to clear so we don't
			# accidentally unroute the peer entirely.
			_wg_log "kernel peer ${_pub} has only IPv6 allowed-ips; not modifying"
			continue
		fi
		if wg set "$_dev" peer "$_pub" allowed-ips "$_new" 2>/dev/null; then
			_updated=$((_updated+1))
		fi
	done <"$_tmp"
	rm -f "$_tmp"
	printf '%s\n' "$_updated"
}

# Pin a single GL.iNet wireguard_server section to IPv4-only:
#  - clear address_v6 on the server section
#  - strip ":"-bearing entries from each peer's client_ip / allowed_ips
# Returns 0 if anything changed, 1 otherwise.
wg_pin_glinet_ipv4() {
	_srv="$1"
	[ -n "$_srv" ] || return 1
	[ -n "$(uci -q get "wireguard_server.${_srv}")" ] || return 1
	_changed=0

	# 1) server section: clear address_v6
	_v6srv="$(uci -q get "wireguard_server.${_srv}.address_v6" 2>/dev/null)"
	if [ -n "$_v6srv" ]; then
		uci -q delete "wireguard_server.${_srv}.address_v6"
		_changed=1
		_wg_log "cleared wireguard_server.${_srv}.address_v6 (was: $_v6srv)"
	fi

	# 2) peers: strip ":"-bearing entries from CSV options
	for _p in $(wg_list_glinet_peers); do
		for _key in client_ip allowed_ips; do
			_cur="$(uci -q get "wireguard_server.${_p}.${_key}" 2>/dev/null)"
			[ -n "$_cur" ] || continue
			_new="$(wg_strip_v6_from_csv "$_cur")"
			if [ "$_cur" != "$_new" ]; then
				if [ -n "$_new" ]; then
					uci -q set "wireguard_server.${_p}.${_key}=$_new"
				else
					uci -q delete "wireguard_server.${_p}.${_key}"
				fi
				_changed=1
				_wg_log "wireguard_server.${_p}.${_key}: stripped IPv6 (now: ${_new:-<unset>})"
			fi
		done
	done

	[ "$_changed" -eq 1 ] && uci -q commit wireguard_server
	return 0
}

# Pin a single netifd wireguard section to IPv4-only:
#  - set network.<iface>.ipv6=0
#  - strip IPv6 entries from network.<iface>.addresses
wg_pin_netifd_ipv4() {
	_iface="$1"
	[ -n "$_iface" ] || return 1
	[ "$(uci -q get "network.${_iface}.proto")" = "wireguard" ] || return 1
	_changed=0
	if [ "$(uci -q get "network.${_iface}.ipv6")" != "0" ]; then
		uci -q set "network.${_iface}.ipv6=0"
		_changed=1
	fi
	_v6=0
	for _a in $(uci -q get "network.${_iface}.addresses" 2>/dev/null); do
		case "$_a" in *:*) _v6=$((_v6+1));; esac
	done
	if [ "$_v6" -gt 0 ]; then
		wg_strip_netifd_ipv6_addrs "$_iface"
		_changed=1
		_wg_log "stripped $_v6 IPv6 entry/entries from network.${_iface}.addresses"
	fi
	[ "$_changed" -eq 1 ] && uci -q commit network
	return 0
}

# Restore the netifd defaults for a section: drop the ipv6=0 marker
# we set; we leave addresses alone (the GUI will re-issue them).
wg_unpin_netifd_ipv4() {
	_iface="$1"
	[ -n "$_iface" ] || return 1
	if [ "$(uci -q get "network.${_iface}.ipv6")" = "0" ]; then
		uci -q delete "network.${_iface}.ipv6"
		uci -q commit network
	fi
}

# Write the per-iface sysctl drop-in that pins disable_ipv6=1 on the
# WG kernel device. Idempotent.
wg_apply_sysctl() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	mkdir -p "$WG_NOIPV6_SYSCTL_DIR"
	_path="${WG_NOIPV6_SYSCTL_DIR}/99-wg-noipv6-${_dev}.conf"
	cat > "$_path" <<EOF
# Managed by gl-sdk4-wireguard-ipv4-only; do not edit by hand.
net.ipv6.conf.${_dev}.disable_ipv6 = 1
EOF
	sysctl -p "$_path" >/dev/null 2>&1 || true
}

wg_remove_sysctl() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	_path="${WG_NOIPV6_SYSCTL_DIR}/99-wg-noipv6-${_dev}.conf"
	[ -f "$_path" ] && rm -f "$_path"
	[ -f "/proc/sys/net/ipv6/conf/$_dev/disable_ipv6" ] \
		&& echo 0 > "/proc/sys/net/ipv6/conf/$_dev/disable_ipv6" 2>/dev/null || true
}

# Install the fw4 nftables drop-in that blocks IPv6 traffic on the
# WG kernel device.
wg_apply_firewall_fw4() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	mkdir -p "$WG_NOIPV6_NFT_DIR"
	_path="${WG_NOIPV6_NFT_DIR}/99-wg-noipv6-${_dev}.nft"
	cat > "$_path" <<EOF
# Managed by gl-sdk4-wireguard-ipv4-only; do not edit by hand.
chain wg_noipv6_input_${_dev} {
	type filter hook input priority -1; policy accept;
	iifname "${_dev}" meta nfproto ipv6 drop
}
chain wg_noipv6_forward_${_dev} {
	type filter hook forward priority -1; policy accept;
	iifname "${_dev}" meta nfproto ipv6 drop
	oifname "${_dev}" meta nfproto ipv6 drop
}
chain wg_noipv6_output_${_dev} {
	type filter hook output priority -1; policy accept;
	oifname "${_dev}" meta nfproto ipv6 drop
}
EOF
	/etc/init.d/firewall reload >/dev/null 2>&1 || true
}

wg_remove_firewall_fw4() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	_path="${WG_NOIPV6_NFT_DIR}/99-wg-noipv6-${_dev}.nft"
	[ -f "$_path" ] && rm -f "$_path"
	/etc/init.d/firewall reload >/dev/null 2>&1 || true
}

# Install the fw3 firewall include + ip6tables drop rules.
wg_apply_firewall_fw3() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	mkdir -p "$WG_NOIPV6_FW3_DIR"
	_path="${WG_NOIPV6_FW3_DIR}/wg-noipv6-${_dev}.sh"
	cat > "$_path" <<EOF
#!/bin/sh
# Managed by gl-sdk4-wireguard-ipv4-only; do not edit by hand.
WG_DEV="${_dev}"
ip6tables -D forwarding_rule -i "\$WG_DEV" -j DROP 2>/dev/null
ip6tables -D forwarding_rule -o "\$WG_DEV" -j DROP 2>/dev/null
ip6tables -D input_rule      -i "\$WG_DEV" -j DROP 2>/dev/null
ip6tables -D output_rule     -o "\$WG_DEV" -j DROP 2>/dev/null
ip6tables -I forwarding_rule -i "\$WG_DEV" -j DROP
ip6tables -I forwarding_rule -o "\$WG_DEV" -j DROP
ip6tables -I input_rule      -i "\$WG_DEV" -j DROP
ip6tables -I output_rule     -o "\$WG_DEV" -j DROP
EOF
	chmod 0755 "$_path"
	_inc_name="wg_noipv6_${_dev}"
	uci -q delete "firewall.${_inc_name}" 2>/dev/null || true
	uci -q set "firewall.${_inc_name}=include"
	uci -q set "firewall.${_inc_name}.path=$_path"
	uci -q set "firewall.${_inc_name}.reload=1"
	uci -q commit firewall
	/etc/init.d/firewall reload >/dev/null 2>&1 || true
}

wg_remove_firewall_fw3() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	_path="${WG_NOIPV6_FW3_DIR}/wg-noipv6-${_dev}.sh"
	[ -f "$_path" ] && rm -f "$_path"
	_inc_name="wg_noipv6_${_dev}"
	if [ -n "$(uci -q get "firewall.${_inc_name}")" ]; then
		uci -q delete "firewall.${_inc_name}"
		uci -q commit firewall
	fi
	if command -v ip6tables >/dev/null 2>&1; then
		ip6tables -D forwarding_rule -i "$_dev" -j DROP 2>/dev/null || true
		ip6tables -D forwarding_rule -o "$_dev" -j DROP 2>/dev/null || true
		ip6tables -D input_rule      -i "$_dev" -j DROP 2>/dev/null || true
		ip6tables -D output_rule     -o "$_dev" -j DROP 2>/dev/null || true
	fi
	/etc/init.d/firewall reload >/dev/null 2>&1 || true
}

# Backend dispatcher.
wg_apply_firewall() {
	case "$(wg_detect_fw_backend)" in
		fw4) wg_apply_firewall_fw4 "$1" ;;
		*)   wg_apply_firewall_fw3 "$1" ;;
	esac
}

wg_remove_firewall() {
	case "$(wg_detect_fw_backend)" in
		fw4) wg_remove_firewall_fw4 "$1" ;;
		*)   wg_remove_firewall_fw3 "$1" ;;
	esac
}

# Apply IPv4-only enforcement for one GL.iNet server section.
# Expects a validated kernel device name; safe to call repeatedly.
wg_apply_glinet() {
	_srv="$1"
	_dev="$2"
	wg_iface_is_valid "$_dev" || {
		_wg_log "refusing to act on invalid iface name '$_dev' for $_srv"
		return 1
	}
	wg_pin_glinet_ipv4 "$_srv"
	wg_apply_sysctl "$_dev"
	wg_apply_firewall "$_dev"
	_n="$(wg_strip_live_ipv6 "$_dev")"
	[ "$_n" -gt 0 ] && _wg_log "removed $_n IPv6 address(es) from $_dev"
	_p="$(wg_reconcile_kernel_peers_ipv4 "$_dev")"
	[ "$_p" -gt 0 ] && _wg_log "reconciled $_p kernel peer(s) on $_dev to IPv4-only"
	mkdir -p "$WG_NOIPV6_STATE_DIR"
	printf '%s\n' "$_dev" > "${WG_NOIPV6_STATE_DIR}/glinet-${_srv}.dev"
	return 0
}

# Apply IPv4-only enforcement for one netifd wireguard interface.
wg_apply_netifd() {
	_iface="$1"
	_dev="$2"
	wg_iface_is_valid "$_dev" || {
		_wg_log "refusing to act on invalid iface name '$_dev' for $_iface"
		return 1
	}
	wg_pin_netifd_ipv4 "$_iface"
	wg_apply_sysctl "$_dev"
	wg_apply_firewall "$_dev"
	_n="$(wg_strip_live_ipv6 "$_dev")"
	[ "$_n" -gt 0 ] && _wg_log "removed $_n IPv6 address(es) from $_dev"
	_p="$(wg_reconcile_kernel_peers_ipv4 "$_dev")"
	[ "$_p" -gt 0 ] && _wg_log "reconciled $_p kernel peer(s) on $_dev to IPv4-only"
	mkdir -p "$WG_NOIPV6_STATE_DIR"
	printf '%s\n' "$_dev" > "${WG_NOIPV6_STATE_DIR}/netifd-${_iface}.dev"
	return 0
}

# Remove enforcement for a previously-pinned device.
wg_clear_dev() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	wg_remove_sysctl "$_dev"
	wg_remove_firewall "$_dev"
}

# Walk every wireguard server section and apply or clear based on the
# per-section enable_ipv6 toggle. This is the entry point used by
# `/etc/init.d/wg-noipv6 reload`, the uci-defaults bootstrap, the RPC
# adapter and the iface hotplug script.
wg_sync_all() {
	mkdir -p "$WG_NOIPV6_STATE_DIR"

	# GL.iNet wireguard_server layout
	for _srv in $(wg_list_glinet_servers); do
		_dev="$(wg_glinet_kernel_dev "$_srv")"
		wg_iface_is_valid "$_dev" || continue
		_state="${WG_NOIPV6_STATE_DIR}/glinet-${_srv}.dev"
		if [ "$(wg_glinet_enable_ipv6 "$_srv")" = "0" ]; then
			wg_apply_glinet "$_srv" "$_dev"
		else
			# enabled (or unset/default): tear down anything we
			# previously installed for this server's last device.
			if [ -f "$_state" ]; then
				_old="$(head -n1 "$_state" 2>/dev/null)"
				wg_iface_is_valid "$_old" && wg_clear_dev "$_old"
				rm -f "$_state"
			fi
			wg_iface_is_valid "$_dev" && wg_clear_dev "$_dev"
		fi
	done

	# netifd layout (proto=wireguard)
	for _iface in $(wg_list_netifd_ifaces); do
		_dev="$(wg_netifd_kernel_dev "$_iface")"
		wg_iface_is_valid "$_dev" || continue
		_state="${WG_NOIPV6_STATE_DIR}/netifd-${_iface}.dev"
		if [ "$(wg_netifd_enable_ipv6 "$_iface")" = "0" ]; then
			wg_apply_netifd "$_iface" "$_dev"
		else
			if [ -f "$_state" ]; then
				_old="$(head -n1 "$_state" 2>/dev/null)"
				wg_iface_is_valid "$_old" && wg_clear_dev "$_old"
				rm -f "$_state"
			fi
			wg_unpin_netifd_ipv4 "$_iface"
			wg_iface_is_valid "$_dev" && wg_clear_dev "$_dev"
		fi
	done
}

# Used at package removal time: drop every drop-in we might have
# installed and restore default behaviour.
wg_clear_all() {
	if [ -d "$WG_NOIPV6_STATE_DIR" ]; then
		for _f in "$WG_NOIPV6_STATE_DIR"/*.dev; do
			[ -f "$_f" ] || continue
			_dev="$(head -n1 "$_f" 2>/dev/null)"
			wg_iface_is_valid "$_dev" && wg_clear_dev "$_dev"
			rm -f "$_f"
		done
	fi
	# Sweep any remaining drop-ins by glob (covers the case where
	# state files were lost but drop-ins linger on disk).
	for _f in "$WG_NOIPV6_SYSCTL_DIR"/99-wg-noipv6-*.conf; do
		[ -f "$_f" ] || continue
		rm -f "$_f"
	done
	for _f in "$WG_NOIPV6_NFT_DIR"/99-wg-noipv6-*.nft; do
		[ -f "$_f" ] || continue
		rm -f "$_f"
	done
	if [ -d "$WG_NOIPV6_FW3_DIR" ]; then
		for _f in "$WG_NOIPV6_FW3_DIR"/wg-noipv6-*.sh; do
			[ -f "$_f" ] || continue
			rm -f "$_f"
		done
	fi
	# Drop any UCI firewall include sections we created.
	for _i in $(uci -q show firewall 2>/dev/null \
			| awk -F. '/^firewall\.wg_noipv6_/ {print $2}' \
			| awk -F= '{print $1}' | sort -u); do
		uci -q delete "firewall.${_i}" 2>/dev/null || true
	done
	uci -q commit firewall
	/etc/init.d/firewall reload >/dev/null 2>&1 || true
	sysctl --system >/dev/null 2>&1 || true
}
