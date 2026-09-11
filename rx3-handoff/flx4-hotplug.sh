#!/bin/bash
# udev helper: when the DDJ-FLX4 sound card appears and the player is not using it, restart the service onto it.
sleep 3
grep -q FLX4 /home/rx3/rx3-rootfs/etc/rx3-ctl 2>/dev/null && pgrep -x rbp-pi >/dev/null && exit 0
systemctl is-active --quiet rx3 || exit 0
logger -t rx3 "DDJ-FLX4 appeared; restarting rx3 onto it"
systemctl restart rx3
