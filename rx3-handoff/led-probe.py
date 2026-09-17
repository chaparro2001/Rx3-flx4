#!/usr/bin/env python3
"""Find out, on the controller itself, which MIDI messages light which LEDs (and which button sends which note).

Stops nothing by itself: the pointer/controller bridges must not be holding the MIDI device, so run
    sudo systemctl stop rx3
first (the probe refuses to start while controller-bridge.py is running). It sends the controller's keep-alive
itself, prints every message the controller sends (press a button to learn its note), and takes commands:

    on  <ch> <note> [vel]    note-on  on channel ch (0-15), velocity vel (default 7F)   e.g.  on 0 1B
    off <ch> <note>          note-on with velocity 0                                      e.g.  off 0 69
    cc  <ch> <cc> <val>      control change                                               e.g.  cc 0 02 7F
    raw <hex bytes>          anything                                                     e.g.  raw 90 1B 7F
    sweep <ch> <from> <to> [vel] [seconds]   light each note in turn (default 0.6 s each), then all off:
                             watch which LED answers to which note                        e.g.  sweep 7 00 0F
    alloff <ch>              velocity 0 on every note of the channel
    q                        quit (turns nothing off - use alloff first if you want a dark controller)

All numbers are hex. usage: led-probe.py [/dev/snd/midiC?D0]   (default: the first connected known controller)
"""
import os, subprocess, sys, threading, time
import controllers

def die(m): print('led-probe: ' + m, file=sys.stderr); sys.exit(1)

if subprocess.run(['pgrep', '-f', r'^python3 \S*controller-bridge\.py'], capture_output=True).returncode == 0:   # anchored: a shell holding this text must not match
    die('controller-bridge.py is running and owns the MIDI device: sudo systemctl stop rx3 first')
found = controllers.detect()
ctl = controllers.CONTROLLERS[found[0][1]] if found else None
dev = sys.argv[1] if len(sys.argv) > 1 else (controllers.midi_device(found[0][0]) if found else None)
if not dev: die('no known controller connected (%s)' % ', '.join(c['name'] for c in controllers.CONTROLLERS.values()))
print('led-probe: %s on %s' % (ctl['name'] if ctl else 'unknown controller', dev))

out = os.open(dev, os.O_WRONLY)
lock = threading.Lock()
def send(b, echo=True):
    with lock: os.write(out, bytes(b))
    if echo: print('  -> ' + ' '.join('%02X' % x for x in b))

if ctl and ctl['init']: send(ctl['init'], echo=False)
if ctl and ctl['keepalive']:
    def keepalive():
        while True: send(ctl['keepalive'], echo=False); time.sleep(ctl['keepalive_period'])
    threading.Thread(target=keepalive, daemon=True).start()
    print('led-probe: keep-alive every %.0f ms' % (ctl['keepalive_period'] * 1000))

# what the controller sends: one line per message, so a button press shows its channel and note
def listen():
    buf = b''
    with open(dev, 'rb', buffering=0) as f:
        while True:
            data = f.read(64)
            if not data: time.sleep(.01); continue
            buf += data
            while buf:
                s = buf[0]
                if s < 0x80 or s >= 0xF0: buf = buf[1:]; continue
                if len(buf) < 3: break
                kind, ch, d1, d2 = s & 0xF0, s & 0x0F, buf[1], buf[2]; buf = buf[3:]
                what = {0x90: 'note-on ', 0x80: 'note-off', 0xB0: 'cc      '}.get(kind, '%02X      ' % kind)
                print('  <- %s ch %X  %02X %02X   (raw %02X %02X %02X)' % (what, ch, d1, d2, s, d1, d2), flush=True)
threading.Thread(target=listen, daemon=True).start()

def h(x): return int(x, 16)
print('commands: on/off/cc/raw/sweep/alloff/q  (hex numbers; help in the file header)')
while True:
    try: line = input('> ').strip().split()
    except EOFError: break
    if not line: continue
    c, a = line[0].lower(), line[1:]
    try:
        if c == 'q': break
        elif c == 'on': send([0x90 | h(a[0]), h(a[1]), h(a[2]) if len(a) > 2 else 0x7F])
        elif c == 'off': send([0x90 | h(a[0]), h(a[1]), 0])
        elif c == 'cc': send([0xB0 | h(a[0]), h(a[1]), h(a[2])])
        elif c == 'raw': send([h(x) for x in a])
        elif c == 'alloff':
            for n in range(128): send([0x90 | h(a[0]), n, 0], echo=False)
            print('  -> all 128 notes off on channel %X' % h(a[0]))
        elif c == 'sweep':
            ch, lo, hi = h(a[0]), h(a[1]), h(a[2]); vel = h(a[3]) if len(a) > 3 else 0x7F; dt = float(a[4]) if len(a) > 4 else 0.6
            for n in range(lo, hi + 1):
                print('  note %02X on channel %X' % (n, ch), flush=True); send([0x90 | ch, n, vel], echo=False); time.sleep(dt); send([0x90 | ch, n, 0], echo=False)
            print('  sweep done')
        else: print('  ? on/off/cc/raw/sweep/alloff/q')
    except (IndexError, ValueError): print('  ? bad arguments (hex numbers)')
