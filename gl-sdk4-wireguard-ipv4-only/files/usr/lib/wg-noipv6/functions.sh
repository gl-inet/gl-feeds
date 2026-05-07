#!/bin/sh
# shellcheck shell=ash
# Shared helpers for gl-sdk4-wireguard-ipv4-only.

WG_NOIPV6_TAG="wg-noipv6"
WG_NOIPV6_SYSCTL_DIR="/etc/sysctl.d"
WG_NOIPV6_NFT_DIR="/etc/nftables.d"
WG_NOIPV6_FW3_DIR="/etc/wg-noipv6/fw3"
WG_NOIPV6_STATE_DIR="/var/run/wg-noipv6"
WG_NOIPV6_DEFAULT_DEV="wgserver"

_wg_log()  { logger -t "$WG_NOIPV6_TAG" -p daemon.notice "$*" 2>/dev/null || true; }
_wg_warn() { logger -t "$WG_NOIPV6_TAG" -p daemon.warn   "$*" 2>/dev/null || true; }

# Reject names that aren't safe to interpolate into root-owned drop-ins.
wg_iface_is_valid() {
	case "$1" in
		''|*[!A-Za-z0-9_-]*|[!A-Za-z]*) return 1 ;;
	esac
	return 0
}

wg_detect_fw_backend() {
	if [ -x /sbin/fw4 ] || command -v nft >/dev/null 2>&1; then
		printf 'fw4\n'
	else
		printf 'fw3\n'
	fi
}

wg_list_sections() {
	[ -f "/etc/config/$1" ] || return 0
	uci -q show "$1" 2>/dev/null | awk -F= -v t="$2" '
		$2 == t {
			n = split($1, parts, ".")
			if (n == 2) print parts[2]
		}
	' | sort -u
}

wg_list_glinet_servers() { wg_list_sections wireguard_server servers; }
wg_list_glinet_peers()   { wg_list_sections wireguard_server peers; }

wg_list_netifd_ifaces() {
	[ -f /etc/config/network ] || return 0
	uci -q show network 2>/dev/null | awk -F= '
		/\.proto=.?wireguard.?$/ {
			n = split($1, parts, ".")
			if (n == 2) print parts[2]
		}
	' | sort -u
}

wg_glinet_kernel_dev() {
	_srv="$1"
	[ -n "$_srv" ] || { printf '%s\n' "$WG_NOIPV6_DEFAULT_DEV"; return; }
	_d="$(uci -q get "wireguard_server.${_srv}.ifname" 2>/dev/null)"
	[ -n "$_d" ] || _d="$(uci -q get "wireguard_server.${_srv}.device" 2>/dev/null)"
	[ -n "$_d" ] || _d="$WG_NOIPV6_DEFAULT_DEV"
	printf '%s\n' "$_d"
}

wg_netifd_kernel_dev() {
	_iface="$1"
	[ -n "$_iface" ] || return 0
	_d="$(uci -q get "network.${_iface}.ifname" 2>/dev/null)"
	[ -n "$_d" ] || _d="$_iface"
	printf '%s\n' "$_d"
}

# Resolve the global IPv6 state surfaced by the GL.iNet /#/ipv6 page.
# Layered fallback so the package works across firmware revisions.
# Echoes '1' when enabled, '0' when disabled.
wg_global_ipv6_enabled() {
	case "$(uci -q get glconfig.general.enable_ipv6 2>/dev/null)" in
		0|no|off|false|disabled) printf '0\n'; return ;;
		1|yes|on|true|enabled)   printf '1\n'; return ;;
	esac
	[ "$(uci -q get network.wan6.disabled 2>/dev/null)" = "1" ] && { printf '0\n'; return; }
	[ "$(uci -q get network.wan6.proto    2>/dev/null)" = "none" ] && { printf '0\n'; return; }
	[ "$(uci -q get network.wan6.auto     2>/dev/null)" = "0" ] && { printf '0\n'; return; }
	printf '1\n'
}

# Strip non-link-local IPv6 from a live device. Echoes the count removed.
wg_strip_live_ipv6() {
	_dev="$1"
	[ -n "$_dev" ] && [ -d "/sys/class/net/$_dev" ] || { printf '0\n'; return 0; }
	_n=0
	for _a in $(ip -6 addr show dev "$_dev" 2>/dev/null \
			| awk '/inet6/ && $2 !~ /^fe80/ {print $2}'); do
		ip -6 addr del "$_a" dev "$_dev" 2>/dev/null && _n=$((_n+1))
	done
	printf '%s\n' "$_n"
}

