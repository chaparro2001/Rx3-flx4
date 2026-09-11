# Paste this into Astra Codex and attach the accompanying RX3 handoff ZIP

You are continuing a working experimental port of the **real Pioneer XDJ-RX3 firmware 1.19 player** to a Raspberry Pi 5, 4GB RAM, Debian. This is the standalone RX3 application, not the Windows/macOS rekordbox desktop application. The user called it “RXC”; the model successfully extracted and running here is XDJ-RX3.

## Goal and user's clarification
Get music playing with complete touchscreen operation, native RX3 behavior and a polished integrated touchscreen interface. Preserve the user's BiteDJ Mixxx fork and EDMC work. The big surrounding buttons/faders are an **interim input/testing adapter**, not the accepted final interface. The user says touch mostly works. Keep native touchscreen functionality, identify and integrate the missing physical-deck/mixer actions, and verify actual headphone cue audio. Do not substitute a fake screenshot or unrelated DJ app.

RX3 firmware was designed for physical transport/mixer controls; showing its stock screen alone does not make every feature touch-operable. Our adapter currently feeds the firmware's own message-queued input system. Finish the integration deliberately; do not promise that removing the border automatically makes the missing hardware controls appear in the stock UI.

## Public sources and exact firmware
- Official source distribution index: https://www.pioneerdj.com/en/support/open-source-code-distribution/gnu-open-source-license/
- Source ZIP A: https://files.microcms-assets.io/assets/3b9e29ce734e49babfedb3f8d1e728e3/f7f69f5f428841898c6d98f9976c23bb/A9BEE4F7-6932-4E11-8D9F-5288F5F79EC2.zip
- Source ZIP B: https://files.microcms-assets.io/assets/3b9e29ce734e49babfedb3f8d1e728e3/5b39b9d79cc747c2b31ff39e22074bad/57CB205B-D45A-4143-BC09-22D8400074C2.zip
- Official firmware: https://downloads.support.alphatheta.com/firmwares/all-in-one-dj-systems/XDJ-RX3/XDJ-RX3_v119.zip
- Sector crypto implementation: https://github.com/Tratosca/rx3-toolkit/blob/main/tools/rx3_firmware/firmware_image.py (MPL-2.0; included with its notice).
- Related CDJ emulator: https://github.com/nsaintot/cdj3k-emu (useful control/storage/audio references; not a drop-in RX3 Pi runtime).
- Earlier Denon research link: https://github.com/icedream/denon-prime4 (not the source of the working RX3 binary).

Do not claim a confirmed GitHub identity for Instagram i.erhan.es or that these repositories belong to that person; that connection is unverified.

## Recover the binary and key without owning a CDJ
The included `aes256.key` is the firmware key file recovered from **Pioneer's public source package**, not someone's SSH key, account credential, or a device-specific secret. `recover-firmware.py` independently retrieves it again from the official archives and checks known SHA256 hashes.

