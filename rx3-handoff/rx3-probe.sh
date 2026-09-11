#!/bin/bash
# Deck/routing probe helpers: playing <1|2> reports whether a deck's time display is changing; peaks prints the meter.
. "$(dirname "$(readlink -f "$0")")/rx3-env.sh"
R=$RX3_ROOT; H=$RX3_HOME; C="python3 $H/rx3-control.py"
region(){ python3 - "$R/dev/fb0" "$@" <<'PY'
import sys,hashlib
fb,x0,y0,x1,y1=sys.argv[1],*map(int,sys.argv[2:6]); d=open(fb,'rb').read(); h=hashlib.md5()
for y in range(y0,y1): h.update(d[(y*1280+x0)*4:(y*1280+x1)*4])
print(h.hexdigest()[:8])
PY
}
playing(){ local x0=$([ "$1" = 1 ] && echo 190 || echo 830); local a=$(region $x0 655 $((x0+200)) 710); sleep 1.1; local b=$(region $x0 655 $((x0+200)) 710); [ "$a" != "$b" ]; }
ensure(){ # ensure <deck> <play|pause>
  for t in 1 2 3; do if playing $1; then [ "$2" = play ] && return 0; else [ "$2" = pause ] && return 0; fi; $C play $1; sleep 1.5; done; echo "deck $1: could not reach $2"; }
peaks(){ sleep 2.5; tail -2 $R/tmp/rx3-audio-peaks | tr "\n" " "; echo; }
sw(){ python3 - "$R" "$@" <<'PY'
import os,struct,sys
r,key,ch,val=sys.argv[1],int(sys.argv[2],0),int(sys.argv[3]),int(sys.argv[4])
f=os.open(r+"/dev/rx3-control",os.O_RDWR|os.O_NONBLOCK); os.write(f,struct.pack("<iiiifi",key,4,ch,val,float(val),0)); os.close(f)
PY
}
case "$1" in
  routing)
    ensure 1 play; ensure 2 pause; echo "deck1 only: $(peaks)"
    ensure 1 pause; ensure 2 play; echo "deck2 only: $(peaks)"
    for v in 0 1 2 3; do sw 0x501f 2 $v; echo "deck2 only, ch2 source=$v: $(peaks)"; done; sw 0x501f 2 0
    $C fader 2 1.0; echo "deck2 only, ch2 fader=1.0: $(peaks)"; $C fader 1 0.0; echo "deck2 only, ch1 fader=0: $(peaks)"; $C fader 1 1.0
    ensure 2 pause; ensure 1 play
    $C cross 0 1.0; echo "deck1 only, cross=1.0(B): $(peaks)"; $C cross 0 0.0; echo "deck1 only, cross=0.0(A): $(peaks)"; $C cross 0 0.5
    $C fader 2 0.0; echo "deck1 only, ch2 fader=0: $(peaks)"; $C fader 2 1.0; echo "deck1 only, ch2 fader=1: $(peaks)"
    ensure 2 play ;;
  *) "$@" ;;
esac
