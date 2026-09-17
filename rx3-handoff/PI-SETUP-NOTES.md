# XDJ-RX3 firmware 1.19 on Raspberry Pi 5 (host `rx3`, user `rx3`) — setup notes, 2026-09-10



## Display: Touch Display 2 (DSI) and HDMI

- The Pi 5 firmware auto-detects the 7" Touch Display 2 (`display_auto_detect=1`, nothing added to
  config.txt): connector `DSI-2` on `card0` (drm-rp1-dsi), native 720x1280 portrait, 32 bpp, its own
  `/dev/fbN` (`drm-rp1-dsidrmf`) separate from the vc4 HDMI card. Touch is a Goodix GT911 on i2c-11,
  `ID_INPUT_TOUCHSCREEN=1`, axes 0..719 x 0..1279 in panel orientation, type-B multitouch.
- `rx3-env.sh` sets `RX3_FB` to the DSI framebuffer when one exists, else the first `/dev/fb*`;
  `RX3_ROTATE` (0/90/180/270 clockwise) overrides the default of 90 for portrait panels, 0 for landscape.
  Both go in the optional `rx3.conf`, which every script sources. Framebuffers exist only for displays
  connected at boot.
- (2026-09-16, branch display-profiles) **No more fixed 1920x1200 canvas.** `pi-controls.h` lays everything out in
  "UI space" = the panel the right way up, in panel pixels (`make_ui`): the button strip is `200*scale` px tall, the
  slider bars `160*scale` wide at least, and the firmware picture is the largest 16:10 rectangle in what is left, with
  the bars growing to meet it. `scale` defaults to fitting the old 1920x1200 design (`min(uw/1920, uh/1200)`), so the
  TD2 and the 1440p HDMI look as before minus the black bars; `RX3_UI_SCALE` overrides it (capped so the chrome never
  takes more than half the panel), `RX3_UI=full|strip|none` drops the bars or all the chrome. The only transform left
  to the panel is the rotation (`ui_to_panel` / `panel_to_ui`); rotate 90 still puts the UI's left edge along the
  panel's top edge. Both binaries build the same `struct ui` from the framebuffer size and those variables, so what
  is drawn and what is touched cannot disagree. 1080p: picture 1440x900 at 240,0 (unchanged size), strip 180 px,
  bars 240 px, no letterbox.
- The presenter no longer composes a canvas: one lookup table turns panel pixels into UI pixels (the rotation) and one
  source column/row per picture column/row samples the firmware's 1280x800 directly - a single resample instead of
  x1.25 up then down. Pictures smaller than 1280x800 get a 2x2 box filter so the RX3's text stays readable.
- **Display profiles** (`displays.py`, same shape as `controllers.py`): td2, hdmi1080, hdmi (any other size), custom.
  Detection = fb name (`dsi`) and `virtual_size`; DSI wins over HDMI. `rx3-env.sh` evals `displays.py env`, which
  fills RX3_FB / RX3_ROTATE / RX3_UI / RX3_UI_SCALE / RX3_TOUCH from the profile but keeps anything rx3.conf already
  set (rx3.conf values are plain shell variables, so rx3-env.sh hands them over explicitly - they are not exported).
  `RX3_DISPLAY=<id>` forces a profile; `displays.py list|detect`; `install.sh doctor` prints the profile in use.
