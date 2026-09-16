#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include "pi-controls.h"
struct __attribute__((packed)) report {uint8_t down,pad;uint16_t x,y;};
struct finger {int x,y,down,active,region;long next_repeat;};
static int control;
static struct ui_state *state;
static long millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000+t.tv_nsec/1000000;}
static void command(int key,int op,int ch,int value,float a){struct command c={key,op,ch,value,a,0};if(write(control,&c,sizeof(c))!=sizeof(c))perror("control");}
/* USB STOP (0x8002) is a plain press/release here: the firmware times the hold itself (ejects ~1.9 s after press, a release
   before that cancels). Never send its code 1 "long-pressed" event without a press: that poisons the slot (the next mount is
   unmounted immediately) until the button is tapped once. */
/* UTILITY is the panel's MENU key held down: press, then operation 1 ("long-pressed"), then release. The 1 goes out
   right after the press so a tap opens UTILITY, and never on its own (see the USB STOP note above). */
static void button(int i,int down){
 const struct button*b=&buttons[i];if(down)state->pressed|=1u<<i;else state->pressed&=~(1u<<i);
 if(b->scroll){if(down)command(b->key,4,0,b->scroll,0);return;}
 command(b->key,down?0:2,b->channel,0,0);
 if(down&&b->hold)command(b->key,1,b->channel,0,0);
}
int main(int argc,char**argv){
 if(argc<3)return 2;
 int replay=!strcmp(argv[1],"--replay"),mouse=!strcmp(argv[1],"--mouse");if(mouse){if(argc<4)return 2;argv++;}
 int in=replay?0:open(argv[1],O_RDONLY),out=open(argv[2],O_RDWR|O_NONBLOCK);control=open(UI_CONTROL,O_RDWR|O_NONBLOCK);
 if(in<0||out<0||control<0){perror("open");return 1;}
 int sf=open(UI_STATE,O_RDWR|O_CREAT,0600);if(sf<0||ftruncate(sf,sizeof(struct ui_state)))return 1;
 state=mmap(0,sizeof(*state),PROT_READ|PROT_WRITE,MAP_SHARED,sf,0);if(state==MAP_FAILED)return 1;
 if(state->magic!=0x52583332)*state=(struct ui_state){0x52583332,{1,.6,0,1,.5,.5},0,1};
 struct input_absinfo ax={.maximum=1919},ay={.maximum=1199};struct finger fingers[10]={0};
 /* Same panel geometry and rotation as the presenter, so a touch lands exactly under what is drawn there. */
 int W=1920,H=1200;{int fb=open(fb_device(),O_RDONLY);if(fb>=0){struct fb_var_screeninfo v;if(!ioctl(fb,FBIOGET_VSCREENINFO,&v)){W=v.xres;H=v.yres;}close(fb);}}
 struct layout L=make_layout(W,H,rotation_for(W,H));
 int direct=mouse||replay;   /* mouse cursor and replay input are already in 1920x1200 canvas coordinates */
 if(!direct&&(ioctl(in,EVIOCGABS(ABS_MT_POSITION_X),&ax)||ioctl(in,EVIOCGABS(ABS_MT_POSITION_Y),&ay))){perror("touch ranges");return 1;}
 fprintf(stderr,"touch bridge: %s, panel %dx%d rotate %d, canvas %dx%d at %d,%d, touch %d..%d x %d..%d\n",direct?"canvas input":"touchscreen",W,H,L.rot,L.dw,L.dh,L.ox,L.oy,ax.minimum,ax.maximum,ay.minimum,ay.maximum);
 if(mouse){fingers[0].x=960;fingers[0].y=600;state->cursor_x=960;state->cursor_y=600;state->cursor_visible=1;fprintf(stderr,"mouse mode: left=touch, wheel=browse, right=back, middle=enter\n");}

 int slot=0,source=-1,ux=0,uy=0,release=0;struct input_event e;struct pollfd p={in,POLLIN,0};
 for(;;){
 int ready=poll(&p,1,10);if(ready<0)return 1;
 if(ready){if(read(in,&e,sizeof(e))!=sizeof(e))break;
  if(mouse){struct finger*f=&fingers[0];
   if(e.type==EV_REL){if(e.code==REL_X)f->x+=e.value;if(e.code==REL_Y)f->y+=e.value;if(f->x<0)f->x=0;if(f->x>1919)f->x=1919;if(f->y<0)f->y=0;if(f->y>1199)f->y=1199;
    if(e.code==REL_WHEEL&&e.value)command(0x420c,4,0,e.value>0?-1:1,0);}
   if(e.type==EV_KEY){if(e.code==BTN_LEFT)f->down=e.value;if(e.code==BTN_RIGHT&&e.value<2)command(0x420d,e.value?0:2,0,0,0);if(e.code==BTN_MIDDLE&&e.value<2)command(0x420c,e.value?0:2,0,0,0);}
   state->cursor_x=f->x;state->cursor_y=f->y;}
  else if(e.type==EV_ABS){if(e.code==ABS_MT_SLOT)slot=e.value;if(slot>=0&&slot<10){struct finger*f=&fingers[slot];if(e.code==ABS_MT_POSITION_X)f->x=e.value;if(e.code==ABS_MT_POSITION_Y)f->y=e.value;if(e.code==ABS_MT_TRACKING_ID)f->down=e.value>=0;}}
  if(e.type!=EV_SYN||e.code!=SYN_REPORT)continue;
 }
 long now=millis();
 for(int i=0;i<10;i++){
  struct finger*f=&fingers[i];int lx,ly;
  if(direct){lx=f->x;ly=f->y;}
  else{int px=(int)((long)(f->x-ax.minimum)*W/(ax.maximum-ax.minimum+1)),py=(int)((long)(f->y-ay.minimum)*H/(ay.maximum-ay.minimum+1));panel_to_canvas(&L,px,py,1,&lx,&ly);}
  if(lx<0)lx=0;if(lx>1919)lx=1919;if(ly<0)ly=0;if(ly>1199)ly=1199;
  if(f->down&&!f->active){f->active=1;f->region=-1;
   if(ly>=BUTTON_TOP){int b=button_at(lx,ly);if(b>=0){f->region=1+b;button(b,1);f->next_repeat=now+400;}}
   else if(lx<160||lx>=1760){int si=(lx<160?0:3)+ly/333;if(si>5)si=5;
    if((si==0||si==3)&&ly%333>=40&&ly%333<80){int ch=si==0?1:2;command(0x5020,0,ch,0,0);command(0x5020,2,ch,0,0);state->headphone_cue^=ch==1?1:2;}
    else f->region=20+si;
   }else if(source<0){source=i;f->region=0;}
   fprintf(stderr,"touch begin slot=%d screen=%d,%d region=%d\n",i,lx,ly,f->region);
  }
  if(f->active&&f->down){
   if(f->region>=20&&ready){int si=f->region-20;float a=(265-(ly-(si%3)*333))/180.f;if(a<0)a=0;if(a>1)a=1;state->level[si]=a;command(slider_keys[si],4,slider_channels[si],0,a);}
   else if(f->region>0&&f->region<=NBUTTONS&&buttons[f->region-1].scroll&&now>=f->next_repeat){button(f->region-1,1);f->next_repeat=now+120;}
   else if(f->region==0){ux=(lx-160)*4/5;uy=ly*4/5;if(ux<0)ux=0;if(ux>1279)ux=1279;if(uy<0)uy=0;if(uy>799)uy=799;}
  }
  if(f->active&&!f->down){if(f->region>0&&f->region<=NBUTTONS)button(f->region-1,0);if(source==i){source=-1;release=10;}f->active=0;}
 }
 if(source>=0||release){struct report r={source>=0,0,37+(1280-ux)*3976/1280,72+uy*3856/800};if(write(out,&r,sizeof(r))!=sizeof(r))perror("touch report");if(source<0)release--;}
 }
 for(int i=0;i<10;i++)if(fingers[i].active&&fingers[i].region>0&&fingers[i].region<=NBUTTONS)button(fingers[i].region-1,0);
 return 0;
}
