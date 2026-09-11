# Rx3-flx4

Run the **Pioneer XDJ-RX3 firmware (v1.19) player on a Raspberry Pi 5**, with a **Pioneer DDJ-FLX4**
as the controller and sound card.

This is the real RX3 firmware executing in an ARM32 chroot — not Mixxx, not an emulator, not a
reimplementation. This repository holds the host-side scaffolding that makes it run: the chroot
builder, launch and shutdown scripts, the `LD_PRELOAD` shims that stand in for the RX3's missing
hardware, a MIDI bridge that maps the FLX4 onto the firmware's internal key events, USB media
handling with copy-on-write, and the notes that document everything non-obvious.

Status: working daily-driver setup. See [`STATUS.md`](STATUS.md) for the feature-by-feature list.

## Firmware is not included

No Pioneer code, firmware image, decryption key, or patched player binary is in this repository, and
none should ever be committed to it — they are Pioneer/AlphaTheta property. `.gitignore` blocks them.

You supply them yourself, from the official sources:

- Firmware: the official XDJ-RX3 v1.19 update package from AlphaTheta support downloads.
- GPL source distribution: Pioneer DJ's open-source code distribution page, which is where the
  per-sector AES key comes from.

`rx3-handoff/ASTRA-PROMPT.md` lists the exact URLs and the recovery sequence, and
`rx3-handoff/recover-firmware.py` plus `firmware_image.py` perform the extraction. Place the results
in `rx3-handoff/extracted/` and `build-rootfs.sh` will assemble the chroot from them.

## Layout

| Path | Purpose |
|------|---------|
| `rx3-handoff/` | Everything that is copied to `/home/rx3/rx3-handoff` on the Pi |
| `host/etc/systemd/system/` | The `rx3` service unit |
| `host/etc/udev/rules.d/` | Hot-plug rules for USB media, the FLX4, and input devices |
| `DEPLOY.md` | Bring-up steps for a fresh Pi 5 |
| `rx3-handoff/PI-SETUP-NOTES.md` | The detailed reference: key codes, audio routing, USB semantics, traps |

### The interesting pieces

- **`fbshim.c` / `control-shim.c`** — preloaded into the firmware. They fake the framebuffer and
  device ioctls, redirect ALSA onto the FLX4, inject control events, call the firmware's own mixer
  routing functions, and hand privileged unmounts to a root helper.
- **`flx4-bridge.py`** — translates DDJ-FLX4 MIDI into the firmware's internal key events, including
  the vendor keep-alive the controller needs to stay awake.
- **`usb-attach.sh` / `usb-hotplug.sh` / `rx3-mtab.sh` / `rx3-priv.sh`** — present USB sticks to the
  firmware through a copy-on-write overlay so the user's media is never modified, and satisfy the
  firmware's mount-table and unmount expectations.
- **`touch-bridge.c` / `fb-present.c`** — display presenter and touch/mouse input adapter.

## Quick start

See [`DEPLOY.md`](DEPLOY.md). In short: install the packages, copy `rx3-handoff/` to the Pi, recover
the firmware into `extracted/`, run `build-rootfs.sh`, install the host config, `systemctl enable --now rx3`.

## Legal

The scaffolding here is original work. It is published for interoperability and personal research on
hardware you own. Pioneer/AlphaTheta firmware is not distributed with it and you need a legitimate
copy to use any of this.
