#!/usr/bin/env python3
"""DJ controller -> XDJ-RX3 firmware control bridge (DDJ-FLX4, DDJ-400; see controllers.py).

Reads raw MIDI from the controller and translates it into the RX3 firmware's
queued key messages on the chroot control FIFO (see control-shim.c):
  struct command {int key,operation,channel,value; float analog; int extra;}
  operation: 0 press, 2 release, 4 rotary/analog, 5 switch/tempo.  channel: 0 global, 1 deck1, 2 deck2.

MIDI map is the DDJ-400 family layout (documented by Pioneer, cross-checked against Mixxx's mappings):
  note-on/off on ch0/ch1 = deck 1/2 buttons, ch6 = mixer/browse buttons, ch4/5 = Beat FX,
  ch7/ch9 = deck 1/2 pads (ch8/10 with SHIFT), CC on ch0/ch1 = deck knobs/faders, ch6 = master.
Usage: controller-bridge.py [/dev/snd/midiC?D0]   (auto-detects the first connected known controller)
       RX3_CONTROLLER=flx4|ddj400 forces the controller profile; a device of "-" reads MIDI from stdin and writes
       outgoing MIDI (LEDs, keep-alive) to $RX3_MIDI_OUT, for testing a mapping without the hardware.
"""
import atexit, glob, os, signal, struct, sys, time, threading

import rx3_env, controllers

# Say why we stop. A SIGTERM/SIGHUP kills Python silently, which once left an empty log and a dead bridge with
# nothing to go on; now the log ends with the signal (and pid of the sender when the kernel provides it).
def _on_signal(signum, frame):
    print('%s controller-bridge: terminated by signal %d (%s)' % (time.strftime('%H:%M:%S'), signum, signal.Signals(signum).name), file=sys.stderr, flush=True)
    sys.exit(128 + signum)
for _sig in (signal.SIGTERM, signal.SIGHUP, signal.SIGINT):
    signal.signal(_sig, _on_signal)
atexit.register(lambda: print('%s controller-bridge: exiting' % time.strftime('%H:%M:%S'), file=sys.stderr, flush=True))
ROOT = rx3_env.ROOT
FIFO = ROOT + '/dev/rx3-control'

# ---- RX3 firmware key ids (keycodes.txt) ----
K = dict(play=0x4101, cue=0x4102, shift=0x4103, vinyl=0x4104, temporange=0x4107, mastertempo=0x4108,
         tempo=0x4109, quantize=0x410b, loopin=0x410c, loopout=0x410d, reloop=0x410e, slip=0x4110,
         master=0x4111, sync=0x4112, hotcue=0x4113, autobeatloop=0x4114, beatjump=0x4116,
         searchfwd=0x411f, searchrev=0x4120, rotary=0x420c, back=0x420d, trackfwd=0x4214, trackrev=0x4215,
         jog=0x4305, jogtouch=0x4306, load=0x4311, masterlv=0x4403, hpmix=0x4405, hplv=0x4406,
         fxonoff=0x448d, fxtime=0x448e, fxdepth=0x448f, beatprev=0x4490, beatnext=0x4491, fxselect=0x448b, fxch=0x448c,
         callnext=0x4322, callprev=0x4323,
         trim=0x5019, eqh=0x501a, eqm=0x501b, eql=0x501c, fader=0x501e, hpcue=0x5020, color=0x509d,
         cfxfilter=0x50a6, cfxknob=0x50a7, cross=0x6017, browse=0x202, usb1=0x209)
PAD = [0x4117 + i for i in range(8)]

fifo = os.open(FIFO, os.O_RDWR | os.O_NONBLOCK)
LOG = os.environ.get('RX3_BRIDGE_LOG')
def send(key, op, ch=0, value=0, analog=0.0):
    if LOG: print('%s key=%04x op=%d ch=%d val=%d a=%.3f' % (time.strftime('%H:%M:%S'), key, op, ch, value, analog), flush=True)
    try: os.write(fifo, struct.pack('<iiiifi', key, op, ch, value, analog, 0))
    except BlockingIOError: pass
def press(key, ch, down): send(key, 0 if down else 2, ch)
def analog(key, ch, v): send(key, 4, ch, 0, v)

# ---- MIDI note -> (key, channel-kind) for deck note channels (0x90/0x91) ----
DECK_NOTES = {0x0B: 'play', 0x0C: 'cue', 0x3F: 'shift', 0x10: 'loopin', 0x11: 'loopout', 0x4D: 'reloop',
              0x58: 'sync', 0x5C: 'master', 0x60: 'temporange', 0x54: 'hpcue', 0x36: 'jogtouch', 0x68: 'quantize',
              0x40: 'searchfwd', 0x3D: 'searchfwd', 0x3E: 'searchrev', 0x51: 'callprev', 0x53: 'callnext'}
