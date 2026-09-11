# Latest steering and media evidence
User clarified surrounding controls are INTERIM. Final goal is integrated native touchscreen operation; user says touchscreen mostly works. Requested a handoff prompt/package for Buddy to give Astra Codex, including firmware links, key and decryption. Created rx3-handoff/ASTRA-PROMPT.md and recovery/compatibility files, source-package aes256.key (no SSH credentials).

Live trace: USB2 first attach opens export.pdb O_RDWR|O_CREAT|O_SYNC and gets EROFS; TMP0000.TMP write also EROFS. Need writable copy-on-write USB view to preserve original files, then retest native library/analysed tracks. Definitive first blocker, not proof it is the only issue.
Temporary duplicate read-only bind at rootfs/media/usb2/sdb1 points to same USB. Sent mount event to proc/udev_usb2 for independent tracing; original USB1 and player7385 remain. No overlay implemented yet. Trace Pi rx3-usb2-trace.log. No raw block device exposed.

# Current checkpoint — September 10, headphone/touch deployment

Goal ACTIVE. Real RX3 player running on Pi; headphone audibility not confirmed yet.

## Current running deployment
- Pi SSH pompu_5@pflx.local with /tmp/piflex-ssh-key and /tmp/piflex-known-hosts. sudo -n. Remote rg unavailable, use grep.
- rbp-pi PID7385; presenter7016; physical touch bridge7019. Verify before using PIDs.
- Estara loaded successfully via replay of actual bridge input events; timer advanced and nonzero headphone samples verified at native second PCM write 0x3c5898.
- User is actively touching physical screen, evidenced rx3-touch.log. Avoid remotely changing decks while they test.
- Both decks may now contain Estara from user's physical LOAD presses. User's play/pause state may differ.
- New 1920x1200 landscape chrome deployed: native display1600x1000 atx160, twelve bottom buttons, six side sliders, per-deck HP CUE.
- Master PCM rx3out routes FLX6 channels1/2; headphone PCM rx3cue routes3/4; shared dmix4ch44100S16_LE, gain.25 per route. Third boothoutputnull.
- Pending async user question: Can hear Estara in FLX6 headphones? options clear/choppy/silent. Last answer to earlier question: uses cue/headphones, sees stacked waveforms.
- No physical audible success claimed. Goal not complete.

## Critical fix this pass
Global snd_pcm_hw_params_set_access override broke dmix MMAP, causing SIGSEGV snd1_pcm_direct_check_interleave in old ALSA during prepare. GDB symbolized confirmed. Fixed wrapper ONLY changes requested4(RW_NONINTERLEAVED) to3(RW_INTERLEAVED); preserves0(MMAP_INTERLEAVED). Both output routes now open and remain stable, log4.8KB.
Pi lacks coredumpctl; used sudo gdb startup and solib-search-path chroot/lib:usr/lib. No core files extracted.

## Input / control adapter
control-shim.c compiled with fbshim.c, sends native queue entry0x37ad64 through manager=*( *(void**)0x026867c0 +0x64). notify1stKeyHandled(manager,3)0x37c8d8 clears physical panel startup gate.
Operations0press,2release,4analog/rotary. Channel0global1/2decks. Full map keycodes.txt. Mixer defaults automatically set: trim/EQ/color.5,faders1,cross.5,master.6,HPlevel.5,HPmix0,HPcue1enabled.
pi-control.py name/channel/optionalfloat drives controlFIFO. mount emits udev FIFOevent. touch-replay.py x y drives same touch-bridge --replay code with Linux input_event; verified ENTER,LOAD1,PLAY1,BROWSE. User physical taps logged for nativecontent andtoolbar.
Native touch repeats10ms for debounce. Slider/touch and presenter share pi-controls.h state36bytes in rootfs/dev/rx3-ui-state.

## Startup
start-rx3.sh updated locally+remotely: starts bridge too; initializes state in-place; waits10sec then sends existing USBmount event. Syntax checked; new launcher has not been exercised end-to-end since user is interacting. Still assumes rootfs mounts from this boot; no bootenable.
First mount event after restart was too early, resulted emptyUSB. Repeated after readiness succeeded.
To browse: USB1, ENTER into Contents, ENTER AAMAR, ENTER album; LOAD1. UI responsiveness needs ~.5-1sec between actions. pi-control.py mount when empty.
Build shim arm-linux-gnueabi-gcc -shared -fPIC -O2 -fomit-frame-pointer -fno-builtin -nostdlib -o rx3-rootfs/lib/fbshim-next.so fbshim.c control-shim.c. Stopplayer before replacing .so. Presenter gcc with pkg-config freetype2. Touchbridge gcc-O2.