On a Linux research workstation, use a virtual environment with `cryptography` and install `7z`, or both `unzip` (with Deflate64 support) and `bsdtar`. Then run:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install cryptography
python3 recover-firmware.py
python3 extract_cramfs.py
```

Recovery details:
1. Each source ZIP contains one piece: `pioneerdj_xdj_rx3.tar.bz2.00` or `.01`. Concatenate **00 then 01**, not ZIP order.
2. In that tarball extract `pioneerdj_xdj_rx3/initramfs.tar.gz`.
3. Inside initramfs the key is `initramfs/usr/local/pdj/aes256.key`.
4. Extract `XDJRX3.UPD` from the official v1.19 ZIP.
5. For this verified update, exclude its **last 16 bytes**. Decrypt the remaining aligned payload in separate 512-byte sectors using AES-256-CBC. IV = sector index as little-endian uint32 followed by twelve zero bytes.
6. Effective AES key = first line of key file, first **31 bytes**, padded with NUL to 32 bytes. Do not use the whole key file or conventional whole-file CBC.
7. Verify `CD001` at ISO offset 32769. The supplied routine validates this. This does not authenticate the excluded update trailer.
8. Extract ISO; application is in `images/pdj.tar.gz`, GUI assets in `images/gui.tar.gz`, libraries/runtime in `images/rootfs.cramfs`.
9. Player path after extraction: `extracted/player/pdj/rbp`. It is an unstripped 32-bit ARM EABI5 ELF using `/lib/ld-linux.so.3`.

Verified original player SHA256:
`60bcbd8876116bf09f0d8f747f95d7c7d3081ebd39d6fe14d56005a22f7f3b09`

All hardcoded patches/input addresses below are for this exact binary. Do not apply them to another firmware version without validating the binary and disassembly.

## What actually works as of September 10
- Real player boots and renders on Pi 5 using the bundled ARM32 libraries, DirectFB software rendering and a framebuffer compatibility shim.
- USB folders browse, MP3 metadata appears, tracks load on both decks, and playback time advances.
- Nonzero master AND headphone PCM samples verified in the native engine while Estara plays.
- Master routed to FLX6 channels 1/2; headphone stream routed to 3/4 through shared ALSA dmix. **User hearing clear audio has not yet been confirmed.** Engine samples alone are not proof of audible output or smooth scheduling.
- Touch bridge accepts physical multitouch events, rotates/maps the coordinates, and debounces native reports. User physical taps are logged. Replayed input events through the same bridge successfully browsed, loaded and played.
- Temporary surrounding controls provide USB1, browse, back, up/down/enter, both loads/cues/play-pause, channel faders, master, crossfader, headphone level/mix and per-deck headphone cue.
- Waveforms/BPM/library mode remain incomplete. Some overview bars appear from playback, but detailed stacked waveform is largely blank.
- BiteDJ stopped for framebuffer ownership, preserved. EDMC service remains active; native EDMC browser integration is unproven.

## Latest concrete media finding — next useful work
A trace of a new USB2 attachment confirmed:
`open("/media/usb2/sdb1/PIONEER/rekordbox/export.pdb", O_RDWR|O_CREAT|O_SYNC, 0600) = -1 EROFS`
The firmware also attempts `TMP0000.TMP` writes. The USB already has export.pdb, exportExt.pdb, USBANLZ and Artwork; its database files initially open read-only successfully, then the read/write database open fails.

Implement a writable **copy-on-write view** with originals preserved (for example, overlay lower = read-only USB, upper/work = dedicated Pi storage), or another evidence-based solution. Do not change the original export database merely to bypass this failure. Retest library recognition and loading an analysed track, then verify detailed waveform/BPM/beatgrid. EROFS is confirmed; that it is the only remaining library problem is not yet proven.

## Runtime and building the supplied compatibility files
Current Pi user/home: `pompu_5`, `/home/pompu_5`; hostname `pflx.local`.
Rootfs: `/home/pompu_5/rx3-rootfs`.
This archive deliberately contains no private SSH credentials. Buddy must use his own authorized connection. On the original machine, reuse the existing authorized session rather than requesting credentials again.

Reconstruct an isolated rootfs from extracted runtime regular files and `runtime-symlinks.json`, preserving absolute symlinks **inside the chroot**. Place the extracted `pdj` and GUI resources at their firmware locations. The original working rootfs is authoritative when available. Inspect the existing environment before rebuilding it.

To build patches, first place the verified original `rbp` at `pi-runtime/rbp`, then:
```sh
arm-linux-gnueabi-as -o pi-clock.o pi-clock.S
arm-linux-gnueabi-objcopy -O binary pi-clock.o pi-clock.bin
python3 patch-player.py
arm-linux-gnueabi-gcc -shared -fPIC -O2 -fomit-frame-pointer -fno-builtin -nostdlib -o fbshim-next.so fbshim.c control-shim.c
gcc -O3 -o rx3-fb-present-next fb-present.c $(pkg-config --cflags --libs freetype2)
gcc -O2 -o rx3-touch-bridge-next touch-bridge.c
```
The last two programs must target the Pi's native aarch64 environment; these commands were run on the Pi. Dependencies include an ARM32 cross compiler/headers and FreeType development files. Keep original rbp intact and deploy patched `rbp-pi`. Stop the corresponding process before replacing mapped executable/shared-library files.

### Critical graphics/clock details
- Original i.MX RTC access replaced with Linux monotonic cycles by `pi-clock.S` and binary patches.
- Disable firmware Vivante DirectFB graphics module to use software rasterization; do not execute vendor hardware initialization.
- Chroot `/dev/fb0` is a **regular 4096000-byte file** representing1280x800 RGB32. Host `/dev/fb0` is the actual1200x1920 Pi display. Never truncate the host framebuffer.
- Current presenter renders landscape1920x1200, scales original content to1600x1000 atx160 and rotates it onto the physical display. Interim controls occupy side/bottom space.
- `fbshim.c` needs `<asm/ioctl.h>` to avoid modern ARM time64 symbol redirection. Strip RTLD_DEEPBIND when loading plugins so interception works.

### Input details
- Physical touch currently `/dev/input/event5`, ili_v3, MT X0..1199/Y0..1919. Rediscover by identity on another boot/machine.
- Native FIFO `/dev/tsc2007_2-0048`, packed6byte down/pad/uint16x/uint16y. Repeated10ms held/release reports required for native debounce.
- Control FIFO `/dev/rx3-control`: little-endian24byte `<iiiifi>` key,operation,channel,value,analog,extra.
- IKeyManager = pointer at offset0x64 of object pointed to by address0x026867c0.
- `notify1stKeyHandled(manager,3)` at0x37c8d8 clears the missing physical-panel startup gate. Without it most key input is ignored.
- Queued `sendKey` at0x37ad64; operation0press,2release,4rotary/analog. Channels0global/1deck1/2deck2. This queues onto native UI thread; avoid arbitrary UI function calls from foreign threads.
- `keycodes.txt`, `control-shim.c`, `pi-controls.h` document IDs. Defaults neutral EQ/trim .5, faders1, cross.5, master.6, HPlevel.5, HPmix0, deck1HPcue enabled.
- `touch-replay.py x y` exercises the same bridge remotely. Avoid competing with a person touching the screen during tests.

### Critical ALSA details
- FLX6 currently `hw:2,0`,4channels44100Hz. Discover stable device identity instead of assuming card2 forever.
- Intercept snd_ctl_open and the capture-info check; expose virtual capture through null PCM. Chroot `/dev/full` is needed by null capture.
- Firmware's three stereo output devices correspond to master, headphones, booth; first two route through asound.conf, booth currently null.
- `snd_pcm_hw_params_get_channels_max` MUST resolve symbol version `ALSA_0.9.0rc4` and cap2. Unversioned dlsym selected an incompatible old ABI.
- Convert requested RW_NONINTERLEAVED(4) to RW_INTERLEAVED(3), because firmware calls readi/writei. **Preserve MMAP_INTERLEAVED(0)** for dmix internals. Overriding every access mode caused a confirmed crash in snd1_pcm_direct_check_interleave.
- Do not expose `/dev/mem` to satisfy the vendor audio recovery path. Stop and inspect repeated audio errors rather than letting logs fill storage.

### Mounts/startup still require completion
Current isolated rootfs has host null/zero/urandom/full and snd binds, fake cpuinfo/proc mounts entries, emulated GPIO/panel FIFOs, and read-only USB at `/media/usb1/sda1`. GPIO backing file is4096 bytes of1; its vendor polling loop is patched off. Do not create host device nodes indiscriminately.

`start-rx3.sh` launches chroot with uid1000 and required video/audio/input groups, presenter and touch bridge, then emits USB mount event after10sec. It still assumes this boot's mounts and device assignments; it is **not** a complete reboot-safe appliance installer. Never run it blindly on a different user's machine. Current launch from user's home works; earlier systemd RootDirectory launch did not and needs investigation.

USB event: write literal `mount /media/usb1/sda1` to chroot `/proc/udev_usb1` FIFO opened O_RDWR|O_NONBLOCK. Do this after storage workers are ready. Avoid a second player instance.

## Working rules and completion checks
Work autonomously on authorized reversible changes. Preserve BiteDJ, music and original firmware. Do not flash any device or run the original apl_start/decrypt_autoexec startup scripts. Their source can be inspected, but they perform unrelated vendor hardware operations.

Inspect live state before acting; PID7385 was last player but PIDs are not durable. Current status is in the newest section of SESSION-STATE.md; older sections are explicitly stale. Keep user informed and do not claim all-touch or audible success based only on log output.

Complete only after real library/track loading, sustained two-deck playback, separate headphone cue/master routing, physical touch alignment and needed integrated controls are verified, with a reproducible launch path and BiteDJ preserved. Read the included code, continue from the concrete EROFS finding, and keep the full goal active while unfinished.

## Handoff verification
The supplied recovery script was executed against the hash-verified cached official ZIPs: source key extraction, sector decryption, ISO extraction and the original player SHA256 all passed. The supplied cramfs extractor also completed. Network downloading itself was not repeated in this packaging test. The live compatibility code had already been built and deployed; a fresh end-to-end installation on Buddy’s machine remains to be done.
