#include <asm/ioctl.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <errno.h>
static unsigned yoffset;
static void clear(void *p,unsigned n){unsigned char *q=p;while(n--)*q++=0;}
int ioctl(int fd,unsigned long request,...){
 va_list ap;va_start(ap,request);void *arg=va_arg(ap,void*);va_end(ap);
 if(((request>>8)&255)==0x70){if((request&0x80000000)&&arg){unsigned n=(request>>16)&0x3fff;if(n<=64)clear(arg,n);}return 0;}
 if(request==0x80046b00){*(unsigned*)arg=3;return 0;}
 if(request==0x80026b01){*(unsigned*)arg=3900;return 0;}
 if(request==0x40046b00||request==0x40026b01)return 0;
 if(request==FBIOGET_FSCREENINFO){struct fb_fix_screeninfo *f=arg;clear(f,sizeof(*f));f->id[0]='R';f->id[1]='X';f->id[2]='3';f->smem_len=1280*800*4;f->type=FB_TYPE_PACKED_PIXELS;f->visual=FB_VISUAL_TRUECOLOR;f->line_length=1280*4;return 0;}
 if(request==FBIOGET_VSCREENINFO){struct fb_var_screeninfo *v=arg;clear(v,sizeof(*v));v->xres=v->xres_virtual=1280;v->yres=v->yres_virtual=800;v->bits_per_pixel=32;v->red.offset=16;v->red.length=8;v->green.offset=8;v->green.length=8;v->blue.length=8;v->height=135;v->width=216;v->pixclock=20000;v->left_margin=40;v->right_margin=40;v->upper_margin=10;v->lower_margin=10;v->hsync_len=20;v->vsync_len=3;return 0;}
 if(request==FBIOPUT_VSCREENINFO){struct fb_var_screeninfo *v=arg;if(v->bits_per_pixel!=32){errno=EINVAL;return -1;}return 0;}
 if(request==FBIOPAN_DISPLAY||request==FBIOPUTCMAP||request==FBIOGETCMAP||request==FBIOBLANK||request==FBIO_WAITFORVSYNC)return 0;
 register long r0 asm("r0")=fd;register long r1 asm("r1")=request;register void *r2 asm("r2")=arg;register long r7 asm("r7")=54;
 asm volatile("svc 0":"+r"(r0):"r"(r1),"r"(r2),"r"(r7):"memory");
 if(r0<0 && r0>=-4095){errno=-r0;return -1;}return r0;
}
extern void *dlsym(void*,const char*);
extern void *dlvsym(void*,const char*,const char*);
extern int write(int,const void*,unsigned);
static void logtext(const char *s){unsigned n=0;while(s[n])n++;write(2,s,n);}
static int logresult(const char *s,int v){char h[12]=" 00000000\n";unsigned u=v;for(int i=8;i>0;i--){h[i]="0123456789abcdef"[u&15];u>>=4;}logtext(s);write(2,h,10);return v;}
void *dlopen(const char *name,int flags){static void *(*real)(const char*,int);if(!real)real=dlsym((void*)-1,"dlopen");return real(name,flags&~8);}
/* USB STOP: the firmware unmounts /media/usbN/<part> itself (do_umount0, then do_umount1 every 10 s, then E-8307)
   but runs unprivileged in the chroot, so hand the unmount to the root helper rx3-priv.sh over /dev/rx3-priv and
   wait until the mountpoint is gone. */
extern int open(const char*,int,...);extern int close(int);extern int usleep(unsigned);
/* glibc 2.13 exports no stat64 symbol (only __xstat64), so use the raw ARM EABI stat64 syscall (195); st_dev is the first u64. */
static long sys_stat64(const char *p,void *b){register long r0 asm("r0")=(long)p;register void *r1 asm("r1")=b;register long r7 asm("r7")=195;asm volatile("svc 0":"+r"(r0):"r"(r1),"r"(r7):"memory");return r0;}
static int is_media(const char *t){const char *m="/media/usb";for(int i=0;m[i];i++)if(t[i]!=m[i])return 0;return 1;}
static int mounted(const char *t){unsigned long long a[32],b[32];char parent[160];unsigned n=0,last=0;while(t[n]&&n<158){parent[n]=t[n];if(t[n]=='/')last=n;n++;}parent[last?last:1]=0;
 if(sys_stat64(t,a)<0)return 0;if(sys_stat64(parent,b)<0)return 1;return a[0]!=b[0];}
static int priv_umount(const char *t){char req[176];unsigned n=0;const char *p="umount ";while(*p)req[n++]=*p++;for(unsigned i=0;t[i]&&n<170;i++)req[n++]=t[i];req[n++]='\n';
 if(!mounted(t))return 0;int fd=open("/dev/rx3-priv",1|04000);if(fd<0)return -1;write(fd,req,n);close(fd);
 for(int i=0;i<60;i++){if(!mounted(t)){logtext("RX3 priv umount done\n");return 0;}usleep(100000);}return -1;}
int umount2(const char *target,int flags){static int(*real)(const char*,int);if(!real)real=dlsym((void*)-1,"umount2");
 if(target&&is_media(target)&&priv_umount(target)==0)return 0;return real(target,flags);}
