#!/bin/bash
# Install the host-side pieces of the RX3 player: helper binaries, udev rules, systemd unit.
# Run from the directory this file lives in, as a normal user (it will ask for sudo).
#   ./install.sh deps     install the Debian packages this needs
#   ./install.sh doctor   check prerequisites only, change nothing
#   ./install.sh          full install (binaries, udev rules, systemd unit)
set -uo pipefail
. "$(dirname "$(readlink -f "$0")")/rx3-env.sh"
ok(){ printf '  \033[32mok\033[0m   %s\n' "$1"; }
bad(){ printf '  \033[31mMISS\033[0m %s\n' "$1"; FAIL=1; }
warn(){ printf '  \033[33mwarn\033[0m %s\n' "$1"; }
FAIL=0

echo "RX3 layout"
echo "  tools   $RX3_HOME"
echo "  user    $RX3_USER"
echo "  chroot  $RX3_ROOT"
echo "  overlays $RX3_USB"
echo

case "$RX3_USERHOME" in
  ''|/|/root|/nonexistent|/usr/sbin|/bin|/dev/null)
    [ -z "${RX3_ALLOW_SYSTEM_USER:-}" ] && bad "owned by '$RX3_USER', whose home is $RX3_USERHOME - run: sudo chown -R \$(id -un):\$(id -gn) \"$RX3_HOME\"";;
esac

echo "Prerequisites"
# Binary name -> apt package, because several differ (arm-linux-gnueabi-gcc lives in
# gcc-arm-linux-gnueabi, 7z in p7zip-full) and that trips people up.
pkg_for(){ case "$1" in
  fuse-overlayfs) echo fuse-overlayfs;;
  rsync) echo rsync;;
  gcc) echo build-essential;;
  arm-linux-gnueabi-gcc) echo gcc-arm-linux-gnueabi;;
  python3) echo python3;;
  uhubctl) echo uhubctl;;
  7z) echo p7zip-full;;
  *) echo "$1";;
esac; }
# sbin is not on a normal user's PATH, so look there too before declaring something missing.
have(){ command -v "$1" >/dev/null || [ -x /usr/sbin/"$1" ] || [ -x /sbin/"$1" ]; }
MISSING=""
need(){ have "$1" && ok "$1" || { bad "$1 not installed (apt package: $(pkg_for "$1"))"; MISSING="$MISSING $(pkg_for "$1")"; }; }
optional(){ have "$1" && ok "$1" || { warn "$1 missing - $2 (apt package: $(pkg_for "$1"))"; MISSING="$MISSING $(pkg_for "$1")"; }; }

for p in fuse-overlayfs rsync gcc arm-linux-gnueabi-gcc python3; do need $p; done
optional uhubctl "only used to power-cycle a stuck FLX4"
python3 -c "import PIL" 2>/dev/null && ok "python3 PIL" || { warn "python3-pil missing (screenshot helpers)"; MISSING="$MISSING python3-pil"; }
python3 -c "import cryptography" 2>/dev/null && ok "python3 cryptography" || { warn "python3-cryptography missing (needed by recover-firmware.py)"; MISSING="$MISSING python3-cryptography"; }
have 7z || have bsdtar || { warn "7z missing (needed by recover-firmware.py to unpack the ISO)"; MISSING="$MISSING p7zip-full"; }
echo

echo "Recovered firmware"
RF="$RX3_HOME/extracted/runtime-files"
NFILES=$( [ -d "$RF" ] && find "$RF" -type f 2>/dev/null | head -2000 | wc -l || echo 0 )
if [ ! -d "$RF" ]; then
  bad "extracted/runtime-files missing - run: python3 recover-firmware.py && python3 extract_cramfs.py"
elif [ ! -f "$RX3_HOME/runtime-symlinks.json" ]; then
  # extract_cramfs.py writes the symlink map last, so this exact pair means it was interrupted.
  bad "extraction incomplete: runtime-files exists but runtime-symlinks.json does not"
  echo "       extract_cramfs.py writes that file last, so it was interrupted or it failed." >&2
  echo "       Re-run it and let it finish:  python3 extract_cramfs.py" >&2