- **Touch**: `RX3_TOUCH=swap,invx,invy` for controllers whose axes do not follow the panel (swap first, on the
  controller's axes; the inversions on panel pixels). Single-touch controllers (ABS_X/ABS_Y + BTN_TOUCH, no ABS_MT_*)
  are handled as finger 0 - before, the bridge exited on them and, since udev tags both kinds ID_INPUT_TOUCHSCREEN,
  `input-hotplug.sh` had already dropped the mouse for them. The bridge also clears `pressed` at start, so a bridge
  that died mid-touch no longer leaves a button lit.
- `--replay` now takes the firmware's own 1280x800 coordinates and `rx3-tap.py` passes them straight through: the tap
  lands on the picture wherever the panel shows it. `--mouse` works in UI pixels (cursor starts at the centre).
- Verified 2026-09-16 on the user's 1920x1080 HDMI touch panel (profile hdmi1080, no rx3.conf): picture, strip,
  bars and touch all right with no RX3_TOUCH needed.

## Controllers (2026-09-14)

- `controllers.py` is the single list: id, USB ids, ALSA hint, keep-alive/init SysEx, Beat FX CH SELECT map,
  SHIFT+PLAY (censor) note. Detection reads `/proc/asound/cardN/usbid` (falls back to the card name); cards are
  numbered in plug-in order, so `detect()` sorted by index = "first connected wins". `RX3_ASOUND` points it at a
  fake tree for tests. rx3-start.sh, controller-hotplug.sh and install.sh (udev rule generation) all use it.
- DDJ-400 vs FLX4 (Mixxx XML diff): identical except CH SELECT (400: ch4 notes 0x10 CH1 / 0x11 CH2 / 0x14 MASTER;
  FLX4: (4,0x10) CH1, (5,0x11) CH2, else MASTER), SHIFT+PLAY censor (400 0x47, FLX4 0x0E), the FLX4's keep-alive
  (400: one init SysEx F0 00 40 05 00 00 02 06 00 03 01 F7), FLX4-only SMART CFX/FADER and BROWSE+SHIFT. The 400
  has the FILTER knobs (B6 17/18) too. DDJ-400 USB id 2b73:0017 (usb-ids.gowdy.us). Pad-mode notes are the same
  hardware codes on both even though Mixxx's 400 XML does not list them.
- `controller-bridge.py -` = MIDI from stdin (unbuffered!) with RX3_CONTROLLER=<id>, RX3_MIDI_OUT for outgoing;
  the synthetic test in the commit message exercised play, censor, all CH SELECT codes, CFX knob and tempo centre.

## Quoted heredocs and udev cgroups (two traps, both hit)

- A `$RX3_*` variable inside a `<<'PY'` heredoc is never expanded: rx3-start.sh created no ui-state and
  make-test-usb.sh created a directory literally named `$RX3_USERHOME`. Pass paths to Python through the
  environment or argv; `install.sh doctor` warns about stray `$RX3_*` directories and `clean` removes them.
- Anything started by a udev helper dies with the helper's transient unit (this is what killed the USB
  overlays earlier); the pointer bridge and the keyboard hotkeys now run as their own units
  (`rx3-pointer`, `rx3-hotkeys-eventN`), the latter deliberately outside rx3.service so ESC/F5 work
  while the player is stopped. Tested with a uinput virtual keyboard (see the Dev_tools commit).

## Paths and identity are resolved, not hardcoded

`rx3-env.sh` (shell) and `rx3_env.py` (Python) work out the layout at run time, so no username,
uid or gid is baked into any script:

- `RX3_HOME` is the directory the scripts live in; `RX3_USER` is whoever owns it; `RX3_USERHOME`
  is that account's home, which holds `rx3-rootfs`, `rx3-usb` and the logs.
- `RX3_UID`/`RX3_GID` come from that account, and `RX3_GROUPS` resolves audio/video/input **by name**
  because the numbers differ between machines (here they happen to be 29/44/996).
- Every value can be overridden by exporting it first.

The udev rules and the systemd unit need absolute paths, so they are shipped as `.in` templates and
`install.sh` fills in `@RX3_HOME@` at install time. `install.sh doctor` checks prerequisites without
changing anything. Note that `uhubctl` lives in `/usr/sbin`, off a normal user's PATH.


## Layout
- `/home/rx3/rx3-handoff/` — recovery scripts, sources, deployment scripts (this directory).
  - `extracted/` — hash-verified official firmware: `player/pdj/rbp` (original), `gui/`, `runtime-files/` (cramfs), `update/`.
  - `rbp-pi` — patched player (pi-clock + GPIO/audio-profile patches from `patch-player.py`).
