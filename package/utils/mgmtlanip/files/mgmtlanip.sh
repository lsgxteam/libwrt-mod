#!/bin/sh
#filename: mgmtlanip.sh

VERSION="v1.0.5-20220315"
TAGNAME="mgmtlanip"

logg()
{
	echo "$*" | logger -s -p user.debug -t ${TAGNAME}
}

### 

# Global Variable Definition 
DISABLED=1
SECOND=10
RESPAWN=1
INTERFACE_BR_NAME='br-lan'
INTERFACE_BR_INDEX='0'
IPV4_BR_ADDRESS='192.168.10.254'
IPV4_BR_NETMASK='255.255.255.0'


create_mgmtlanip_config()
{
	touch /etc/config/mgmtlanip
	cat /dev/null > /etc/config/mgmtlanip
	uci -q batch <<-EOF >/dev/null
		delete mgmtlanip.@mgmtlanip[0]
		add mgmtlanip mgmtlanip
		set mgmtlanip.@mgmtlanip[-1].disabled='1'
		set mgmtlanip.@mgmtlanip[-1].second='10'
		set mgmtlanip.@mgmtlanip[-1].respawn='1'
		set mgmtlanip.@mgmtlanip[-1].interface_br_name='br-lan'
		set mgmtlanip.@mgmtlanip[-1].interface_br_index='0'
		set mgmtlanip.@mgmtlanip[-1].ipv4_br_address='192.168.10.254'
		set mgmtlanip.@mgmtlanip[-1].ipv4_br_netmask='255.255.255.0'
		commit mgmtlanip
	EOF
}

get_mgmtlanip_config()
{
	DISABLED=$( uci get mgmtlanip.@mgmtlanip[-1].disabled )
	SECOND=$( uci get mgmtlanip.@mgmtlanip[-1].second )
	RESPAWN=$( uci get mgmtlanip.@mgmtlanip[-1].respawn )
	INTERFACE_BR_NAME="$( uci get mgmtlanip.@mgmtlanip[-1].interface_br_name )"
	INTERFACE_BR_INDEX="$( uci get mgmtlanip.@mgmtlanip[-1].interface_br_index )"
	IPV4_BR_ADDRESS="$( uci get mgmtlanip.@mgmtlanip[-1].ipv4_br_address )"
	IPV4_BR_NETMASK="$( uci get mgmtlanip.@mgmtlanip[-1].ipv4_br_netmask )"
}

###

_fix_mgmtlanip()
{
	local lan_ifname lan_ifidx lan_ipad lan_netm lan_mask is_lan
	[ $# -ne 4 ] && return 0
	lan_ifname="$1"
	lan_ifidx="$2"
	lan_ipad="$3"
	lan_netm="$4"
	lan_mask="$( /bin/ipcalc.sh $lan_ipad $lan_netm | grep PREFIX | awk -F = '{print $2}' )"
	lan_brd="$( /bin/ipcalc.sh $lan_ipad $lan_netm | grep BROADCAST | awk -F = '{print $2}' )"
	is_lan=0
	is_first=0
	route_count=0
	match_count=0

	for ifname in "$( ip -4 addr show | grep inet | grep ${lan_ipad}/${lan_mask} | awk '{print $NF, $2}' | awk '{print $1}' )"
	do
		if [ "$ifname" = "${lan_ifname}:${lan_ifidx}" ]; then
			is_lan=1
			#route_count=$( ip -4 addr show dev ${lan_ifname} | grep inet | wc -l )
			#[ $route_count -eq 1 ] && is_first=1
			#[ $route_count -ge 2 ] && is_first=$( ip -4 addr show dev ${lan_ifname} | grep -m 1 inet | grep ${lan_ipad}/${lan_mask} | wc -l )
			#[ $is_first -eq 1 ] && ip -4 addr del ${lan_ipad}/${lan_mask} dev ${lan_ifname} && is_lan=0 && 
			#	logg "* _fix_mgmtlanip - del first item - ip -4 addr del ${lan_ipad}/${lan_mask} dev ${lan_ifname} "
		else
			[ ! -z "$ifname" ] && ip -4 addr del ${lan_ipad}/${lan_mask} dev ${ifname} && is_lan=0 && 
				logg "* _fix_mgmtlanip - del other item - ip -4 addr del ${lan_ipad}/${lan_mask} dev ${ifname} "
		fi
	done

	route_count=$( ip -4 addr show dev ${lan_ifname} | grep inet | wc -l )
	[ $route_count -ge 0 ] && [ $is_lan -eq 0 ] && 
		[ ! -z "$lan_ifname" ] && ip -4 addr add ${lan_ipad}/${lan_mask} brd ${lan_brd} dev ${lan_ifname} label ${lan_ifname}:${lan_ifidx} && is_lan=1 && 
		logg "* _fix_mgmtlanip - ip -4 addr add ${lan_ipad}/${lan_mask} brd ${lan_brd} dev ${lan_ifname} label ${lan_ifname}:${lan_ifidx} "

	return $is_lan
}


### ( Main )
# TODO: 
#   * 
#

mgmtlanip_main()
{
	[ -f "/etc/config/mgmtlanip" ] || create_mgmtlanip_config 
	[ -f "/etc/config/mgmtlanip" ] && get_mgmtlanip_config 
	[ ${DISABLED} -eq 1 ] && logg "* mgmtlanip_main - service disable, will be exit. " && return 0

	[ $( ubus call network.device status "{\"name\":\"$INTERFACE_BR_NAME\"}" | jsonfilter -q -e '@.up' ) == "true" ] && 
		_fix_mgmtlanip "$INTERFACE_BR_NAME" "$INTERFACE_BR_INDEX" "$IPV4_BR_ADDRESS" "$IPV4_BR_NETMASK" 

	return 0
}


SECOND=10
[ $# -eq 1 ] && SECOND=$1
while true
do
	mgmtlanip_main
	sleep $SECOND
done

exit 0

### ( End )
