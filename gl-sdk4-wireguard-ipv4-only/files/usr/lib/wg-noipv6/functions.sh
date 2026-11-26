#!/bin/sh
# shellcheck shell=ash
# Shared helpers for gl-sdk4-wireguard-ipv4-only.

WG_NOIPV6_TAG="wg-noipv6"
WG_NOIPV6_SYSCTL_DIR="/etc/sysctl.d"
WG_NOIPV6_NFT_DIR="/etc/nftables.d"
WG_NOIPV6_FW3_DIR="/etc/wg-noipv6/fw3"
WG_NOIPV6_STATE_DIR="/var/run/wg-noipv6"
WG_NOIPV6_BACKUP_DIR="/etc/wg-noipv6/backup"
WG_NOIPV6_DEFAULT_DEV="wgserver"

# All logging funnels through here so the tag and error swallowing live
# in exactly one place.
_wg_logger() {
	_prio="$1"; shift
	logger -t "$WG_NOIPV6_TAG" -p "$_prio" "$*" 2>/dev/null || true
}

_wg_log()  { _wg_logger daemon.notice "$@"; }
_wg_warn() { _wg_logger daemon.warn   "$@"; }

# Headline message: always to syslog; also to stderr when stderr is a TTY
# and WG_NOIPV6_QUIET is unset (hooks set it to 1 to stay quiet).
_wg_say() {
	_wg_logger daemon.notice "$@"
	[ "${WG_NOIPV6_QUIET:-0}" = "1" ] && return 0
	[ -t 2 ] || return 0
	printf '%s: %s\n' "$WG_NOIPV6_TAG" "$*" >&2
}

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
	# UCI keys for the proto option look like 'network.<iface>.proto=...',
	# i.e. they always split into 3 dot-separated parts; the iface name
	# we want is parts[2].
	uci -q show network 2>/dev/null | awk -F= '
		/\.proto=.?wireguard.?$/ {
			n = split($1, parts, ".")
			if (n == 3) print parts[2]
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

# ---------------------------------------------------------------------------
# Snapshot / restore
# ---------------------------------------------------------------------------
# The disable path strips IPv6 fields from UCI. Originals are saved under
# WG_NOIPV6_BACKUP_DIR (mode 0700, files 0600) before stripping and replayed
# when global IPv6 returns. Format: KEY=VALUE per line, KEY contains no '='.

_wg_snap_path()        { printf '%s/%s-%s.snap\n' "$WG_NOIPV6_BACKUP_DIR" "$1" "$2"; }
_wg_snap_path_glinet() { _wg_snap_path glinet "$1"; }
_wg_snap_path_netifd() { _wg_snap_path netifd "$1"; }

# Prepare backup dir + a 0600 temp file for $1; echo the temp path. Caller
# writes to that path then renames over $1.
_wg_snap_open() {
	mkdir -p "$WG_NOIPV6_BACKUP_DIR"
	chmod 0700 "$WG_NOIPV6_BACKUP_DIR" 2>/dev/null || true
	_t="${1}.tmp"
	: > "$_t"
	chmod 0600 "$_t" 2>/dev/null || true
	printf '%s\n' "$_t"
}

# Count snapshot files of a given source ('glinet' or 'netifd').
_wg_count_snaps() {
	_n=0
	[ -d "$WG_NOIPV6_BACKUP_DIR" ] || { printf '0\n'; return 0; }
	for _f in "$WG_NOIPV6_BACKUP_DIR"/"$1"-*.snap; do
		[ -f "$_f" ] && _n=$((_n+1))
	done
	printf '%s\n' "$_n"
}

# Best-effort init.d reloads. Safe no-op when the script is absent.
#
# When _WG_NOIPV6_DEFER_KICK=1 is set in the environment of the caller,
# the kick is skipped and the request is recorded in _WG_NOIPV6_PENDING_*
# so the caller can issue a single coalesced reload at the end of a
# multi-step operation (see wg_clear_all). This avoids cascading firewall
# / wireguard_server / network reloads which on GL.iNet 4.x can leave the
# router with broken outbound DNS for several seconds.
_wg_kick_wgserver() {
	if [ "${_WG_NOIPV6_DEFER_KICK:-0}" = "1" ]; then
		_WG_NOIPV6_PENDING_WGSERVER=1
		return 0
	fi
	[ -x /etc/init.d/wireguard_server ] && \
		/etc/init.d/wireguard_server reload >/dev/null 2>&1 || true
}
_wg_kick_network() {
	if [ "${_WG_NOIPV6_DEFER_KICK:-0}" = "1" ]; then
		_WG_NOIPV6_PENDING_NETWORK=1
		return 0
	fi
	[ -x /etc/init.d/network ] && \
		/etc/init.d/network reload >/dev/null 2>&1 || true
}
_wg_kick_firewall() {
	if [ "${_WG_NOIPV6_DEFER_KICK:-0}" = "1" ]; then
		_WG_NOIPV6_PENDING_FIREWALL=1
		return 0
	fi
	[ -x /etc/init.d/firewall ] && \
		/etc/init.d/firewall reload >/dev/null 2>&1 || true
}

# Save the fields wg_pin_glinet_ipv4 is about to strip. Refuses to overwrite
# an existing snapshot so a re-sync while still disabled doesn't capture the
# already-stripped state.
wg_snapshot_glinet() {
	_srv="$1"
	[ -n "$_srv" ] || return 1
	_path="$(_wg_snap_path_glinet "$_srv")"
	[ -f "$_path" ] && return 0
	_tmp="$(_wg_snap_open "$_path")"

	_v6srv="$(uci -q get "wireguard_server.${_srv}.address_v6" 2>/dev/null)"
	[ -n "$_v6srv" ] && printf 'server.address_v6=%s\n' "$_v6srv" >> "$_tmp"

	# Peer sections are global to wireguard_server, not per-server.
	for _p in $(wg_list_glinet_peers); do
		for _key in client_ip allowed_ips; do
			_cur="$(uci -q get "wireguard_server.${_p}.${_key}" 2>/dev/null)"
			[ -n "$_cur" ] || continue
			case "$_cur" in *:*) ;; *) continue ;; esac
			printf 'peer.%s.%s=%s\n' "$_p" "$_key" "$_cur" >> "$_tmp"
		done
	done

	mv "$_tmp" "$_path"
	return 0
}