- `/home/rx3/rx3-rootfs/` — isolated ARM32 chroot built by `build-rootfs.sh` (re-runnable; rebuilds from `extracted/`).
  - `/root/pdj/rbp-pi`, `/root/gui`, `/root/settings` (writable), `/lib/fbshim.so` (fbshim.c + control-shim.c).
  - `/dev/fb0` regular 1280x800 RGB32 file; `/dev/tsc2007_2-0048` touch FIFO; `/dev/rx3-control` control FIFO;
    `/dev/gpiodrv` 4096x0x01; `/dev/subucom_spi*` FIFOs; `/dev/printkdrv0` bind of /dev/null; null/zero/urandom/full/snd binds.
  - `/proc` is a fake directory: `cpuinfo`, `mounts`, `udev_usb*` FIFOs, `jog/*`. `/sys` fake backlight + paudiog.
  - `/etc/rx3-ctl` = ALSA ctl name used by the shim (`hw:CARD=...`); `/etc/asound.conf` generated from `asound.conf` template.
- `/home/rx3/rx3-usb/usb1/{lower,upper,work}` — copy-on-write view of the USB stick (fuse-overlayfs; kernel overlayfs
  refuses FAT lowers). The firmware's writes (PIONEER/, TMP0000.TMP, export.pdb rw open) land in `upper`, originals untouched.

## Launch
- `systemctl start rx3` runs `rx3-start.sh` (enabled at boot). Essentials it performs:
  - bind mounts (`mount-rx3.sh`), `kernel.sched_rt_runtime_us=-1`, **`ulimit -r 99`** (without RT priority the firmware's
    `acre_tsk` fails -> `CmnFunc_Error` sleeps forever -> NetworkMonitor timer segfaults on a null UI manager),
  - picks the audio card: DDJ-FLX4/FLX6 if present, else `snd_aloop pcm_substreams=1` (dmix's O_APPEND slave sharing
    needs exactly one substream),
  - starts the player as uid 1000 in the chroot, then the FLX4 MIDI bridge / presenter / touch bridge when hardware exists,
  - starts the `rx3-priv` root helper, attaches present `/dev/sd?1` partitions as USB1/USB2 and sends the mount events.
- PipeWire/WirePlumber are masked for user rx3 (they grabbed the sound cards and would seize the FLX4).
- Hot-plug: `99-rx3-usb.rules` -> `usb-hotplug.sh` attaches/detaches USB partitions while running.

## Tools
- `rx3-control.py <key|0xHEX> [deck] [analog]` / `mount usb1` / `rotary ±N` — drive the firmware (keys in keycodes.txt).
- `rx3-test-play.sh` — headless smoke test (browse -> load deck 1 -> play). `fb2png.py` snapshots the virtual screen.
- `usb-attach.sh <dev|image> usb1`, `make-test-usb.sh` (64 MB FAT image with two WAV tones).
- `flx4-bridge.py` — DDJ-FLX4 raw MIDI -> control FIFO (map per Mixxx/DDJ-400 layout; jog + pad modes need on-hardware tuning).

## Verified headless (before display / controller were attached)
- Player boots, renders the stock RX3 UI to the virtual framebuffer, browses USB1, loads a WAV on deck 1, plays.
- Master on loopback ch1/2 and headphone cue on ch3/4 both carry the 440 Hz test tone (dmix routing from asound.conf).

## Boot/reboot findings (evening of 2026-09-10)
- First unattended reboot came up with the RX3 UI on HDMI but no Wi-Fi: NetworkManager's netplan-backed profiles in
  `/etc/netplan/90-NM-*.yaml` were found truncated to 0 bytes (written during shutdown, not flushed). The Wi-Fi profile was
  recreated from the rpi-imager seed `/boot/firmware/network-config` with nmcli and now lives in
  `/etc/NetworkManager/system-connections/preconfigured.nmconnection`. Ethernet works out of the box.
