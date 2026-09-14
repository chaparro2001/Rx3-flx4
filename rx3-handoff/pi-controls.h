#ifndef PI_CONTROLS_H
#define PI_CONTROLS_H
/* Override at build time: gcc -DRX3_ROOT_PATH='"/home/you/rx3-rootfs"' ... (install.sh does this). */
#ifndef RX3_ROOT_PATH
#define RX3_ROOT_PATH "/home/rx3/rx3-rootfs"
#endif
#define UI_STATE RX3_ROOT_PATH "/dev/rx3-ui-state"
#define UI_CONTROL RX3_ROOT_PATH "/dev/rx3-control"
#define CONTENT_X 160
#define CONTENT_W 1600
#define CONTENT_H 1000
struct command {int key,operation,channel,value;float analog;int extra;};
struct ui_state {unsigned magic;float level[6];unsigned pressed;unsigned headphone_cue;int cursor_x,cursor_y,cursor_visible;};
struct button {const char *label;int key,channel,scroll;unsigned color;};
static const struct button buttons[12]={
 {"SOURCE",0x201,0,0,0x08699c},{"BROWSE",0x202,0,0,0x08699c},
 {"BACK",0x420d,0,0,0x283542},{"UP",0x420c,0,-1,0x283542},
 {"DOWN",0x420c,0,1,0x283542},{"ENTER",0x420c,0,0,0x283542},
 {"LOAD 1",0x4311,1,0,0x08699c},{"USB STOP 1 (hold)",0x8002,1,0,0x7a2f2f},
 {"PLAY / PAUSE 1",0x4101,1,0,0x12623a},{"LOAD 2",0x4311,2,0,0x08699c},
 {"USB STOP 2 (hold)",0x8002,2,0,0x7a2f2f},{"PLAY / PAUSE 2",0x4101,2,0,0x12623a}
};
/* Where the 1920x1200 canvas lands on a panel of W x H pixels when drawn rotated `rot` degrees clockwise:
   the letterboxed footprint is lw x lh canvas units scaled by sc, placed at (ox,oy). The presenter and the
   touch bridge both use this, so what is drawn and what is touched always agree. */
struct layout {int W,H,rot,lw,lh,dw,dh,ox,oy;double sc;};
static struct layout make_layout(int W,int H,int rot){
 struct layout L={W,H,rot};int side=rot==90||rot==270;L.lw=side?1200:1920;L.lh=side?1920:1200;
 double sx=W*1.0/L.lw,sy=H*1.0/L.lh;L.sc=sx<sy?sx:sy;L.dw=(int)(L.lw*L.sc);L.dh=(int)(L.lh*L.sc);L.ox=(W-L.dw)/2;L.oy=(H-L.dh)/2;return L;}
/* Panel pixel -> canvas pixel. Outside the footprint: returns 0 when clamp is 0, else clamps to the nearest edge. */
static int panel_to_canvas(const struct layout*L,int px,int py,int clamp,int*cx,int*cy){
 int inside=px>=L->ox&&px<L->ox+L->dw&&py>=L->oy&&py<L->oy+L->dh;if(!inside&&!clamp)return 0;
 int u=(int)((px-L->ox)/L->sc),v=(int)((py-L->oy)/L->sc);
 if(u<0)u=0;if(u>=L->lw)u=L->lw-1;if(v<0)v=0;if(v>=L->lh)v=L->lh-1;
 switch(L->rot){case 90:*cx=v;*cy=1199-u;break;case 180:*cx=1919-u;*cy=1199-v;break;case 270:*cx=1919-v;*cy=u;break;default:*cx=u;*cy=v;}
 return 1;}
/* RX3_ROTATE picks the rotation (0/90/180/270, clockwise); otherwise portrait panels get 90, landscape 0. */
static int rotation_for(int W,int H){const char*e=getenv("RX3_ROTATE");
 if(e&&*e){int r=atoi(e);if(r==0||r==90||r==180||r==270)return r;fprintf(stderr,"RX3_ROTATE=%s ignored: use 0, 90, 180 or 270\n",e);}
 return H>W?90:0;}
/* RX3_FB names the framebuffer to draw on (rx3-env.sh picks the DSI panel when one exists). */
static const char *fb_device(void){const char*e=getenv("RX3_FB");return e&&*e?e:"/dev/fb0";}
static const char *slider_names[6]={"DECK 1","MASTER","HP MIX","DECK 2","HP LEVEL","CROSS"};
static const int slider_keys[6]={0x501e,0x4403,0x4405,0x501e,0x4406,0x6017};
static const int slider_channels[6]={1,0,0,2,0,0};
#endif
