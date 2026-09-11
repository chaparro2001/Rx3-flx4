#!/bin/bash
# Start the right pointer bridge for the RX3 screen: a touchscreen if present, otherwise a USB mouse. Idempotent.
R=/home/rx3/rx3-rootfs; B=/home/rx3/rx3-touch-bridge
pgrep -x rbp-pi >/dev/null || exit 0
sleep 1
touch=""; mouse=""
for ev in /dev/input/event*; do
  p=$(udevadm info -q property -n $ev 2>/dev/null)
  echo "$p" | grep -q "ID_INPUT_TOUCHSCREEN=1" && { touch=$ev; break; }
  echo "$p" | grep -q "ID_INPUT_MOUSE=1" && [ -z "$mouse" ] && mouse=$ev
done
if [ -n "$touch" ]; then mode=""; dev=$touch; elif [ -n "$mouse" ]; then mode="--mouse"; dev=$mouse; else exit 0; fi
cur=$(pgrep -a -f "^/home/rx3/rx3-touch-bridge" | head -1)
case "$cur" in *"$dev"*) exit 0;; esac          # already bridging this device
pkill -f "^/home/rx3/rx3-touch-bridge"; sleep 0.5
nohup sudo -u rx3 $B $mode $dev $R/dev/tsc2007_2-0048 > /home/rx3/rx3-touch.log 2>&1 < /dev/null &
logger -t rx3 "pointer bridge started: $mode $dev"