# Restore a glinet snapshot, then delete it. No-op if absent. Triggers a
# wireguard_server reload only when something actually changed.
wg_restore_glinet() {
	_srv="$1"
	[ -n "$_srv" ] || return 1
	_path="$(_wg_snap_path_glinet "$_srv")"
	[ -f "$_path" ] || return 0
	_changed=0

	# Read line-by-line; KEY is everything up to the first '=', VALUE is the
	# rest (so '=' inside the value is preserved).
	while IFS= read -r _line || [ -n "$_line" ]; do
		[ -n "$_line" ] || continue
		_k="${_line%%=*}"
		_v="${_line#*=}"
		case "$_k" in
			server.address_v6)
				[ -n "$(uci -q get "wireguard_server.${_srv}")" ] || continue
				_cur="$(uci -q get "wireguard_server.${_srv}.address_v6" 2>/dev/null)"
				[ "$_cur" = "$_v" ] && continue
				uci -q set "wireguard_server.${_srv}.address_v6=$_v"
				_changed=1
				_wg_log "restored wireguard_server.${_srv}.address_v6 (= $_v)"
				;;
			peer.*)
				_rest="${_k#peer.}"
				_peer="${_rest%%.*}"
				_field="${_rest#*.}"
				[ -n "$_peer" ] && [ -n "$_field" ] || continue
				[ "$_peer" = "$_field" ] && continue
				[ -n "$(uci -q get "wireguard_server.${_peer}")" ] || continue
				_cur="$(uci -q get "wireguard_server.${_peer}.${_field}" 2>/dev/null)"
				[ "$_cur" = "$_v" ] && continue
				uci -q set "wireguard_server.${_peer}.${_field}=$_v"
				_changed=1
				_wg_log "restored wireguard_server.${_peer}.${_field} (= $_v)"
				;;
		esac
	done < "$_path"

	if [ "$_changed" -eq 1 ]; then
		uci -q commit wireguard_server
		_wg_kick_wgserver
	fi
	rm -f "$_path"
	return 0
}

