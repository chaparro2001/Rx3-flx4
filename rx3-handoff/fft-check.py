import sys,wave,struct,math
w=wave.open(sys.argv[1]); n=w.getnchannels(); fr=w.readframes(w.getnframes()); k=len(fr)//(2*n)
s=struct.unpack('<%dh'%(k*n),fr[:k*n*2])
for c in range(n):
    ch=s[c::n][:8192]; best=(0,0)
    for f in range(100,1200,10):
        re=sum(v*math.cos(2*math.pi*f*i/44100) for i,v in enumerate(ch)); im=sum(v*math.sin(2*math.pi*f*i/44100) for i,v in enumerate(ch))
        m=math.hypot(re,im); best=max(best,(m,f))
    print('ch%d dominant ~%d Hz'%(c+1,best[1]))
