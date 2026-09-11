#!/bin/bash
# Headless smoke test: USB1 -> Contents -> first album -> load deck 1 -> play. Verifies each step from the virtual screen.
R=/home/rx3/rx3-rootfs; H=/home/rx3/rx3-handoff; C="python3 $H/rx3-control.py"
region(){ python3 - "$R/dev/fb0" "$1" "$2" "$3" "$4" <<'PY'
import sys,hashlib
fb,x0,y0,x1,y1=sys.argv[1],*map(int,sys.argv[2:6]); d=open(fb,'rb').read(); h=hashlib.md5()
for y in range(y0,y1): h.update(d[(y*1280+x0)*4:(y*1280+x1)*4])
print(h.hexdigest()[:8])
PY
}
title(){ region 100 0 700 50; }
deck1(){ region 100 715 520 795; }
samples(){ local s=""; for i in 1 2 3; do s="$s $($1)"; sleep 0.7; done; echo "$s"; }
press_until_title_changes(){ # <label> <key...>
  local label=$1; shift; local t0=$(title)
  for try in 1 2 3 4; do "$@"; sleep 3; [ "$(title)" != "$t0" ] && { echo "$label: ok (try $try)"; return 0; }; done
  echo "$label: title unchanged"; return 1
}
$C mount usb1; sleep 6
for i in 1 2 3; do $C back; sleep 2; done            # climb to the root folder list if already browsing
press_until_title_changes usb1 $C usb1 || echo "usb1: already in folder view"
press_until_title_changes enter-contents $C enter || exit 1
press_until_title_changes enter-album $C enter || exit 1
before=$(samples deck1)
for try in 1 2 3; do
  $C load 1; sleep 4; after=$(samples deck1); hit=0
  for a in $after; do case " $before " in *" $a "*) hit=1;; esac; done
  [ $hit = 0 ] && { echo "load1: ok (try $try)"; break; }
  echo "load1: deck unchanged (try $try)"
done
[ $hit = 0 ] || exit 1
$C play 1; sleep 2; echo "play1: sent"
python3 $H/fb2png.py $R/dev/fb0 /home/rx3/rx3-test-play.png >/dev/null
