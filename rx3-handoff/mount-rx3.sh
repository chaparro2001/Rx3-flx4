#!/bin/bash
# Prepare host bind mounts for the RX3 chroot. Runs as root. Idempotent.
set -u
R=/home/rx3/rx3-rootfs
for f in null zero urandom random full; do mountpoint -q $R/dev/$f || mount --bind /dev/$f $R/dev/$f; done
mountpoint -q $R/dev/snd || mount --bind /dev/snd $R/dev/snd
mountpoint -q $R/dev/shm || mount -t tmpfs -o size=64m,mode=1777 tmpfs $R/dev/shm
mountpoint -q $R/tmp || mount -t tmpfs -o size=256m,mode=1777,uid=1000,gid=1000 tmpfs $R/tmp
echo "1.19" > $R/tmp/smdj.rev; echo "1.19 [1.19:1.19]" > $R/tmp/smdj2.rev; chown 1000:1000 $R/tmp/*.rev
echo mounts-ok
