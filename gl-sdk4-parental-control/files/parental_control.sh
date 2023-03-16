. /usr/share/libubox/jshn.sh
. /lib/functions.sh

SET_BASE="0"
ADD_RULE="1"
ADD_GROUP="2"
CLEAN_RULE="3"
CLEAN_GROUP="4"
SET_RULE="5"
SET_GROUP="6"
UPDATE_URL=""
UPDATE_TIME=""
UPDATE_EN="0"

str_action_num()
{
case $1 in
"DROP")
	echo 0
;;
"ACCEPT")
	echo 1
;;
"POLICY_DROP")
	echo 2
;;
"POLICY_ACCEPT")
	echo 3
;;
esac    
}

url_to_app_feature()
{
	case "$1" in
		\[*\]) echo "$1"|sed -e "s/\[\|\|\]//g"| tr ',' ' '
		;;
		*) echo "tcp;;;$1;;"
		;;
	esac
}


config_apply()
{
    test -z "$1" && return 1
    
	if [ -e "/dev/parental_control" ];then
    	#[ "$DEBUG" = "1" ] && echo "config json str=$1"
    	echo "$1" >/dev/parental_control
	fi
}

load_base_config()
{
    local drop_anonymous src_dev
    config_get drop_anonymous "global" "drop_anonymous" "0"
    config_get src_dev "global" "src_dev"
    config_get UPDATE_TIME "global" "update_time"
    config_get UPDATE_URL "global" "update_url"
    config_get UPDATE_EN "global" "auto_update" "0"

    json_init
    json_add_int "op" $SET_BASE
    json_add_object "data"
    json_add_int "drop_anonymous" $drop_anonymous
    json_add_string "src_dev" "$src_dev"
    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

load_rule()
{
    json_init
    json_add_int "op" $ADD_RULE
    json_add_object "data"
    json_add_array "rules"

    load_rule_cb(){
        local config=$1
        local action apps action_str blacklist
        config_get action_str "$config" "action"
        config_get apps "$config" "apps"
        config_get blacklist "$config" "blacklist"
        action="$(str_action_num $action_str)"
        json_add_object ""
        json_add_string "id" "$config"  
        json_add_int "action" $action
        [ -n "$apps" ] && {
            json_add_array "apps"
            for app in $apps;do
                json_add_int "" $app
            done
            json_select ..
        }
        [ -n "$blacklist" ] && {
            json_add_array "blacklist"
            for item in $blacklist;do
                for i in $(url_to_app_feature $item);do
                    json_add_string "" "$i" 
                done
            done
            json_select ..
        }
        json_select ..
    }

    config_foreach load_rule_cb rule

    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

load_group()
{
    json_init
    json_add_int "op" $ADD_GROUP
    json_add_object "data"
    json_add_array "groups"

    load_group_cb(){
        local config=$1
        local rule macs
        config_get rule "$config" "default_rule"
        config_get macs "$config" "macs"
        json_add_object ""
        json_add_string "id" "$config"
        json_add_string "rule" $rule
        [ -n "$macs" ] && {
            json_add_array "macs"
            for mac in $macs;do
                json_add_string "" $mac
            done
            json_select ..
        }
        json_select ..
    }

    config_foreach load_group_cb group

    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}


clean_rule()
{
    json_init

    json_add_int "op" $CLEAN_RULE
    json_add_object "data"

    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

clean_group()
{
    json_init

    json_add_int "op" $CLEAN_GROUP
    json_add_object "data"

    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup
}

set_group_rule()
{
    local config=$1
    local rule=$2
    local rule macs

    json_init
    json_add_int "op" $SET_GROUP
    json_add_object "data"
    json_add_array "groups"
        
    config_get macs "$config" "macs"
    json_add_object ""
    json_add_string "id" "$config"  
    json_add_string "rule" $rule
    [ -n "$macs" ] && {
        json_add_array "macs"
        for mac in $macs;do
            json_add_string "" $mac
        done
            json_select ..
    }
    json_select ..

    json_str=`json_dump`
    config_apply "$json_str"
    json_cleanup    
}

update_feature_lib()
{
    local url="$1"
    local cur_v="$(cat /etc/parental_control/app_feature.cfg |head -n1|tr -cd '[0-9]')"
    local new_v=""
    curl  -LSs --connect-timeout 30 -m 30 "$url" -o /tmp/app_feature.cfg && {
        new_v="$(cat /tmp/app_feature.cfg |head -n1|tr -cd '[0-9]')"
        [ "$new_v" -gt "$cur_v" ] && {
            logger -t 'parental_control' "update app featere library from $cur_v to $new_v"
            mv /tmp/app_feature.cfg /etc/parental_control/app_feature.cfg
            sleep 3 && /etc/init.d/parental_control restart &
            return
        }
    }
}