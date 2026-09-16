#!/usr/bin/env python3
"""Known displays and how to find the one that is connected.

A profile is what the presenter and the touch bridge need to know about a panel beyond what the framebuffer
reports: which way to turn the picture (RX3_ROTATE), how much of it the on-screen controls take (RX3_UI,
RX3_UI_SCALE) and how the touch controller's axes relate to the panel (RX3_TOUCH). All of it travels as
environment variables, so supporting a new panel is one more entry here, with nothing to rebuild.

Detection reads /sys/class/graphics/fbN (name and virtual_size); a DSI panel is preferred over HDMI when both
are connected, then the first profile whose name/size rules match wins. RX3_DISPLAY=<id> in rx3.conf forces a
profile. Any RX3_* value already set in the environment is kept, which is how rx3.conf overrides one setting
of a profile (e.g. RX3_TOUCH=invy) without copying the rest.

CLI:  displays.py list     -> every profile, one per line
      displays.py detect   -> "id=hdmi1080 fb=/dev/fb0 name=vc4drmfb size=1920x1080" for the display in use (exit 1: none)
      displays.py env      -> shell assignments for rx3-env.sh to eval
      RX3_SYSFS points detection at a fake /sys/class/graphics tree, for tests.
"""
import glob, os, shlex, sys

# Ordered: the first profile whose rules match the framebuffer is used, so the generic HDMI entry goes last.
#   fbname  substring the framebuffer's name must contain (None: any)
#   size    exact (width, height) the framebuffer must report (None: any)
#   rotate  '' lets the binaries decide (portrait panels 90, landscape 0)
#   ui      full = strip + slider bars, strip = buttons only, none = the firmware picture alone
#   ui_scale  chrome scale, '' = fit the original 1920x1200 design to the panel
#   touch   comma-separated swap / invx / invy for controllers whose axes do not follow the panel
DISPLAYS = [
    ('td2', dict(name='Raspberry Pi Touch Display 2 (7" DSI, 720x1280 portrait)',
                 fbname='dsi', size=(720, 1280), rotate='90', ui='full', ui_scale='', touch='')),
    ('hdmi1080', dict(name='HDMI 1920x1080, touch or not',
                 fbname=None, size=(1920, 1080), rotate='0', ui='full', ui_scale='', touch='')),
    ('hdmi', dict(name='HDMI, any other resolution',
                 fbname=None, size=None, rotate='', ui='full', ui_scale='', touch='')),
    # Never auto-selected: RX3_DISPLAY=custom plus RX3_ROTATE / RX3_UI / RX3_UI_SCALE / RX3_TOUCH in rx3.conf.
    ('custom', dict(name='whatever rx3.conf says', fbname=None, size=None, auto=False,
                 rotate='', ui='', ui_scale='', touch='')),
]
PROFILES = dict(DISPLAYS)
VARS = ('rotate', 'ui', 'ui_scale', 'touch')      # -> RX3_ROTATE, RX3_UI, RX3_UI_SCALE, RX3_TOUCH

def _read(p):
    try: return open(p).read().strip()
    except OSError: return ''

def framebuffers(sysfs='/sys/class/graphics'):
    """Connected framebuffers as [(device, name, width, height)], DSI panels first, then in fb order."""
    fbs = []
    for d in sorted(glob.glob(os.path.join(sysfs, 'fb[0-9]*'))):
        name = _read(os.path.join(d, 'name'))
        try: w, h = (int(v) for v in _read(os.path.join(d, 'virtual_size')).split(','))
        except ValueError: w = h = 0
        fbs.append(('/dev/' + os.path.basename(d), name, w, h))
    return sorted(fbs, key=lambda f: (0 if 'dsi' in f[1].lower() else 1, f[0]))

def profile_for(fb):
    dev, name, w, h = fb
    for pid, p in DISPLAYS:
        if not p.get('auto', True): continue
        if p['fbname'] and p['fbname'] not in name.lower(): continue
        if p['size'] and p['size'] != (w, h): continue
        return pid
    return None

def detect(sysfs='/sys/class/graphics'):
    """(profile id, framebuffer tuple) for the display in use; (None, None) when nothing is connected."""
    fbs = framebuffers(sysfs)
    forced_fb = os.environ.get('RX3_FB', '')
    if forced_fb:
        fbs = [f for f in fbs if f[0] == forced_fb] or [(forced_fb, '', 0, 0)]
    forced = os.environ.get('RX3_DISPLAY', '')
    if forced:
        if forced not in PROFILES:
            print('displays.py: RX3_DISPLAY=%s is not a known profile (%s)' % (forced, ', '.join(PROFILES)), file=sys.stderr)
            return None, fbs[0] if fbs else None
        return forced, (fbs[0] if fbs else None)
    for fb in fbs:
        pid = profile_for(fb)
        if pid: return pid, fb
    return None, None

if __name__ == '__main__':
    a = sys.argv[1:]
    sysfs = os.environ.get('RX3_SYSFS', '/sys/class/graphics')
    if a[:1] == ['list']:
        for pid, p in DISPLAYS:
            rule = ' '.join(x for x in (p['fbname'] and 'fb~' + p['fbname'], p['size'] and '%dx%d' % p['size']) if x) or ('-' if p.get('auto', True) else 'manual')
            print('%-9s %-12s %s' % (pid, rule, p['name']))
        sys.exit(0)
    if a[:1] == ['detect']:
        pid, fb = detect(sysfs)
        if not pid or not fb: sys.exit(1)
        print('id=%s fb=%s name=%s size=%dx%d' % (pid, fb[0], fb[1], fb[2], fb[3])); sys.exit(0)
    if a[:1] == ['env']:
        pid, fb = detect(sysfs)
        p = PROFILES.get(pid, {})
        out = {'RX3_DISPLAY': pid or '', 'RX3_FB': (fb[0] if fb else '')}
        for v in VARS: out['RX3_' + v.upper()] = p.get(v, '')
        for k, v in out.items():
            cur = os.environ.get(k, '')
            print('%s=%s' % (k, shlex.quote(cur if cur else v)))
        sys.exit(0)
    print(__doc__); sys.exit(2)
