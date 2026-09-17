#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "pi-controls.h"
static uint32_t *frame,*chrome;   /* UI space, uw x uh; chrome is the static part, frame the composed picture */
static int UW,UH;
static double S;                  /* chrome scale (see make_ui) */
#define PX(v) ((int)((v)*S+.5))
static FT_Face face;
static void box(int x,int y,int w,int h,uint32_t c){for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)if(xx>=0&&xx<UW&&yy>=0&&yy<UH)frame[yy*UW+xx]=c;}
static int textwidth(const char *s,int size){
 FT_Set_Pixel_Sizes(face,0,size);int w=0;for(const char*p=s;*p;p++){if(FT_Load_Char(face,*p,FT_LOAD_DEFAULT))continue;w+=face->glyph->advance.x>>6;}return w;}
/* Largest size from `size` down to 10 px whose text still fits in `w`, so a long label never spills out of its cell. */
static int fitsize(const char *s,int w,int size){while(size>10&&textwidth(s,size)>w)size--;return size;}
static void label(int cx,int cy,const char *s,int size,uint32_t c){
 if(size<6)size=6;int w=textwidth(s,size);
 int x=cx-w/2;for(const char*p=s;*p;p++){if(FT_Load_Char(face,*p,FT_LOAD_RENDER))continue;FT_GlyphSlot g=face->glyph;
 for(unsigned y=0;y<g->bitmap.rows;y++)for(unsigned i=0;i<g->bitmap.width;i++){
 int px=x+g->bitmap_left+i,py=cy+size/3-g->bitmap_top+y;unsigned a=g->bitmap.buffer[y*g->bitmap.pitch+i];
 if(px<0||px>=UW||py<0||py>=UH||!a)continue;uint32_t old=frame[py*UW+px],v=0;
 for(int k=0;k<3;k++){unsigned shift=k*8;v|=((((c>>shift)&255)*a+((old>>shift)&255)*(255-a))/255)<<shift;}frame[py*UW+px]=v;
 }x+=g->advance.x>>6;}
}
/* The text and size for button i of page p in a cell `w` wide: the full label at the largest size that fits, else the
   brief one. Labels and cells never change, so this is worked out once per button and kept. */
static const char *button_text(int p,int i,int w,int *size){
 static const char *text[NPAGES][NBUTTONS];static int sizes[NPAGES][NBUTTONS];const struct button*b=&pages[p].buttons[i];
 if(!text[p][i]){int room=w-PX(24),want=PX(26);text[p][i]=b->label;sizes[p][i]=fitsize(b->label,room,want);
  if(textwidth(b->label,sizes[p][i])>room&&b->brief){text[p][i]=b->brief;sizes[p][i]=fitsize(b->brief,room,want);}}
 *size=sizes[p][i];return text[p][i];}
static uint32_t lighten(uint32_t c){uint32_t r=0;for(int k=0;k<3;k++){unsigned v=(c>>(k*8))&255;v+=(255-v)*2/5;r|=v<<(k*8);}return r;}
/* A lit button (engine says its toggle is on) is drawn paler with a bright bar along its bottom edge. */
static void drawbutton(const struct ui*u,int p,int i,int down,int lit){int x,y,w,h,size;const struct button*b=&pages[p].buttons[i];
 if(!b->label)return;button_rect(u,i,&x,&y,&w,&h);
 int m=PX(4);box(x+m,y+m,w-2*m,h-2*m,down?0x536f84:lit?lighten(b->color):b->color);
 if(lit)box(x+m,y+h-m-PX(6),w-2*m,PX(6),0xeaf3fa);
 const char *t=button_text(p,i,w,&size);label(x+w/2,y+h/2,t,size,0xffffff);}
/* The deck flags are trusted only while the shim keeps bumping state_seq (it stops with the player). */
static int state_fresh(const struct ui_state*st){static unsigned seen;static long since=-100000;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
 long now=t.tv_sec*1000+t.tv_nsec/1000000;if(st->state_seq!=seen){seen=st->state_seq;since=now;}return now-since<2000;}
/* The strip is part of the static chrome; it is redrawn there only when the touch bridge switches page. */
static void draw_strip(const struct ui*u,int p){uint32_t *live=frame;frame=chrome;
 box(u->sx,u->sy,u->sw,u->sh,0x101820);for(int i=0;i<pages[p].n;i++)drawbutton(u,p,i,0,0);frame=live;}
