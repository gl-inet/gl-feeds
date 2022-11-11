#!/bin/sh

. /lib/functions/gl_util.sh

DHCP_FILE="/tmp/dnsmasq.d/drop-in-dhcp"
DHCP_INFO="/tmp/drop-in-dhcp-info"

POOL_START
POOL_END
POOL_NETMASK
POOL_LEASETIME
UPSTREAM_GATEWAY
LOCAL_IPADDR

setup_dhcp_for_wan()
{
    echo "dhcp-range=set:wan,${POOL_START},${POOL_END},${POOL_NETMASK},${POOL_LEASETIME}" >/tmp/dnsmasq.d/drop-in-dhcp
    /etc/init.d/dnsmasq restart
}

remove_dhcp_for_wan()
{
    rm /tmp/dnsmasq.d/drop-in-dhcp
    /etc/init.d/dnsmasq restart
}

check_dhcp_server()
{
    local WAN="$(get_wan)"
    local INFO="$(dhcpdiscover -i ${WAN} -p -t 5 ${LOCAL_IPADDR:+-b $LOCAL_IPADDR})"
    if [ -n "$INFO" ];then
        echo $INFO > $DHCP_INFO
    else
        rm $DHCP_INFO
    fi
}


