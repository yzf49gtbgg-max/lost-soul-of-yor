#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <string.h>

PSP_MODULE_INFO("Lost Soul of Yor", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define W 480
#define H 272
#define BW 512
#define PI 3.14159265f

static unsigned int list[262144] __attribute__((aligned(16)));
static int running = 1;
static float px = 0.0f, pz = 3.0f, yaw = 0.0f;
static float bx = 0.0f, bz = -3.0f;
static float bossHP = 100.0f, playerHP = 100.0f, stamina = 100.0f;
static float attackTimer = 0.0f, bossAttackTimer = 1.0f, dodgeTimer = 0.0f, hitFlash = 0.0f;
static int phase = 1, victory = 0;

struct V3C { unsigned int color; float x,y,z; };
struct V2C { unsigned int color; float x,y; };

static struct V3C cube[] = {
 {0xffffffff,-.5f,-.5f,-.5f},{0xffffffff,.5f,-.5f,-.5f},{0xffffffff,.5f,.5f,-.5f},{0xffffffff,-.5f,.5f,-.5f},
 {0xffffffff,-.5f,-.5f,.5f},{0xffffffff,.5f,-.5f,.5f},{0xffffffff,.5f,.5f,.5f},{0xffffffff,-.5f,.5f,.5f}
};
static unsigned short ci[] = {0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,3,2,6,3,6,7,1,5,6,1,6,2,0,3,7,0,7,4};

static int exit_cb(int a,int b,void*c){running=0;return 0;}
static int cb_thread(SceSize a,void*b){int id=sceKernelCreateCallback("Exit",exit_cb,0);sceKernelRegisterExitCallback(id);sceKernelSleepThreadCB();return 0;}
static void setup_callbacks(void){int t=sceKernelCreateThread("cb",cb_thread,0x11,0xFA0,THREAD_ATTR_USER,0);if(t>=0)sceKernelStartThread(t,0,0);}

static void rect(float x,float y,float w,float h,unsigned int color){
 struct V2C v[4]={{color,x,y},{color,x+w,y},{color,x,y+h},{color,x+w,y+h}};
 sceGuDisable(GU_DEPTH_TEST); sceGuDrawArray(GU_TRIANGLE_STRIP,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,0,v); sceGuEnable(GU_DEPTH_TEST);
}
static void box(float x,float y,float z,float sx,float sy,float sz,unsigned int color){
 struct V3C v[8]; int i; for(i=0;i<8;i++){v[i]=cube[i];v[i].color=color;}
 sceGumPushMatrix(); sceGumTranslate(&(ScePspFVector3){x,y,z}); sceGumScale(&(ScePspFVector3){sx,sy,sz});
 sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_INDEX_16BIT|GU_TRANSFORM_3D,36,ci,v); sceGumPopMatrix();
}
static void draw_world(void){
 int i;
 /* dark stone arena */
 for(i=-5;i<=5;i++){
   box((float)i*1.3f,-1.05f,-1.0f,1.25f,.08f,9.0f,(i&1)?0xff25232d:0xff302c37);
 }
 /* broken pillars and distant ruins */
 for(i=0;i<8;i++){
   float x=(i<4?-5.5f:5.5f), z=-5.0f+(i%4)*2.8f;
   box(x,-.1f,z,.38f,2.1f,.38f,0xff292631);
   box(x,.95f,z,.58f,.18f,.58f,0xff514653);
 }
 /* pale moon / spectral light represented by layered floating slabs */
 box(0,4.3f,-8.0f,2.1f,2.1f,.12f,0xff777080);
 /* player: dark cloak, torso, head, sword */
 box(px,-.25f,pz,.62f,1.0f,.42f,0xff17151d);
 box(px,.48f,pz,.34f,.38f,.34f,0xffa9a1a8);
 box(px-.36f,-.45f,pz+.12f,.28f,.85f,.55f,0xff09090e);
 box(px+.48f,-.05f,pz-.18f,.12f,.95f,.12f,0xffc8c8d0);
 /* Yor: spectral white body with violet-black sword */
 box(bx,0.0f,bz,.95f,1.7f,.65f,phase==1?0xffd9d6e2:0xffaaa0bd);
 box(bx,1.05f,bz,.48f,.48f,.45f,0xffeeeaf4);
 box(bx-.72f,.1f,bz,.22f,1.0f,.25f,0xffb9b1c9);
 box(bx+.72f,.1f,bz,.22f,1.0f,.25f,0xffb9b1c9);
 box(bx+1.15f,.0f,bz-.25f,.12f,2.0f,.12f,0xff572a86);
 box(bx+1.15f,1.05f,bz-.25f,.2f,.55f,.2f,0xff17111f);
}
static void draw_hud(void){
 /* minimalist PSP-friendly bars; Arabic text is planned for the next asset pass */
 rect(18,16,184,12,0xff24202a); rect(18,16,184*(playerHP/100.0f),12,0xffa52d45);
 rect(18,34,184,7,0xff24202a); rect(18,34,184*(stamina/100.0f),7,0xff6a9b70);
 rect(278,16,184,12,0xff24202a); rect(278,16,184*(bossHP/100.0f),12,0xff8b315d);
 rect(18,246,444,2,0xff62566c);
 if(victory) rect(150,112,180,34,0xffd9c9ed);
}
static void init_gu(void){
 void *fb0=(void*)0x44000000, *fb1=(void*)0x44088000, *z=(void*)0x44000000+0x110000;
 sceGuInit(); sceGuStart(GU_DIRECT,list);
 sceGuDrawBuffer(GU_PSM_8888,fb0,BW); sceGuDispBuffer(W,H,fb1,BW); sceGuDepthBuffer(z,BW);
 sceGuOffset(2048-W/2,2048-H/2); sceGuViewport(2048,2048,W,H); sceGuScissor(0,0,W,H); sceGuEnable(GU_SCISSOR_TEST);
 sceGuDepthRange(65535,0); sceGuDepthFunc(GU_GEQUAL); sceGuEnable(GU_DEPTH_TEST); sceGuFrontFace(GU_CW); sceGuShadeModel(GU_SMOOTH);
 sceGuClearColor(0xff100e18); sceGuClearDepth(0); sceGuFinish(); sceGuSync(0,0); sceGuDisplay(GU_TRUE);
}
static void frame(void){
 sceGuStart(GU_DIRECT,list); sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
 sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(58.0f,16.0f/9.0f,.5f,80.0f);
 sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
 sceGumLookAt(&(ScePspFVector3){px,2.4f,pz+6.2f},&(ScePspFVector3){px,0.1f,-1.2f},&(ScePspFVector3){0,1,0});
 sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); draw_world(); draw_hud();
 sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}
