#!/bin/bash
# udev helper: attach/detach a USB partition as the RX3's USB1 or USB2 while the player runs. Serialized with a lock.
ACTION=$1; DEV=$2; H=/home/rx3/rx3-handoff; R=/home/rx3/rx3-rootfs; U=/home/rx3/rx3-usb; PART=$(basename "$DEV")
pgrep -x rbp-pi >/dev/null || exit 0
exec 9>/run/rx3-usb.lock; flock -w 30 9 || exit 1     # children must not inherit fd 9 (fuse-overlayfs daemonizes)
lower_of(){ findmnt -n -o SOURCE $U/$1/lower 2>/dev/null; }
case "$ACTION" in
  add)
    for P in usb1 usb2; do [ "$(lower_of $P)" = "$DEV" ] && exit 0; done      # already attached
    PORT=""; for P in usb1 usb2; do [ -z "$(lower_of $P)" ] && { PORT=$P; break; }; done
    [ -z "$PORT" ] && { logger -t rx3 "no free RX3 USB slot for $DEV"; exit 0; }
    sleep 2
    $H/usb-attach.sh "$DEV" $PORT 9>&- && sudo -u rx3 python3 $H/rx3-control.py mount $PORT /media/$PORT/$PART 9>&-
    logger -t rx3 "$PORT attached $DEV" ;;
  remove)
    for PORT in usb1 usb2; do
      if [ "$(lower_of $PORT)" = "$DEV" ]; then
        sudo -u rx3 python3 $H/rx3-control.py umount $PORT /media/$PORT/$PART 9>&-
        for mp in $R/media/$PORT/*; do mountpoint -q "$mp" && umount -l "$mp"; done
        umount -l $U/$PORT/lower 2>/dev/null; rm -f $R/dev/$PART; $H/rx3-mtab.sh
        logger -t rx3 "$PORT detached $DEV"
      fi
    done ;;
esac
