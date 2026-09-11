#!/bin/bash
# Launch the XDJ-RX3 player (firmware 1.19) inside its chroot on this Raspberry Pi 5. Runs as root.
# Layout: chroot /home/rx3/rx3-rootfs, tools/sources /home/rx3/rx3-handoff, logs /home/rx3/rx3-*.log
set -u
R=/home/rx3/rx3-rootfs; H=/home/rx3/rx3-handoff; U=rx3
LOG=/home/rx3/rx3-player.log
if pgrep -x rbp-pi >/dev/null; then echo "RX3 player already running"; exit 0; fi

# --- host preparation ---------------------------------------------------------
$H/mount-rx3.sh >/dev/null
# Root helper that performs the firmware's own USB STOP unmounts (see rx3-priv.sh); its FIFO must exist before launch.
systemctl is-active -q rx3-priv.service || systemd-run --quiet --unit=rx3-priv --collect -p Restart=on-failure $H/rx3-priv.sh
for i in $(seq 1 20); do [ -p $R/dev/rx3-priv ] && break; sleep 0.1; done
# Keep the kernel default RT throttle (95%) so runaway SCHED_RR firmware threads cannot starve Wi-Fi/USB work.
sysctl -q -w kernel.sched_rt_runtime_us=950000
touch $R/dev/printkdrv0; mountpoint -q $R/dev/printkdrv0 || mount --bind /dev/null $R/dev/printkdrv0

# --- audio card: DDJ-FLX4 when present, otherwise the ALSA loopback for headless testing --------
CARD=""
for wait in $(seq 1 30); do                       # the controller can enumerate a few seconds after boot
  for c in /proc/asound/card*/id; do grep -qE "FLX4|FLX6" $c 2>/dev/null && CARD=$(cat $c) && break; done
  [ -n "$CARD" ] && break
  # A bus-powered FLX4 often fails enumeration at power-on ("device not accepting address"); one port power-cycle usually fixes it.
  [ $wait = 10 ] && command -v uhubctl >/dev/null && uhubctl -l 1 -a cycle -d 2 >/dev/null 2>&1
  sleep 1
done
if [ -z "$CARD" ]; then
  lsmod | grep -q snd_aloop || modprobe snd_aloop pcm_substreams=1
  CARD=Loopback
fi
echo "hw:CARD=$CARD" > $R/etc/rx3-ctl
sed "s/hw:2,0/hw:CARD=$CARD,DEV=0/" $H/asound.conf > $R/etc/asound.conf
echo "audio card: $CARD"

# --- reset interim UI state, then start the firmware -------------------------------------------
python3 - <<'PY'
import os,struct
with os.fdopen(os.open('/home/rx3/rx3-rootfs/dev/rx3-ui-state',os.O_RDWR|os.O_CREAT,0o600),'r+b') as f:
    f.write(struct.pack('<I6fII',0x52583332,1,.6,0,1,.5,.5,0,1))
PY
chown $U:$U $R/dev/rx3-ui-state
ulimit -r 99; ulimit -l unlimited; ulimit -c 0
cd /home/rx3
nohup chroot --userspec=1000:44 --groups=29,44,996 $R /bin/busybox sh -c \
  "cd /root/pdj && exec env LD_PRELOAD=/lib/fbshim.so /root/pdj/rbp-pi -a" > $LOG 2>&1 < /dev/null &
echo "player started (pid $!)"

# --- host-side helpers: DDJ-FLX4 MIDI bridge, display presenter, touch bridge -------------------
sleep 4
if grep -q FLX4 /proc/asound/card*/id 2>/dev/null; then
  pgrep -f "^python3 /home/rx3/rx3-handoff/flx4-bridge" >/dev/null || nohup sudo -u $U env RX3_BRIDGE_LOG=1 python3 $H/flx4-bridge.py > /home/rx3/rx3-flx4.log 2>&1 < /dev/null &
fi
if [ -e /dev/fb0 ] && [ -x /home/rx3/rx3-fb-present ]; then
  pgrep -x rx3-fb-present >/dev/null || nohup sudo -u $U /home/rx3/rx3-fb-present $R/dev/fb0 > /home/rx3/rx3-present.log 2>&1 < /dev/null &
fi
$H/input-hotplug.sh     # touchscreen if present, else USB mouse

# --- USB media: attach the first removable partition via copy-on-write overlay and notify -------
( sleep 8; for dev in /dev/sd?1; do [ -b "$dev" ] && $H/usb-hotplug.sh add "$dev"; done ) > /home/rx3/rx3-usb.log 2>&1 < /dev/null &
exit 0