# Drop ":"-bearing entries from a comma-separated list.
wg_strip_v6_from_csv() {
	printf '%s' "$1" | awk -v RS=',' '{
		gsub(/^[ \t]+|[ \t]+$/, "", $0)
		if (length($0) == 0) next
		if (index($0, ":") > 0) next
		if (out == "") out = $0
		else            out = out "," $0
	} END { print out }'
}

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

# Re-program live kernel peers whose AllowedIPs still carry IPv6.
# Required because wireguard_server may not re-push peer programming
# after a UCI rewrite + restart. Echoes the count of peers updated.
wg_reconcile_kernel_peers_ipv4() {
	_dev="$1"
	[ -n "$_dev" ] && [ -d "/sys/class/net/$_dev" ] || { printf '0\n'; return 0; }
	command -v wg >/dev/null 2>&1 || { printf '0\n'; return 0; }
	_tmp="/tmp/.wg_noipv6_peers.$$"
	wg show "$_dev" allowed-ips >"$_tmp" 2>/dev/null || {
		rm -f "$_tmp"; printf '0\n'; return 0
	}
	_updated=0
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
			# Refuse to clear: stripping would leave the peer with no
			# AllowedIPs and unroute it entirely.
			_wg_warn "kernel peer ${_pub} on ${_dev} is IPv6-only; not modifying"
			continue
		fi
		if wg set "$_dev" peer "$_pub" allowed-ips "$_new" 2>/dev/null; then
			_updated=$((_updated+1))
		fi
	done <"$_tmp"
	rm -f "$_tmp"
	printf '%s\n' "$_updated"
}

# Pin a GL.iNet server section (and every peer) to IPv4-only in UCI.
wg_pin_glinet_ipv4() {
	_srv="$1"
	[ -n "$_srv" ] || return 1
	[ -n "$(uci -q get "wireguard_server.${_srv}")" ] || return 1
	_changed=0

	_v6srv="$(uci -q get "wireguard_server.${_srv}.address_v6" 2>/dev/null)"
	if [ -n "$_v6srv" ]; then
		uci -q delete "wireguard_server.${_srv}.address_v6"
		_changed=1
		_wg_log "cleared wireguard_server.${_srv}.address_v6 (was: $_v6srv)"
	fi

	for _p in $(wg_list_glinet_peers); do
		for _key in client_ip allowed_ips; do
			_cur="$(uci -q get "wireguard_server.${_p}.${_key}" 2>/dev/null)"
			[ -n "$_cur" ] || continue
			_new="$(wg_strip_v6_from_csv "$_cur")"
			[ "$_cur" = "$_new" ] && continue
			if [ -n "$_new" ]; then
				uci -q set "wireguard_server.${_p}.${_key}=$_new"
			else
				uci -q delete "wireguard_server.${_p}.${_key}"
			fi
			_changed=1
			_wg_log "wireguard_server.${_p}.${_key}: stripped IPv6 (now: ${_new:-<unset>})"
		done
	done

	[ "$_changed" -eq 1 ] && uci -q commit wireguard_server
	return 0
}

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

wg_unpin_netifd_ipv4() {
	_iface="$1"
	[ -n "$_iface" ] || return 1
	if [ "$(uci -q get "network.${_iface}.ipv6")" = "0" ]; then
		uci -q delete "network.${_iface}.ipv6"
		uci -q commit network
	fi
}

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

# Host-side enforcement on a kernel device. Idempotent.
wg_apply_dev_hostside() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	wg_apply_sysctl "$_dev"
	wg_apply_firewall "$_dev"
	_n="$(wg_strip_live_ipv6 "$_dev")"
	[ "$_n" -gt 0 ] && _wg_log "removed $_n IPv6 address(es) from $_dev"
	_p="$(wg_reconcile_kernel_peers_ipv4 "$_dev")"
	[ "$_p" -gt 0 ] && _wg_log "reconciled $_p kernel peer(s) on $_dev to IPv4-only"
	return 0
}

