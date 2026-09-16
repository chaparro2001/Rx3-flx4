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
static struct ui u;
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
/* RX3_TOUCH: "swap", "invx", "invy" (comma-separated) for touch controllers whose axes do not follow the panel's.
   swap is applied first, on the controller's axes; the inversions then act on panel pixels. */
static int t_swap,t_invx,t_invy;
static void touch_flags(void){const char*e=getenv("RX3_TOUCH");if(!e||!*e)return;char buf[64];strncpy(buf,e,sizeof(buf)-1);buf[sizeof(buf)-1]=0;
 for(char*t=strtok(buf,", ");t;t=strtok(0,", ")){
  if(!strcmp(t,"swap"))t_swap=1;else if(!strcmp(t,"invx"))t_invx=1;else if(!strcmp(t,"invy"))t_invy=1;
  else fprintf(stderr,"RX3_TOUCH: unknown flag '%s' (use swap, invx, invy)\n",t);}}
int main(int argc,char**argv){
 if(argc<3)return 2;
 int replay=!strcmp(argv[1],"--replay"),mouse=!strcmp(argv[1],"--mouse");if(mouse){if(argc<4)return 2;argv++;}
 int in=replay?0:open(argv[1],O_RDONLY),out=open(argv[2],O_RDWR|O_NONBLOCK);control=open(UI_CONTROL,O_RDWR|O_NONBLOCK);
 if(in<0||out<0||control<0){perror("open");return 1;}
 int sf=open(UI_STATE,O_RDWR|O_CREAT,0600);if(sf<0||ftruncate(sf,sizeof(struct ui_state)))return 1;
 state=mmap(0,sizeof(*state),PROT_READ|PROT_WRITE,MAP_SHARED,sf,0);if(state==MAP_FAILED)return 1;
 if(state->magic!=0x52583332)*state=(struct ui_state){0x52583332,{1,.6,0,1,.5,.5},0,1,0,0,0};
 state->pressed=0;   /* a bridge that died mid-touch leaves its button lit; nobody else clears it */
 struct finger fingers[10]={0};
 /* Same panel geometry, rotation and chrome as the presenter, so a touch lands exactly under what is drawn there. */
 int W=1920,H=1200;{int fb=open(fb_device(),O_RDONLY);if(fb>=0){struct fb_var_screeninfo v;if(!ioctl(fb,FBIOGET_VSCREENINFO,&v)){W=v.xres;H=v.yres;}close(fb);}}
 u=ui_from_env(W,H);touch_flags();
 int direct=mouse||replay;   /* mouse: UI pixels; replay: the firmware's own 1280x800 coordinates */
 /* Multitouch (type B) controllers report ABS_MT_*; single-touch ones (resistive, some USB HID panels) only ABS_X/ABS_Y
    with BTN_TOUCH. Both are handled; the second kind is finger 0. */
 struct input_absinfo ax={.maximum=SRC_W-1},ay={.maximum=SRC_H-1};int mt=1;
 if(!direct&&(ioctl(in,EVIOCGABS(ABS_MT_POSITION_X),&ax)||ioctl(in,EVIOCGABS(ABS_MT_POSITION_Y),&ay))){
  mt=0;if(ioctl(in,EVIOCGABS(ABS_X),&ax)||ioctl(in,EVIOCGABS(ABS_Y),&ay)){perror("touch ranges (neither ABS_MT_POSITION nor ABS_X/Y)");return 1;}}
 fprintf(stderr,"touch bridge: %s, panel %dx%d rotate %d, ui %dx%d mode %s, picture %dx%d at %d,%d, strip %d px, bars %d px",
  replay?"replay (1280x800 input)":mouse?"mouse":mt?"multitouch":"single-touch",W,H,u.rot,u.uw,u.uh,ui_mode_name(u.mode),u.cw,u.ch,u.cx,u.cy,u.sh,u.bw);
 if(!direct)fprintf(stderr,", touch %d..%d x %d..%d%s%s%s",ax.minimum,ax.maximum,ay.minimum,ay.maximum,t_swap?" swap":"",t_invx?" invx":"",t_invy?" invy":"");
 fprintf(stderr,"\n");
 if(mouse){fingers[0].x=u.uw/2;fingers[0].y=u.uh/2;state->cursor_x=fingers[0].x;state->cursor_y=fingers[0].y;state->cursor_visible=1;fprintf(stderr,"mouse mode: left=touch, wheel=browse, right=back, middle=enter\n");}

 int slot=0,source=-1,ux=0,uy=0,release=0;struct input_event e;struct pollfd p={in,POLLIN,0};
 for(;;){
 int ready=poll(&p,1,10);if(ready<0)return 1;
 if(ready){if(read(in,&e,sizeof(e))!=sizeof(e))break;
  if(mouse){struct finger*f=&fingers[0];
   if(e.type==EV_REL){if(e.code==REL_X)f->x+=e.value;if(e.code==REL_Y)f->y+=e.value;if(f->x<0)f->x=0;if(f->x>=u.uw)f->x=u.uw-1;if(f->y<0)f->y=0;if(f->y>=u.uh)f->y=u.uh-1;
    if(e.code==REL_WHEEL&&e.value)command(0x420c,4,0,e.value>0?-1:1,0);}
   if(e.type==EV_KEY){if(e.code==BTN_LEFT)f->down=e.value;if(e.code==BTN_RIGHT&&e.value<2)command(0x420d,e.value?0:2,0,0,0);if(e.code==BTN_MIDDLE&&e.value<2)command(0x420c,e.value?0:2,0,0,0);}
   state->cursor_x=f->x;state->cursor_y=f->y;}
  else if(mt||replay){if(e.type==EV_ABS){if(e.code==ABS_MT_SLOT)slot=e.value;if(slot>=0&&slot<10){struct finger*f=&fingers[slot];if(e.code==ABS_MT_POSITION_X)f->x=e.value;if(e.code==ABS_MT_POSITION_Y)f->y=e.value;if(e.code==ABS_MT_TRACKING_ID)f->down=e.value>=0;}}}
  else{struct finger*f=&fingers[0];if(e.type==EV_ABS){if(e.code==ABS_X)f->x=e.value;if(e.code==ABS_Y)f->y=e.value;}if(e.type==EV_KEY&&e.code==BTN_TOUCH)f->down=e.value;}
  if(e.type!=EV_SYN||e.code!=SYN_REPORT)continue;
 }
 long now=millis();
 for(int i=0;i<10;i++){
  struct finger*f=&fingers[i];int lx,ly;
  if(mouse){lx=f->x;ly=f->y;}
  else if(replay){lx=u.cx+(int)((long)f->x*u.cw/SRC_W);ly=u.cy+(int)((long)f->y*u.ch/SRC_H);}
  else{int tx=f->x,ty=f->y;const struct input_absinfo*rx=&ax,*ry=&ay;if(t_swap){tx=f->y;ty=f->x;rx=&ay;ry=&ax;}
   int px=(int)((long)(tx-rx->minimum)*W/(rx->maximum-rx->minimum+1)),py=(int)((long)(ty-ry->minimum)*H/(ry->maximum-ry->minimum+1));
   if(t_invx)px=W-1-px;if(t_invy)py=H-1-py;panel_to_ui(&u,px,py,&lx,&ly);}
  if(lx<0)lx=0;if(lx>=u.uw)lx=u.uw-1;if(ly<0)ly=0;if(ly>=u.uh)ly=u.uh-1;
  if(f->down&&!f->active){f->active=1;f->region=-1;
   int b=button_at(&u,lx,ly),si=b<0?slider_at(&u,lx,ly):-1;
   if(b>=0){f->region=1+b;button(b,1);f->next_repeat=now+400;}
   else if(si>=0){int x,y;double s;slider_box(&u,si,&x,&y,&s);double dy=(ly-y)/s;   /* back into the 160x333 design box */
    if((si==0||si==3)&&dy>=40&&dy<80){int ch=si==0?1:2;command(0x5020,0,ch,0,0);command(0x5020,2,ch,0,0);state->headphone_cue^=ch==1?1:2;}
    else f->region=20+si;
   }else if(ly<u.sy&&source<0){source=i;f->region=0;}   /* anything else above the strip is the firmware picture */
   fprintf(stderr,"touch begin slot=%d ui=%d,%d region=%d\n",i,lx,ly,f->region);
  }
  if(f->active&&f->down){
   if(f->region>=20&&ready){int si=f->region-20,x,y;double s;slider_box(&u,si,&x,&y,&s);float a=(float)((265-(ly-y)/s)/180.);if(a<0)a=0;if(a>1)a=1;state->level[si]=a;command(slider_keys[si],4,slider_channels[si],0,a);}
   else if(f->region>0&&f->region<=NBUTTONS&&buttons[f->region-1].scroll&&now>=f->next_repeat){button(f->region-1,1);f->next_repeat=now+120;}
   else if(f->region==0){ux=(int)((long)(lx-u.cx)*SRC_W/u.cw);uy=(int)((long)(ly-u.cy)*SRC_H/u.ch);if(ux<0)ux=0;if(ux>=SRC_W)ux=SRC_W-1;if(uy<0)uy=0;if(uy>=SRC_H)uy=SRC_H-1;}
  }
  if(f->active&&!f->down){if(f->region>0&&f->region<=NBUTTONS)button(f->region-1,0);if(source==i){source=-1;release=10;}f->active=0;}
 }
 if(source>=0||release){struct report r={source>=0,0,37+(SRC_W-ux)*3976/SRC_W,72+uy*3856/SRC_H};if(write(out,&r,sizeof(r))!=sizeof(r))perror("touch report");if(source<0)release--;}
 }
 for(int i=0;i<10;i++)if(fingers[i].active&&fingers[i].region>0&&fingers[i].region<=NBUTTONS)button(fingers[i].region-1,0);
 return 0;
}
