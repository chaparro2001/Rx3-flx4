#!/bin/sh
# One-shot install on postmarketOS (Alpine), e.g. a Lenovo IdeaPad Duet. Run as your normal user, from anywhere:
#   sh install-pmos.sh
# It does what INSTALL.md does by hand on a Pi, with the Alpine differences filled in:
#   packages (apk), sudo for the wheel group (the scripts use sudo; pmOS may ship only doas), the ARM32 shim compiler
#   (Alpine has no glibc arm-linux-gnueabi-gcc, so a clang wrapper with Alpine's armv7 headers stands in),
#   the firmware recovery, the chroot and ./install.sh. Re-running it skips what is already done.
# The desktop stays: an "XDJ-RX3" icon swaps it for the player, and holding ESC for 1 s brings it back.
# Needs the systemd edition of postmarketOS: the player's helpers run as transient systemd units.
set -eu
H=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
say(){ printf '\n\033[1m== %s\033[0m\n' "$1"; }
die(){ printf '\033[31mERROR:\033[0m %s\n' "$1" >&2; exit 1; }

[ "$(id -u)" != 0 ] || die "run this as your normal user, not root (it asks for your password when it needs it)"
[ -f /etc/alpine-release ] || die "this is not postmarketOS/Alpine; on Debian or Raspberry Pi OS follow INSTALL.md"
[ "$(uname -m)" = aarch64 ] || die "expected an aarch64 machine, this is $(uname -m)"
[ -d /run/systemd/system ] || die "postmarketOS is running OpenRC. The player needs the systemd edition:
       reinstall with a systemd image from https://images.postmarketos.org (or: pmbootstrap init -> systemd: always)"
[ "$(stat -c %U "$H")" = "$(id -un)" ] || die "$H is owned by $(stat -c %U "$H"), not you; fix with: doas chown -R $(id -un): $(dirname "$H")"
if command -v sudo >/dev/null 2>&1; then AS_ROOT=sudo; else AS_ROOT=doas; fi

say "kernel"
# The firmware is 32-bit ARM, so the kernel must run AArch32 programs (CONFIG_COMPAT). Checked again for real
# once the chroot exists, because /proc/config.gz is not always there.
if [ -r /proc/config.gz ]; then
  zcat /proc/config.gz | grep -q '^CONFIG_COMPAT=y' || die "this kernel has no CONFIG_COMPAT: it cannot run the 32-bit firmware"
  for c in FUSE_FS OVERLAY_FS SND_USB_AUDIO DRM_FBDEV_EMULATION; do
    zcat /proc/config.gz | grep -qE "^CONFIG_$c=[ym]" && echo "  ok   CONFIG_$c" || echo "  warn CONFIG_$c not set"
  done
else
  echo "  /proc/config.gz not available, the 32-bit check runs after the chroot is built"
fi
ls /dev/fb[0-9]* >/dev/null 2>&1 && echo "  ok   framebuffer $(ls /dev/fb[0-9]* | tr '\n' ' ')" || echo "  warn no /dev/fb* - nothing can be shown"

say "packages"
# Only what is missing goes to apk, so a re-run does not need the package database at all. When it does, the desktop's
# software centre may be holding it ("Unable to lock database"): wait for it instead of failing.
apk_add(){
  for try in $(seq 1 30); do
    out=$($AS_ROOT apk add "$@" 2>&1) && { echo "$out" | tail -1; return 0; }
    echo "$out" | grep -q "lock database" || { echo "$out" >&2; return 1; }
    [ $try = 1 ] && echo "  another program is using apk (software centre / updates?), waiting for it..."
    sleep 10
  done
  echo "$out" >&2; return 1
}
have_pkg(){ apk info -e "$1" >/dev/null 2>&1; }
NEED=""
for p in bash coreutils util-linux findutils kmod \
  python3 py3-pillow py3-cryptography fuse3 fuse-overlayfs exfatprogs alsa-utils rsync unzip libarchive-tools \
  build-base linux-headers freetype-dev pkgconf font-dejavu clang lld; do have_pkg $p || NEED="$NEED $p"; done
$AS_ROOT true || die "need your password for $AS_ROOT"   # ask now: apk_add captures output, which would hide the prompt
if [ -n "$NEED" ]; then apk_add $NEED || die "could not install:$NEED"; else echo "  all installed"; fi
# Names that changed between Alpine releases: the first one that exists wins. pgrep -a needs procps, not busybox.
for alts in procps-ng:procps 7zip:p7zip libgpiod polkit xdg-user-dirs; do
  done_=""; for p in $(echo $alts | tr : ' '); do have_pkg $p && { done_=$p; break; }; done
  [ -z "$done_" ] && for p in $(echo $alts | tr : ' '); do apk_add "$p" >/dev/null 2>&1 && { done_=$p; break; }; done
  echo "  ${done_:-(none of $alts available, skipped)}"
done
$AS_ROOT modprobe fuse 2>/dev/null || true

