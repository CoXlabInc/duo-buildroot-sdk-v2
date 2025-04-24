#!/bin/sh

insmod /mnt/system/ko/dot11ah.ko
insmod /mnt/system/ko/morse.ko country=KR
wpa_supplicant_s1g -t -D nl80211 -s -i wlan0 -c /var/run/wpa_supplicant.conf -B
