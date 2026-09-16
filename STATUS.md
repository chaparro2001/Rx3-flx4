# Status — 2026-09-11

## Working and verified on hardware

**System**
- Real XDJ-RX3 1.19 firmware boots in the chroot and renders its own UI on HDMI, letterboxed to the display.
- Starts at boot through the `rx3` systemd service, bringing up display, bridges and USB media automatically.
- USB mouse navigation with an on-screen cursor: left click = touch, wheel = browse selector, right = BACK, middle = ENTER.
  A touch panel is picked up automatically in preference to the mouse.
- On-screen buttons: SOURCE, BROWSE, SHORTCUT, MENU / UTILITY (hold), BACK, UP/DOWN, ENTER, LOAD 1/2, PLAY/PAUSE 1/2,
  USB STOP 1/2, plus KEYBOARD (see Work in progress). MENU verified on the player 2026-09-16.
- Display profiles (`displays.py`: td2, hdmi1080, hdmi, custom) picked from the connected framebuffer; the interface is
  laid out on the panel itself (no letterbox), with `RX3_UI`, `RX3_UI_SCALE` and `RX3_TOUCH` in `rx3.conf` to adjust
  chrome and touch axes. Verified 2026-09-16 on a 1920x1080 HDMI touch panel: picture and touch right without any setting.

**USB media**
- Two sticks presented as USB1 and USB2, through a copy-on-write overlay so the user's files are never modified.
- Hot-plug, per-slot eject via a real hold on USB STOP, physical removal and re-insert — all without a restart.
- Library browsing, track load, playback on both decks, with analysed waveforms and BPM from the rekordbox export.

**Audio**
- Output through the FLX4: master on channels 1/2, headphone cue on 3/4.
- Deck 1 routed to CH1, deck 2 to CH2, crossfader assigned A/B.

**DDJ-FLX4 control**
- Transport: play/cue, hot cues, jog (scratch and touch release), tempo slider, sync, loops, beat jump, cue/loop call.
- Mixer: channel faders, EQ, trim, master and headphone level, headphone cue buttons.
- Browse encoder and track load.
- Beat FX: on/off, type select, channel select, beat length, depth.
- Colour FX knobs with the DJ filter selected, plus SMART CFX cycling.
- Vendor keep-alive so the controller never drops MIDI or mutes itself.

**Host**
- Wi-Fi and Ethernet, SSH key login, controller re-enumeration after power glitches.

## Work in progress

- **Pad mode LEDs**: the HOT CUE and BEAT JUMP buttons stay dark on the FLX4, though PAD FX and SAMPLER light. The pads themselves work.
- **Hot cue pad LEDs**: pads do not light to show which cues are set. Needs decoding of the firmware's panel LED stream over the emulated SPI FIFOs.
- **KEYBOARD (0x216) does nothing** from the main screen. It sits among the firmware's touch-GUI keys (Shortcut,
  DeckInfoSelect, TouchPanelOn), so it probably only acts inside the SEARCH screen; to be checked, else the button
  should send SEARCH (0x205) instead.
- **UTILITY by holding MENU / UTILITY** for over a second: the firmware times the hold itself (sending it its
  "long-pressed" code just opened MENU). Verify the hold on the on-screen button opens the UTILITY screen.
- **Unmapped pad modes**: pad FX, sampler, keyboard and key shift are not mapped, as the RX3 has no direct equivalent for most of them.
- **Device names**: SOURCE shows USB1/USB2 rather than each stick's volume label.
- **No auto-restart**: if the player process crashes the service does not restart it; `systemctl restart rx3` is needed.
- **Power**: the Pi still reports under-voltage with the FLX4 attached on a stock supply. An official 27 W supply or a powered hub is recommended.
- **Untested by a person**: touch panel input, recording, MIC input, AUX, link/export features, and long-session stability.
- **Deployment**: `DEPLOY.md` is a manual recipe rather than a one-shot installer.