- The DDJ-FLX4 failed USB enumeration at that boot (`device not accepting address, error -71`) and the Pi logged
  under-voltage (`vcgencmd get_throttled` = 0x50000, 5V rail 4.80-4.85 V). **Use the official 27 W Pi 5 supply or a
  powered USB hub for the controller.** With the FLX4 absent the start script falls back to the loopback card;
  `98-rx3-flx4.rules` restarts the service onto the FLX4 when it appears.
- `rx3.service` now waits for network-online, keeps the default RT throttle (950000), and `rx3-stop.sh` unmounts the
  overlays/binds so shutdown does not hang. Kill patterns are anchored (`pkill -f "^python3 .../flx4-bridge"`) because an
  unanchored pattern killed the SSH session that contained the same text.

## Still to do on real hardware
- DDJ-FLX4 controls: card id `DDJFLX4`, 4 outputs at 44.1 kHz (1/2 master, 3/4 phones) confirmed streaming. The MIDI bridge
  is unverified by a human: check play/cue/load/browse first, then jog scaling (`RX3_JOG_SCALE`), pad modes, Smart CFX.
  `journalctl`/`/home/rx3/rx3-flx4.log` shows every MIDI event and the key it sent (RX3_BRIDGE_LOG=1).
- Display: HDMI 2560x1440 16 bpp works via the generic presenter (letterboxed 1920x1200 canvas). No touchscreen is attached,
  so the on-screen interim buttons are display-only; a USB/DSI touch panel will be picked up by the touch bridge automatically.
- **DDJ-FLX4 keep-alive**: the controller stops sending MIDI (and its outputs go silent) unless the host sends the vendor
  SysEx `F0 00 40 05 00 00 04 05 00 50 02 F7` every ~200 ms (rekordbox/Mixxx do this). `flx4-bridge.py` now sends it from a
  thread; it also exits when the MIDI device disappears so the hot-plug rule restarts everything on re-plug.
- (2026-09-14) FLX4 attached at Pi power-on comes up LIT BUT SILENT on USB: no enumeration attempt at all in the
  kernel log, and no uhubctl cycle (2/8/20 s) revives it because the Pi 5 has no per-port VBUS switching
  ("off" only disables the port). Worse, a boot-time uhubctl cycle on the USB3 root hub made a USB3 stick
  vanish for good. The RP1 has one `USB_VBUS_EN` line (gpiochip0 line 42, no kernel consumer) for all ports:
  `timeout 5 gpioset -c gpiochip0 USB_VBUS_EN=0; gpioset ... =1` is a real power cut and brought the FLX4 and
  the stick back. `rx3-start.sh` does that at 10 s (and 22 s) when the FLX4 is absent and nothing is mounted.
  Verified across a reboot: card DDJFLX4 chosen at 30 s, both sticks attached, bridge up.
- Older note: FLX4 present at power-on frequently fails enumeration (error -71); `rx3-start.sh` power-cycled USB hub 1 (uhubctl) after
  10 s without the card. Otherwise re-plug it; `98-rx3-flx4.rules` restarts the service onto it.
- **Crossfader**: the firmware's crossfader assign per channel defaults to THRU (inert) because the CROSS FADER CURVE panel
  switch state never arrives. `control-shim.c` now calls `djengine::DjEngineIF::setCrossFaderAssign` (0x4cc0c, uses the
  global engine pointer at 0x011493c0) with CH1=A (0,1) and CH2=B (1,2), mirroring the firmware's `allinone_debug::mixeron`.
  Enum values: EnMixerInput 0/1 = CH1/CH2; EnCrossFaderAssign 1 = A, 2 = B, other = THRU.
- Firmware debug UDP socket on 127.0.0.1:20001 (thread DebugSub, commands in `allinone_debug::*`) — packet format not yet
  worked out; plain "mixeron" had no effect.
- Bridge v2 maps Beat FX (ch4/5 notes 0x47 on/off, 0x4A/0x4B beat -/+, 0x63/0x64 select, 0x10/0x11 CH SELECT; CC ch4 0x02/0x22
  level/depth), Sound Color FX (ch6 CC 0x17/0x18 -> COLOR knob ch1/ch2; FILTER selected at start, ch6 note 0x63 cycles
  effects), CUE/LOOP CALL (0x51/0x53 -> memory cue prev/next), pad-mode and headphone-cue LEDs (local state only).