wg_snapshot_netifd() {
	_iface="$1"
	[ -n "$_iface" ] || return 1
	_path="$(_wg_snap_path_netifd "$_iface")"
	[ -f "$_path" ] && return 0
	_tmp="$(_wg_snap_open "$_path")"

	# Always record the pre-strip ipv6 setting (empty value = was unset).
	_ipv6cur="$(uci -q get "network.${_iface}.ipv6" 2>/dev/null)"
	printf 'ipv6=%s\n' "$_ipv6cur" >> "$_tmp"

	for _a in $(uci -q get "network.${_iface}.addresses" 2>/dev/null); do
		case "$_a" in *:*) printf 'address=%s\n' "$_a" >> "$_tmp" ;; esac
	done

	mv "$_tmp" "$_path"
	return 0
}

wg_restore_netifd() {
	_iface="$1"
	[ -n "$_iface" ] || return 1
	_path="$(_wg_snap_path_netifd "$_iface")"
	[ -f "$_path" ] || return 0
	[ -n "$(uci -q get "network.${_iface}")" ] || { rm -f "$_path"; return 0; }

	_changed=0
	_saw_ipv6=0
	_saw_ipv6_val=""
	_saw_addrs=""
	while IFS= read -r _line || [ -n "$_line" ]; do
		[ -n "$_line" ] || continue
		_k="${_line%%=*}"
		_v="${_line#*=}"
		case "$_k" in
			ipv6)    _saw_ipv6=1; _saw_ipv6_val="$_v" ;;
			address) _saw_addrs="$_saw_addrs $_v" ;;
		esac
	done < "$_path"

	if [ "$_saw_ipv6" -eq 1 ]; then
		if [ -z "$_saw_ipv6_val" ]; then
			if [ -n "$(uci -q get "network.${_iface}.ipv6")" ]; then
				uci -q delete "network.${_iface}.ipv6" 2>/dev/null && _changed=1
			fi
		else
			_cur="$(uci -q get "network.${_iface}.ipv6")"
			if [ "$_cur" != "$_saw_ipv6_val" ]; then
				uci -q set "network.${_iface}.ipv6=$_saw_ipv6_val"
				_changed=1
			fi
		fi
	fi

	for _a in $_saw_addrs; do
		_present=0
		for _ex in $(uci -q get "network.${_iface}.addresses" 2>/dev/null); do
			[ "$_ex" = "$_a" ] && { _present=1; break; }
		done
		[ "$_present" -eq 1 ] && continue
		uci -q add_list "network.${_iface}.addresses=$_a" 2>/dev/null || continue
		_changed=1
		_wg_log "restored IPv6 address ${_a} on network.${_iface}"
	done

	[ "$_changed" -eq 1 ] && uci -q commit network
	rm -f "$_path"
	return 0
}

# Restore every snapshot we hold, regardless of source. Used by
# `wg-noipv6 clear-all` so removing the package never strands the user with
# stripped configs. Only restores UCI -- network reload is the caller's job.
wg_restore_all() {
	[ -d "$WG_NOIPV6_BACKUP_DIR" ] || return 0
	for _f in "$WG_NOIPV6_BACKUP_DIR"/glinet-*.snap; do
		[ -f "$_f" ] || continue
		_n="${_f##*/glinet-}"; _n="${_n%.snap}"
		[ -n "$_n" ] && wg_restore_glinet "$_n"
	done
	for _f in "$WG_NOIPV6_BACKUP_DIR"/netifd-*.snap; do
		[ -f "$_f" ] || continue
		_n="${_f##*/netifd-}"; _n="${_n%.snap}"
		[ -n "$_n" ] && wg_restore_netifd "$_n"
	done
}

