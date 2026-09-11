# XDJ-RX3 firmware on Raspberry Pi 5 — working snapshot (2026-09-11)

Bring-up guide for the deployment that runs Pioneer XDJ-RX3 firmware 1.19 on a Raspberry Pi 5 with a DDJ-FLX4 as
controller and sound card. Taken from the Pi (`rx3@rx3.local`) right after the USB eject / re-insert flow
was verified working.

What is here:

- `rx3-handoff/` — exact copy of `/home/rx3/rx3-handoff` on the Pi: recovered firmware (`extracted/`,
  patched `rbp-pi`), the chroot builder, start/stop scripts, shims, FLX4 MIDI bridge, USB hot-plug helpers,
  udev rules, notes. **`PI-SETUP-NOTES.md` inside it is the detailed reference.**
- `host/etc/systemd/system/rx3.service`, `host/etc/udev/rules.d/*.rules` — the host-side config as installed.
Not included: the Pioneer firmware (see the README — you supply it), the 6.4 GB chroot
(`/home/rx3/rx3-rootfs`), which `build-rootfs.sh` rebuilds from `extracted/` in about a minute, compiled
binaries, and the Pi's Wi-Fi/password settings.

## Deploying to a fresh Pi 5

1. Flash Raspberry Pi OS (64-bit, trixie) with user `rx3`, enable SSH, log in.
2. Packages:
   ```bash
   sudo apt install -y fuse-overlayfs uhubctl exfatprogs alsa-utils python3-pil gcc build-essential gcc-arm-linux-gnueabi strace
   ```
3. Copy `rx3-handoff/` to `/home/rx3/rx3-handoff`, then `chmod +x /home/rx3/rx3-handoff/*.sh`.
   Recover the firmware into `rx3-handoff/extracted/` (see the README).
   Build the two host helpers:
   ```bash
   cd /home/rx3/rx3-handoff
   gcc -O2 -o /home/rx3/rx3-fb-present fb-present.c
   gcc -O2 -o /home/rx3/rx3-touch-bridge touch-bridge.c
   ```
4. Build the chroot (as user rx3, no sudo): `/home/rx3/rx3-handoff/build-rootfs.sh` — this also compiles
   `fbshim.so` with the ARM cross compiler.
5. Host config (as root):
   ```bash
   sudo cp host/etc/systemd/system/rx3.service /etc/systemd/system/
   sudo cp host/etc/udev/rules.d/9?-rx3-*.rules /etc/udev/rules.d/ && sudo udevadm control --reload
   systemctl --user mask pipewire pipewire-pulse wireplumber pipewire.socket pipewire-pulse.socket   # as rx3
   sudo systemctl set-default multi-user.target && sudo systemctl disable lightdm
   sudo systemctl enable --now rx3
   ```
6. Plug in the FLX4 (use the official 27 W supply or a powered hub), HDMI display, USB sticks with a
   rekordbox export. The UI appears on HDMI; a USB mouse works until a touch panel is attached.

Everything non-obvious (real-time limits, audio routing, key codes, USB STOP semantics, the fake mount
table, the root unmount helper) is documented in `rx3-handoff/PI-SETUP-NOTES.md`.
