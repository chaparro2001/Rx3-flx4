#!/usr/bin/env python3
import sys
from PIL import Image
src,dst=sys.argv[1],sys.argv[2]
w,h=(int(sys.argv[3]),int(sys.argv[4])) if len(sys.argv)>4 else (1280,800)
data=open(src,'rb').read(w*h*4)
Image.frombuffer('RGB',(w,h),data,'raw','BGRX',0,1).save(dst)
print(dst, 'nonzero bytes:', sum(1 for b in data[::97] if b))
