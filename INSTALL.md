# Installing

Run every command **on the Raspberry Pi**, as your normal login user. Nothing here needs a
particular username: the scripts work out where they live and which account owns them.

## 1. Get the files and the packages

```bash
git clone https://github.com/mutlisensor/Rx3-flx4.git
cd Rx3-flx4/rx3-handoff
chmod +x *.sh
./install.sh deps          # installs every Debian package this needs
```

Do not use `sudo` for the clone or the copy. The files must be owned by your own account, because
that is how the scripts work out where to build the chroot.

You can run everything from the clone, as above, or copy `rx3-handoff` somewhere more permanent
such as `~/rx3-handoff` and work there. Either is fine.

If you prefer to install the packages yourself instead of `./install.sh deps`:

```bash
sudo apt update
sudo apt install -y fuse-overlayfs uhubctl exfatprogs alsa-utils python3-pil python3-cryptography \
                    gcc build-essential gcc-arm-linux-gnueabi rsync p7zip-full
```

Note that two of these are not named after the command they provide: the `arm-linux-gnueabi-gcc`
compiler comes from **`gcc-arm-linux-gnueabi`**, and `7z` comes from **`p7zip-full`**.

## 2. Recover the firmware

The firmware is not in this repository and never will be — you supply it. These two scripts
download the official packages, decrypt them, and unpack what the player needs.

```bash
python3 recover-firmware.py     # downloads the official firmware + GPL source, decrypts, verifies
python3 extract_cramfs.py       # unpacks the root filesystem into extracted/runtime-files
```

**Run them in that order, and run both.** `recover-firmware.py` alone is not enough:
`extract_cramfs.py` is what produces `extracted/runtime-files/` and `runtime-symlinks.json`,
and the chroot is built from those.

After this you should have:

```
extracted/player/pdj/rbp        the original player binary (hash-verified)
extracted/gui/                  the GUI resources
extracted/runtime-files/        the root filesystem
runtime-symlinks.json           the symlink map
```

## 3. Check you are ready

```bash
./install.sh doctor
```

This changes nothing. It lists each prerequisite as `ok` or `MISS` and tells you which script to
run for anything missing. Fix every `MISS` before going on.

## 4. Build the chroot

```bash
./build-rootfs.sh
```

This assembles the chroot, patches the player, and compiles the preload shim. It stops with an
explicit message if step 2 was incomplete. Expect roughly 6.5 GB in `~/rx3-rootfs` and a minute of
work. You do not run `patch-player.py` yourself — `build-rootfs.sh` calls it at the right moment,
after copying the recovered player into place.

## 5. Install the host side

```bash
./install.sh
```

Builds the two helper binaries, generates the udev rules and the systemd unit with your real paths,
keeps PipeWire off the sound cards, and sets the Pi to boot without a desktop so the player owns the
framebuffer.

## 6. Run it

```bash
sudo systemctl enable --now rx3
journalctl -u rx3 -f
```

Plug in the DDJ-FLX4, an HDMI display, and a USB stick with a rekordbox export. The RX3 interface
appears on the display. A USB mouse works as a pointer until you attach a touchscreen.

---

# Troubleshooting

**"I ran recover-firmware.py and extract_cramfs.py — what now?"**
`./install.sh doctor`, then `./build-rootfs.sh`, then `./install.sh`. In that order.

**"extraction incomplete: runtime-files exists but runtime-symlinks.json does not"**
`extract_cramfs.py` was interrupted or it failed. It writes the symlink map as its very last step,
so that pair of symptoms means it never reached the end. Run it again:

```bash
python3 extract_cramfs.py
```

It must finish with `Extraction complete.` If it does not, the run did not count.

**`PermissionError: ... extracted/runtime-files/bin/bashbug`** (or any other file there)
A bug in older copies of `extract_cramfs.py`: firmware files are written with their original
read-only modes, so a second run could not overwrite them, and every retry after an interruption
failed at the same place. Pull the latest version and run it again. If you would rather not pull,
`rm -rf extracted/runtime-files` first and the old script will get through.

**"A script made `~/rx3-rootfs` but there is nothing inside it."**
`extracted/runtime-files/` was missing or empty when `build-rootfs.sh` ran, so there was nothing to
copy in. Run `python3 extract_cramfs.py`, confirm `extracted/runtime-files/` has content, then run
`./build-rootfs.sh` again. Current versions of the script refuse to start in this situation instead
of leaving you an empty directory.

**"patch-player.py says the path is wrong."**
Do not run it directly. It expects `pi-runtime/rbp`, which `build-rootfs.sh` puts there by copying
the recovered player just before calling it. Run `./build-rootfs.sh` instead.

**"Everything is hardcoded to /home/rx3 or /home/pompu_5."**
Fixed. Paths are resolved at runtime by `rx3-env.sh` and `rx3_env.py` from the location of the
scripts and the account that owns them, so any username works. If you cloned before this change,
pull again. The only remaining `/home/pompu_5` references are in `rx3-handoff/legacy/`, which is
dead prototype code you should ignore — see the README in that directory.

**Overriding the layout.** Export any of these before running anything to place things elsewhere:

| Variable | Default |
|---|---|
| `RX3_HOME` | the directory the scripts are in |
| `RX3_USER` | the account that owns `RX3_HOME` |
| `RX3_ROOT` | `~/rx3-rootfs` |
| `RX3_USB` | `~/rx3-usb` |
| `RX3_BINDIR` | `~` (helper binaries) |
| `RX3_LOGDIR` | `~` (`rx3-*.log`) |

**"WARNING: ... is owned by a system account"**
You copied the files with `sudo`, so the directory belongs to root and the chroot would be built in
root's home instead of yours. Fix it with:

```bash
sudo chown -R $(id -un):$(id -gn) ~/rx3-handoff
```

**Under-voltage warnings or the FLX4 not enumerating.** Use the official 27 W supply or a powered
USB hub. The controller draws enough to brown out a Pi 5 on an underpowered supply.

**The deeper reference.** `rx3-handoff/PI-SETUP-NOTES.md` documents the key codes, audio routing,
USB semantics and every non-obvious trap found while building this.