int umount(const char *target){return umount2(target,0);}
static void *peak_pcm[4];static const char *peak_name[4];static int peak_val[4],peak_n[4];
static void peak_register(void *pcm,const char *name){for(int i=0;i<4;i++){if(peak_pcm[i]==pcm||!peak_pcm[i]){peak_pcm[i]=pcm;peak_name[i]=name;return;}}}
int snd_pcm_open(void **pcm,const char *name,int stream,int mode){
 static int (*real)(void**,const char*,int,int);if(!real)real=dlsym((void*)-1,"snd_pcm_open");
 unsigned n=0;while(name[n])n++;const char *target=stream?"null":(n&&name[n-1]=='0'?"rx3out":(n&&name[n-1]=='1'?"rx3cue":"null"));
 logtext("PCM open ");logtext(name);int r=logresult(stream?" capture":" playback",real(pcm,target,stream,mode));
 if(r>=0&&!stream)peak_register(*pcm,target);return r;
}
#define WRAP2(name) int name(void*a,void*b){static int(*real)(void*,void*);if(!real)real=dlsym((void*)-1,#name);return logresult(#name,real(a,b));}
#define WRAP3(name) int name(void*a,void*b,int c){static int(*real)(void*,void*,int);if(!real)real=dlsym((void*)-1,#name);int v=real(a,b,c);logresult(#name " value",c);return logresult(#name,v);}
WRAP2(snd_pcm_hw_params)
WRAP2(snd_pcm_hw_params_any)
int snd_pcm_hw_params_set_access(void*a,void*b,int c){static int(*real)(void*,void*,int);if(!real)real=dlsym((void*)-1,"snd_pcm_hw_params_set_access");return logresult("interleaved access",real(a,b,c==4?3:c));}
static int peak_fmt[4];static void peak_setfmt(void *pcm,int f){for(int i=0;i<4;i++)if(peak_pcm[i]==pcm)peak_fmt[i]=f;}
int snd_pcm_hw_params_set_format(void*a,void*b,int c){static int(*real)(void*,void*,int);if(!real)real=dlsym((void*)-1,"snd_pcm_hw_params_set_format");int v=real(a,b,c);if(v>=0)peak_setfmt(a,c);logresult("snd_pcm_hw_params_set_format value",c);return logresult("snd_pcm_hw_params_set_format",v);}
WRAP3(snd_pcm_hw_params_set_channels)
extern int open(const char*,int,...);extern int read(int,void*,unsigned);extern int close(int);
/* ALSA control device for the RX3's card queries: read once from /etc/rx3-ctl (e.g. "hw:CARD=DDJFLX4"), default hw:2. */
static const char *ctl_name(void){static char buf[64];if(!buf[0]){int fd=open("/etc/rx3-ctl",0);int n=fd<0?0:read(fd,buf,sizeof(buf)-1);if(fd>=0)close(fd);if(n<0)n=0;buf[n]=0;while(n>0&&(buf[n-1]=='\n'||buf[n-1]==' '))buf[--n]=0;if(!buf[0]){buf[0]='h';buf[1]='w';buf[2]=':';buf[3]='2';buf[4]=0;}}return buf;}
int snd_ctl_open(void **ctl,const char *name,int mode){static int(*real)(void**,const char*,int);if(!real)real=dlsym((void*)-1,"snd_ctl_open");logtext("CTL open ");logtext(ctl_name());return logresult("",real(ctl,ctl_name(),mode));}
int snd_pcm_hw_params_get_channels_max(const void *p,unsigned *v){static int(*real)(const void*,unsigned*);if(!real)real=dlvsym((void*)-1,"snd_pcm_hw_params_get_channels_max","ALSA_0.9.0rc4");int r=real(p,v);if(r>=0&&*v>2)*v=2;logresult("channels max",*v);return logresult("channels max result",r);}
int snd_ctl_pcm_info(void *ctl,void *info){
 static int(*real)(void*,void*);static int(*stream)(const void*);
 if(!real)real=dlsym((void*)-1,"snd_ctl_pcm_info");
 if(!stream)stream=dlsym((void*)-1,"snd_pcm_info_get_stream");
 if(stream(info)==1)return 0; /* Virtual silent capture, exposed by the null PCM. */
 return real(ctl,info);
}

/* Audio peak meter: track the loudest 16-bit sample written to each output PCM and report every 256 writes to /tmp/rx3-audio-peaks. */
extern int snd_pcm_hw_params_get_channels(const void*,unsigned*);
long snd_pcm_writei(void *pcm,const void *buf,unsigned long frames){
 static long(*real)(void*,const void*,unsigned long);if(!real)real=dlsym((void*)-1,"snd_pcm_writei");
 int slot=-1;for(int i=0;i<4;i++){if(peak_pcm[i]==pcm){slot=i;break;}}
 if(slot>=0){unsigned long n=frames*2;int m=peak_val[slot];int f=peak_fmt[slot];
  if(f==6||f==10){const int *s=buf;for(unsigned long i=0;i<n;i++){int v=s[i];if(f==6)v=(int)((unsigned)v<<8)>>16;else v>>=16;if(v<0)v=-v;if(v>m)m=v;}}   /* S24_LE in 32-bit words, S32_LE */
  else{const short *s=buf;for(unsigned long i=0;i<n;i++){int v=s[i];if(v<0)v=-v;if(v>m)m=v;}}
  peak_val[slot]=m;
  if(++peak_n[slot]>=256){peak_n[slot]=0;char line[64];int k=0;const char *t=peak_name[slot]?peak_name[slot]:"pcm";while(*t&&k<20)line[k++]=*t++;line[k++]=' ';line[k++]='f';line[k++]='0'+f%10;line[k++]=' ';
   char num[8];int d=0;int x=m;do{num[d++]='0'+x%10;x/=10;}while(x);while(d)line[k++]=num[--d];line[k++]='\n';
   int fd=open("/tmp/rx3-audio-peaks",01|0100|02000,0644);if(fd>=0){write(fd,line,k);close(fd);}peak_val[slot]=0;}}
 return real(pcm,buf,frames);
}
