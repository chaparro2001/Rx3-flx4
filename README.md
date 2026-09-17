# Rx3-flx4

Emulation, and cross-compatibility testing **Pioneer XDJ-RX3 firmware (v1.19) player on a Raspberry Pi 5**, with a **Pioneer DDJ-FLX4** or **DDJ-400**
as the controller and sound card (the FLX4 is verified on hardware; the 400 is mapped from Pioneer's layout and
Mixxx's mapping and awaits a real unit). The connected controller is detected automatically.

This is a working research repository, not a finished firmware port or installer.

Status: working daily-driver setup, verified on a Raspberry Pi 5 with a 1920x1080 HDMI touch panel and the
Raspberry Pi Touch Display 2. See [`STATUS.md`](STATUS.md) for the feature-by-feature list. Everything lives
on `main`; the display is picked from a profile list at start, so there are no per-screen branches any more.

## What it does today

**The player.** The real XDJ-RX3 firmware runs in a chroot, draws its own interface, browses USB sticks
(through a copy-on-write overlay, so your media is never written to), loads and plays on both decks with
analysed waveforms and BPM, and starts at boot as a systemd service.

**The screen.** Any HDMI display or the Touch Display 2, touch or not. The RX3 picture is laid out on the
panel as large as it fits at 16:10, with an on-screen control strip around it - no black bars. Displays are
matched against a short profile list (`displays.py list`); rotation, chrome size and touch-axis fixes are one
line each in `rx3.conf`. A USB mouse works as a pointer when there is no touch panel.

**The on-screen strip.** Two rows of ten buttons, in pages: SOURCE, BROWSE, SHORTCUT, SEARCH, UTILITY,
BACK, UP/DOWN, ENTER, X-FADER (crossfader on/off), and per deck LOAD, USB STOP, PLAY/PAUSE and a DECK
button that opens that deck's page (MASTER TEMPO, QUANTIZE, BACK). Buttons that are toggles light up from
the firmware's own state: PLAY, MASTER TEMPO, QUANTIZE, HP CUE, X-FADER. Two slider bars carry the channel
faders, master and headphone levels and crossfader for use without a controller.

**The controller (DDJ-FLX4 / DDJ-400).** Transport, cues, loops, beat jump, pads, sync, tempo, jog with
scratch, the mixer (faders, EQ, trim, colour FX, master and headphone levels, headphone cue), Beat FX, browse
and load. And the controller's lights show the firmware's state, not a guess: pad mode buttons, PLAY/CUE,
channel CUE, SYNC/MASTER, LOOP IN/OUT/RELOOP, the hot cue pads (which cues the loaded track has) and the
channel level meters in dB.

**How state gets out of the firmware.** The player binary is unstripped, so `control-shim.c` (preloaded into
it) calls the engine's own getters - `isPlaying`, `isMasterTempo`, `isLooping`, `isRegisteredHotCue`,
`getInputChLevelMono`... - twenty times a second and publishes the answers into a small shared file. The
presenter reads it to light the on-screen buttons and the controller bridge reads it to drive the LEDs. Adding
another indicator is one getter address and one bit.

**Tools.** `rx3-control.py` sends any firmware key from the shell (`state` prints what the shim publishes);
`led-probe.py` finds a controller's LED map on the device itself (it runs alongside the bridge); keyboard
hotkeys stop/restart the player and dump diagnostics; `rx3-logs.sh` follows everything in one stream.

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
| `rx3-handoff/` | Everything that is copied to the Pi, into `~/rx3-handoff` |
| `rx3-handoff/install.sh` | Host-side installer, and `install.sh doctor` to check prerequisites |
| `rx3-handoff/rx3-env.sh`, `rx3_env.py` | Path resolution — why no username is hardcoded |
| `rx3-handoff/*.in` | Templates for the udev rules and systemd unit, filled in by `install.sh` |
| `rx3-handoff/rx3.conf` | Optional per-machine settings (display profile, rotation, touch axes, meter range); not in git |
| `rx3-handoff/legacy/` | Dead prototype code from the first machine. Ignore it |
| `INSTALL.md` | Step-by-step bring-up, and troubleshooting |
| `rx3-handoff/PI-SETUP-NOTES.md` | The detailed reference: key codes, audio routing, USB semantics, traps |

### The interesting pieces

- **`fbshim.c` / `control-shim.c`** — preloaded into the firmware. They fake the framebuffer and
  device ioctls, redirect ALSA onto the FLX4, inject control events, call the firmware's own mixer
  routing functions, and hand privileged unmounts to a root helper.
- **`controller-bridge.py` / `controllers.py`** — translate the controller's MIDI into the firmware's internal
  key events and drive its LEDs from the published state; the table of known controllers (USB id,
  keep-alive, the few codes that differ) lives in `controllers.py`, and detection picks the first one plugged in.
- **`usb-attach.sh` / `usb-hotplug.sh` / `rx3-mtab.sh` / `rx3-priv.sh`** — present USB sticks to the
  firmware through a copy-on-write overlay so the user's media is never modified, and satisfy the
  firmware's mount-table and unmount expectations.
- **`fb-present.c` / `touch-bridge.c` / `pi-controls.h`** — display presenter and touch/mouse adapter. The
  header holds the one layout both use (button pages, slider bars, the picture rectangle, the shared state
  struct), so what is drawn and what is touched cannot disagree.
- **`displays.py`** — the display profile list and detection, same shape as `controllers.py`.
- **`led-probe.py`** — light any LED on the controller by hand and see what each button sends, for mapping a
  controller from the hardware rather than from other projects' files.

## Quick start

Full steps are in **[`INSTALL.md`](INSTALL.md)**. In short, on the Pi:

```bash
git clone https://github.com/mutlisensor/Rx3-flx4.git   # not with sudo: you must own these files
cd Rx3-flx4/rx3-handoff && chmod +x *.sh
./install.sh deps                                       # Debian packages
python3 recover-firmware.py && python3 extract_cramfs.py   # you supply the firmware
./install.sh doctor                                     # checks prerequisites, changes nothing
./build-rootfs.sh                                       # assembles the chroot
./install.sh                                            # udev rules, systemd unit, helper binaries
sudo systemctl enable --now rx3
```

To stop the player and hand the Pi back to its desktop, run `./install.sh desktop`. That also
unmasks PipeWire, without which the desktop comes back silent.

`./install.sh doctor` is the thing to run whenever something is unclear: it reports every
prerequisite as ok or missing, names the apt package or the script that fixes each one, and
changes nothing.

No username is baked in. `rx3-env.sh` and `rx3_env.py` resolve every path from where the scripts
live and who owns them, and each one can be overridden with an `RX3_*` environment variable.

After a `git pull`, what to rebuild depends on what changed: Python scripts only need `sudo systemctl restart
rx3`; the presenter, touch bridge or `pi-controls.h` need `./install.sh` first; `control-shim.c` needs the shim
rebuilt into the chroot (the one `gcc` line under "== shim" in `build-rootfs.sh` - no need to rebuild the
chroot itself).

## Legal

The scaffolding here is original work. It is published for interoperability and personal research on
hardware you own. Pioneer/AlphaTheta firmware is not distributed with it and you need a legitimate
copy to use any of this.