static void update(void){
 SceCtrlData c; static unsigned int old=0; sceCtrlPeekBufferPositive(&c,1);
 float dx=(c.Lx-128.0f)/128.0f, dz=(c.Ly-128.0f)/128.0f;
 unsigned int pressed=c.Buttons & ~old; old=c.Buttons;
 if(c.Buttons&PSP_CTRL_START) running=0;
 if(dodgeTimer>0)dodgeTimer-=1.0f/60.0f;
 if(attackTimer>0)attackTimer-=1.0f/60.0f;
 if(bossAttackTimer>0)bossAttackTimer-=1.0f/60.0f;
 if(hitFlash>0)hitFlash-=1.0f/60.0f;
 if(!victory){
   if(dodgeTimer<=0){px+=dx*.075f;pz+=dz*.075f;}
   if(px>4.5f)px=4.5f;if(px< -4.5f)px=-4.5f;if(pz>5.0f)pz=5.0f;if(pz< -5.5f)pz=-5.5f;
   if((pressed&PSP_CTRL_CIRCLE)&&stamina>=18){dodgeTimer=.32f;stamina-=18;}
   if((pressed&PSP_CTRL_SQUARE)&&attackTimer<=0&&stamina>=10){attackTimer=.35f;stamina-=10;if(fabsf(px-bx)<1.8f&&fabsf(pz-bz)<2.1f)bossHP-=phase==1?8:6;}
   if((pressed&PSP_CTRL_TRIANGLE)&&attackTimer<=0&&stamina>=25){attackTimer=.65f;stamina-=25;if(fabsf(px-bx)<2.0f&&fabsf(pz-bz)<2.3f)bossHP-=phase==1?17:13;}
   if((pressed&PSP_CTRL_CROSS)&&stamina>=5){stamina-=5;}
   if(stamina<100)stamina+=.12f;
   if(bossHP<=50)phase=2;
   if(bossHP<=0){bossHP=0;victory=1;}
   /* boss advances and periodically strikes */
   float vx=px-bx, vz=pz-bz, d=sqrtf(vx*vx+vz*vz)+.001f;
   if(d>1.55f){bx+=vx/d*.018f;bz+=vz/d*.018f;}
   if(bossAttackTimer<=0){if(d<2.25f&&dodgeTimer<=0)playerHP-=phase==1?9:14;bossAttackTimer=phase==1?2.1f:1.25f;}
   if(playerHP<0)playerHP=0;
 }
}
int main(void){
 setup_callbacks(); sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);init_gu();
 while(running){update();frame();}
 sceGuTerm();sceKernelExitGame();return 0;
}
