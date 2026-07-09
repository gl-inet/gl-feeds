#!/bin/sh

. /lib/functions.sh

DNSMASQ_GROUP="dnsmasq"
DNSMASQ_GROUP_ID="453"
DNSMASQ_VPN_USER="dnsmasq_vpn"
DNSMASQ_VPN_USER_ID="456"

fix_dnsmasq_vpn_user() {
    local gid
    local uid

    if ! group_exists "$DNSMASQ_GROUP"; then
        group_add "$DNSMASQ_GROUP" "$DNSMASQ_GROUP_ID"
    fi

    gid="$(awk -F: -v name="$DNSMASQ_GROUP" '$1 == name { print $3; exit }' /etc/group)"
    [ -z "$gid" ] && return 1

    if ! user_exists "$DNSMASQ_VPN_USER"; then
        uid="$DNSMASQ_VPN_USER_ID"

        if cut -d: -f3 /etc/passwd | grep -qx "$uid"; then
            uid=""
        fi

        user_add "$DNSMASQ_VPN_USER" "$uid" "$gid" "$DNSMASQ_VPN_USER" "/var/run/$DNSMASQ_VPN_USER" "/bin/false"
    fi

    user_exists "$DNSMASQ_VPN_USER" || return 1

    if [ -f /etc/shadow ] && ! grep -q "^${DNSMASQ_VPN_USER}:" /etc/shadow; then
        echo "${DNSMASQ_VPN_USER}:x:0:0:99999:7:::" >> /etc/shadow
    fi

    group_add_user "$DNSMASQ_GROUP" "$DNSMASQ_VPN_USER"
}

fix_dnsmasq_vpn_user

exit 0