/* Slider i from its 160x333 design box (slider_box gives the origin and scale; the touch bridge inverts the same numbers). */
static void drawslider(const struct ui*u,int i,const struct ui_state*st,int fresh){
 int x,y;double s;slider_box(u,i,&x,&y,&s);
#define D(v) ((int)((v)*s+.5))
 float n=st->level[i];if(n<0)n=0;if(n>1)n=1;int h=D(180*n);
 box(x+D(70),y+D(85),D(20),D(180),0x35434e);box(x+D(70),y+D(265)-h,D(20),h,0x199feb);box(x+D(30),y+D(257)-h,D(100),D(16),0xeaf3fa);
 if(i==0||i==3){int ch=i==0?0:1;int on=fresh?(st->deck[ch]&DECK_HP_CUE)!=0:(st->headphone_cue>>ch)&1;   /* engine truth when published, else the touch toggle */
  box(x+D(8),y+D(43),D(144),D(32),on?0x126db0:0x35434e);label(x+D(80),y+D(60),"HP CUE",D(19),0xffffff);}
 char val[24];snprintf(val,sizeof(val),"%d%%",(int)(n*100+.5));label(x+D(80),y+D(305),val,D(25),0xd1dae2);
#undef D
}
int main(int argc,char**argv){
 if(argc<2)return 2;
 const char*fbpath=argc>2?argv[2]:fb_device();
 int src=open(argv[1],O_RDONLY),dst=open(fbpath,O_RDWR);if(src<0||dst<0){perror(src<0?argv[1]:fbpath);return 1;}
 struct fb_fix_screeninfo f;struct fb_var_screeninfo v;ioctl(dst,FBIOGET_FSCREENINFO,&f);ioctl(dst,FBIOGET_VSCREENINFO,&v);
 if(v.bits_per_pixel!=32&&v.bits_per_pixel!=16){fprintf(stderr,"Need a 16- or 32-bit framebuffer (got %u bpp)\n",v.bits_per_pixel);return 1;}
 int bpp16=v.bits_per_pixel==16;
 int W=v.xres,H=v.yres;struct ui u=ui_from_env(W,H);UW=u.uw;UH=u.uh;S=u.scale;
 frame=malloc(sizeof(uint32_t)*UW*UH);chrome=malloc(sizeof(uint32_t)*UW*UH);
 /* One UI index per panel pixel (the rotation), so the copy loop below is a plain lookup. */
 int *idx=malloc(sizeof(int)*W*H);if(!frame||!chrome||!idx)return 1;
 for(int py=0;py<H;py++)for(int px=0;px<W;px++){int ux,uy;panel_to_ui(&u,px,py,&ux,&uy);idx[py*W+px]=uy*UW+ux;}
 /* The firmware picture: one source column per picture column and one source row per picture row. When the picture
    is smaller than the source, each pixel averages a 2x2 block so text stays readable on small panels. */
 int *col=malloc(sizeof(int)*u.cw),*row=malloc(sizeof(int)*u.ch);if(!col||!row)return 1;
 for(int x=0;x<u.cw;x++)col[x]=(int)((long)x*SRC_W/u.cw);for(int y=0;y<u.ch;y++)row[y]=(int)((long)y*SRC_H/u.ch);
 int filter=u.cw<SRC_W;
 fprintf(stderr,"presenter: %s %dx%d %s, rotate %d, ui %dx%d mode %s scale %.2f, picture %dx%d at %d,%d%s, strip %d px, bars %d px\n",
  fbpath,W,H,bpp16?"16bpp":"32bpp",u.rot,UW,UH,ui_mode_name(u.mode),S,u.cw,u.ch,u.cx,u.cy,filter?" (2x2 filtered)":"",u.sh,u.bw);
 uint32_t *s=mmap(0,SRC_W*SRC_H*4,PROT_READ,MAP_SHARED,src,0);unsigned char *d=mmap(0,f.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,dst,0);if(s==MAP_FAILED||d==MAP_FAILED)return 1;
 int sf=open(UI_STATE,O_RDWR|O_CREAT,0600);if(sf<0||ftruncate(sf,sizeof(struct ui_state)))return 1;
 struct ui_state *state=mmap(0,sizeof(*state),PROT_READ|PROT_WRITE,MAP_SHARED,sf,0);if(state==MAP_FAILED)return 1;
 if(state->magic!=0x52583332){*state=(struct ui_state){0x52583332,{1,.6,0,1,.5,.5},0,1,0,0,0,0,{0,0},0};}
 /* Any scalable sans will do. Try the usual Debian/Raspbian packages in turn rather than depending on
    one font package, and say which paths were tried instead of exiting silently. $RX3_FONT overrides. */
 static const char *fonts[]={
  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
  "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
  "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
  "/usr/share/fonts/truetype/piboto/Piboto-Regular.ttf",
  "/usr/share/fonts/truetype/crosextra/Carlito-Regular.ttf",0};
 FT_Library ft;
 if(FT_Init_FreeType(&ft)){fprintf(stderr,"rx3-fb-present: cannot initialise FreeType\n");return 1;}
 const char *chosen=getenv("RX3_FONT");
 if(!chosen||FT_New_Face(ft,chosen,0,&face)){
  chosen=0;
  for(int i=0;fonts[i];i++) if(!FT_New_Face(ft,fonts[i],0,&face)){chosen=fonts[i];break;}
 }
 if(!chosen){
  fprintf(stderr,"rx3-fb-present: no usable font found. Install one with:\n"
                 "  sudo apt install fonts-dejavu-core\n"
                 "Or point RX3_FONT at a .ttf. Looked for:\n");
  for(int i=0;fonts[i];i++) fprintf(stderr,"  %s\n",fonts[i]);
  return 1;
 }
 box(0,0,UW,UH,0x101820);
 if(u.bw)for(int i=0;i<6;i++){int x,y;double sc;slider_box(&u,i,&x,&y,&sc);label(x+(int)(80*sc),y+(int)(27*sc),slider_names[i],(int)(23*sc+.5),0xffffff);}
 memcpy(chrome,frame,sizeof(uint32_t)*UW*UH);
 int shown=-1;
 for(;;){
 int pg=state->page>=0&&state->page<NPAGES?state->page:0;
 if(u.sh&&pg!=shown){draw_strip(&u,pg);shown=pg;}
 memcpy(frame,chrome,sizeof(uint32_t)*UW*UH);
 for(int y=0;y<u.ch;y++){uint32_t *out=frame+(u.cy+y)*UW+u.cx;const uint32_t *in=s+row[y]*SRC_W;
  if(!filter)for(int x=0;x<u.cw;x++)out[x]=in[col[x]];
  else{const uint32_t *in2=row[y]+1<SRC_H?in+SRC_W:in;
   for(int x=0;x<u.cw;x++){int c=col[x],c2=c+1<SRC_W?c+1:c;uint32_t a=in[c],b=in[c2],e=in2[c],g=in2[c2];
    out[x]=((a>>2)&0x3f3f3f)+((b>>2)&0x3f3f3f)+((e>>2)&0x3f3f3f)+((g>>2)&0x3f3f3f);}}}   /* per-channel mean, no carry between channels */
 int fresh=state_fresh(state);
 if(u.sh){for(int i=0;i<pages[pg].n;i++){const struct button*b=&pages[pg].buttons[i];int down=state->pressed&(1u<<i);
  int lit=fresh&&b->light&&b->channel>=1&&b->channel<=2&&(state->deck[b->channel-1]&b->light);if(down||lit)drawbutton(&u,pg,i,down,lit);}}
 if(state->cursor_visible){int cx=state->cursor_x,cy=state->cursor_y;   /* arrow pointer: black outline, white fill */
  for(int y=0;y<22;y++)for(int x=0;x<=y&&x<16;x++){int px=cx+x,py=cy+y;if(px<0||px>=UW||py<0||py>=UH)continue;
   int edge=(x==0||x==y||y==21||x==15);frame[py*UW+px]=edge?0x000000:0xffffff;}}
 if(u.bw)for(int i=0;i<6;i++)drawslider(&u,i,state,fresh);
 for(int py=0;py<H;py++){const int *m=idx+py*W;unsigned char *line=d+py*f.line_length;
  if(bpp16){uint16_t*row16=(uint16_t*)line;for(int px=0;px<W;px++){uint32_t c=frame[m[px]];row16[px]=(uint16_t)(((c>>8)&0xf800)|((c>>5)&0x07e0)|((c>>3)&0x001f));}}
  else{uint32_t*row32=(uint32_t*)line;for(int px=0;px<W;px++)row32[px]=frame[m[px]];}}
 usleep(33333);
 }
}
