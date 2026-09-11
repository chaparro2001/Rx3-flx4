import sys; from PIL import Image
w,h,bpp=int(sys.argv[2]),int(sys.argv[3]),int(sys.argv[4]); data=open('/dev/fb0','rb').read(w*h*bpp//8)
im=Image.frombuffer('RGB',(w,h),data,'raw','BGR;16' if bpp==16 else 'BGRX',0,1); im.resize((w//4,h//4)).save(sys.argv[1]); print('saved',sys.argv[1])
