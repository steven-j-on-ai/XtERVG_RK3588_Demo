#!/bin/bash

# 目前只支持 rk3588 / rdkx3
arch="rk3588"
# 目前只支持 Ubuntu 20/22
user="root"
lsb=`lsb_release -r -s | awk -F "." '{print $1}'`
if [ "$lsb" = "20" ]; then
	:
elif [ "$lsb" = "22" ]; then
	:
else
	# 不支持的Ubuntu版本
	echo "Unsupported system version($lsb) exit ."
	exit
fi

# 配置DNS, 如已配置可注释掉
var=`cat /etc/resolv.conf | grep "114.114.114.114"`
if [[ -z "$var" ]]; then
	echo "nameserver 114.114.114.114" >> /etc/resolv.conf
fi

# 安装所需依赖, 如已安装可注释掉
apt-get update
apt-get -y install cron libcjson-dev libcurl4-openssl-dev sqlite3 libsqlite3-dev

# 部署所需环境
rpath=/usr/local/xt
mkdir -p $rpath/bin $rpath/lib $rpath/conf $rpath/db $rpath/logs $rpath/scripts $rpath/models $rpath/debug $rpath/screenshot
if [ ! -e /etc/ld.so.conf.d/xt.conf ]; then
	echo "$rpath/lib" > /etc/ld.so.conf.d/xt.conf
fi
if [ ! -e $rpath/db/xt.db ]; then
	cp -f db/xt.db $rpath/db
fi
if [ ! -e $rpath/conf/xt.conf ]; then
	cp -f conf/xt.conf $rpath/conf
fi

cp -f libs/* $rpath/lib
cp -f scripts/* $rpath/scripts
cp -f bin/$arch/$lsb/* $rpath/bin
cp -f models/* $rpath/models
ldconfig

chown -Rf $user:$user $rpath
chmod -Rf 777 $rpath

var=`ps -ef | grep -v grep | grep "$rpath/bin/monitor"`
if [ -n "$var" ]; then
	killall monitor
fi

var=`ps -ef | grep -v grep | grep "$rpath/bin/xftp"`
if [ -n "$var" ]; then
	killall xftp
fi

$rpath/scripts/process_guard.sh

if [ "$arch" = "rk3588" ]; then
	# 开启定时任务
	var=`cat /etc/ppp.sh | grep "$rpath/scripts/process_guard.sh"`
	if [ -n "$var" ]; then
		:
	else
		echo "$rpath/scripts/process_guard.sh" >> /etc/ppp.sh
	fi
elif [ "$arch" = "rdkx3" ]; then
	# 开启定时任务
	var=`crontab -u $user -l | grep "$rpath/scripts/process_guard.sh"`
	if [ -n "$var" ]; then
		:
	else
		crontab -u $user -l > cron
		echo "* * * * * $rpath/scripts/process_guard.sh" >> cron
		echo "* * * * * sleep 10; $rpath/scripts/process_guard.sh" >> cron
		echo "* * * * * sleep 20; $rpath/scripts/process_guard.sh" >> cron
		echo "* * * * * sleep 30; $rpath/scripts/process_guard.sh" >> cron
		echo "* * * * * sleep 40; $rpath/scripts/process_guard.sh" >> cron
		echo "* * * * * sleep 50; $rpath/scripts/process_guard.sh" >> cron
		crontab -u $user cron
		unlink cron
		systemctl enable cron
	fi
fi
