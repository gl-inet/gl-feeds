function get_wan_if_name ()
{
	echo "eth0.2"
}

function handle_switch_config ()
{
	mode=$1
	if [ "$mode" == "ap" ]; then
		/sbin/uci set network.lan.ifname="eth0.1 eth0.2"
	elif [ "$mode" == "gw" ]; then
		/sbin/uci set network.lan.ifname="eth0.1"
	fi
	/sbin/uci commit network
}

function get_sf1200_serial_num ()
{
    suff=`dd if=/dev/mtd3 bs=1 skip=$((0x430)) count=16 2>/dev/null`
    echo $suff
}