- Crossfader verified with the meter after the assign fix (deck1 silent at full B, deck2 silent at full A).
- `rx3-tap.py x y` injects a tap at native 1280x800 coordinates through the bridge's replay mode (landscape layout), e.g.
  TRACK category (50,230), on-screen LOAD 1 (1079,124) / LOAD 2 (1208,124) in the track list.
- Library views (ARTIST/ALBUMS/TRACK/KEY/PLAYLIST/HISTORY) work with the user's stick after re-attach.
- **Player routing**: without the INPUT SELECT panel switches both mixer inputs were fed by player 1 (deck 2 inaudible,
  CH2 fader/EQ acting on deck 1). `control-shim.c` calls `djengine::DjEngineIF::setRoute(player, input)` (0x50598) with
  (0,0) and (1,1) five seconds after the crossfader assign. Verified by toggling each deck's play while metering each channel.
- **Tempo slider key 0x4109 requires operation 5 and a signed analog**: -1 = full minus, 0 = centre, +1 = full plus (confirmed on screen: -0.5 shows -5.00 % with the 10 % range). The FLX4 fader sends 0 at the top, so the bridge sends (fader-0.5)*2. Note DjEngineIF::getTempoSlider reads back the magnitude for values between -1 and 0.
- (old note) (`PlayerInnards::onKey_TempoSlider` rejects anything else); analog
  0.0 = -range (fader top), 1.0 = +range. Faders/EQ/trim accept op 4. `rx3-control.py tempo <deck> <0..1>` uses op 5.
- **Switch-type keys use operation 5 with the value**: BeatEffectSW 0x448b (0 DELAY, 1 ECHO, 2 PING PONG, 3 SPIRAL, 4 HELIX,
  5 REVERB, 6 FLANGER, 7 PHASER, 8 FILTER, 9 TRANS, 10 ROLL, 11 SLIP ROLL, 12 PITCH, 13 VINYL BRAKE) and BfxChSW 0x448c
  (0 CH1, 1 CH2, 2 MIC, 3 CF.A, 4 CF.B, then MASTER). The bridge's FX SELECT cycles the list; CH SELECT maps 1/2/MASTER
  (`RX3_FXCH_MASTER` env overrides the MASTER value).
- **Jog**: after JogWheel (0x4305, op 4, value = ticks) the RX3 keeps a "rotating" state until a zero-value jog report
  arrives; a scratch (touch, ticks, release) otherwise leaves Play ignored until a needle click. The bridge sends value 0
  when the wheel is idle for 80 ms and on touch release. JogTouch: op 0 = touch, any other op = release.
- **HOT CUE key (0x4113) toggles HOT CUE <-> GATE CUE**; the bridge only forwards pad-mode keys when the mode changes.
- Colour FX: keys 0x50a1..0x50a6 are normal press/release (op 0/2) but **must carry the mixer channel (1/2)**; global
  (ch 0) and op 5 do nothing. 0x509d colour knob works with op 4. Verified with `rx3-control.py query`
  (engine getters: route, crossfader assign, fader, trim, cfx type/colour, playing, tempo, realmixer). FILTER = type 1.