# Re-program live kernel peers whose AllowedIPs still carry IPv6.
# wireguard_server may not re-push peer programming after a UCI rewrite +
# restart. Echoes the count of peers updated.
wg_reconcile_kernel_peers_ipv4() {
	_dev="$1"
	[ -n "$_dev" ] && [ -d "/sys/class/net/$_dev" ] || { printf '0\n'; return 0; }
	command -v wg >/dev/null 2>&1 || { printf '0\n'; return 0; }
	_tmp="$(mktemp /tmp/wg-noipv6-peers.XXXXXX 2>/dev/null)" \
		|| _tmp="/tmp/.wg_noipv6_peers.$$"
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
			# Refuse to clear: would unroute an IPv6-only peer entirely.
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
	_wg_kick_firewall
}

wg_remove_firewall_fw4() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	_path="${WG_NOIPV6_NFT_DIR}/99-wg-noipv6-${_dev}.nft"
	[ -f "$_path" ] && rm -f "$_path"
	_wg_kick_firewall
}

# fw3: emit/run the same four ip6tables rules. Action is '-I' or '-D'.
# In emit mode -D probes get stderr silenced; -I lines stay loud so any insert
# failure surfaces in the firewall log on reload.
_wg_fw3_rules() {
	_action="$1"; _dev="$2"; _emit="${3:-run}"
	_redir=""
	[ "$_action" = "-D" ] && _redir=" 2>/dev/null"
	for _spec in \
		"forwarding_rule -i" \
		"forwarding_rule -o" \
		"input_rule      -i" \
		"output_rule     -o"; do
		# shellcheck disable=SC2086 # intentional word-splitting
		set -- $_spec
		if [ "$_emit" = "emit" ]; then
			printf 'ip6tables %s %s %s "$WG_DEV" -j DROP%s\n' \
				"$_action" "$1" "$2" "$_redir"
		else
			ip6tables "$_action" "$1" "$2" "$_dev" -j DROP 2>/dev/null || true
		fi
	done
}

wg_apply_firewall_fw3() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	mkdir -p "$WG_NOIPV6_FW3_DIR"
	_path="${WG_NOIPV6_FW3_DIR}/wg-noipv6-${_dev}.sh"
	{
		echo "#!/bin/sh"
		echo "# Managed by gl-sdk4-wireguard-ipv4-only; do not edit by hand."
		echo "WG_DEV=\"${_dev}\""
		_wg_fw3_rules -D "$_dev" emit
		_wg_fw3_rules -I "$_dev" emit
	} > "$_path"
	chmod 0755 "$_path"
	_inc_name="wg_noipv6_${_dev}"
	uci -q delete "firewall.${_inc_name}" 2>/dev/null || true
	uci -q set "firewall.${_inc_name}=include"
	uci -q set "firewall.${_inc_name}.path=$_path"
	uci -q set "firewall.${_inc_name}.reload=1"
	uci -q commit firewall
	_wg_kick_firewall
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
	command -v ip6tables >/dev/null 2>&1 && _wg_fw3_rules -D "$_dev" run
	_wg_kick_firewall
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

# Apply IPv4-only enforcement for one section. _src is 'glinet' or 'netifd'.
_wg_apply_section() {
	_src="$1"; _name="$2"; _dev="$3"
	wg_iface_is_valid "$_dev" || {
		_wg_warn "refusing to act on invalid iface name '$_dev' for $_name"
		return 1
	}
	case "$_src" in
		glinet) wg_snapshot_glinet "$_name"; wg_pin_glinet_ipv4 "$_name" ;;
		netifd) wg_snapshot_netifd "$_name"; wg_pin_netifd_ipv4 "$_name" ;;
		*)      return 1 ;;
	esac
	wg_apply_dev_hostside "$_dev"
	mkdir -p "$WG_NOIPV6_STATE_DIR"
	printf '%s\n' "$_dev" > "${WG_NOIPV6_STATE_DIR}/${_src}-${_name}.dev"
	_wg_log "enforced IPv4-only on ${_src}/${_name} (${_dev})"
}