# SHIFT+PLAY (censor) is added per controller at start-up: 0x0E on the FLX4, 0x47 on the DDJ-400.
# The pad mode buttons (PAD_MODES below) are handled before this table: they drive the mode LEDs and forward the RX3's
# HOT CUE / BEAT JUMP / BEAT LOOP key only when the mode actually changes.
MIXER_NOTES = {0x46: ('load', 1), 0x47: ('load', 2), 0x41: ('rotary_press', 0), 0x42: ('back', 0)}
DECK_CC_14 = {0x00: 'tempo'}                       # MSB 0x00 + LSB 0x20 (14-bit)
DECK_CC = {0x04: 'trim', 0x07: 'eqh', 0x0B: 'eqm', 0x0F: 'eql', 0x13: 'fader'}
MASTER_CC = {0x1F: 'cross', 0x0C: 'hpmix', 0x0D: 'hplv', 0x08: 'masterlv'}
JOG_CC = {0x21: 'bend', 0x22: 'scratch', 0x23: 'bend', 0x29: 'search'}

msb = {}
# The RX3 treats jog ticks as an ongoing rotation until it sees a zero-value jog report (the physical jog reports its
# stopping). These controllers only send ticks while turning, so report zero when the wheel has been idle for a moment.
jog_last = {1: 0.0, 2: 0.0}; jog_active = {1: False, 2: False}
def jog_stop(deck):
    if jog_active[deck]: jog_active[deck] = False; send(K['jog'], 4, deck, 0, 0.0)
def jog_moved(deck): jog_last[deck] = time.time(); jog_active[deck] = True
def jog_watchdog():
    while True:
        time.sleep(0.03); now = time.time()
        for d in (1, 2):
            if jog_active[d] and now - jog_last[d] > 0.08: jog_stop(d)
threading.Thread(target=jog_watchdog, daemon=True).start()
jog_scale = float(os.environ.get('RX3_JOG_SCALE', '1'))

def note(status, n, vel):
    ch = status & 0x0F; down = (status & 0xF0) == 0x90 and vel > 0
    if ch in (0, 1):
        deck = ch + 1
        if n in PAD_MODES:
            # Pad mode buttons: light the one pressed (measured on the FLX4 with led-probe.py: each mode LED answers
            # to its own note, 0x7F on / 0 off, and nothing else touches it), then forward the RX3's own mode key only
            # when its mode must change - its HOT CUE key toggles HOT CUE <-> GATE CUE on a repeat press.
            if not down: return
            pad_mode[deck] = n; show_pad_mode(deck); show_pads(deck)
            want = RX3_PAD_MODES.get(n)
            if want is None or rx3_mode[deck] == want: return    # pad fx / sampler / keyboard / key shift: no RX3 equivalent
            rx3_mode[deck] = want; press(K[want], deck, True); press(K[want], deck, False); return
        name = DECK_NOTES.get(n)
        if name == 'jogtouch':
            if not down: jog_stop(deck)     # the RX3 needs a zero jog report or Play stays blocked after a scratch
            press(K['jogtouch'], deck, down); return
        if name == 'hpcue': press(K['hpcue'], deck, down); return
        if name == 'loopin' and down: loop_in_pending[deck] = True
        if name: press(K[name], deck, down); return
    if ch in (4, 5):                        # BEAT FX section
        global beatfx_index
        if n == 0x47: press(K['fxonoff'], 0, down); return
        if n == 0x4A: press(K['beatprev'], 0, down); return
        if n == 0x4B: press(K['beatnext'], 0, down); return
        # RX3 BEAT FX list (switch keys use operation 5 with the value): 0 DELAY 1 ECHO 2 PING PONG 3 SPIRAL 4 HELIX 5 REVERB
        # 6 FLANGER 7 PHASER 8 FILTER 9 TRANS 10 ROLL 11 SLIP ROLL 12 PITCH 13 VINYL BRAKE
        if n == 0x63 and down: beatfx_index = (beatfx_index + 1) % 14; send(K['fxselect'], 5, 0, beatfx_index, float(beatfx_index)); return
        if n == 0x64 and down: beatfx_index = (beatfx_index - 1) % 14; send(K['fxselect'], 5, 0, beatfx_index, float(beatfx_index)); return
        if n in (0x10, 0x11, 0x14) and down:   # CH SELECT slide -> RX3 values: 0 CH1, 1 CH2, 2 MIC, 3 CF.A, 4 CF.B, MASTER = FXCH_MASTER
            sel = CTL['fxch'].get((ch, n), 'master'); sel = FXCH_MASTER if sel == 'master' else sel
            send(K['fxch'], 5, 0, sel, float(sel)); return
        return
    elif ch == 6:
        global cfx_index
        if n == 0x63 and down: cfx_index = (cfx_index + 1) % len(CFX); select_cfx(); return
        m = MIXER_NOTES.get(n)
        if m:
            key, deck = m
            if key == 'rotary_press': press(K['rotary'], 0, down)
            else:
                if key == 'load' and down: loop_in_pending[deck] = False
                press(K[key], deck, down)
            return
    elif ch in (7, 9):                      # performance pads, deck 1 / deck 2
        deck = 1 if ch == 7 else 2
        if n < 0x08 or 0x20 <= n < 0x28 or 0x60 <= n < 0x68:
            press(PAD[n & 7], deck, down)
            if not down: show_pads(deck)    # the controller darkens a pad when it is released; put the hot cue state back
            return
    elif ch in (8, 10):                     # shift + pads
        deck = 1 if ch == 8 else 2
        if n < 0x08: press(K['shift'], deck, True); press(PAD[n & 7], deck, down); press(K['shift'], deck, False); return