## Remaining
- User headphone audibility confirmation; investigate output routing if silent/choppy.
- Waveforms: mostly blank stacked waveform, overview shows some generated blue bars after playback; BPM blank. USB has PIONEER/rekordbox/export.pdb7MB,exportExt.pdb,USBANLZ,Artwork but UI only folder mode. Could be DB accessibility/read-only requirements; unconfirmed. Need inspect without disrupting user's live testing.
- Native touch coordinates physical confirmation; controls loops/sync/tempo/EQ not all exposed yet.
- Reboot/mount setup and sustained two-deck audio verification.
- Preserve BiteDJ and user USB files. USB bind remains read-only. No /dev/mem exposure or original vendor startup.

Screenshots: headphone-test.png shows Estara playing with newchrome; native-touch-test.png shows user loaded bothdecks, folderinfo, overview beginning bluebars. Generated from real /dev/fb0 rotated-90. Never truncate hostfb.

# Previous checkpoint (stale details superseded above)

# RX3 on Pi: current checkpoint, September 10

Goal still active: play real music and make all needed controls usable by touch.

## Verified this pass
- Touch handler accepted down=1, x=1181, y=400 when given repeated reports. Original touch bridge only sent changed evdev events, missing firmware debouncing. Updated touch-bridge.c polls at 10ms and repeats held/release reports. Running PID4849 (comm is truncated, use pgrep -f, not full-name pkill -x).
- Audio engine now opens ALL firmware virtual inputs/outputs and FLX6 hardware playback is RUNNING, hw_ptr/appl_ptr advancing. Player PID5975. Log only 4.3KB and stable.
- No music loaded yet. No audible music confirmed. Main stream goes FLX6; second/third outputs and inputs are null PCMs, so headphone cue still needs proper routing.
- current-player.png is the current real framebuffer screenshot. Landscape, no tracks loaded.

## Audio fixes in fbshim.c
- snd_ctl_open redirects to hw:2, required for device property queries.
- snd_ctl_pcm_info reports virtual capture available; capture PCM is ALSA null.
- snd_pcm_open maps device names ending 0 playback to rx3out; other playback and capture to null.
- get_channels_max caps at TWO channels (firmware uses stereo buffers per device). MUST use dlvsym ALSA_0.9.0rc4 here: unversioned dlsym selects old return-value ABI and leaves output pointer untouched!
- set_access forces RW_INTERLEAVED=3. Firmware asks RW_NONINTERLEAVED=4 but actually calls readi/writei. Without this, failed writes enter vendor DMA recovery loop and flood logs. Never expose /dev/mem.
- Added bind mount /dev/full in chroot; ALSA null capture needs it.
- Existing asound.conf routes master through plug/route to FLX6 hw:2,0 four channels with gain0.25.
- temporary parameter logs in shim useful, remove once stable.

## Next tasks / investigation
Need USB event after restart: write `mount /media/usb1/sda1` to rootfs/proc/udev_usb1 FIFO with O_RDWR|O_NONBLOCK. Readonly USB mount exists. No need actual mount syscalls by RX3.
Need emulate original hardware controls (Browse, USB1, Load1/2, Play/Cue) via onscreen strip or overlay, since original RX3 touchscreen lacks those hardware buttons.
Useful native entry: uif::IKeyManager::sendKey at0x37ad64 signature (this,int key,int operation,int channel,long value,float value,long extra). Queues safely when called from another thread. Key operations touch uses1 down,2up. Need key IDs.
KeyManager constructor0x33f4a8 first word push{r3,r4,r5,r6,r7,lr}=0xe92d40f8. Could capture this via patch trampoline calling shim ioctl with private request before original constructor. ioctl@plt0xec1c. Original RTC space0x4234c..0x423d4; pi-clock.bin occupies68 bytes through0x42390; ~68bytes remain for small trampoline. Must preserve r0/r1 etc and original constructor prologue. Shim control thread can read FIFO and call sendKey after startup.
Alternative pointer via TouchPanelHandler: handler[0] is KeyRecieveNotification*, then +0x64 holds IKeyManager*; handler received by solveCoordToKey0x2dc104. Heap changes each restart.
UiKey_Usb1=0x11a3cc, UiKey_Browse0x119e94; these are lower-level UI functions expecting pointer to key record (>=12 bytes), may not be thread-safe so prefer sendKey. Browse global data functions are C.

GDB startup debugging works (avoids unsafe inferior function calls): /tmp/rx3-start-debug.gdb on Pi executes /usr/sbin/chroot with userspec/groups, catch exec busybox then continue catch rbp, delete catchpoints then break relevant address before continue. At0x44c84 initialization error juce wide string pointer stored sp+84. Last successful audio setup supersedes old Invalid argument.
Do NOT call initializeAudioDevice through gdb on live main thread: an attempted inferior call caused process to disappear after detach. No second simultaneous rbp.

SSH key /tmp/piflex-ssh-key, known hosts /tmp/piflex-known-hosts, pompu_5@pflx.local; sudo -n works.
Build shim to fbshim-next.so, stop rbp, then rename shared library; never overwrite running .so.
Remote start-rx3.sh has --groups=29,44,995,991 (local copy needed syncing). Start script still assumes mounts and touch bridge already present; improve later. No reboot startup enabled. BiteDJ stopped as authorized, original preserved.
