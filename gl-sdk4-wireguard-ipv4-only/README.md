# gl-sdk4-wireguard-ipv4-only

Per-server **IPv4-only** enforcement for the GL.iNet WireGuard
backend, integrated with `/etc/config/wireguard_server`.

This package adds a `enable_ipv6` UCI option to each WireGuard
`servers` section. When the option is set to `0`, IPv6 is forcibly
suppressed on the WG interface and stays suppressed across GUI
regeneration, ifup events, kernel device reappearance and reboots.
When the option is `1` (or unset), behaviour is unchanged.

The implementation mirrors the upstream reference
[configure-wireguard-ipv4-only.sh](https://github.com/blackoutsecure/bos-scripts-collection/blob/main/linux/openwrt/network-security/wireguard-ipv4-only/configure-wireguard-ipv4-only.sh)
but is wired into GL.iNet's `wireguard_server` config layout instead
of being driven by a one-shot install / uninstall script.

## UCI option

```
config servers 'main_server'
    option enable_ipv6 '0'    # 0 = enforce IPv4-only, 1 = default
    ...
```

The same option is recognised on `proto=wireguard` sections in
`/etc/config/network` for routers that use the netifd layout.

## What enforcement does

When `enable_ipv6=0`, on every UCI change, ifup event or kernel
netdev appearance, the package will:

* Strip `address_v6` from the WG `servers` section.
* Strip `:`-bearing entries from each peer's `client_ip` and
  `allowed_ips` (so AllowedIPs become IPv4-only).
* Set `network.<wg-iface>.ipv6=0` and remove IPv6 entries from
  `network.<wg-iface>.addresses` (netifd layout only).
* Install `/etc/sysctl.d/99-wg-noipv6-<dev>.conf` with
  `net.ipv6.conf.<dev>.disable_ipv6 = 1`.
* Install an nftables drop-in (`fw4`) or a fw3 firewall include
  that drops every IPv6 packet entering, leaving or being forwarded
  through the WG kernel device.
* Remove any non-link-local IPv6 address currently bound to the WG
  kernel device.
* Re-program any kernel-side peer whose AllowedIPs still contain
  IPv6 entries (`wg set <dev> peer <pub> allowed-ips ...`).

When `enable_ipv6=1` (or unset), every drop-in we own for that
device is removed and live state is restored to the default.

## On-disk layout

```
/usr/sbin/wg-noipv6                          # CLI: sync|apply|clear|clear-all|status
/usr/lib/wg-noipv6/functions.sh              # POSIX-sh helper library
/etc/init.d/wg-noipv6                        # reload-only service (procd)
/etc/hotplug.d/iface/99-wg-noipv6            # netifd ifup hook
/etc/hotplug.d/net/99-wg-noipv6              # kernel netdev hook (glinet)
/etc/uci-defaults/80-wg-noipv6               # first-boot bootstrap
/usr/lib/oui-httpd/rpc/wireguard_ipv4_only   # GUI RPC adapter
```

## CLI

```
wg-noipv6 sync         # re-evaluate enable_ipv6 for every WG server
wg-noipv6 apply <dev>  # force-apply IPv4-only on a kernel device
wg-noipv6 clear <dev>  # remove enforcement for a kernel device
wg-noipv6 clear-all    # remove every drop-in this package owns
wg-noipv6 status       # PASS/FAIL audit per WG server
```

`sync` is idempotent and is what every other entry point (init.d,
hotplug, RPC adapter) calls.

## RPC

The Lua adapter at `/usr/lib/oui-httpd/rpc/wireguard_ipv4_only`
exposes:

* `get_config({ server? })` -> `{ server, enable_ipv6 }`
* `set_config({ server?, enable_ipv6 })` -> `{ server, enable_ipv6 }`
* `get_status({})` -> `{ servers: [{ server, enable_ipv6, enforced,
  kernel_dev, live_global_ipv6 }, ...] }`

`server` defaults to the first `servers` section in
`/etc/config/wireguard_server`, which on GL.iNet 4.x firmware is
typically `main_server`.

## Security notes

* All paths interpolating a device name into root-owned drop-ins
  (sysctl, nftables, fw3 includes) validate the name against
  `[A-Za-z][A-Za-z0-9_-]*` first.
* The RPC `set_config` rejects unknown server section names.
* Peers that are IPv6-only (no IPv4 AllowedIPs at all) are left
  untouched on the kernel side -- silently removing them would
  unroute the peer entirely.
