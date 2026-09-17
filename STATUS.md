# Status — 2026-09-17

## Working and verified on hardware

**System**
- Real XDJ-RX3 1.19 firmware boots in the chroot and renders its own UI on HDMI, letterboxed to the display.
- Starts at boot through the `rx3` systemd service, bringing up display, bridges and USB media automatically.
- USB mouse navigation with an on-screen cursor: left click = touch, wheel = browse selector, right = BACK, middle = ENTER.
  A touch panel is picked up automatically in preference to the mouse.
- On-screen buttons, 2 rows of 10, in pages: the main page has SOURCE, BROWSE, SHORTCUT, SEARCH, UTILITY, BACK,
  UP/DOWN, ENTER, X-FADER (crossfader on/off, starts off, lit from the engine's assignment; verified 2026-09-17) and per
  deck LOAD, USB STOP, PLAY/PAUSE and a DECK 1 / DECK 2 button; DECK n opens that deck's page
  with MASTER TEMPO, QUANTIZE and a BACK to the main page. Verified on the player 2026-09-16/17.
- PLAY/PAUSE, MASTER TEMPO and QUANTIZE buttons light up from the firmware's own state (the control shim publishes
  `isPlaying` / `isMasterTempo` / `UiGetPlayQuantizeOn` into the shared ui_state every 200 ms). Verified 2026-09-17.
- Display profiles (`displays.py`: td2, hdmi1080, hdmi, custom) picked from the connected framebuffer; the interface is
  laid out on the panel itself (no letterbox), with `RX3_UI`, `RX3_UI_SCALE` and `RX3_TOUCH` in `rx3.conf` to adjust
  chrome and touch axes. Verified 2026-09-16 on a 1920x1080 HDMI touch panel: picture and touch right without any setting.

**USB media**
- Two sticks presented as USB1 and USB2, through a copy-on-write overlay so the user's files are never modified.
- Hot-plug, per-slot eject via a real hold on USB STOP, physical removal and re-insert — all without a restart.
- Library browsing, track load, playback on both decks, with analysed waveforms and BPM from the rekordbox export.

**Audio**
- Output through the FLX4: master on channels 1/2, headphone cue on 3/4.
- Deck 1 routed to CH1, deck 2 to CH2. The crossfader starts inert (THRU); the on-screen X-FADER button assigns it A/B.

**DDJ-FLX4 control**
- Transport: play/cue, hot cues, jog (scratch and touch release), tempo slider, sync, loops, beat jump, cue/loop call.
- Mixer: channel faders, EQ, trim, master and headphone level, headphone cue buttons.
- Browse encoder and track load.
- Beat FX: on/off, type select, channel select, beat length, depth.
- Colour FX knobs with the DJ filter selected, plus SMART CFX cycling.
- Vendor keep-alive so the controller never drops MIDI or mutes itself.
- Pad mode buttons light to show the selected mode (fixed 2026-09-17: HOT CUE and BEAT JUMP were being handled as plain
  keys before the LED code ran; measured on the FLX4 that every mode LED answers to its own note). Verified on the FLX4.
- PLAY lights while the deck plays and CUE while it is stopped, the channel CUE buttons follow the real headphone cue,
  and in HOT CUE mode the pads show which hot cues the loaded track has - all from the firmware's own state, which the
  control shim publishes into ui_state and the bridge reads. Verified on the FLX4 2026-09-17.
- LOOP IN / LOOP OUT / RELOOP light as on a CDJ (in point set or looping / looping / a loop to return to), from
  `isLooping` and `isPossibleToReLoop`; the "in point set, no loop yet" stretch is the bridge's own note of the
  LOOP IN press. Verified on the FLX4 2026-09-17.
- SYNC and MASTER light from `isSyncOn` / `getSyncMaster`, and the channel level meters follow the engine's own dB
  reading (`getInputChLevelMono`, -24..+10 dB mapped to the meter; `RX3_METER_MIN` / `RX3_METER_MAX` adjust). Verified
  on the FLX4 2026-09-17. `led-probe.py` can now run alongside the bridge to test LEDs without stopping the player.

**Host**
- Wi-Fi and Ethernet, SSH key login, controller re-enumeration after power glitches.

## Work in progress

- **Unlit FLX4 buttons**: SLIP and BEAT FX ON/OFF do not reflect state yet; their getters still need finding. Same path
  as the rest: shim bit, bridge LED.
- **Unmapped pad modes**: pad FX, sampler, keyboard and key shift are not mapped, as the RX3 has no direct equivalent for most of them.
- **Device names**: SOURCE shows USB1/USB2 rather than each stick's volume label.
- **No auto-restart**: if the player process crashes the service does not restart it; `systemctl restart rx3` is needed.
- **Power**: the Pi still reports under-voltage with the FLX4 attached on a stock supply. An official 27 W supply or a powered hub is recommended.
- **Untested by a person**: touch panel input, recording, MIC input, AUX, link/export features, and long-session stability.
- **Deployment**: `DEPLOY.md` is a manual recipe rather than a one-shot installer.
