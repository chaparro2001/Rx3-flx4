import sys,wave,struct,math
w=wave.open(sys.argv[1]); n=w.getnchannels(); fr=w.readframes(w.getnframes()); k=len(fr)//(2*n)
s=struct.unpack('<%dh'%(k*n),fr[:k*n*2])
for c in range(n):
    ch=s[c::n]; rms=math.sqrt(sum(v*v for v in ch)/max(1,len(ch))); print('ch%d rms=%.0f peak=%d'%(c+1,rms,max(abs(v) for v in ch)))
