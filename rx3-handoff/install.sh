#!/bin/bash
# Install the host-side pieces of the RX3 player: helper binaries, udev rules, systemd unit.
# Run from the directory this file lives in, as a normal user (it will ask for sudo).
#   ./install.sh          full install
#   ./install.sh doctor   check prerequisites only, change nothing
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

if [ "$RX3_UID" -lt 1000 ] && [ -z "${RX3_ALLOW_SYSTEM_USER:-}" ]; then
  bad "this directory is owned by '$RX3_USER', a system account - run: sudo chown -R \$(id -un):\$(id -gn) $RX3_HOME"
fi

echo "Prerequisites"
# sbin is not on a normal user's PATH, so look there too before declaring something missing.
have(){ command -v "$1" >/dev/null || [ -x /usr/sbin/"$1" ] || [ -x /sbin/"$1" ]; }
for p in fuse-overlayfs rsync gcc arm-linux-gnueabi-gcc python3; do
  have $p && ok "$p" || bad "$p not installed"
done
have uhubctl && ok "uhubctl" || warn "uhubctl missing (only used to power-cycle a stuck FLX4)"
python3 -c "import PIL" 2>/dev/null && ok "python3 PIL" || warn "python3-pil missing (screenshot helpers only)"
[ -d "$RX3_HOME/extracted/runtime-files" ] && ok "extracted/runtime-files" || bad "extracted/runtime-files missing - run recover-firmware.py then extract_cramfs.py"
[ -f "$RX3_HOME/runtime-symlinks.json" ] && ok "runtime-symlinks.json" || bad "runtime-symlinks.json missing - run extract_cramfs.py"
[ -f "$RX3_HOME/extracted/player/pdj/rbp" ] && ok "recovered player binary" || bad "extracted/player/pdj/rbp missing - run recover-firmware.py"
[ -d "$RX3_ROOT/root/pdj" ] && ok "chroot built" || warn "chroot not built yet - run ./build-rootfs.sh"
echo

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
sudo systemctl set-default multi-user.target >/dev/null
sudo systemctl disable lightdm >/dev/null 2>&1
ok "booting to multi-user (no desktop)"

echo
echo "Done. Start it with:  sudo systemctl enable --now rx3"
echo "Logs:  $RX3_LOGDIR/rx3-player.log   journalctl -u rx3 -f"