- **Deck state on the buttons** (2026-09-17): `control-shim.c` runs a second thread that every 200 ms calls
  `DjEngineIF::isPlaying(ch)` (0x45984) and `isMasterTempo(ch)` (0x46354, from `nm -C` on the unstripped rbp) and
  `pwrite`s two flag words + a sequence counter at byte 52 of `/dev/rx3-ui-state` (the presenter's mmap; the shim is
  built -nostdlib so it cannot include pi-controls.h, hence the fixed offset with a `_Static_assert` on the other side).
  The presenter draws PLAY and MASTER TEMPO lit (paler + bright bottom bar) while the flags say so, and treats them as
  unknown when the counter stops moving for 2 s. Rebuilding the shim = the one gcc line in build-rootfs.sh ("== shim"),
  no need to rebuild the chroot. QUANTIZE has no engine getter (`isDeckQuantizeAvailable` is availability, not on/off;
  `setQuantizeBeatValue` sets the utility beat value); the per-deck on/off is player-UI state. Disassembling the
  firmware's own indicator, `QuantizeIndicator(deck)` 0x124a7c, shows it: `UiGetPlayQuantizeOn(deck)` 0xfd36c (0 = off),
  then `UiGetPlayQuantizeAvail(deck)` 0xfd39c to dim it when no beat grid is usable. The shim calls the first with deck
  0/1 for bit 4. Neighbours worth knowing: `UiGetUtilQuantizeValue` 0xfe784 (0..3 = the beat value), `UiSetQuantizeOnOff`
  0xfe184, `ui::StatWatcher::getPlayQuantizeOn(ENUM_DECK)` 0x2c01c4 underneath, and the same `UiGet*` family exists for
  Beat FX (`UiGetMixBeatFxQuantizeOn` 0xfe4e0, `UiGetMixBeatFxType` 0xfe498, `UiGetMixBeatFxCh` 0xfe4bc).
- `rx3-control.py query` dumps engine state via the control shim (key 0xFFFF) to `/tmp/rx3-query.txt` in the chroot; the
  getters need the DjEngineIF instance (global 0x011492d8) as `this` — passing NULL crashes the player.
- **USB STOP (0x8002)**: send a plain press (code 0) and release (code 2) on channel = slot (1 = USB1, 2 = USB2); the
  firmware times the hold itself and ejects ~1.9 s after the press, a release before that cancels. Code 1 is its internal
  "long-pressed" event: sending it without a preceding press ejects instantly but leaves the slot poisoned — the next
  mount on that slot is unmounted immediately (`do_umount0` 0.4 s after the mount message, then E-8307). A single tap
  (0 then 2) on the slot clears that state. The touch bridge and `rx3-control.py usbstop <slot>` therefore only send 0/2.
  Eject unloads the decks and drops the slot from SOURCE (key 0x201).
- Firmware udev FIFO protocol (from its own rules): plug = `connect` on `/proc/udev_usbctnN`, then `mount /media/usbN/<part>`
  on `/proc/udev_usbN`; unplug = `umount <path>` then `disconnect`. `rx3-control.py mount|umount|remount <usbN> [path]` and
  `usb-hotplug.sh` follow this. The firmware opens the FIFOs O_RDWR, so short writer sessions are fine.
- What the firmware does with a mount (strace, 2026-09-11): probes `/dev/<basename of path>` with libblkid, reads
  `/etc/mtab` (-> fake `/proc/mounts`), stats the mountpoint, scans PIONEER/, and **orders `do_umount0 <path>` for every
  slot whose path is missing from mtab**. Hence: mount paths use the real partition name (`/media/usb1/sda1`,
  `/media/usb2/sdb1`), `usb-attach.sh` creates a read-only block node in the chroot `/dev`, and `rx3-mtab.sh` regenerates
  the fake mount table after every attach/detach.
- On USB STOP the firmware unmounts the path itself (`do_umount0`, then `do_umount1` MNT_DETACH every 10 s, then E-8307
  "no response"). It runs unprivileged, so `fbshim.so` hooks `umount`/`umount2` for `/media/usb*` and writes
  `umount <path>` to the `/dev/rx3-priv` FIFO; root helper `rx3-priv.sh` (transient unit `rx3-priv`, started by
  `rx3-start.sh`) performs the unmount and refreshes mtab; the hook waits until the mountpoint is gone (raw stat64 syscall:
  glibc 2.13 exports no `stat64` symbol and an unresolved one kills the player with exit 127).
