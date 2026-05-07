# gl-sdk4-wireguard-ipv4-only

Native **IPv4-only** enforcement for the GL.iNet WireGuard backend,
keyed off the existing global IPv6 toggle on `/#/ipv6` -- no new
GUI control. When IPv6 is disabled there, every WireGuard interface
on the router is pinned to IPv4-only. When IPv6 is re-enabled, the
GL.iNet default behaviour returns.

Reconciliation runs on every UCI change, ifup, kernel netdev
appearance and at boot.

## How "global IPv6" is detected

Layered fallback in `wg_global_ipv6_enabled` (first match wins):

1. `glconfig.general.enable_ipv6`
2. `network.wan6.disabled` == `1`
3. `network.wan6.proto` == `none`
4. `network.wan6.auto` == `0`
5. Default: enabled

## CLI

```
wg-noipv6 sync         # reconcile every WG server against global IPv6 state
wg-noipv6 apply <dev>  # force-apply IPv4-only on a kernel device
wg-noipv6 clear <dev>  # remove enforcement for a kernel device
wg-noipv6 clear-all    # remove every drop-in this package owns
wg-noipv6 status       # PASS/FAIL audit per WG server
```

## RPC

`/usr/lib/oui-httpd/rpc/wireguard_ipv4_only` -- read-only:

* `get_status({})` -> `{ global_ipv6_enabled, servers: [...] }`
* `sync({})` -> `{ ok = true }`

## Files

```
/usr/sbin/wg-noipv6                          CLI
/usr/lib/wg-noipv6/functions.sh              helper library
/etc/init.d/wg-noipv6                        reload-only service (procd)
/etc/hotplug.d/iface/99-wg-noipv6            netifd ifup hook
/etc/hotplug.d/net/99-wg-noipv6              kernel netdev hook
/etc/uci-defaults/80-wg-noipv6               first-boot bootstrap
/usr/lib/oui-httpd/rpc/wireguard_ipv4_only   GUI RPC
```
