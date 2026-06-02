# dnsmasq — GL.iNet Feed Patches

This feed tracks the upstream OpenWrt `dnsmasq` package with GL.iNet-specific patches applied on top.

## Patches

### fix: VPN connection failure with global Kill Switch + AdGuardHome (dnsmasq v2.92)

**Issue:** `#SDK-22992`
**Author:** zongxin.yang <zongxin.yang@gl-inet.com>

When both the **global Kill Switch** and **AdGuardHome** were enabled simultaneously, VPN connections failed due to incorrect DNS resolution priority. The root cause was an inverted default/conditional logic for the `localuse` parameter in the dnsmasq init script (`dnsmasq.init`).

#### What changed

| Location | Before | After |
|---|---|---|
| `dnsmasq_start()` — default | `localuse=1` | `localuse=0` |
| `dnsmasq_start()` — conditional | `[ "$resolvfile" != "/tmp/resolv.conf.d/resolv.conf.auto" ] && localuse=0` | `[ "$resolvfile" = "/tmp/resolv.conf.d/resolv.conf.auto" ] && localuse=1` |
| `dnsmasq_stop()` — default | `localuse=1` | `localuse=0` |
| `dnsmasq_stop()` — conditional | `[ "$noresolv" = 0 ] && [ "$resolvfile" != "/tmp/resolv.conf.d/resolv.conf.auto" ] && localuse=0` | `[ "$noresolv" = 0 ] && [ "$resolvfile" = "/tmp/resolv.conf.d/resolv.conf.auto" ] && localuse=1` |

The logic is now **opt-in** rather than **opt-out**:

- **Default:** `localuse=0` — dnsmasq does **not** claim `/tmp/resolv.conf` by default.
- **Enabled only when** `resolvfile` points to the standard auto-generated resolver file (`/tmp/resolv.conf.d/resolv.conf.auto`), in which case `localuse` is set to `1`.

This ensures that when a custom DNS backend (e.g. AdGuardHome) manages resolution, dnsmasq does not overwrite `/tmp/resolv.conf`, preserving the correct DNS priority chain required by the Kill Switch feature.

## Upgrade Notes

When bumping to a newer upstream dnsmasq version, verify the following:

1. **Re-apply the `localuse` patch.** The upstream init script defaults `localuse` to `1` and uses an opt-out conditional. Our patch inverts both to opt-in. Check `dnsmasq_start()` and `dnsmasq_stop()` in `files/dnsmasq.init` and confirm the patched logic is preserved.

2. **Watch for upstream refactors.** If the upstream project restructures the init script (e.g. moves the `localuse` handling into a helper function or changes the variable name), the patch will need to be adapted accordingly.

3. **Test the interaction between Kill Switch, AdGuardHome, and VPN.** After any upgrade, validate the full scenario: enable global Kill Switch, enable AdGuardHome, then establish a VPN connection and confirm DNS resolution works correctly end-to-end.
