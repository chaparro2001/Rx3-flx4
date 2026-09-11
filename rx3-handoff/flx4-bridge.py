#!/usr/bin/env python3
"""DDJ-FLX4 -> XDJ-RX3 firmware control bridge.

Reads raw MIDI from the DDJ-FLX4 and translates it into the RX3 firmware's
queued key messages on the chroot control FIFO (see control-shim.c):
  struct command {int key,operation,channel,value; float analog; int extra;}
  operation: 0 press, 2 release, 4 rotary/analog.  channel: 0 global, 1 deck1, 2 deck2.

MIDI map follows the DDJ-FLX4 documented layout (same family as DDJ-400):
  note-on/off on ch0/ch1 = deck 1/2 buttons, ch6 = mixer/browse buttons,
  ch7/ch9 = deck 1/2 pads, CC on ch0/ch1 = deck knobs/faders, ch6 = master.
Usage: flx4-bridge.py [/dev/snd/midiC?D0]  (auto-detects the FLX4 if omitted)
"""
import glob, os, struct, sys, time, threading

ROOT = os.environ.get('RX3_ROOT', '/home/rx3/rx3-rootfs')
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
              0x58: 'sync', 0x5C: 'master', 0x60: 'temporange', 0x54: 'hpcue', 0x36: 'jogtouch',
              0x1B: 'hotcue', 0x6D: 'autobeatloop', 0x20: 'beatjump', 0x0E: 'slip', 0x68: 'quantize',
              0x40: 'searchfwd', 0x3D: 'searchfwd', 0x3E: 'searchrev', 0x51: 'callprev', 0x53: 'callnext'}
# 0x1B hot cue mode, 0x6D beat loop mode, 0x20 beat jump mode select the pad mode on the RX3 too.
MIXER_NOTES = {0x46: ('load', 1), 0x47: ('load', 2), 0x41: ('rotary_press', 0), 0x42: ('back', 0)}
DECK_CC_14 = {0x00: 'tempo'}                       # MSB 0x00 + LSB 0x20 (14-bit)
DECK_CC = {0x04: 'trim', 0x07: 'eqh', 0x0B: 'eqm', 0x0F: 'eql', 0x13: 'fader'}
MASTER_CC = {0x1F: 'cross', 0x0C: 'hpmix', 0x0D: 'hplv', 0x08: 'masterlv'}
JOG_CC = {0x21: 'bend', 0x22: 'scratch', 0x23: 'bend', 0x29: 'search'}

msb = {}
# The RX3 treats jog ticks as an ongoing rotation until it sees a zero-value jog report (the physical jog reports its
# stopping). The FLX4 only sends ticks while turning, so report zero when the wheel has been idle for a moment.
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
        name = DECK_NOTES.get(n)
        if name == 'jogtouch':
            if not down: jog_stop(deck)     # the RX3 needs a zero jog report or Play stays blocked after a scratch
            press(K['jogtouch'], deck, down); return
        if name == 'hpcue': press(K['hpcue'], deck, down); return
        if name: press(K[name], deck, down); return
    if ch in (0, 1) and n in PAD_MODES:
        deck = ch + 1
        if not down: return
        pad_mode[deck] = n; show_pad_mode(deck)
        # Forward to the RX3 only when its own pad mode must change. Its HOT CUE key toggles HOT CUE <-> GATE CUE.
        if n == 0x1B:
            if rx3_mode[deck] == 'hotcue': return
            rx3_mode[deck] = 'hotcue'
        elif n == 0x20:
            if rx3_mode[deck] == 'beatjump': return
            rx3_mode[deck] = 'beatjump'
        elif n == 0x6D:
            if rx3_mode[deck] == 'beatloop': return
            rx3_mode[deck] = 'beatloop'
        else: return                        # pad fx / sampler / keyboard / key shift have no RX3 equivalent
    if ch in (0, 1) and n == 0x54 and down: hp_cue[ch + 1] = not hp_cue[ch + 1]; led(status, 0x54, hp_cue[ch + 1])
    if ch in (4, 5):                        # BEAT FX section
        global beatfx_index
        if n == 0x47: press(K['fxonoff'], 0, down); return
        if n == 0x4A: press(K['beatprev'], 0, down); return
        if n == 0x4B: press(K['beatnext'], 0, down); return
        # RX3 BEAT FX list (switch keys use operation 5 with the value): 0 DELAY 1 ECHO 2 PING PONG 3 SPIRAL 4 HELIX 5 REVERB
        # 6 FLANGER 7 PHASER 8 FILTER 9 TRANS 10 ROLL 11 SLIP ROLL 12 PITCH 13 VINYL BRAKE
        if n == 0x63 and down: beatfx_index = (beatfx_index + 1) % 14; send(K['fxselect'], 5, 0, beatfx_index, float(beatfx_index)); return
        if n == 0x64 and down: beatfx_index = (beatfx_index - 1) % 14; send(K['fxselect'], 5, 0, beatfx_index, float(beatfx_index)); return
        if n in (0x10, 0x11) and down:      # CH SELECT slide -> RX3 values: 0 CH1, 1 CH2, 2 MIC, 3 CF.A, 4 CF.B, MASTER = FXCH_MASTER
            sel = 0 if (ch, n) == (4, 0x10) else 1 if (ch, n) == (5, 0x11) else FXCH_MASTER
            send(K['fxch'], 5, 0, sel, float(sel)); return
        return
    elif ch == 6:
        global cfx_index
        if n == 0x63 and down: cfx_index = (cfx_index + 1) % len(CFX); select_cfx(); return
        m = MIXER_NOTES.get(n)
        if m:
            key, deck = m
            if key == 'rotary_press': press(K['rotary'], 0, down)
            else: press(K[key], deck, down)
            return
    elif ch in (7, 9):                      # performance pads, deck 1 / deck 2
        deck = 1 if ch == 7 else 2
        if n < 0x08 or 0x20 <= n < 0x28 or 0x60 <= n < 0x68:
            press(PAD[n & 7], deck, down); return
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
            send(K['tempo'], 5, deck, 0, val); return            # op 5 only; FLX4 sends 0 at the top = -tempo, like the RX3 fader
        if c in DECK_CC: analog(K[DECK_CC[c]], deck, v / 127.0); return
        if c in (0x24, 0x27, 0x2B, 0x2F, 0x33): return   # LSB echoes of the knobs, ignore
        if c in JOG_CC:
            delta = v - 64                  # FLX4 sends 64 +/- ticks
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

