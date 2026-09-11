#!/bin/sh
set -eu
cd /home/pompu_5
if pgrep -x rbp-pi >/dev/null; then
 echo 'RX3 player is already running.'
 exit 0
fi
# Reset the touchscreen's displayed controls to match firmware startup defaults.
python3 - <<'PY_STATE'
import os,struct
with os.fdopen(os.open('/home/pompu_5/rx3-rootfs/dev/rx3-ui-state',os.O_RDWR|os.O_CREAT,0o600),'r+b') as f:
    f.write(struct.pack('<I6fII',0x52583332,1,.6,0,1,.5,.5,0,1))
PY_STATE
nohup sudo -n chroot --userspec=1000:44 --groups=29,44,995,991 /home/pompu_5/rx3-rootfs /bin/busybox env LD_PRELOAD=/lib/fbshim.so /root/pdj/rbp-pi -a > /home/pompu_5/rx3-player.log 2>&1 < /dev/null &
if ! pgrep -x rx3-fb-present >/dev/null; then
 nohup /home/pompu_5/rx3-fb-present /home/pompu_5/rx3-rootfs/dev/fb0 > /home/pompu_5/rx3-present.log 2>&1 < /dev/null &
fi
if ! pgrep -f '^(/home/pompu_5/|./)rx3-touch-bridge /dev/input/' >/dev/null; then
 nohup /home/pompu_5/rx3-touch-bridge /dev/input/event5 /home/pompu_5/rx3-rootfs/dev/tsc2007_2-0048 > /home/pompu_5/rx3-touch.log 2>&1 < /dev/null &
fi
# Storage workers initialize after the display; report the existing read-only USB.
sleep 10
if pgrep -x rbp-pi >/dev/null && mountpoint -q /home/pompu_5/rx3-rootfs/media/usb1/sda1; then
 python3 /home/pompu_5/pi-control.py mount
fi