- USB hot-plug: `usb-hotplug.sh` assigns slots by checking which `rx3-usb/usbN/lower` has a source (findmnt), serializes
  with `flock -w 30` on /run/rx3-usb.lock, and closes fd 9 for children (`9>&-`). The udev rule runs it through
  `systemd-run` (udevd has PrivateMounts=yes, so mounts from a RUN program would be invisible to the player), and
  fuse-overlayfs runs as its own transient unit `rx3-overlay-usbN` — inside the helper's cgroup systemd killed it the
  moment the helper exited, which is why re-inserted sticks showed up empty / not at all. `rx3-start.sh` attaches present
  sticks in the background. Verified 2026-09-11 with simulated unplug/replug (`echo 0/1 > /sys/bus/usb/devices/<port>/authorized`):
  USB STOP -> pull -> re-insert relists the slot; surprise removal + re-insert too.
- Key operation codes seen in handlers: Jog 4/0/1/5, Sync fires on release (2), AutoBeatLoop 1, BeatJumpLoopMove 3, Pad 1/3.
- **Settings keys on the touch strip** (2026-09-16): SHORTCUT 0x210, KEYBOARD 0x216, UTILITY = MENU 0x206 with the
  firmware's "long-pressed" code 1 right after the press, then release (`hold` in the button table; `rx3-control.py
  utility` / `hold <key>` do the same from the shell). On the player this shows the same screen as a plain MENU tap,
  which is the screen the user wants, so there is one UTILITY button and no separate MENU. Code 1 always follows a
  press: on its own it poisons a USB STOP slot (see above), and `rx3-control.py hold usbstop` is refused.
  KEYBOARD 0x216 did nothing from the main screen (it is in the touch-GUI key block with Shortcut, DeckInfoSelect,
  TouchPanelOn, so it probably only means something inside a text-entry screen); the button sends SEARCH 0x205
  instead, which is what "keyboard" is for. Still unmapped: Info 0x20b, TagList 0x203, DeckInfoSelect 0x213, and
  Keyboard 0x216 itself.
- The on-screen strip is 2 rows of 10 cells showing one **page** at a time (`pages[]` in `pi-controls.h`): the main
  page (row 1 navigation, row 2 LOAD / USB STOP / PLAY / DECK n per deck) and one page per deck (MASTER TEMPO 0x4108,
  QUANTIZE 0x410b on channel 1/2, and BACK). A button with `page >= 0` switches the strip instead of sending a key: the
  touch bridge releases anything still held on the old page, clears `pressed`, and writes the page into `ui_state`; the
  presenter redraws the strip into its chrome only when that changes. A bridge start resets to the main page. An entry
  with no label is an empty cell. Geometry lives in `pi-controls.h`
  (`BUTTON_COLS/ROWS`, `button_rect`, `button_at`) and both the presenter and the touch bridge use it, so a layout
  change cannot make the drawn button and the touched button disagree. Labels shrink to fit their cell (26 px down to 15).
- `rx3-deck.sh state|play|pause <deck>` — screen-verified deck helpers (only valid on the main screen, not the browser).
- Verified with the meter: both channel faders, trims, EQ and tempo work; tempo appears dead only after SYNC until the
  fader passes the synced value (Pioneer takeover behaviour).
- Power: replace the supply (see above) before judging USB stability.
- Pointer: `rx3-touch-bridge --mouse <event>` gives a cursor (drawn by the presenter): left click = touch, wheel = browse
  selector, right = BACK, middle = ENTER. `input-hotplug.sh` (also via `97-rx3-input.rules`) prefers a touchscreen
  (udev ID_INPUT_TOUCHSCREEN) over a mouse, so the touch panel will take over automatically when connected.
- USB overlay upper/work dirs are keyed by partition UUID (`rx3-usb/usb1/cow-<UUID>/`); a shared upper once hid the real
  PIONEER folder behind an opaque directory created for the test image.
- Rekordbox library: firmware 1.19 only reads `PIONEER/rekordbox/export.pdb` (classic "Device Library"). The user's stick
  FLOWRENS carries only `exportLibrary.db` (Device Library Plus) + `exportExt.pdb`, so library mode is empty; folder browsing
  of the audio files works. Re-export from rekordbox with the classic Device Library format enabled, then re-test analysed
  waveforms/beatgrids through the copy-on-write layer.
