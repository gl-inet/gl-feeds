# gl-sdk4-wireguard-ipv4-only

Pins every WireGuard interface on a GL.iNet router to IPv4-only when global
IPv6 is disabled on `/#/ipv6`. No new GUI control. When IPv6 is re-enabled
the original UCI is restored from on-disk snapshots and stock behaviour
returns.

## Symmetry

The disable path is destructive: it strips IPv6 from `wireguard_server` and
`network` and disables IPv6 on the kernel device. To make it reversible the
originals are saved under `/etc/wg-noipv6/backup/` (mode 0700, files 0600)
**before** anything is stripped, and replayed on enable. After a glinet
restore the daemon is reloaded so it picks up the restored UCI.

Snapshotted fields:

* glinet: `wireguard_server.<srv>.address_v6`,
  `wireguard_server.<peer>.client_ip`, `…allowed_ips`
* netifd: `network.<iface>.ipv6`, IPv6 entries from `…addresses`

## Uninstall

`opkg remove gl-sdk4-wireguard-ipv4-only` runs `wg-noipv6 clear-all`. Order
matters because the WireGuard daemon needs the kernel device IPv6-capable
again before it will re-add v6 addresses:

1. Tear down host-side enforcement (sysctl drop-ins, fw rules,
   `disable_ipv6=0`).
2. Drop the package's firewall UCI includes; reload firewall.
3. `sysctl --system` so `/proc` reflects only the user's settings.
4. Restore every UCI snapshot; reload `wireguard_server`.
5. Reload `network` only if any netifd snapshot was restored.
6. Remove `/etc/wg-noipv6/backup/` and `/var/run/wg-noipv6/`.

## Global-IPv6 detection

First match wins:

1. `glconfig.general.enable_ipv6`
2. `network.wan6.disabled` == `1`
3. `network.wan6.proto` == `none`
4. `network.wan6.auto` == `0`
5. default: enabled

## When sync runs

| Trigger                                                              | Hook                                |
|----------------------------------------------------------------------|-------------------------------------|
| First boot after install                                             | `/etc/uci-defaults/80-wg-noipv6`    |
| Boot                                                                 | `/etc/init.d/wg-noipv6` start       |
| `uci commit` of `wireguard_server`/`network`/`firewall`/`glconfig`   | procd reload trigger                |
| netifd `ifup` of a tracked WG interface                              | `/etc/hotplug.d/iface/99-wg-noipv6` |
| Kernel netdev `add` for `wg*` / `*wgserver*`                         | `/etc/hotplug.d/net/99-wg-noipv6`   |
| Manual                                                               | `wg-noipv6 sync` or RPC `sync`      |

No polling. Toggling `/#/ipv6` writes `glconfig`, which is in the trigger
list, so the IPv6 page alone drives a sync.

## CLI

```
wg-noipv6 sync         # default; reconcile every WG section
wg-noipv6 apply <dev>  # force IPv4-only on a kernel device
wg-noipv6 clear <dev>  # remove enforcement on a kernel device
wg-noipv6 clear-all    # restore snapshots and remove every drop-in
wg-noipv6 status       # PASS/FAIL audit per section
```

`status` audits both states: when IPv6 is disabled it verifies enforcement
is in place; when enabled it verifies no leftover state remains.

## RPC

`/usr/lib/oui-httpd/rpc/wireguard_ipv4_only` (read-only):

* `get_status({})` → `{ global_ipv6_enabled, servers: [...] }`
* `sync({})` → `{ ok = true }`

## Logging

Tag `wg-noipv6` (`logread -e wg-noipv6`). Every CLI command emits exactly
one summary line so it never looks like a no-op, plus per-key detail lines
to syslog. Hooks set `WG_NOIPV6_QUIET=1` to suppress stderr; syslog is
always written.

```
$ wg-noipv6 sync
wg-noipv6: sync: global IPv6 disabled; enforced IPv4-only on 1 glinet + 0 netifd section(s)
```

## Files

```
/usr/sbin/wg-noipv6                           CLI
/usr/lib/wg-noipv6/functions.sh               helper library
/etc/init.d/wg-noipv6                         reload-only procd service
/etc/hotplug.d/iface/99-wg-noipv6             netifd ifup hook
/etc/hotplug.d/net/99-wg-noipv6               kernel netdev hook
/etc/uci-defaults/80-wg-noipv6                first-boot bootstrap
/usr/lib/oui-httpd/rpc/wireguard_ipv4_only    GUI RPC
/etc/wg-noipv6/backup/                        UCI snapshots (on demand)
/var/run/wg-noipv6/                           live state files
```
