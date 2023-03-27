#!/bin/sh
. /lib/netifd/netifd-wireless.sh

init_wireless_driver "$@"

drv_mtk_init_device_config() {
    config_add_int txpower
    config_add_array channels
    config_add_string country
}

drv_mtk_init_iface_config() {
    config_add_string ifname 'macaddr:macaddr' 'bssid:macaddr'
    config_add_boolean hidden isolate wmm rsn_preauth ieee80211k igmp_snooping
    config_add_int maxassoc ieee80211w 'port:port'
    config_add_string 'server:ip4addr'
}

drv_mtk_cleanup() {
    return
}

drv_mtk_setup() {
    json_add_string device "$1"

    ubus call mtk-wifi setup "$(json_dump)"

    local wait=30
    local up

    while [ $wait -gt 0 ];
    do
        json_load "$(ubus call mtk-wifi status)"
        json_get_vars up

        [ $up -eq 1 ] && {
            wireless_set_up
            return
        }

        let wait=wait-1
        sleep 1
    done
}

drv_mtk_teardown() {
    ubus call mtk-wifi teardown "{\"device\": \"$1\"}"
}

add_driver mtk
