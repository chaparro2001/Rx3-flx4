#ifndef PI_CONTROLS_H
#define PI_CONTROLS_H
/* Override at build time: gcc -DRX3_ROOT_PATH='"/home/you/rx3-rootfs"' ... (install.sh does this). */
#ifndef RX3_ROOT_PATH
#define RX3_ROOT_PATH "/home/rx3/rx3-rootfs"
#endif
#include <stddef.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define UI_STATE RX3_ROOT_PATH "/dev/rx3-ui-state"
#define UI_CONTROL RX3_ROOT_PATH "/dev/rx3-control"
/* The firmware draws its screen at this size, whatever the panel. */
#define SRC_W 1280
#define SRC_H 800
struct command {int key,operation,channel,value;float analog;int extra;};
/* Shared between the presenter, the touch bridge and the control shim inside the chroot (as /dev/rx3-ui-state).
   `deck[n]` is what the firmware's own engine says about player n (DECK_* bits), published by control-shim.c
   every 200 ms with `state_seq` bumped each time; the presenter treats the flags as unknown when that stops.
   control-shim.c cannot include this header (-nostdlib build), so it writes the last seven words at byte 52. */
struct ui_state {unsigned magic;float level[6];unsigned pressed;unsigned headphone_cue;int cursor_x,cursor_y,cursor_visible;int page;
 unsigned deck[2];unsigned state_seq;unsigned hotcue[2];   /* hotcue[n]: bit k = hot cue A+k of the track on player n is set */
 unsigned chlevel[2];                                       /* chlevel[n]: the engine's own reading of mixer input n (getInputChLevelMono) */
 unsigned ui_flags;                                         /* UI_* below: set by the touch bridge, followed by the presenter */
 unsigned gesture,gesture_seq;};                            /* last side-band gesture (side<<8 | G_*), bumped per gesture */
#define UI_BARS_HIDDEN 1     /* SETTINGS > BARS: the slider bars are hidden (remembered in ~/.rx3-ui by the touch bridge) */
#define DECK_PLAYING 1
#define DECK_MASTER_TEMPO 2
#define DECK_QUANTIZE 4
#define DECK_HP_CUE 8        /* mixer channel n is sent to the headphones */
#define DECK_LOOPING 16      /* a loop is playing */
#define DECK_RELOOP 32       /* there is a loop to return to (RELOOP/EXIT would do something) */
#define DECK_SYNC 64         /* beat sync is on */
#define DECK_MASTER 128      /* this player is the sync master */
#define DECK_XFADER 256      /* the crossfader is assigned (CH1=A, CH2=B); clear = THRU, the crossfader does nothing. Set in both words. */
#define DECK_BEATFX 512      /* the Beat FX is on (one effect for the mixer, so set in both words) */
/* Keys above 0xFFF0 are commands for control-shim.c rather than firmware keys: */
#define CMD_XFADER 0xFFFE    /* press toggles the crossfader assignment */
#define CMD_CLOSE 0xFFFD     /* held 1 s: stop the player. Handled by touch-bridge.c itself (systemctl), never sent to the shim;
                                started from the XDJ-RX3 icon, rx3-session.sh then brings the desktop back */
#define CMD_BARS 0xFFFC      /* press shows/hides the slider bars (UI_BARS_HIDDEN). Also handled by touch-bridge.c only */
#define CMD_BRIGHT_UP 0xFFFA /* SETTINGS: backlight a step up / down (touch-bridge.c, through sysfs) */
#define CMD_BRIGHT_DOWN 0xFFFB
#define UI_STATE_DECK_OFFSET 52
_Static_assert(offsetof(struct ui_state,deck)==UI_STATE_DECK_OFFSET&&offsetof(struct ui_state,hotcue)==UI_STATE_DECK_OFFSET+12&&offsetof(struct ui_state,chlevel)==UI_STATE_DECK_OFFSET+20,"control-shim.c writes deck[2], state_seq, hotcue[2], chlevel[2] from this offset");
/* The button strip: BUTTON_ROWS rows of BUTTON_COLS cells under the content area, showing one *page* of buttons at a
   time. The main page has DECK 1 and DECK 2 as large 2x2 buttons at either end (into each deck's page) and the browser,
   SETTINGS and X-FADER between them. A deck page is that deck alone, both rows, with larger buttons and a BACK to the main page. SETTINGS likewise: BARS,
   the IP address, CLOSE and BACK.
   A page button sends nothing to the firmware. An entry with no label is an empty cell (drawn as background, touches
   ignored), as are cells past the page's count; so are the cells a wider button (`span`) covers.
   `scroll` makes the button a repeating rotary step (the browse selector); `hold` adds the firmware's operation 1
   ("long-pressed") right after the press - UTILITY is the panel's MENU key (0x206) held down. */
