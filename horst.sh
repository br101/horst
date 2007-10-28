#!/bin/sh

if grep -q ath[0-9]: /proc/net/dev;then
	BASE=wifi0
	if [ -n "$1" ] && [ -z "${1#wifi[0-9]}" ];then
		BASE=$1
		shift
	fi
	WLDEV=mon0
	wlanconfig $WLDEV create wlandev $BASE wlanmode monitor >/dev/null
	ip link set dev $WLDEV up
	horst -i $WLDEV $*
	ip link set dev $WLDEV down
	wlanconfig $WLDEV destroy
else
	horst $*
fi