def find_flx4():
    for d in sorted(glob.glob('/dev/snd/midiC*D0')):
        card = d.split('midiC')[1].split('D')[0]
        try:
            if 'FLX4' in open('/proc/asound/card%s/id' % card).read(): return d
        except OSError: pass
    return None

dev = sys.argv[1] if len(sys.argv) > 1 else find_flx4()
if not dev: print('DDJ-FLX4 MIDI device not found', file=sys.stderr); sys.exit(1)
print('flx4-bridge: reading', dev, flush=True)

# The FLX4 only reports controls (and keeps its audio path active) while a host keeps polling it: rekordbox and Mixxx
# send this vendor SysEx every 200 ms (Mixxx: "reverse engineered with Wireshark"). It doubles as a control-position query.
KEEPALIVE = bytes([0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7])
import threading
try: midi_out = os.open(dev, os.O_WRONLY)          # one shared output handle: keep-alive and LED feedback
except OSError as e: print('flx4-bridge: cannot open MIDI out:', e, file=sys.stderr, flush=True); sys.exit(3)
out_lock = threading.Lock()
def midi_write(b):
    with out_lock:
        try: os.write(midi_out, b)
        except OSError as e: print('flx4-bridge: MIDI out failed:', e, file=sys.stderr, flush=True); os._exit(3)
def keepalive():
    while True: midi_write(KEEPALIVE); time.sleep(0.2)
threading.Thread(target=keepalive, daemon=True).start()

# ---- LED feedback (the firmware's own panel LEDs are not available to us yet, so we mirror what we know locally) ----
def led(status, note, on):
    midi_write(bytes([status, note, 0x7F if on else 0x00]))
    if LOG: print('%s led %02x %02x %s' % (time.strftime('%H:%M:%S'), status, note, 'on' if on else 'off'), flush=True)
PAD_MODES = [0x1B, 0x6D, 0x20, 0x22, 0x1E, 0x69, 0x6F]      # hot cue, beat loop, beat jump, sampler, pad fx1, keyboard, key shift
pad_mode = {1: 0x1B, 2: 0x1B}
rx3_mode = {1: 'hotcue', 2: 'hotcue'}          # what the firmware is in (hotcue / beatjump / beatloop)
hp_cue = {1: True, 2: False}                                 # control-shim.c enables deck 1 cue at startup
def show_pad_mode(deck):
    for n in PAD_MODES: led(0x90 + deck - 1, n, n == pad_mode[deck])
def init_leds():
    time.sleep(1.0)
    for d in (1, 2): show_pad_mode(d); led(0x90 + d - 1, 0x54, hp_cue[d])
threading.Thread(target=init_leds, daemon=True).start()

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
with open(dev, 'rb', buffering=0) as f:
    while True:
        try: data = f.read(64)
        except OSError as e: print('flx4-bridge: MIDI in failed:', e, file=sys.stderr, flush=True); sys.exit(3)
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