say "sudo"
# The player scripts call sudo. Newer postmarketOS already has one (sudo-rs, or sudo) and installing the other conflicts,
# so a working sudo is used as it is. Only a doas-only system gets sudo-rs, allowed for wheel (the pmOS user is in it).
if ! command -v sudo >/dev/null 2>&1; then
  id -nG | tr ' ' '\n' | grep -qx wheel || die "$(id -un) is not in the wheel group"
  doas apk add sudo-rs || doas apk add sudo || die "could not install sudo"
  if ! doas test -f /etc/sudoers.d/rx3-wheel; then
    echo '%wheel ALL=(ALL:ALL) ALL' > /tmp/rx3-wheel.$$
    doas install -m 440 -o root -g root /tmp/rx3-wheel.$$ /etc/sudoers.d/rx3-wheel; rm -f /tmp/rx3-wheel.$$
    doas grep -qE '^[@#]includedir /etc/sudoers.d' /etc/sudoers || echo '@includedir /etc/sudoers.d' | doas tee -a /etc/sudoers >/dev/null
    doas visudo -c >/dev/null || { doas rm -f /etc/sudoers.d/rx3-wheel; die "sudoers check failed"; }
  fi
fi
AS_ROOT=sudo
sudo -v || die "sudo does not accept your password"
echo "  ok"

say "font"
# rx3-fb-present and install.sh look under /usr/share/fonts/truetype (Debian's layout); Alpine puts DejaVu one level up.
[ -e /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf ] || {
  sudo mkdir -p /usr/share/fonts/truetype; sudo ln -sfn ../dejavu /usr/share/fonts/truetype/dejavu; }
ls /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf

say "ARM32 shim compiler"
# build-rootfs.sh builds fbshim.so with arm-linux-gnueabi-gcc -nostdlib: no C library is linked, only headers are read
# (constants, struct layouts) and every call is resolved against the firmware's glibc at run time. Alpine's armv7
# headers give the same 32-bit ARM layouts, and clang + lld do the compiling, with the same soft-float ABI.
SYS=/opt/rx3-armv7-sysroot
if [ ! -f $SYS/usr/include/linux/fb.h ]; then
  REPO=$(grep -E '^https?://.*/alpine/[^/]+/main/?$' /etc/apk/repositories | head -1)
  : "${REPO:=https://dl-cdn.alpinelinux.org/alpine/edge/main}"
  KEYS="--allow-untrusted"; [ -d /usr/share/apk/keys/armv7 ] && KEYS="--keys-dir /usr/share/apk/keys/armv7"
  sudo mkdir -p $SYS
  sudo apk add --root $SYS --arch armv7 --initdb --no-scripts --no-cache $KEYS -X "$REPO" musl-dev linux-headers \
    || die "could not fetch the armv7 headers from $REPO"
fi
cat > /tmp/rx3-armcc.$$ <<EOF
#!/bin/sh
# Installed by install-pmos.sh: stands in for Debian's gcc-arm-linux-gnueabi to build the RX3 shim.
exec clang --target=arm-linux-gnueabi -march=armv7-a -mfloat-abi=soft --sysroot=$SYS -fuse-ld=lld "\$@"
EOF
sudo install -m 755 /tmp/rx3-armcc.$$ /usr/local/bin/arm-linux-gnueabi-gcc; rm -f /tmp/rx3-armcc.$$
echo 'int f(int x){return x+1;}' > /tmp/rx3-cc.$$.c
arm-linux-gnueabi-gcc -shared -fPIC -nostdlib -o /tmp/rx3-cc.$$.so /tmp/rx3-cc.$$.c || die "the ARM32 compiler wrapper does not work"
rm -f /tmp/rx3-cc.$$.c /tmp/rx3-cc.$$.so
echo "  ok   /usr/local/bin/arm-linux-gnueabi-gcc"

cd "$H"; chmod +x ./*.sh
say "firmware"
if [ -f extracted/player/pdj/rbp ] && [ -f runtime-symlinks.json ]; then
  echo "  already recovered (extracted/)"
else
  python3 recover-firmware.py
  python3 extract_cramfs.py
fi

say "chroot"
./build-rootfs.sh
# rx3-env.sh is bash (BASH_SOURCE), this script is plain sh: ask bash where the chroot is.
RX3_ROOT=$(bash -c '. ./rx3-env.sh 2>/dev/null; echo "$RX3_ROOT"')
sudo chroot "$RX3_ROOT" /bin/busybox true 2>/dev/null \
  || die "the kernel cannot run the 32-bit firmware (no CONFIG_COMPAT / AArch32 support)"
echo "  ok   the 32-bit firmware runs on this kernel"

say "install"
RX3_KEEP_DESKTOP=1 ./install.sh
# An earlier version of this script made the player start at boot instead of the desktop: undo that.
sudo systemctl set-default graphical.target >/dev/null

say "done"
echo "Tap the XDJ-RX3 icon (app list, and the desktop folder if there is one) to start the player."
echo "The desktop closes while it runs. Hold ESC for 1 second on the keyboard to stop it and get the desktop back."
echo "  Logs:  $H/rx3-logs.sh"
echo "If the picture is upside down:  echo RX3_ROTATE=90 >> $H/rx3.conf"
echo "The Duet has one USB-C port: connect the controller and the USB stick through a hub, ideally a powered one."
