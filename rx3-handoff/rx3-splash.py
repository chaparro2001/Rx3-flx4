#!/usr/bin/env python3
"""Start-up picture: shown on the framebuffer from the moment rx3-start.sh begins until the presenter takes over.

The image is the panel seen the right way up (landscape on the Duet: 1920x1200), any size or format Pillow reads. It is
fitted whole, centred, with the rest filled in the colour of its top-left pixel, then turned by RX3_ROTATE exactly as
the presenter turns its picture (pi-controls.h ui_to_panel) and written in the framebuffer's own pixel format.
The converted frame is cached, so only the first start after changing the image or the display pays for it.

Which image: RX3_SPLASH, else splash.png / splash.jpg next to this file, else a plain generated one ("XDJ-RX3",
"Starting..."). Your own image stays out of git (.gitignore): pictures are often someone's trademark.

usage: rx3-splash.py [/dev/fbN]      (rx3-start.sh runs it as root; RX3_FB and RX3_ROTATE come from rx3-env.sh)"""
import hashlib, os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
BG = (0x10, 0x18, 0x20)          # the strip's background, for the generated picture

def read(path, default=''):
    try: return open(path).read().strip()
    except OSError: return default

def generated(w, h):
    img = Image.new('RGB', (w, h), BG)
    d = ImageDraw.Draw(img)
    for size, text, y, colour in ((h // 7, 'XDJ-RX3', h * 0.42, (255, 255, 255)), (h // 22, 'Starting...', h * 0.62, (0xa9, 0xb6, 0xc1))):
        try: font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', size)
        except OSError:
            try: font = ImageFont.load_default(size)        # Pillow >= 10.1 scales its built-in font
            except TypeError: font = ImageFont.load_default()
        d.text((w / 2, y), text, fill=colour, font=font, anchor='mm')
    return img

def main():
    fb = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('RX3_FB') or '/dev/fb0'
    sysfs = os.path.join(os.environ.get('RX3_SYSFS', '/sys/class/graphics'), os.path.basename(fb))   # RX3_SYSFS: tests
    try: W, H = (int(v) for v in read(sysfs + '/virtual_size').split(','))
    except ValueError: sys.exit('rx3-splash: cannot read the size of %s' % fb)
    bpp = int(read(sysfs + '/bits_per_pixel', '32')); stride = int(read(sysfs + '/stride', '0')) or W * bpp // 8
    if bpp not in (16, 32): sys.exit('rx3-splash: %d bpp framebuffer not supported' % bpp)
    rot = os.environ.get('RX3_ROTATE', '')
    rot = int(rot) if rot in ('0', '90', '180', '270') else (90 if H > W else 0)   # same default as the presenter
    uw, uh = (H, W) if rot in (90, 270) else (W, H)                               # the panel the right way up

    src = os.environ.get('RX3_SPLASH') or next((p for p in (os.path.join(HERE, n) for n in ('splash.png', 'splash.jpg', 'splash.jpeg')) if os.path.exists(p)), '')
    st = os.stat(src) if src else None
    key = hashlib.sha1(repr((src, st and (st.st_size, st.st_mtime_ns), W, H, bpp, stride, rot)).encode()).hexdigest()[:16]
    cache = os.path.join(HERE, 'build', 'rx3-splash-%s.raw' % key)                 # build/ is git-ignored
    try:
        frame = open(cache, 'rb').read()
    except OSError:
        if src:
            pic = Image.open(src).convert('RGB')
            img = Image.new('RGB', (uw, uh), pic.getpixel((0, 0)))
            s = min(uw / pic.width, uh / pic.height)
            pic = pic.resize((max(1, round(pic.width * s)), max(1, round(pic.height * s))), Image.LANCZOS)
            img.paste(pic, ((uw - pic.width) // 2, (uh - pic.height) // 2))
        else:
            img = generated(uw, uh)
        # ui_to_panel: 90 puts the UI's left edge along the panel's top (clockwise), 270 the other way.
        img = {90: lambda i: i.transpose(Image.ROTATE_270), 180: lambda i: i.transpose(Image.ROTATE_180),
               270: lambda i: i.transpose(Image.ROTATE_90)}.get(rot, lambda i: i)(img)
        raw = img.tobytes('raw', 'BGRX' if bpp == 32 else 'BGR;16')
        row = W * bpp // 8
        frame = raw if stride == row else b''.join(raw[y * row:(y + 1) * row].ljust(stride, b'\0') for y in range(H))
        try:
            os.makedirs(os.path.dirname(cache), exist_ok=True)
            for old in os.listdir(os.path.dirname(cache)):
                if old.startswith('rx3-splash-'): os.remove(os.path.join(os.path.dirname(cache), old))
            open(cache, 'wb').write(frame)
        except OSError: pass
    with open(fb, 'r+b') as f: f.write(frame)

if __name__ == '__main__':
    main()
