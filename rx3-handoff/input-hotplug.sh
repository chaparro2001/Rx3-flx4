#!/bin/bash
# Start the right pointer bridge for the RX3 screen: a touchscreen if present, otherwise a USB mouse. Idempotent.
. "$(dirname "$(readlink -f "$0")")/rx3-env.sh"
R=$RX3_ROOT; B=$RX3_BINDIR/rx3-touch-bridge
pgrep -x rbp-pi >/dev/null || exit 0
sleep 1
touch=""; mouse=""
for ev in /dev/input/event*; do
  p=$(udevadm info -q property -n $ev 2>/dev/null)
  echo "$p" | grep -q "ID_INPUT_TOUCHSCREEN=1" && { touch=$ev; break; }
  echo "$p" | grep -q "ID_INPUT_MOUSE=1" && [ -z "$mouse" ] && mouse=$ev
done
if [ -n "$touch" ]; then mode=""; dev=$touch; elif [ -n "$mouse" ]; then mode="--mouse"; dev=$mouse; else exit 0; fi
cur=$(pgrep -a -f "^$RX3_BINDIR/rx3-touch-bridge" | head -1)
case "$cur" in *"$dev"*) exit 0;; esac          # already bridging this device
pkill -f "^$RX3_BINDIR/rx3-touch-bridge"; sleep 0.5
nohup sudo -u $RX3_USER $B $mode $dev $R/dev/tsc2007_2-0048 > $RX3_USERHOME/rx3-touch.log 2>&1 < /dev/null &
logger -t rx3 "pointer bridge started: $mode $dev"
