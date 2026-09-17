#!/usr/bin/env python3
"""Send RX3 firmware keys / USB mount events into the running chroot player.
usage: rx3-control.py mount [usb1|usb2] [chroot-path]
       rx3-control.py <keyname|0xHEX> [channel 0/1/2] [analog 0..1]
       rx3-control.py rotary <+N|-N>            (browse selector)
       rx3-control.py press <keyname> [channel]   (press only)
       rx3-control.py release <keyname> [channel] (release only)
       rx3-control.py hold <keyname> [channel]    (press + "long-pressed" + release)
       rx3-control.py utility                     (= hold menu: the UTILITY screen)
       rx3-control.py state                       (what the control shim publishes about each deck)
"""
import os,struct,time,sys
import rx3_env
root=rx3_env.ROOT
keys={'usb1':0x209,'usb2':0x20a,'browse':0x202,'source':0x201,'menu':0x206,'info':0x20b,'search':0x205,'playlist':0x204,
 'taglist':0x203,'shortcut':0x210,'keyboard':0x216,
 'play':0x4101,'cue':0x4102,'shift':0x4103,'vinyl':0x4104,'sync':0x4112,'master':0x4111,'load':0x4311,'enter':0x420c,'back':0x420d,
 'hotcue':0x4113,'beatloop':0x4114,'beatjump':0x4116,'loopin':0x410c,'loopout':0x410d,'reloop':0x410e,'slip':0x4110,'quantize':0x410b,
 'headphones':0x4406,'hpmix':0x4405,'hpcue':0x5020,'masterlv':0x4403,'cross':0x6017,'fader':0x501e,'trim':0x5019,'eqh':0x501a,'eqm':0x501b,'eql':0x501c,'color':0x509d,
 'tempo':0x4109,'temporange':0x4107,'mastertempo':0x4108,'jog':0x4305,'jogtouch':0x4306,'trackfwd':0x4214,'trackrev':0x4215,'deckselect':0x4212,'usbstop':0x8002}
for i in range(8): keys['pad%d'%(i+1)]=0x4117+i
def key_of(name): return keys[name] if name in keys else int(name,0)
a=sys.argv[1:]
if not a: print(__doc__); sys.exit(2)
def fifo(name,msg):
    f=os.open(root+'/proc/'+name,os.O_RDWR|os.O_NONBLOCK); os.write(f,msg.encode()); os.close(f)
if a[0] in ('mount','umount','remount'):
    # Firmware udev protocol (12-usb-memory-auto-mount.rules / 75-usb-caution.rules): plug = "connect" on udev_usbctnN then
    # "mount <path>" on udev_usbN; unplug = "umount <path>" then "disconnect". After USB STOP the firmware waits for that.
    port=a[1] if len(a)>1 else 'usb1'; n=port[-1]; path=a[2] if len(a)>2 else '/media/%s/sda1'%port
    if a[0] in ('umount','remount'): fifo('udev_'+port,'umount '+path); time.sleep(1); fifo('udev_usbctn'+n,'disconnect'); time.sleep(2)
    if a[0] in ('mount','remount'): fifo('udev_usbctn'+n,'connect'); time.sleep(1.5); fifo('udev_'+port,'mount '+path)
    sys.exit(0)
if a[0]=='state':
    # struct ui_state (pi-controls.h): the shim's deck flags and sequence counter sit at byte 52
    with open(root+'/dev/rx3-ui-state','rb') as st: st.seek(52); d1,d2,seq=struct.unpack('<III',st.read(12))
    names=[(1,'playing'),(2,'master tempo'),(4,'quantize'),(8,'headphone cue')]
    for d,f in ((1,d1),(2,d2)): print('deck %d: %s'%(d,', '.join(n for b,n in names if f&b) or '-'))
    print('sequence %d (run twice: if it does not move, the shim is not publishing)'%seq); sys.exit(0)
if a[0]=='query':
    f=os.open(root+'/dev/rx3-control',os.O_RDWR|os.O_NONBLOCK); os.write(f,struct.pack('<iiiifi',0xFFFF,0,0,0,0.0,0)); os.close(f); time.sleep(0.3)
    print(open(root+'/tmp/rx3-query.txt').read(),end=''); sys.exit(0)
f=os.open(root+'/dev/rx3-control',os.O_RDWR|os.O_NONBLOCK)
def send(k,op,ch=0,val=0,analog=0.0): os.write(f,struct.pack('<iiiifi',k,op,ch,val,analog,0))
if a[0]=='rotary': send(0x420c,4,0,int(a[1])); sys.exit(0)
if a[0] in('press','release'): send(key_of(a[1]),0 if a[0]=='press' else 2,int(a[2]) if len(a)>2 else 0); sys.exit(0)
# Held key: the panel sends the press, then operation 1 ("long-pressed"), then the release. Same as the on-screen UTILITY.
# USB STOP is excluded on purpose: the firmware times that hold itself and a bare code 1 poisons the slot (PI-SETUP-NOTES).
if a[0] in('hold','utility'):
    if a[0]=='hold' and len(a)<2: print(__doc__); sys.exit(2)
    k=keys['menu'] if a[0]=='utility' else key_of(a[1]); ch=int(a[2]) if a[0]=='hold' and len(a)>2 else 0
    if k==keys['usbstop']: print('use "usbstop <slot>": the firmware times that hold itself',file=sys.stderr); sys.exit(2)
    send(k,0,ch); time.sleep(.1); send(k,1,ch); time.sleep(.1); send(k,2,ch); sys.exit(0)
k=key_of(a[0]); ch=int(a[1]) if len(a)>1 else 0
if a[0]=='usbstop': slot=int(a[1]) if len(a)>1 else 1; send(k,0,slot); time.sleep(2.5); send(k,2,slot); sys.exit(0)   # USB STOP <slot>: the firmware times the hold itself (~1.9 s after press); never send code 1 without a press
if len(a)>2: send(k,5 if a[0]=='tempo' else 4,ch,0,float(a[2]))   # tempo slider needs operation 5
else: send(k,0,ch); time.sleep(.1); send(k,2,ch)