wg_apply_glinet() { _wg_apply_section glinet "$@"; }
wg_apply_netifd() { _wg_apply_section netifd "$@"; }

wg_clear_dev() {
	_dev="$1"
	wg_iface_is_valid "$_dev" || return 1
	wg_remove_sysctl "$_dev"
	wg_remove_firewall "$_dev"
	_wg_log "removed enforcement on ${_dev}"
}

# Tear down enforcement using the on-disk state file as the source of
# truth for the previous device name (in case it has changed under us).
_wg_teardown_section() {
	_state="$1"
	_dev="$2"
	_old=""
	if [ -f "$_state" ]; then
		_old="$(head -n1 "$_state" 2>/dev/null)"
		wg_iface_is_valid "$_old" && wg_clear_dev "$_old"
		rm -f "$_state"
	fi
	# Avoid clearing the same dev twice when the state file matched.
	[ "$_old" = "$_dev" ] && return 0
	wg_iface_is_valid "$_dev" && wg_clear_dev "$_dev"
}

# Reconcile every WG server / interface against the global IPv6 state.
# Always emits exactly one summary line via _wg_say so the caller -- procd
# trigger, hotplug, or interactive shell -- can see what was done.
wg_sync_all() {
	mkdir -p "$WG_NOIPV6_STATE_DIR"

	_off=0
	[ "$(wg_global_ipv6_enabled)" = "0" ] && _off=1

	_g_apply=0; _n_apply=0
	_g_restore=0; _n_restore=0

	for _srv in $(wg_list_glinet_servers); do
		_dev="$(wg_glinet_kernel_dev "$_srv")"
		wg_iface_is_valid "$_dev" || continue
		if [ "$_off" -eq 1 ]; then
			wg_apply_glinet "$_srv" "$_dev" && _g_apply=$((_g_apply+1))
		else
			_wg_teardown_section "${WG_NOIPV6_STATE_DIR}/glinet-${_srv}.dev" "$_dev"
			[ -f "$(_wg_snap_path_glinet "$_srv")" ] && _g_restore=$((_g_restore+1))
			wg_restore_glinet "$_srv"
		fi
	done

	for _iface in $(wg_list_netifd_ifaces); do
		_dev="$(wg_netifd_kernel_dev "$_iface")"
		wg_iface_is_valid "$_dev" || continue
		if [ "$_off" -eq 1 ]; then
			wg_apply_netifd "$_iface" "$_dev" && _n_apply=$((_n_apply+1))
		else
			wg_unpin_netifd_ipv4 "$_iface"
			_wg_teardown_section "${WG_NOIPV6_STATE_DIR}/netifd-${_iface}.dev" "$_dev"
			[ -f "$(_wg_snap_path_netifd "$_iface")" ] && _n_restore=$((_n_restore+1))
			wg_restore_netifd "$_iface"
		fi
	done

	if [ "$_off" -eq 1 ]; then
		if [ $((_g_apply + _n_apply)) -eq 0 ]; then
			_wg_say "sync: global IPv6 disabled; no WireGuard sections to enforce"
		else
			_wg_say "sync: global IPv6 disabled; enforced IPv4-only on ${_g_apply} glinet + ${_n_apply} netifd section(s)"
		fi
	elif [ $((_g_restore + _n_restore)) -eq 0 ]; then
		_wg_say "sync: global IPv6 enabled; nothing to restore"
	else
		_wg_say "sync: global IPv6 enabled; restored ${_g_restore} glinet + ${_n_restore} netifd section(s) from snapshot"
	fi
}