elif [ "$NFILES" -lt 100 ]; then
  bad "extracted/runtime-files has only $NFILES files - re-run: python3 extract_cramfs.py"
else
  ok "extracted/runtime-files ($NFILES+ files)"
  ok "runtime-symlinks.json"
fi
[ -f "$RX3_HOME/extracted/player/pdj/rbp" ] && ok "recovered player binary" || bad "extracted/player/pdj/rbp missing - run: python3 recover-firmware.py"
[ -d "$RX3_ROOT/root/pdj" ] && ok "chroot built" || warn "chroot not built yet - run ./build-rootfs.sh"
echo

if [ -n "$MISSING" ]; then
  echo "Install what is missing with:"
  echo "  sudo apt install -y $(echo $MISSING | tr ' ' '\n' | sort -u | tr '\n' ' ' | sed 's/ *$//')"
  echo "  (or just run: ./install.sh deps)"
  echo
fi

if [ "${1:-}" = deps ]; then
  echo "== installing packages"
  sudo apt update
  sudo apt install -y fuse-overlayfs uhubctl exfatprogs alsa-utils python3-pil python3-cryptography \
                      gcc build-essential gcc-arm-linux-gnueabi rsync p7zip-full || exit 1
  echo "Done. Now run: ./install.sh doctor"
  exit 0
fi
if [ "${1:-}" = doctor ]; then
  [ $FAIL -eq 0 ] && echo "All prerequisites present." || echo "Fix the MISS lines above, then re-run."
  exit $FAIL
fi
[ $FAIL -ne 0 ] && { echo "Prerequisites missing - fix the MISS lines above, then re-run."; exit 1; }

echo "== helper binaries"
gcc -O2 -DRX3_ROOT_PATH="\"$RX3_ROOT\"" -o "$RX3_BINDIR/rx3-fb-present" "$RX3_HOME/fb-present.c" || exit 1
gcc -O2 -DRX3_ROOT_PATH="\"$RX3_ROOT\"" -o "$RX3_BINDIR/rx3-touch-bridge" "$RX3_HOME/touch-bridge.c" || exit 1
ok "built rx3-fb-present and rx3-touch-bridge in $RX3_BINDIR"

echo "== udev rules and systemd unit"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
for f in 97-rx3-input.rules 98-rx3-flx4.rules 99-rx3-usb.rules; do
  sed "s|@RX3_HOME@|$RX3_HOME|g" "$RX3_HOME/$f.in" > "$tmp/$f"
done
sed "s|@RX3_HOME@|$RX3_HOME|g" "$RX3_HOME/rx3.service.in" > "$tmp/rx3.service"
sudo install -m 644 "$tmp"/*.rules /etc/udev/rules.d/ || exit 1
sudo install -m 644 "$tmp/rx3.service" /etc/systemd/system/ || exit 1
sudo udevadm control --reload
sudo systemctl daemon-reload
ok "installed udev rules and rx3.service"

echo "== audio: keep PipeWire off the sound cards"
systemctl --user mask --now pipewire pipewire-pulse wireplumber pipewire.socket pipewire-pulse.socket 2>/dev/null
ok "PipeWire masked for $RX3_USER"

echo "== console: give the player the framebuffer"
# The player draws straight to /dev/fb0, so a running desktop would fight it for the display.
if [ "$(systemctl get-default)" != multi-user.target ]; then
  echo "  This Pi currently boots to a desktop. The player needs the framebuffer to itself, so"
  echo "  the desktop will be disabled and the Pi will boot to a console from now on."
  echo "  To put it back later:  sudo systemctl set-default graphical.target && sudo systemctl enable lightdm"
fi
sudo systemctl set-default multi-user.target >/dev/null
sudo systemctl disable lightdm >/dev/null 2>&1
ok "booting to multi-user (no desktop)"

echo
echo "Done. Start it with:  sudo systemctl enable --now rx3"
echo "Logs:  $RX3_LOGDIR/rx3-player.log   journalctl -u rx3 -f"
