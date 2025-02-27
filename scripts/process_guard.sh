#!/bin/bash


rpath=/usr/local/xt

sql=`sqlite3 $rpath/db/xt.db "SELECT channel_no FROM m_channel;"`
for channel in $sql; do
	var=`ps -ef | grep -v grep | grep "$rpath/bin/xftp $channel"`
	if [[ -z "$var" ]]; then
		$rpath/bin/xftp "$channel" 1920 1080 > $rpath/logs/xftp_"$channel"_"$(date +%Y%m%d%H%M%S).log" 2>&1 &
	fi
done


var=`ps -ef | grep -v grep | grep "$rpath/bin/monitor"`
if [[ -z "$var" ]]; then
	$rpath/bin/monitor > $rpath/logs/monitor_"$(date +%Y%m%d%H%M%S).log" 2>&1 &
fi