#define BUTTON_COLS 10
#define BUTTON_ROWS 2
#define NBUTTONS (BUTTON_COLS*BUTTON_ROWS)     /* cells per page */
/* `brief` is drawn instead of `label` when the cell is too narrow for the full text even at the smallest font.
   `page` >= 0 makes the button switch the strip to that page instead of sending `key`.
   `light` is a DECK_* bit: the button is drawn lit while the engine reports it set for `channel`.
   `span` > 1 widens the button over the next cells of its row, `rows` > 1 makes it that many rows tall (leave the
   covered cells EMPTY). `deck` 1/2 draws that deck's colour along the button's top edge, so it is plain which deck a
   button belongs to. */
struct button {const char *label,*brief;int key,channel,scroll,hold,page,light;unsigned color;int span,deck,rows;};
enum {PAGE_MAIN,PAGE_DECK1,PAGE_DECK2,PAGE_SETTINGS,NPAGES};
#define C_NAV 0x08699c
#define C_SET 0x4b3a6d
#define C_KEY 0x283542
#define C_LOAD 0x08699c
#define C_STOP 0x7a2f2f
#define C_PLAY 0x12623a
#define C_DECK 0x5a4a1f
#define DECK1_COLOR 0x1aa3d9    /* cyan */
#define DECK2_COLOR 0xf08a24    /* orange */
#define KEY(l,b,k,ch,col) {l,b,k,ch,0,0,-1,0,col,0,0}
#define LIT(l,b,k,ch,bit,col) {l,b,k,ch,0,0,-1,bit,col,0,0}
#define DKEY(l,b,k,ch,col,span) {l,b,k,ch,0,0,-1,0,col,span,ch}          /* a deck's own button: marked with its colour */
#define DLIT(l,b,k,ch,bit,col,span) {l,b,k,ch,0,0,-1,bit,col,span,ch}
#define GOTO(l,b,pg,col,span,deck) {l,b,0,0,0,0,pg,0,col,span,deck}    /* switch to page `pg` */
#define TALL_GOTO(l,pg,deck) {l,0,0,0,0,0,pg,0,C_DECK,2,deck,2}           /* ... as a 2x2 button, the height of the strip */
#define EMPTY {0,0,0,0,0,0,-1,0,0,0,0}
/* A deck page, cells by span:  LOAD 2 | PLAY / PAUSE 4 | MASTER TEMPO 2 | QUANTIZE 2
                               USB STOP 2 | (6 free: SYNC, CUE, loops ...) | BACK 2 */
