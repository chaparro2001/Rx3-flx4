# Legacy files — do not run these

These are from the first prototype machine (a different Pi, user `pompu_5`, host `pflx.local`).
They contain hardcoded paths for that machine and are superseded by the scripts one level up.

They are kept only as historical reference for how the port was bootstrapped:

- `start-rx3.sh` — superseded by `../rx3-start.sh`
- `pi-control.py` — superseded by `../rx3-control.py`
- `pi-mixer-init.py` — the mixer init is now done inside `../control-shim.c`
- `touch-replay.py` — superseded by `../rx3-tap.py`
- `SESSION-STATE.md` — notes from the prototype phase

If you are installing, ignore this directory entirely and follow the root `README.md`.