def cc(status, c, v):
    ch = status & 0x0F
    if ch in (0, 1):
        deck = ch + 1
        if c in DECK_CC_14: msb[(ch, c)] = v; return
        if c in (0x20,):                    # tempo LSB
            m = msb.get((ch, 0x00), 0); val = (m << 7 | v) / 16383.0
            # The engine's tempo slider is signed: -1 = full minus, 0 = centre, +1 = full plus (DjEngineIF::getTempoSlider
            # reads back exactly what is sent). Feeding it the raw 0..1 fader made the centre detent +half range and the
            # top 0 %. The FLX4 sends 0 at the top, which is the minus end, like the RX3's own fader.
            send(K['tempo'], 5, deck, 0, (val - 0.5) * 2.0); return   # op 5 only
        if c in DECK_CC: analog(K[DECK_CC[c]], deck, v / 127.0); return
        if c in (0x24, 0x27, 0x2B, 0x2F, 0x33): return   # LSB echoes of the knobs, ignore
        if c in JOG_CC:
            delta = v - 64                  # the controller sends 64 +/- ticks
            if c == 0x29: delta *= 10
            send(K['jog'], 4, deck, int(delta * jog_scale), float(delta)); jog_moved(deck); return
    elif ch == 4:
        if c == 0x02: msb[(4, 2)] = v; return
        if c == 0x22: analog(K['fxdepth'], 0, (msb.get((4, 2), 0) << 7 | v) / 16383.0); return
    elif ch == 6:
        if c == 0x17: analog(K['color'], 1, v / 127.0); return
        if c == 0x18: analog(K['color'], 2, v / 127.0); return
        if c in (0x37, 0x38): return
        if c == 0x40: send(K['rotary'], 4, 0, v if v < 64 else v - 128, 0.0); return   # relative encoder: 1..63 cw, 127..65 ccw
        if c in MASTER_CC: analog(K[MASTER_CC[c]], 0, v / 127.0); return

found = controllers.detect()
forced = os.environ.get('RX3_CONTROLLER')
if forced:
    ctl_id = forced
elif found:
    ctl_id = found[0][1]
    if len(found) > 1: print('controller-bridge: %d controllers connected, using the first plugged in (%s)' % (len(found), controllers.CONTROLLERS[ctl_id]['name']), flush=True)
else:
    print('controller-bridge: no known DJ controller connected (%s)' % ', '.join(c['name'] for c in controllers.CONTROLLERS.values()), file=sys.stderr); sys.exit(1)
CTL = controllers.CONTROLLERS[ctl_id]
dev = sys.argv[1] if len(sys.argv) > 1 else (controllers.midi_device(found[0][0]) if found else None)
if not dev: print('controller-bridge: %s has no MIDI device' % CTL['name'], file=sys.stderr); sys.exit(1)
print('controller-bridge: %s on %s' % (CTL['name'], dev), flush=True)
DECK_NOTES[CTL['censor']] = 'slip'          # SHIFT+PLAY (censor) drives the RX3's SLIP; the note differs per controller

