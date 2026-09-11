"""Resolve the RX3 install layout for the Python tools (mirrors rx3-env.sh).

Nothing is hardcoded to a username: the account that owns this directory is the
account the player runs as, and its home holds the chroot, overlays and logs.
Any value can be overridden by exporting the matching RX3_* environment variable.
"""
import os, pwd

HOME = os.environ.get('RX3_HOME') or os.path.dirname(os.path.abspath(__file__))
try:
    USER = os.environ.get('RX3_USER') or pwd.getpwuid(os.stat(HOME).st_uid).pw_name
    USERHOME = os.environ.get('RX3_USERHOME') or pwd.getpwnam(USER).pw_dir
except KeyError:
    USER = os.environ.get('RX3_USER') or ''
    USERHOME = os.environ.get('RX3_USERHOME') or os.path.dirname(HOME)

ROOT = os.environ.get('RX3_ROOT') or os.path.join(USERHOME, 'rx3-rootfs')
USB = os.environ.get('RX3_USB') or os.path.join(USERHOME, 'rx3-usb')
BINDIR = os.environ.get('RX3_BINDIR') or USERHOME

if os.stat(HOME).st_uid < 1000 and not os.environ.get('RX3_ALLOW_SYSTEM_USER'):
    import sys
    print("WARNING: %s is owned by '%s' (a system account), so the chroot would go to %s.\n"
          "         Fix with:  sudo chown -R $(id -un):$(id -gn) %s" % (HOME, USER, ROOT, HOME),
          file=sys.stderr)
