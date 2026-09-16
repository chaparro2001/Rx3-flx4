#ifndef PI_CONTROLS_H
#define PI_CONTROLS_H
/* Override at build time: gcc -DRX3_ROOT_PATH='"/home/you/rx3-rootfs"' ... (install.sh does this). */
#ifndef RX3_ROOT_PATH
#define RX3_ROOT_PATH "/home/rx3/rx3-rootfs"
#endif
#define UI_STATE RX3_ROOT_PATH "/dev/rx3-ui-state"
#define UI_CONTROL RX3_ROOT_PATH "/dev/rx3-control"
/* The firmware draws its screen at this size, whatever the panel. */
#define SRC_W 1280
#define SRC_H 800
struct command {int key,operation,channel,value;float analog;int extra;};
struct ui_state {unsigned magic;float level[6];unsigned pressed;unsigned headphone_cue;int cursor_x,cursor_y,cursor_visible;};
/* The button strip: BUTTON_ROWS rows of BUTTON_COLS cells under the content area. Row 1 is navigation, row 2 the two
   decks. An entry with no label is an empty cell (drawn as background, touches ignored), as are cells past NBUTTONS.
   `scroll` makes the button a repeating rotary step (the browse selector); `hold` adds the firmware's operation 1
   ("long-pressed") right after the press - UTILITY is the panel's MENU key (0x206) held down. QUANTIZE and MASTER
   TEMPO are toggles whose state only the RX3's own deck display shows (the firmware reports no LED state to us). */
#define BUTTON_COLS 10
#define BUTTON_ROWS 2
#define NBUTTONS 20
/* `brief` is drawn instead of `label` when the cell is too narrow for the full text even at the smallest font. */
struct button {const char *label,*brief;int key,channel,scroll,hold;unsigned color;};
static const struct button buttons[NBUTTONS]={
 {"SOURCE",0,0x201,0,0,0,0x08699c},{"BROWSE",0,0x202,0,0,0,0x08699c},
 {"SHORTCUT",0,0x210,0,0,0,0x4b3a6d},{"SEARCH",0,0x205,0,0,0,0x4b3a6d},
 {"UTILITY",0,0x206,0,0,1,0x4b3a6d},{"BACK",0,0x420d,0,0,0,0x283542},
 {"UP",0,0x420c,0,-1,0,0x283542},{"DOWN",0,0x420c,0,1,0,0x283542},
 {"ENTER",0,0x420c,0,0,0,0x283542},{0,0,0,0,0,0,0},
 {"LOAD 1",0,0x4311,1,0,0,0x08699c},{"USB STOP 1 (hold)","EJECT 1",0x8002,1,0,0,0x7a2f2f},
 {"PLAY / PAUSE 1","PLAY 1",0x4101,1,0,0,0x12623a},{"QUANTIZE 1","Q 1",0x410b,1,0,0,0x5a4a1f},{"MASTER TEMPO 1","MT 1",0x4108,1,0,0,0x5a4a1f},
 {"LOAD 2",0,0x4311,2,0,0,0x08699c},{"USB STOP 2 (hold)","EJECT 2",0x8002,2,0,0,0x7a2f2f},
 {"PLAY / PAUSE 2","PLAY 2",0x4101,2,0,0,0x12623a},{"QUANTIZE 2","Q 2",0x410b,2,0,0,0x5a4a1f},{"MASTER TEMPO 2","MT 2",0x4108,2,0,0,0x5a4a1f}
};
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
/* Which button a UI point lands on, or -1 for the empty cells and everything outside the strip. */
static inline int button_at(const struct ui*u,int x,int y){
 if(u->sh==0||y<u->sy||y>=u->sy+u->sh)return -1;
 int row=0;while(row<BUTTON_ROWS-1&&y>=u->sy+(row+1)*u->sh/BUTTON_ROWS)row++;   /* same split as button_rect */
 for(int col=0;col<BUTTON_COLS;col++){int i=row*BUTTON_COLS+col,bx,by,bw,bh;if(i>=NBUTTONS)break;
  button_rect(u,i,&bx,&by,&bw,&bh);if(x>=bx&&x<bx+bw)return buttons[i].label?i:-1;}
 return -1;}
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
/* RX3_FB names the framebuffer to draw on (rx3-env.sh picks the DSI panel when one exists). */
static inline const char *fb_device(void){const char*e=getenv("RX3_FB");return e&&*e?e:"/dev/fb0";}
#endif