import threading
# The MIDI output is opened shareable (ALSA rawmidi: O_APPEND, which the kernel only allows together with O_NONBLOCK),
# so led-probe.py can light LEDs alongside a running bridge without stopping the player.
try: midi_out = os.open(dev if dev != '-' else os.environ.get('RX3_MIDI_OUT', '/dev/null'), os.O_WRONLY | os.O_APPEND | (os.O_NONBLOCK if dev != '-' else os.O_CREAT), 0o644)   # keep-alive, init, LEDs
except OSError as e: print('controller-bridge: cannot open MIDI out:', e, file=sys.stderr, flush=True); sys.exit(3)
out_lock = threading.Lock()
def midi_write(b):
    with out_lock:
        for attempt in range(50):           # non-blocking: a full output buffer says EAGAIN, so wait a moment and retry
            try: os.write(midi_out, b); return
            except BlockingIOError: time.sleep(0.002)
            except OSError as e: print('controller-bridge: MIDI out failed:', e, file=sys.stderr, flush=True); os._exit(3)
        print('controller-bridge: MIDI out stuck (buffer never drained)', file=sys.stderr, flush=True); os._exit(3)
if CTL['init']: midi_write(CTL['init'])
if CTL['keepalive']:
    def keepalive():
        while True: midi_write(CTL['keepalive']); time.sleep(CTL['keepalive_period'])
    threading.Thread(target=keepalive, daemon=True).start()

# ---- LED feedback (the firmware's own panel LEDs are not available to us yet, so we mirror what we know locally) ----
def led(status, note, on):
    midi_write(bytes([status, note, 0x7F if on else 0x00]))
    if LOG: print('%s led %02x %02x %s' % (time.strftime('%H:%M:%S'), status, note, 'on' if on else 'off'), flush=True)
PAD_MODES = [0x1B, 0x6D, 0x20, 0x22, 0x1E, 0x6B, 0x69, 0x6F]   # hot cue, beat loop, beat jump, sampler, pad fx1, pad fx2, keyboard, key shift
RX3_PAD_MODES = {0x1B: 'hotcue', 0x20: 'beatjump', 0x6D: 'autobeatloop'}   # the ones the RX3 has, and its key for each
pad_mode = {1: 0x1B, 2: 0x1B}
rx3_mode = {1: 'hotcue', 2: 'hotcue'}          # what the firmware is in (hotcue / beatjump / autobeatloop); HOT CUE at power-on
def show_pad_mode(deck):
    for n in PAD_MODES: led(0x90 + deck - 1, n, n == pad_mode[deck])
def init_leds():
    time.sleep(1.0)
    for d in (1, 2): show_pad_mode(d)       # PLAY / CUE / headphone CUE come from the engine (deck_state_watch), not from here
threading.Thread(target=init_leds, daemon=True).start()

# ---- PLAY / CUE LEDs from the firmware's own deck state -------------------------------------------------------
# control-shim.c publishes, every 200 ms, one flag word per deck (bit 1 playing, 2 master tempo, 4 quantize) plus a
# sequence counter at byte 52 of the shared ui_state file (struct ui_state in pi-controls.h). PLAY lights while the
# deck plays and CUE while it is stopped, as on a CDJ; the headphone CUE button (0x54) follows bit 8, the mixer channel's
# real headphone-cue state, so the screen's HP CUE, the FLX4 and the firmware can no longer disagree. All go dark when
# the counter stops moving (player gone).
UI_STATE = ROOT + '/dev/rx3-ui-state'
def deck_state_watch():
    shown = {1: None, 2: None}; shown_hp = {1: None, 2: None}; shown_loop = {1: None, 2: None}; shown_sync = {1: None, 2: None}; shown_level = {1: None, 2: None}; last_seq = None; last_change = time.time()
    hotcues[1] = hotcues[2] = None
    while True:
        time.sleep(0.05)
        try:
            with open(UI_STATE, 'rb') as f: f.seek(52); raw = f.read(28)
            if len(raw) < 28: continue
            d1, d2, seq, h1, h2, l1, l2 = struct.unpack('<IIIIIII', raw)
        except OSError: continue
        if seq != last_seq: last_seq = seq; last_change = time.time()
        fresh = time.time() - last_change < 2.0
        for deck, flags in ((1, d1), (2, d2)):
            playing = bool(flags & 1) if fresh else None
            if playing != shown[deck]:
                shown[deck] = playing
                led(0x90 + deck - 1, 0x0B, playing is True); led(0x90 + deck - 1, 0x0C, playing is False)
            hp = bool(flags & 8) if fresh else False
            if hp != shown_hp[deck]: shown_hp[deck] = hp; led(0x90 + deck - 1, 0x54, hp)
            # Loop LEDs as on a CDJ: IN while the in point is set or the loop plays, OUT while it plays, RELOOP while
            # there is a loop to go back to. The engine has no "in point set" getter, so that part is the bridge's own
            # note of a LOOP IN press, dropped once the loop plays or another track is loaded.
            sync = (bool(flags & 64), bool(flags & 128)) if fresh else (False, False)   # SYNC on, this deck is the sync master
            if sync != shown_sync[deck]: shown_sync[deck] = sync; led(0x90 + deck - 1, 0x58, sync[0]); led(0x90 + deck - 1, 0x5C, sync[1])
            looping = bool(flags & 16) if fresh else False; can_reloop = bool(flags & 32) if fresh else False
            if looping: loop_in_pending[deck] = False
            loop = (looping or loop_in_pending[deck], looping, can_reloop)
            if loop != shown_loop[deck]:
                shown_loop[deck] = loop
                led(0x90 + deck - 1, 0x10, loop[0]); led(0x90 + deck - 1, 0x11, loop[1]); led(0x90 + deck - 1, 0x4D, loop[2])
        for deck, mask in ((1, h1), (2, h2)):
            mask = mask & 0xFF if fresh else 0
            if mask != hotcues[deck]: hotcues[deck] = mask; show_pads(deck)
        # Channel level meters: CC 0x02 on the deck channel, 0..127 (measured with led-probe.py). The engine's raw level
        # is scaled by RX3_METER_MAX (its full-scale value, found by watching `rx3-control.py state` while a track plays);
        # until that is set the meters stay dark.
        if METER_MAX:
            for deck, lvl in ((1, l1), (2, l2)):
                v = min(127, int(lvl * 127 / METER_MAX)) if fresh and lvl < 0x80000000 else 0
                if v != shown_level[deck]: shown_level[deck] = v; midi_write(bytes([0xB0 + deck - 1, 0x02, v]))