wg_apply_glinet() {
	_srv="$1"
	_dev="$2"
	wg_iface_is_valid "$_dev" || {
		_wg_warn "refusing to act on invalid iface name '$_dev' for $_srv"
		return 1
	}
	wg_pin_glinet_ipv4 "$_srv"
	wg_apply_dev_hostside "$_dev"
	mkdir -p "$WG_NOIPV6_STATE_DIR"
	printf '%s\n' "$_dev" > "${WG_NOIPV6_STATE_DIR}/glinet-${_srv}.dev"
}

wg_apply_netifd() {
	_iface="$1"
	_dev="$2"
	wg_iface_is_valid "$_dev" || {
		_wg_warn "refusing to act on invalid iface name '$_dev' for $_iface"
		return 1
	}
	wg_pin_netifd_ipv4 "$_iface"
	wg_apply_dev_hostside "$_dev"
	mkdir -p "$WG_NOIPV6_STATE_DIR"
	printf '%s\n' "$_dev" > "${WG_NOIPV6_STATE_DIR}/netifd-${_iface}.dev"
}

wg_clear_dev() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	wg_remove_sysctl "$_dev"
	wg_remove_firewall "$_dev"
}

# Tear down enforcement using the on-disk state file as the source of
# truth for the previous device name (in case it has changed under us).
_wg_teardown_section() {
	_state="$1"
	_dev="$2"
	if [ -f "$_state" ]; then
		_old="$(head -n1 "$_state" 2>/dev/null)"
		wg_iface_is_valid "$_old" && wg_clear_dev "$_old"
		rm -f "$_state"
	fi
	wg_iface_is_valid "$_dev" && wg_clear_dev "$_dev"
}

# Reconcile every WG server / interface against the global IPv6 state.
wg_sync_all() {
	mkdir -p "$WG_NOIPV6_STATE_DIR"

	_off=0
	[ "$(wg_global_ipv6_enabled)" = "0" ] && _off=1

	for _srv in $(wg_list_glinet_servers); do
		_dev="$(wg_glinet_kernel_dev "$_srv")"
		wg_iface_is_valid "$_dev" || continue
		if [ "$_off" -eq 1 ]; then
			wg_apply_glinet "$_srv" "$_dev"
		else
			_wg_teardown_section "${WG_NOIPV6_STATE_DIR}/glinet-${_srv}.dev" "$_dev"
		fi
	done

	for _iface in $(wg_list_netifd_ifaces); do
		_dev="$(wg_netifd_kernel_dev "$_iface")"
		wg_iface_is_valid "$_dev" || continue
		if [ "$_off" -eq 1 ]; then
			wg_apply_netifd "$_iface" "$_dev"
		else
			wg_unpin_netifd_ipv4 "$_iface"
			_wg_teardown_section "${WG_NOIPV6_STATE_DIR}/netifd-${_iface}.dev" "$_dev"
		fi
	done
}

# Remove every drop-in this package may have installed (used by prerm).
wg_clear_all() {
	if [ -d "$WG_NOIPV6_STATE_DIR" ]; then
		for _f in "$WG_NOIPV6_STATE_DIR"/*.dev; do
			[ -f "$_f" ] || continue
			_dev="$(head -n1 "$_f" 2>/dev/null)"
			wg_iface_is_valid "$_dev" && wg_clear_dev "$_dev"
			rm -f "$_f"
		done
	fi
	for _f in "$WG_NOIPV6_SYSCTL_DIR"/99-wg-noipv6-*.conf \
	          "$WG_NOIPV6_NFT_DIR"/99-wg-noipv6-*.nft \
	          "$WG_NOIPV6_FW3_DIR"/wg-noipv6-*.sh; do
		[ -f "$_f" ] && rm -f "$_f"
	done
	_changed=0
	for _i in $(uci -q show firewall 2>/dev/null \
			| awk -F. '/^firewall\.wg_noipv6_/ { sub(/=.*/, "", $2); print $2 }' \
			| sort -u); do
		uci -q delete "firewall.${_i}" 2>/dev/null && _changed=1
	done
	[ "$_changed" -eq 1 ] && uci -q commit firewall
	/etc/init.d/firewall reload >/dev/null 2>&1 || true
	sysctl --system >/dev/null 2>&1 || true
}
