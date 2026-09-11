import os,struct,time
f=os.open('/home/pompu_5/rx3-rootfs/dev/rx3-control',os.O_RDWR|os.O_NONBLOCK)
def send(k,ch,val,raw=0):
 os.write(f,struct.pack('<iiiifi',k,4,ch,raw,val,0));time.sleep(.05)
for ch in (1,2):
 for k,v in ((0x5019,.5),(0x501a,.5),(0x501b,.5),(0x501c,.5),(0x509d,.5),(0x501e,1.)):
  send(k,ch,v)
send(0x6017,0,.5)
send(0x4403,0,.6)
os.close(f)