#define DECK_PAGE(n) \
 DKEY("LOAD " #n,0,0x4311,n,C_LOAD,2),EMPTY,DLIT("PLAY / PAUSE " #n,"PLAY " #n,0x4101,n,DECK_PLAYING,C_PLAY,4),EMPTY,EMPTY,EMPTY, \
 DLIT("MASTER TEMPO " #n,"MT " #n,0x4108,n,DECK_MASTER_TEMPO,C_DECK,2),EMPTY,DLIT("QUANTIZE " #n,"Q " #n,0x410b,n,DECK_QUANTIZE,C_DECK,2),EMPTY, \
 DKEY("USB STOP " #n " (hold)","EJECT " #n,0x8002,n,C_STOP,2),EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY, \
 GOTO("BACK",0,PAGE_MAIN,C_KEY,2,0),EMPTY
/* Main page:  DECK 1 | SOURCE BROWSE SHORTCUT SEARCH UTILITY SETTINGS | DECK 2
                (2x2)  | BACK   UP     DOWN     ENTER  X-FADER   .       | (2x2) */
static const struct button main_buttons[]={
 TALL_GOTO("DECK 1",PAGE_DECK1,1),EMPTY,KEY("SOURCE",0,0x201,0,C_NAV),KEY("BROWSE",0,0x202,0,C_NAV),KEY("SHORTCUT",0,0x210,0,C_SET),
 KEY("SEARCH",0,0x205,0,C_SET),{"UTILITY",0,0x206,0,0,1,-1,0,C_SET,0,0},GOTO("SETTINGS",0,PAGE_SETTINGS,C_SET,1,0),
 TALL_GOTO("DECK 2",PAGE_DECK2,2),EMPTY,
 EMPTY,EMPTY,KEY("BACK",0,0x420d,0,C_KEY),{"UP",0,0x420c,0,-1,0,-1,0,C_KEY,0,0},{"DOWN",0,0x420c,0,1,0,-1,0,C_KEY,0,0},KEY("ENTER",0,0x420c,0,C_KEY),
 LIT("X-FADER","XF",CMD_XFADER,1,DECK_XFADER,C_SET),EMPTY,EMPTY,EMPTY};
static const struct button deck1_buttons[]={DECK_PAGE(1)};
static const struct button deck2_buttons[]={DECK_PAGE(2)};
/* SETTINGS:  BARS 2   | IP address (4 cells)            | .  . | CLOSE 2
               BRIGHT - | brightness % | BRIGHT + | battery (4 cells) | . | BACK 2
   BARS is drawn lit while the bars are on screen (fb-present.c), not from a deck bit. The information cells are empty
   here, so touches pass through them, and the presenter draws one box across each group. BRIGHT -/+ repeat when held. */
#define SETTINGS_INFO_FIRST 2
#define SETTINGS_INFO_CELLS 4
#define SETTINGS_BRIGHT_CELL (BUTTON_COLS+1)
#define SETTINGS_BATTERY_FIRST (BUTTON_COLS+3)
#define SETTINGS_BATTERY_CELLS 4
static const struct button settings_buttons[]={
 {"BARS",0,CMD_BARS,0,0,0,-1,0,C_SET,2,0,0},EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,
 {"CLOSE (hold)","CLOSE",CMD_CLOSE,0,0,0,-1,0,C_STOP,2,0,0},EMPTY,
 {"BRIGHT -","DIM",CMD_BRIGHT_DOWN,0,1,0,-1,0,C_SET,0,0,0},EMPTY,{"BRIGHT +","BRIGHT",CMD_BRIGHT_UP,0,1,0,-1,0,C_SET,0,0,0},
 EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,GOTO("BACK",0,PAGE_MAIN,C_KEY,2,0),EMPTY
};
struct page {const struct button *buttons;int n;};
#define PAGE(t) {t,(int)(sizeof(t)/sizeof*(t))}
static const struct page pages[NPAGES]={PAGE(main_buttons),PAGE(deck1_buttons),PAGE(deck2_buttons),PAGE(settings_buttons)};
static const char *slider_names[6]={"DECK 1","MASTER","HP MIX","DECK 2","HP LEVEL","CROSS"};
static const int slider_keys[6]={0x501e,0x4403,0x4405,0x501e,0x4406,0x6017};
static const int slider_channels[6]={1,0,0,2,0,0};

/* ---- Layout -------------------------------------------------------------------------------------------------
   Everything is laid out in "UI space": the panel seen the right way up, in panel pixels (W x H swapped for a
   90/270 rotation). The only transform left to the physical panel is that rotation, so nothing is letterboxed
   and nothing is resampled twice. The chrome (button strip, slider bars) keeps the proportions of the original
   1920x1200 design scaled by `scale`; the firmware picture takes the largest 16:10 rectangle in what is left.
   The presenter and the touch bridge both build this from the same inputs, so what is drawn and what is
   touched always agree. */
enum {UI_FULL,UI_STRIP,UI_NONE};    /* strip + slider bars / strip only / firmware picture only */
struct ui {
 int W,H,rot;            /* the panel, and how the UI is turned onto it (0/90/180/270 clockwise) */
 int uw,uh;              /* UI space */
 double scale;           /* chrome scale: 1 = the sizes of the 1920x1200 design */
 int mode;
 int cx,cy,cw,ch;        /* the firmware picture */
 int sx,sy,sw,sh;        /* the button strip */
 int bw;                 /* slider bar width: left bar at x=0, right bar at uw-bw, both sy tall (0 without bars) */
};
#define DESIGN_STRIP_H 200
#define DESIGN_BAR_W 160
#define DESIGN_SLIDER_H 333
static inline struct ui make_ui(int W,int H,int rot,double scale,int mode){
 struct ui u;u.W=W;u.H=H;u.rot=rot;u.mode=mode;int side=rot==90||rot==270;u.uw=side?H:W;u.uh=side?W:H;
 if(scale<=0){double sx=u.uw/1920.,sy=u.uh/1200.;scale=sx<sy?sx:sy;}
 /* Keep the chrome to at most half the panel each way, whatever RX3_UI_SCALE asks for, so the picture never vanishes. */
 double mx=u.uw*0.5/(2*DESIGN_BAR_W),my=u.uh*0.5/DESIGN_STRIP_H,cap=mx<my?mx:my;if(scale>cap)scale=cap;if(scale<0.25)scale=0.25;u.scale=scale;
 u.sh=mode==UI_NONE?0:(int)(DESIGN_STRIP_H*scale+.5);u.sy=u.uh-u.sh;u.sx=0;u.sw=u.uw;
 int bwmin=mode==UI_FULL?(int)(DESIGN_BAR_W*scale+.5):0,aw=u.uw-2*bwmin,ah=u.sy;
 if((long)aw*SRC_H>=(long)ah*SRC_W){u.ch=ah;u.cw=(int)((long)ah*SRC_W/SRC_H);}else{u.cw=aw;u.ch=(int)((long)aw*SRC_H/SRC_W);}
 u.cx=(u.uw-u.cw)/2;u.cy=(u.sy-u.ch)/2;u.bw=mode==UI_FULL?u.cx:0;   /* bars grow to meet the picture */
 return u;}
/* UI pixel <-> panel pixel. Rotate 90 puts the UI's left edge along the panel's top edge. */
static inline void ui_to_panel(const struct ui*u,int ux,int uy,int*px,int*py){
 switch(u->rot){case 90:*px=u->uh-1-uy;*py=ux;break;case 180:*px=u->uw-1-ux;*py=u->uh-1-uy;break;case 270:*px=uy;*py=u->uw-1-ux;break;default:*px=ux;*py=uy;}}
static inline void panel_to_ui(const struct ui*u,int px,int py,int*ux,int*uy){
 switch(u->rot){case 90:*ux=py;*uy=u->uh-1-px;break;case 180:*ux=u->uw-1-px;*uy=u->uh-1-py;break;case 270:*ux=u->uw-1-py;*uy=px;break;default:*ux=px;*uy=py;}
 if(*ux<0)*ux=0;if(*ux>=u->uw)*ux=u->uw-1;if(*uy<0)*uy=0;if(*uy>=u->uh)*uy=u->uh-1;}
/* Cell i of the strip. Any width left over is spread over the leftmost cells so the row ends exactly at sw. */
static inline void button_rect(const struct ui*u,int i,int*x,int*y,int*w,int*h){
 int col=i%BUTTON_COLS,row=i/BUTTON_COLS,base=u->sw/BUTTON_COLS,extra=u->sw%BUTTON_COLS;
 *x=u->sx+col*base+(col<extra?col:extra);*w=base+(col<extra);
 *y=u->sy+row*u->sh/BUTTON_ROWS;*h=u->sy+(row+1)*u->sh/BUTTON_ROWS-*y;}
/* Button i's box: its cell, widened over the next `span`-1 cells of its row and down `rows`-1 rows, never past the
   strip's edges. */
static inline void button_box(const struct ui*u,const struct button*b,int i,int*x,int*y,int*w,int*h){
 button_rect(u,i,x,y,w,h);int row=i/BUTTON_COLS,col=i%BUTTON_COLS,x2,y2,w2,h2;
 int lastcol=col+(b->span>1?b->span-1:0),lastrow=row+(b->rows>1?b->rows-1:0);
 if(lastcol>=BUTTON_COLS)lastcol=BUTTON_COLS-1;if(lastrow>=BUTTON_ROWS)lastrow=BUTTON_ROWS-1;
 if(lastcol>col||lastrow>row){button_rect(u,lastrow*BUTTON_COLS+lastcol,&x2,&y2,&w2,&h2);*w=x2+w2-*x;*h=y2+h2-*y;}}
/* Which button of page `pg` a UI point lands on, or -1 for the empty cells and everything outside the strip. Every
   labelled button is tested by its whole box, so a wide or tall one is found from any cell it covers. */
static inline int button_at(const struct ui*u,const struct page*pg,int x,int y){
 if(u->sh==0||y<u->sy||y>=u->sy+u->sh)return -1;
 for(int i=0;i<pg->n&&i<NBUTTONS;i++){const struct button*b=&pg->buttons[i];int bx,by,bw,bh;if(!b->label)continue;
  button_box(u,b,i,&bx,&by,&bw,&bh);if(x>=bx&&x<bx+bw&&y>=by&&y<by+bh)return i;}
 return -1;}
static inline const struct page *page_of(int n){return &pages[n>=0&&n<NPAGES?n:0];}
/* Slider i (0-2 left bar, 3-5 right bar) is drawn from a 160x333 design box: its origin and the scale that
   fits it, centred in its slot. Touch converts back through the same numbers. */
static inline void slider_box(const struct ui*u,int i,int*x,int*y,double*s){
 int slot=u->sy/3,bx=i<3?0:u->uw-u->bw,by=(i%3)*slot;double sx=u->bw/(double)DESIGN_BAR_W,sy=slot/(double)DESIGN_SLIDER_H;
 *s=sx<sy?sx:sy;*x=bx+(int)((u->bw-DESIGN_BAR_W**s)/2);*y=by+(int)((slot-DESIGN_SLIDER_H**s)/2);}
/* Which slider bar a UI point is in: 0 left, 1 right, -1 neither (also for the strip and the picture). */
static inline int slider_at(const struct ui*u,int x,int y){
 if(u->bw==0||y>=u->sy)return -1;int slot=u->sy/3,k=slot?y/slot:0;if(k>2)k=2;   /* same slots as slider_box */
 if(x<u->bw)return k;if(x>=u->uw-u->bw)return 3+k;return -1;}
/* RX3_ROTATE picks the rotation (0/90/180/270, clockwise); otherwise portrait panels get 90, landscape 0. */
static inline int rotation_for(int W,int H){const char*e=getenv("RX3_ROTATE");
 if(e&&*e){int r=atoi(e);if(r==0||r==90||r==180||r==270)return r;fprintf(stderr,"RX3_ROTATE=%s ignored: use 0, 90, 180 or 270\n",e);}
 return H>W?90:0;}
/* RX3_UI = full | strip | none, RX3_UI_SCALE = chrome scale (empty = fit the 1920x1200 design to the panel). */
static inline struct ui ui_from_env(int W,int H){
 const char*m=getenv("RX3_UI");int mode=UI_FULL;
 if(m&&*m){if(!strcmp(m,"strip"))mode=UI_STRIP;else if(!strcmp(m,"none"))mode=UI_NONE;else if(strcmp(m,"full"))fprintf(stderr,"RX3_UI=%s ignored: use full, strip or none\n",m);}
 const char*s=getenv("RX3_UI_SCALE");double scale=s&&*s?atof(s):0;
 return make_ui(W,H,rotation_for(W,H),scale,mode);}
static inline const char *ui_mode_name(int mode){return mode==UI_STRIP?"strip":mode==UI_NONE?"none":"full";}
/* Gesture bands: with no slider bars, the strips beside the picture (above the button strip) take one-finger gestures,
   which the firmware never sees (touch-bridge.c); the presenter draws hints there and flashes what was done.
   0 = left band (deck 1), 1 = right band (deck 2), -1 = anywhere else. Bands narrower than 40 px are not used. */
static inline int gesture_band(const struct ui*u,int x,int y){
 if(u->bw||y>=u->sy||u->cx<40)return -1;
 return x<u->cx?0:x>=u->cx+u->cw?1:-1;}
static inline void gesture_band_box(const struct ui*u,int side,int*x,int*w){*x=side?u->cx+u->cw:0;*w=side?u->uw-*x:u->cx;}
enum {G_NONE,G_ENTER,G_BACK,G_LOAD,G_DECK};
/* The mode in force: RX3_UI's, minus the bars while SETTINGS > BARS hides them. */
static inline int ui_mode_now(int base,const struct ui_state*st){return base==UI_FULL&&(st->ui_flags&UI_BARS_HIDDEN)?UI_STRIP:base;}
/* RX3_FB names the framebuffer to draw on (rx3-env.sh picks the DSI panel when one exists). */
static inline const char *fb_device(void){const char*e=getenv("RX3_FB");return e&&*e?e:"/dev/fb0";}
/* ---- Backlight and battery (SETTINGS) -------------------------------------------------------------------------
   The backlight is the first /sys/class/backlight entry (RX3_BACKLIGHT=<name> picks another); install.sh's udev rule
   lets the video group write it, which is how the touch bridge sets it. The battery is the first power supply of type
   Battery that is not some device's own (a pen, a mouse: scope Device). An HDMI Pi has neither; the boxes then say so. */
static inline int sysfs_read(const char*dir,const char*name,char*buf,int n){char p[300];snprintf(p,sizeof p,"%s/%s",dir,name);
 FILE*f=fopen(p,"r");if(!f)return -1;int ok=fgets(buf,n,f)!=0;fclose(f);if(!ok)return -1;buf[strcspn(buf,"\n")]=0;return 0;}
static inline long sysfs_long(const char*dir,const char*name){char b[32];return sysfs_read(dir,name,b,sizeof b)?-1:atol(b);}
static inline int sysfs_find(const char*cls,const char*type,char*dir,int n){DIR*d=opendir(cls);struct dirent*e;int found=0;if(!d)return 0;
 while(!found&&(e=readdir(d))){char p[300],t[32];if(e->d_name[0]=='.')continue;snprintf(p,sizeof p,"%s/%s",cls,e->d_name);
  if(type&&(sysfs_read(p,"type",t,sizeof t)||strcmp(t,type)))continue;
  if(type&&!sysfs_read(p,"scope",t,sizeof t)&&!strcmp(t,"Device"))continue;
  snprintf(dir,n,"%s",p);found=1;}
 closedir(d);return found;}
static inline int backlight_dir(char*dir,int n){const char*e=getenv("RX3_BACKLIGHT");
 if(e&&*e){snprintf(dir,n,"/sys/class/backlight/%s",e);return 1;}return sysfs_find("/sys/class/backlight",0,dir,n);}
/* Brightness as a percentage of max_brightness, -1 without a backlight. */
static inline int backlight_percent(void){char d[256];if(!backlight_dir(d,sizeof d))return -1;
 long m=sysfs_long(d,"max_brightness"),b=sysfs_long(d,"brightness");return m>0&&b>=0?(int)((b*100+m/2)/m):-1;}
/* Battery charge in percent (-1: no battery); *plugged is set while it charges or sits full on the charger. */
static inline int battery_percent(int*plugged){char d[256],s[32];*plugged=0;
 if(!sysfs_find("/sys/class/power_supply","Battery",d,sizeof d))return -1;
 if(!sysfs_read(d,"status",s,sizeof s))*plugged=!strcmp(s,"Charging")||!strcmp(s,"Full")||!strcmp(s,"Not charging");
 long c=sysfs_long(d,"capacity");if(c>=0)return c>100?100:(int)c;
 long now=sysfs_long(d,"energy_now"),full=sysfs_long(d,"energy_full");
 if(now<0||full<=0){now=sysfs_long(d,"charge_now");full=sysfs_long(d,"charge_full");}
 return now>=0&&full>0?(int)(now*100/full):-1;}
#define BATTERY_LOW 15     /* at or below, and not plugged: a red line over the strip and a red SETTINGS button */
#endif