# Package prerm helper. Restore everything to the pre-install state without
# touching the global IPv6 toggle.
#
# IMPORTANT: defer all init.d reloads until the very end and issue at most
# one each, in safe order (firewall -> wireguard_server -> network). On
# GL.iNet 4.x, multiple cascading firewall reloads overlapping with a
# wireguard_server reload can leave the router's own outbound traffic
# (including DNS) blackholed for several seconds.
wg_clear_all() {
	_wg_say "uninstall: starting full restore"

	_g_total="$(_wg_count_snaps glinet)"
	_n_total="$(_wg_count_snaps netifd)"

	# Defer every kick from here on; we'll coalesce at the end.
	_WG_NOIPV6_DEFER_KICK=1
	_WG_NOIPV6_PENDING_FIREWALL=0
	_WG_NOIPV6_PENDING_WGSERVER=0
	_WG_NOIPV6_PENDING_NETWORK=0
	export _WG_NOIPV6_DEFER_KICK

	# 1. Tear down host-side enforcement (sysctl drop-ins, fw4 nft drop-ins,
	#    fw3 scripts). wg_clear_dev will set _WG_NOIPV6_PENDING_FIREWALL=1
	#    via _wg_kick_firewall instead of reloading immediately.
	if [ -d "$WG_NOIPV6_STATE_DIR" ]; then
		for _f in "$WG_NOIPV6_STATE_DIR"/*.dev; do
			[ -f "$_f" ] || continue
			_dev="$(head -n1 "$_f" 2>/dev/null)"
			wg_iface_is_valid "$_dev" && wg_clear_dev "$_dev"
			rm -f "$_f"
		done
	fi
	# Belt-and-suspenders: drop any leftover drop-in files whose state file
	# was already gone (e.g. someone deleted /var/run by hand).
	for _f in "$WG_NOIPV6_SYSCTL_DIR"/99-wg-noipv6-*.conf \
	          "$WG_NOIPV6_NFT_DIR"/99-wg-noipv6-*.nft \
	          "$WG_NOIPV6_FW3_DIR"/wg-noipv6-*.sh; do
		[ -f "$_f" ] || continue
		rm -f "$_f"
		_WG_NOIPV6_PENDING_FIREWALL=1
	done

	# 2. Drop our firewall UCI includes (fw3 case). One commit if anything
	#    changed; the firewall reload is still deferred.
	_changed=0
	for _i in $(uci -q show firewall 2>/dev/null \
			| awk -F. '/^firewall\.wg_noipv6_/ { sub(/=.*/, "", $2); print $2 }' \
			| sort -u); do
		uci -q delete "firewall.${_i}" 2>/dev/null && _changed=1
	done
	if [ "$_changed" -eq 1 ]; then
		uci -q commit firewall
		_WG_NOIPV6_PENDING_FIREWALL=1
	fi

	# 3. Restore UCI snapshots. wg_restore_glinet will set
	#    _WG_NOIPV6_PENDING_WGSERVER via the deferred kick rather than
	#    reloading wireguard_server once per server.
	wg_restore_all
	[ "$_n_total" -gt 0 ] && _WG_NOIPV6_PENDING_NETWORK=1

	# 4. Coalesced reloads, in the only safe order:
	#    a) firewall first -- our drop-ins must be gone before
	#       wireguard_server tries to bring wgserver back with v6 addrs.
	#    b) wireguard_server next -- so the daemon re-emits its own zone
	#       on top of a clean firewall.
	#    c) network last -- only when netifd UCI was actually mutated.
	unset _WG_NOIPV6_DEFER_KICK
	[ "$_WG_NOIPV6_PENDING_FIREWALL" = "1" ] && _wg_kick_firewall
	[ "$_WG_NOIPV6_PENDING_WGSERVER" = "1" ] && _wg_kick_wgserver
	[ "$_WG_NOIPV6_PENDING_NETWORK"  = "1" ] && _wg_kick_network

	# 5. Drop on-disk backup + state.
	rm -rf "$WG_NOIPV6_BACKUP_DIR" 2>/dev/null || true
	rmdir /etc/wg-noipv6 2>/dev/null || true
	rm -rf "$WG_NOIPV6_STATE_DIR" 2>/dev/null || true

	_wg_say "uninstall: complete; restored ${_g_total} glinet + ${_n_total} netifd section(s)"
}
