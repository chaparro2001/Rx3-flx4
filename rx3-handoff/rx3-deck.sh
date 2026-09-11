#!/bin/bash
# Deck state helpers: `rx3-deck.sh state`, `rx3-deck.sh play <1|2>`, `rx3-deck.sh pause <1|2>` (verified from the screen).
. "$(dirname "$(readlink -f "$0")")/rx3-env.sh"
R=$RX3_ROOT; H=$RX3_HOME; C="python3 $H/rx3-control.py"
region(){ python3 - "$R/dev/fb0" "$@" <<'PY'
import sys,hashlib
fb,x0,y0,x1,y1=sys.argv[1],*map(int,sys.argv[2:6]); d=open(fb,'rb').read(); h=hashlib.md5()
for y in range(y0,y1): h.update(d[(y*1280+x0)*4:(y*1280+x1)*4])
print(h.hexdigest()[:8])
PY
}
time_region(){ [ "$1" = 1 ] && region 190 655 390 710 || region 830 655 1030 710; }
moving(){ local a=$(time_region $1); sleep 1.2; [ "$(time_region $1)" != "$a" ]; }
case "$1" in
  state) echo "deck1 $(moving 1 && echo MOVING || echo STATIC) deck2 $(moving 2 && echo MOVING || echo STATIC)";;
  play)  for i in 1 2 3 4; do moving $2 && { echo "deck $2 playing"; exit 0; }; $C trackrev $2; sleep 0.8; $C play $2; sleep 1.5; done; echo "deck $2: could not start"; exit 1;;
  pause) for i in 1 2 3 4; do moving $2 || { echo "deck $2 paused"; exit 0; }; $C play $2; sleep 1.5; done; echo "deck $2: could not pause"; exit 1;;
esac
