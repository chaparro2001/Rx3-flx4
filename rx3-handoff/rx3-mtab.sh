#!/bin/bash
# Regenerate the chroot's fake /proc/mounts (= /etc/mtab) so the firmware's mount-table check sees the USB overlays.
# The firmware re-reads it after every "mount" message and unmounts any slot that is not listed.
R=/home/rx3/rx3-rootfs; T=$R/proc/mounts.new
{
  printf 'rootfs / rootfs rw 0 0\nproc /proc proc rw,relatime 0 0\nsysfs /sys sysfs rw,relatime 0 0\ntmpfs /tmp tmpfs rw,relatime 0 0\n'
  printf 'ubi6:settings /root/settings ubifs rw,relatime 0 0\nubi10:gui /root/gui ubifs ro,relatime 0 0\n'
  for P in usb1 usb2; do
    for mp in $R/media/$P/*; do
      mountpoint -q "$mp" || continue
      SRC=$(findmnt -n -o SOURCE /home/rx3/rx3-usb/$P/lower 2>/dev/null); FS=$(findmnt -n -o FSTYPE /home/rx3/rx3-usb/$P/lower 2>/dev/null)
      [ "$FS" = exfat ] && FS=fuseblk       # the real unit mounts exFAT through mount.exfat-fuse
      printf '%s /media/%s/%s %s rw,noatime 0 0\n' "${SRC:-/dev/sda1}" "$P" "$(basename "$mp")" "${FS:-vfat}"
    done
  done
} > $T && mv $T $R/proc/mounts && chmod 644 $R/proc/mounts