# Pad LEDs in HOT CUE mode show which hot cues the loaded track has (hotcue mask from the shim, bit k = pad k+1).
# Addressing is the pads' own note range for that mode (channel 7 / 9, notes 0x00-0x07), still to be confirmed on the
# FLX4 - in the other pad modes the pads are left alone.
hotcues = {1: None, 2: None}
loop_in_pending = {1: False, 2: False}
METER_MAX = int(os.environ.get('RX3_METER_MAX') or 0)      # rx3-start.sh passes it empty when rx3.conf does not set it
def show_pads(deck):
    if pad_mode[deck] != 0x1B: return
    mask = hotcues[deck] or 0
    for k in range(8): led(0x97 if deck == 1 else 0x99, k, bool(mask >> k & 1))
threading.Thread(target=deck_state_watch, daemon=True).start()

# ---- Sound Color FX: the RX3 needs an effect selected before the per-channel COLOR knobs do anything ----
# Engine effect slots (SoundColorFxManager ctor order = type number): 1 FILTER, 2 NOISE, 3 SWEEP, 4 DUB ECHO, 5 SPACE,
# 6 CRUSH. Keys: 0x50a6 filter, 0x50a4 noise, 0x50a3 sweep, 0x50a2 dub echo, 0x50a1 space, 0x50a5 crush (pressing the
# active effect's key again switches it off). Start on FILTER; the SMART CFX button cycles from there.
CFX = [0x50a6, 0x50a1, 0x50a2, 0x50a3, 0x50a4, 0x50a5]
cfx_index = 0
def select_cfx():
    for ch in (1, 2):                       # effect type is per mixer channel; the key must carry the channel
        press(CFX[cfx_index], ch, True); time.sleep(0.05); press(CFX[cfx_index], ch, False)
threading.Timer(2.0, select_cfx).start()
beatfx_index = 0
FXCH_MASTER = int(os.environ.get('RX3_FXCH_MASTER', '5'))

buf = b''
with open(dev if dev != '-' else 0, 'rb', buffering=0) as f:
    while True:
        try: data = f.read(64)
        except OSError as e: print('controller-bridge: MIDI in failed:', e, file=sys.stderr, flush=True); sys.exit(3)
        if not data: time.sleep(.01); continue
        buf += data
        while buf:
            s = buf[0]
            if s < 0x80: buf = buf[1:]; continue          # skip stray data bytes
            if s >= 0xF0: buf = buf[1:]; continue          # ignore system messages
            if len(buf) < 3: break
            d1, d2 = buf[1], buf[2]; buf = buf[3:]
            kind = s & 0xF0
            if LOG: print('%s midi %02x %02x %02x' % (time.strftime('%H:%M:%S'), s, d1, d2), flush=True)
            if kind in (0x80, 0x90): note(s, d1, d2)
            elif kind == 0xB0: cc(s, d1, d2)
