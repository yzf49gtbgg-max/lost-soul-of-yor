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
#define WORLD_MIN_X -8.1f
#define WORLD_MAX_X  8.1f
#define WORLD_MIN_Z -9.25f
#define WORLD_MAX_Z  8.0f

static unsigned int list[262144] __attribute__((aligned(16)));
static int running = 1;
static float px = 0.0f, pz = 6.0f, playerYaw = 0.0f;
static float bx = 0.0f, bz = -5.7f;
static float bossHP = 100.0f, playerHP = 100.0f, stamina = 100.0f, staminaRegenDelay = 0.0f;
static float attackTimer = 0.0f, bossAttackTimer = 1.0f, bossWindup = 0.0f, dodgeTimer = 0.0f, hitFlash = 0.0f;
static float cameraYaw = 0.0f, cameraPitch = 0.24f, gait = 0.0f, worldTime = 0.0f;
static float dodgeX = 0.0f, dodgeZ = -1.0f;
static float hurtTimer = 0.0f;
static int phase = 1, victory = 0, defeated = 0, targetLocked = 0, playerMoving = 0, guarding = 0, bossStrikePending = 0;
static int flaskCharges = 3;
static int paused = 0;

struct V3C { unsigned int color; float x, y, z; };
struct V2C { unsigned int color; float x, y; };
struct Collider { float x, y, z, hx, hy, hz; };

static const struct V3C cube[] = {
 {0xffffffff,-.5f,-.5f,-.5f},{0xffffffff,.5f,-.5f,-.5f},{0xffffffff,.5f,.5f,-.5f},{0xffffffff,-.5f,.5f,-.5f},
 {0xffffffff,-.5f,-.5f,.5f},{0xffffffff,.5f,-.5f,.5f},{0xffffffff,.5f,.5f,.5f},{0xffffffff,-.5f,.5f,.5f}
};
static const unsigned short ci[] = {0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,3,2,6,3,6,7,1,5,6,1,6,2,0,3,7,0,7,4};

/* Walkable ruined route: side rubble, broken columns, and the existing boss gate. */
static const struct Collider colliders[] = {
 { -5.10f,1.05f, 2.05f,.62f,1.05f,.75f },
 {  5.20f,1.10f, 1.55f,.72f,1.10f,.85f },
 { -4.45f,.42f,-1.35f,1.35f,.42f,.42f },
 {  4.55f,.45f,-2.15f,1.25f,.45f,.45f },
 { -2.55f,1.35f,-4.20f,.42f,1.35f,.42f },
 {  2.65f,1.30f,-4.55f,.42f,1.30f,.42f },
 { -2.05f,1.50f,-8.35f,.40f,1.50f,.48f },
 {  2.05f,1.50f,-8.35f,.40f,1.50f,.48f }
};
#define COLLIDER_COUNT (sizeof(colliders) / sizeof(colliders[0]))

static int exit_cb(int a, int b, void *c) { (void)a; (void)b; (void)c; running = 0; return 0; }
static int cb_thread(SceSize a, void *b) {
 int id;
 (void)a; (void)b;
 id = sceKernelCreateCallback("Exit", exit_cb, 0);
 if (id >= 0) sceKernelRegisterExitCallback(id);
 sceKernelSleepThreadCB();
 return 0;
}
static void setup_callbacks(void) {
 int t = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, THREAD_ATTR_USER, 0);
 if (t >= 0) sceKernelStartThread(t, 0, 0);
}

static void rect(float x, float y, float w, float h, unsigned int color) {
 struct V2C v[4] = {{color,x,y},{color,x+w,y},{color,x,y+h},{color,x+w,y+h}};
 sceGuDisable(GU_DEPTH_TEST);
 sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 4, 0, v);
 sceGuEnable(GU_DEPTH_TEST);
}
static void box(float x, float y, float z, float sx, float sy, float sz, unsigned int color) {
 struct V3C v[8];
 int i;
 for (i=0; i<8; i++) { v[i] = cube[i]; v[i].color = color; }
 sceGumPushMatrix();
 sceGumTranslate(&(ScePspFVector3){x,y,z});
 sceGumScale(&(ScePspFVector3){sx,sy,sz});
 sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888|GU_VERTEX_32BITF|GU_INDEX_16BIT|GU_TRANSFORM_3D, 36, ci, v);
 sceGumPopMatrix();
}
static int point_in_collider(float x, float y, float z, float padding) {
 unsigned int i;
 for (i=0; i<COLLIDER_COUNT; i++) {
  const struct Collider *c = &colliders[i];
  if (fabsf(x-c->x) < c->hx+padding && fabsf(z-c->z) < c->hz+padding && y < c->y+c->hy+padding && y > c->y-c->hy-padding) return 1;
 }
 return 0;
}
static int player_blocked(float x, float z, float radius) {
 unsigned int i;
 if (x < WORLD_MIN_X || x > WORLD_MAX_X || z < WORLD_MIN_Z || z > WORLD_MAX_Z) return 1;
 for (i=0; i<COLLIDER_COUNT; i++) {
  const struct Collider *c = &colliders[i];
  if (fabsf(x-c->x) < c->hx+radius && fabsf(z-c->z) < c->hz+radius) return 1;
 }
 return 0;
}
static void move_player(float dx, float dz) {
 float nx = px + dx, nz = pz + dz;
 if (!player_blocked(nx, pz, .30f)) px = nx;
 if (!player_blocked(px, nz, .30f)) pz = nz;
 if (px < WORLD_MIN_X+.30f) px = WORLD_MIN_X+.30f;
 if (px > WORLD_MAX_X-.30f) px = WORLD_MAX_X-.30f;
 if (pz < WORLD_MIN_Z+.30f) pz = WORLD_MIN_Z+.30f;
 if (pz > WORLD_MAX_Z-.30f) pz = WORLD_MAX_Z-.30f;
}
static void move_boss(float dx, float dz) {
 float nx = bx + dx, nz = bz + dz;
 if (!player_blocked(nx, bz, .48f)) bx = nx;
 if (!player_blocked(bx, nz, .48f)) bz = nz;
 if (bx < WORLD_MIN_X+.5f) bx = WORLD_MIN_X+.5f;
 if (bx > WORLD_MAX_X-.5f) bx = WORLD_MAX_X-.5f;
 if (bz < WORLD_MIN_Z+.5f) bz = WORLD_MIN_Z+.5f;
 if (bz > WORLD_MAX_Z-.5f) bz = WORLD_MAX_Z-.5f;
}

static int player_faces_boss(void) {
 float dx=bx-px, dz=bz-pz;
 float distance=sqrtf(dx*dx+dz*dz)+.001f;
 float facing=dx*(-sinf(playerYaw))+dz*(-cosf(playerYaw));
 return facing>distance*.35f;
}

static int player_can_hit(float range) {
 float dx=bx-px, dz=bz-pz;
 float distance=sqrtf(dx*dx+dz*dz)+.001f;
 float forwardX=-sinf(playerYaw), forwardZ=-cosf(playerYaw);
 float facing=dx*forwardX+dz*forwardZ;
 return distance<range && facing>distance*.35f;
}

static void draw_column(float x, float z, float height) {
 box(x, height*.5f, z, .62f, height, .62f, 0xff282530);
 box(x, height+.08f, z, .82f, .16f, .82f, 0xff514957);
 box(x, .10f, z, .88f, .20f, .88f, 0xff38333e);
}
static void draw_world(void) {
 int x, z;
 /* Uneven stone floor tiles make a connected route rather than a flat arena. */
 for (z=-9; z<=7; z+=2) {
  for (x=-7; x<=7; x+=2) {
   unsigned int c = (((x+z)/2)&1) ? 0xff302d36 : 0xff39343e;
   box((float)x,-.12f,(float)z,1.96f,.12f,1.96f,c);
  }
 }
 /* Outer ruin walls and a gated arch frame the walkable area. */
 box(-8.05f,1.05f,-.55f,.42f,2.10f,17.3f,0xff282530);
 box( 8.05f,1.05f,-.55f,.42f,2.10f,17.3f,0xff282530);
 box(-5.45f,.95f,-9.18f,4.75f,1.90f,.42f,0xff282530);
 box( 5.45f,.95f,-9.18f,4.75f,1.90f,.42f,0xff282530);
 box(0.0f,2.80f,-8.92f,2.05f,.46f,.55f,0xff3c3542);
 box(-2.05f,1.56f,-8.35f,.80f,3.12f,.96f,0xff342f3a);
 box( 2.05f,1.56f,-8.35f,.80f,3.12f,.96f,0xff342f3a);
 box(-2.05f,3.18f,-8.35f,.92f,.28f,1.00f,0xff514957);
 box( 2.05f,3.18f,-8.35f,.92f,.28f,1.00f,0xff514957);
 /* Broken columns and rubble leave multiple clear paths through the ruins. */
 draw_column(-5.10f, 2.05f, 2.10f);
 draw_column( 5.20f, 1.55f, 2.20f);
 draw_column(-2.55f,-4.20f, 2.70f);
 draw_column( 2.65f,-4.55f, 2.60f);
 box(-4.45f,.42f,-1.35f,2.70f,.84f,.84f,0xff514957);
 box( 4.55f,.45f,-2.15f,2.50f,.90f,.90f,0xff514957);
 box(-5.90f,.58f,-5.25f,1.55f,1.16f,1.10f,0xff302c36);
 box( 5.85f,.50f,-5.80f,1.70f,1.00f,1.05f,0xff302c36);
 /* Low broken side walls and cracked lintels add depth without blocking the route. */
 box(-6.65f,.55f,-.80f,.42f,1.10f,4.20f,0xff37323c);
 box( 6.70f,.55f,-1.40f,.42f,1.10f,4.60f,0xff37323c);
 box(-4.90f,2.55f,-6.80f,2.30f,.22f,.30f,0xff4a434f);
 box( 4.75f,2.20f,-7.20f,2.50f,.22f,.32f,0xff4a434f);
}

static void draw_limb(float x, float y, float z, float length, float width, float swing, unsigned int color) {
 sceGumPushMatrix();
 sceGumTranslate(&(ScePspFVector3){x,y,z});
 sceGumRotateX(swing);
 box(0.0f,-length*.48f,0.0f,width,length,width*.72f,color);
 sceGumPopMatrix();
}
static void draw_lloyd(void) {
 float stride = playerMoving ? sinf(gait)*.48f : 0.0f;
 float bob = playerMoving ? fabsf(sinf(gait*2.0f))*.045f : 0.0f;
 unsigned int armor = hitFlash > 0.0f ? 0xff756571 : 0xff17171e;
 sceGumPushMatrix();
 sceGumTranslate(&(ScePspFVector3){px,0.0f,pz});
 sceGumRotateY(playerYaw);
 /* Articulated low-poly body: independently animated arms and legs. */
 box(0.0f,1.20f+bob,0.0f,.76f,1.02f,.46f,armor);
 box(0.0f,1.19f+bob,.28f,.82f,1.20f,.22f,0xff0c0c12);
 box(0.0f,1.70f+bob,-.015f,.86f,.34f,.50f,0xff24232d);
 box(-.43f,1.66f+bob,-.02f,.20f,.30f,.54f,0xff34313b);
 box( .43f,1.66f+bob,-.02f,.20f,.30f,.54f,0xff34313b);
 draw_limb(-.20f,.78f,0.0f,.78f,.27f,stride,0xff202029);
 draw_limb( .20f,.78f,0.0f,.78f,.27f,-stride,0xff202029);
 box(-.20f,.12f,-.04f,.36f,.20f,.48f,0xff111117);
 box( .20f,.12f,-.04f,.36f,.20f,.48f,0xff111117);
 draw_limb(-.52f,1.55f,0.0f,.72f,.23f,-stride*.72f,0xff191820);
 draw_limb( .52f,1.55f,0.0f,.72f,.23f,stride*.72f,0xff191820);
 box(-.52f,1.14f,-.02f,.25f,.22f,.28f,0xff302d38);
 box( .52f,1.14f,-.02f,.25f,.22f,.28f,0xff302d38);
 if (guarding) box(0.0f,1.18f,-.62f,.72f,.92f,.12f,0xff6c607d);
 /* Visible face and dark short hair follow the approved reference direction. */
 box(0.0f,2.02f+bob,-.035f,.42f,.48f,.36f,0xffc8a99c);
 box(0.0f,2.27f+bob,-.03f,.48f,.22f,.42f,0xff121218);
 box(-.17f,2.13f+bob,-.22f,.16f,.22f,.12f,0xff17171e);
 box( .14f,2.13f+bob,-.22f,.16f,.22f,.12f,0xff17171e);
 box(-.09f,2.03f+bob,-.228f,.045f,.035f,.025f,0xff211a1b);
 box( .09f,2.03f+bob,-.228f,.045f,.035f,.025f,0xff211a1b);
 box(0.0f,1.79f+bob,-.03f,.48f,.12f,.40f,0xff1b1a22);
 /* Drawn blade: it sweeps across the body during the quick/heavy attack window. */
 sceGumPushMatrix();
 sceGumTranslate(&(ScePspFVector3){.52f,1.18f,-.28f});
 sceGumRotateY(attackTimer>0.0f ? (attackTimer>(attackTimer>.36f?.36f:.18f)?-.92f:.92f) : -.18f);
 box(0.0f,-.43f,-.10f,.105f,.88f,.105f,0xffc5c3d0);
 box(0.0f,.035f,-.10f,.27f,.11f,.18f,0xff41344d);
 box(0.0f,.16f,-.10f,.12f,.16f,.13f,0xff17131e);
 sceGumPopMatrix();
 /* Sheathed sword remains visible on the back as a second silhouette. */
 box(.36f,1.30f,.34f,.10f,.92f,.10f,0xffb9b5c2);
 box(.36f,1.76f,.34f,.28f,.10f,.20f,0xff403348);
 sceGumPopMatrix();
}
static void draw_yor_spirit(void) {
 float hover=sinf(worldTime*2.1f)*.10f;
 float sway=sinf(worldTime*1.35f)*.075f;
 float hair=sinf(worldTime*1.8f)*.045f;
 unsigned int robe=phase==1?0xffe5e1ec:0xffbdb1d2;
 unsigned int shade=phase==1?0xffc9c2d5:0xff9586b2;
 /* A warning sigil stays on the floor while Yor commits to her strike. */
 if (bossWindup>0.0f) box(bx,.035f,bz,2.25f,.045f,2.25f,0xff633b83);
 /* Turn Yor toward Lloyd so her weapon and robe silhouette follow the duel. */
 sceGumPushMatrix();
 sceGumTranslate(&(ScePspFVector3){bx,0.0f,bz});
 sceGumRotateY(atan2f(-(px-bx),-(pz-bz)));
 sceGumTranslate(&(ScePspFVector3){-bx,0.0f,-bz});
 /* Hovering layered spirit robes, with separated trailing tatters. */
 box(bx,1.02f+hover,bz,.92f,1.18f,.66f,robe);
 box(bx,1.45f+hover,bz+.015f,.72f,.92f,.58f,0xfff0edf5);
 box(bx+sway*.4f,.43f+hover,bz-.04f,.76f,.78f,.58f,shade);
 box(bx-.34f-sway,.25f+hover,bz-.10f,.20f,.62f,.22f,robe);
 box(bx+.36f+sway,.28f+hover,bz-.08f,.18f,.58f,.20f,robe);
 /* Veiled head, long dark hair and pale spectral face. */
 box(bx,1.92f+hover,bz,.48f,.50f,.44f,0xfff2eff8);
 box(bx,2.13f+hover,bz-.035f,.52f,.25f,.48f,0xffd8d2e3);
 box(bx,1.92f+hover,bz-.255f,.38f,.23f,.035f,0xff756b86);
 box(bx-.22f,1.58f+hover+hair,bz-.12f,.13f,.82f,.16f,0xff282331);
 box(bx+.22f,1.56f+hover-hair,bz-.12f,.13f,.86f,.16f,0xff282331);
 box(bx,1.55f+hover,bz+.25f,.40f,.92f,.12f,0xff302a3b);
 /* Sleeves drift away from the body like torn cloth. */
 box(bx-.62f-sway,1.12f+hover,bz,.24f,.92f,.28f,robe);
 box(bx+.62f+sway,1.10f+hover,bz,.24f,.90f,.28f,robe);
 box(bx-.72f-sway,.63f+hover,bz-.03f,.18f,.42f,.20f,shade);
 box(bx+.72f+sway,.61f+hover,bz-.03f,.18f,.42f,.20f,shade);
 /* Black spirit blade with a violet core and crossguard. */
 box(bx+.94f,1.12f+hover,bz-.25f,.15f,1.48f,.16f,0xff17111f);
 box(bx+.94f,1.20f+hover,bz-.25f,.055f,1.18f,.07f,0xff7138a9);
 box(bx+.94f,1.88f+hover,bz-.25f,.36f,.10f,.20f,0xff30213f);
 box(bx+.94f,.34f+hover,bz-.25f,.22f,.22f,.22f,0xff241a31);
 /* Small drifting fragments make the spirit silhouette readable in motion. */
 box(bx-.82f-sway,1.78f+hover,bz-.12f,.10f,.10f,.10f,0xffa985dc);
 box(bx+.82f+sway,1.42f+hover,bz+.02f,.09f,.12f,.09f,0xff8051c0);
 box(bx-.55f,2.35f+hover,bz+.06f,.08f,.08f,.08f,0xffd5c2f0);
 sceGumPopMatrix();
}
static int camera_blocked(float x, float y, float z) {
 if (x < WORLD_MIN_X || x > WORLD_MAX_X || z < WORLD_MIN_Z || z > WORLD_MAX_Z) return 1;
 return point_in_collider(x,y,z,.16f);
}
static void camera_pose(float *ex, float *ey, float *ez, float *tx, float *ty, float *tz) {
 float dist=5.6f, horiz, dx, dy, dz, t, desiredX, desiredY, desiredZ;
 *tx = targetLocked && bossHP>0.0f ? bx : px;
 *ty = targetLocked && bossHP>0.0f ? 1.05f : 1.18f;
 *tz = targetLocked && bossHP>0.0f ? bz : pz;
 horiz = dist*cosf(cameraPitch);
 dx = sinf(cameraYaw)*horiz;
 dy = sinf(cameraPitch)*dist;
 dz = cosf(cameraYaw)*horiz;
 desiredX=px+dx; desiredY=1.25f+dy; desiredZ=pz+dz;
 for (t=.08f; t<=1.0f; t+=.05f) {
  float sx=*tx+(desiredX-*tx)*t;
  float sy=*ty+(desiredY-*ty)*t;
  float sz=*tz+(desiredZ-*tz)*t;
  if (camera_blocked(sx,sy,sz)) { dist=fmaxf(1.45f,dist*(t-.07f)); break; }
 }
 horiz = dist*cosf(cameraPitch);
 *ex = px + sinf(cameraYaw)*horiz;
 *ey = 1.25f + sinf(cameraPitch)*dist;
 *ez = pz + cosf(cameraYaw)*horiz;
}
static void draw_hud(void) {
 float php=fmaxf(0.0f,fminf(1.0f,playerHP/100.0f));
 float st=fmaxf(0.0f,fminf(1.0f,stamina/100.0f));
 float bhp=fmaxf(0.0f,fminf(1.0f,bossHP/100.0f));
 rect(18,16,184,12,0xff24202a); rect(18,16,184*php,12,0xffa52d45);
 rect(18,34,184,7,0xff24202a); rect(18,34,184*st,7,0xff6a9b70);
 if (bossHP>0.0f) { rect(278,16,184,12,0xff24202a); rect(278,16,184*bhp,12,0xff8b315d); }
 /* Three healing flasks; filled markers show the remaining charges. */
 rect(18,222,54,5,0xff24202a);
 { int i; for (i=0;i<3;i++) { unsigned int c=i<flaskCharges?0xff65b7ad:0xff49434f; rect(20+i*17,211,10,10,c); rect(22+i*17,208,6,4,c); } }
 rect(18,246,444,2,targetLocked?0xffa36acb:0xff62566c);
 if (victory) { rect(150,108,180,4,0xffd9c9ed); rect(150,112,180,34,0xff393044); rect(150,146,180,4,0xffd9c9ed); }
 if (defeated) { rect(112,102,256,5,0xffa52d45); rect(112,107,256,58,0xff21151f); rect(112,165,256,5,0xffa52d45); rect(222,184,36,20,0xffd9c9ed); }
 if (paused) {
  rect(132,70,216,4,0xffbda9d4); rect(132,74,216,124,0xff211d2a); rect(132,198,216,4,0xffbda9d4);
  rect(164,102,152,7,0xffd9c9ed); rect(164,126,112,5,0xff71647f); rect(164,150,132,5,0xff71647f);
 }
}
static void init_gu(void) {
 void *fb0=(void*)0x44000000, *fb1=(void*)0x44088000, *z=(void*)0x44110000;
 sceGuInit(); sceGuStart(GU_DIRECT,list);
 sceGuDrawBuffer(GU_PSM_8888,fb0,BW); sceGuDispBuffer(W,H,fb1,BW); sceGuDepthBuffer(z,BW);
 sceGuOffset(2048-W/2,2048-H/2); sceGuViewport(2048,2048,W,H); sceGuScissor(0,0,W,H); sceGuEnable(GU_SCISSOR_TEST);
 sceGuDepthRange(65535,0); sceGuDepthFunc(GU_GEQUAL); sceGuEnable(GU_DEPTH_TEST); sceGuFrontFace(GU_CW); sceGuShadeModel(GU_SMOOTH);
 sceGuClearColor(0xff100e18); sceGuClearDepth(0); sceGuFinish(); sceGuSync(0,0); sceGuDisplay(GU_TRUE);
}
static void frame(void) {
 float ex,ey,ez,tx,ty,tz;
 camera_pose(&ex,&ey,&ez,&tx,&ty,&tz);
 sceGuStart(GU_DIRECT,list); sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
 sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(58.0f,16.0f/9.0f,.5f,80.0f);
 sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
 sceGumLookAt(&(ScePspFVector3){ex,ey,ez},&(ScePspFVector3){tx,ty,tz},&(ScePspFVector3){0,1,0});
 sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
 draw_world(); draw_lloyd(); if (!victory) draw_yor_spirit(); draw_hud();
 sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}
static void update(void) {
 SceCtrlData c;
 static unsigned int old=0;
 float dt=1.0f/60.0f, ax, ay, mx, mz, length;
 worldTime+=dt;
 unsigned int pressed;
 float vx, vz, distance;
 sceCtrlPeekBufferPositive(&c,1);
 pressed=c.Buttons & ~old; old=c.Buttons;
 if (paused) { if (pressed & (PSP_CTRL_START|PSP_CTRL_CROSS)) paused=0; return; }
 if (pressed & PSP_CTRL_START) { paused=1; return; }
 if (c.Buttons & PSP_CTRL_LEFT) cameraYaw-=.025f;
 if (c.Buttons & PSP_CTRL_RIGHT) cameraYaw+=.025f;
 if (c.Buttons & PSP_CTRL_UP) cameraPitch=fminf(.68f,cameraPitch+.018f);
 if (c.Buttons & PSP_CTRL_DOWN) cameraPitch=fmaxf(.10f,cameraPitch-.018f);
 if ((pressed & PSP_CTRL_LTRIGGER) && bossHP>0.0f) {
  float tx=bx-px, tz=bz-pz;
  if (targetLocked) targetLocked=0;
  else if (sqrtf(tx*tx+tz*tz)<10.0f) targetLocked=1;
 }
 if (bossHP<=0.0f) targetLocked=0;
 ax=(c.Lx-128.0f)/128.0f; ay=(c.Ly-128.0f)/128.0f;
 if (fabsf(ax)<.16f) ax=0.0f;
 if (fabsf(ay)<.16f) ay=0.0f;
 length=sqrtf(ax*ax+ay*ay);
 if (length>1.0f) { ax/=length; ay/=length; }
 mx=ax*cosf(cameraYaw)+ay*sinf(cameraYaw);
 mz=-ax*sinf(cameraYaw)+ay*cosf(cameraYaw);
 playerMoving=(fabsf(mx)+fabsf(mz))>.05f;
 if (playerMoving) {
  gait+=dt*10.0f;
  if (!targetLocked) playerYaw=atan2f(-mx,-mz);
 }
 if (targetLocked) playerYaw=atan2f(-(bx-px),-(bz-pz));
 if (dodgeTimer>0.0f) dodgeTimer-=dt;
 if (attackTimer>0.0f) attackTimer-=dt;
 if (bossAttackTimer>0.0f) bossAttackTimer-=dt;
 if (bossWindup>0.0f) { bossWindup-=dt; if (bossWindup<=0.0f) { bossWindup=0.0f; bossStrikePending=1; } }
 if (hitFlash>0.0f) hitFlash-=dt;
 if (hurtTimer>0.0f) hurtTimer-=dt;
 guarding = (!victory && !defeated && (c.Buttons & PSP_CTRL_RTRIGGER) && stamina>0.0f);
 if (defeated && (pressed & PSP_CTRL_CROSS)) {
  px=0.0f; pz=6.0f; playerYaw=0.0f; bx=0.0f; bz=-5.7f;
  bossHP=100.0f; playerHP=100.0f; stamina=100.0f; staminaRegenDelay=0.0f; flaskCharges=3;
  attackTimer=0.0f; bossAttackTimer=1.0f; bossWindup=0.0f; bossStrikePending=0; dodgeTimer=0.0f; hurtTimer=0.0f;
  phase=1; victory=0; defeated=0; targetLocked=0; guarding=0;
 }
 if (victory && (pressed & PSP_CTRL_CROSS)) {
  px=0.0f; pz=6.0f; playerYaw=0.0f; bx=0.0f; bz=-5.7f;
  bossHP=100.0f; playerHP=100.0f; stamina=100.0f; staminaRegenDelay=0.0f; flaskCharges=3;
  attackTimer=0.0f; bossAttackTimer=1.0f; bossWindup=0.0f; bossStrikePending=0; dodgeTimer=0.0f; hurtTimer=0.0f;
  phase=1; victory=0; defeated=0; targetLocked=0; guarding=0;
 }
 if (!victory && !defeated) {
  /* Select consumes one of three flasks and restores health, never exceeding max HP. */
  if ((pressed&PSP_CTRL_SELECT) && flaskCharges>0 && playerHP<100.0f) {
   playerHP=fminf(100.0f,playerHP+35.0f);
   flaskCharges--;
   hitFlash=.12f;
  }
  if ((pressed&PSP_CTRL_CIRCLE) && stamina>=18.0f && dodgeTimer<=0.0f) {
   dodgeTimer=.30f; stamina-=18.0f; staminaRegenDelay=.75f;
   if (playerMoving) { dodgeX=mx; dodgeZ=mz; }
   else { dodgeX=-sinf(playerYaw); dodgeZ=-cosf(playerYaw); }
  }
  if (dodgeTimer>0.0f) move_player(dodgeX*5.8f*dt,dodgeZ*5.8f*dt);
  else move_player(mx*3.2f*dt,mz*3.2f*dt);
  if ((pressed&PSP_CTRL_SQUARE) && attackTimer<=0.0f && stamina>=10.0f) {
   attackTimer=.36f; stamina-=10.0f; staminaRegenDelay=.75f;
   if (player_can_hit(2.35f)) { bossHP-=8.0f; hitFlash=.12f; }
  }
  if ((pressed&PSP_CTRL_TRIANGLE) && attackTimer<=0.0f && stamina>=25.0f) {
   attackTimer=.68f; stamina-=25.0f; staminaRegenDelay=.90f;
   if (player_can_hit(2.85f)) { bossHP-=17.0f; hitFlash=.18f; }
  }
  if (guarding) { stamina=fmaxf(0.0f,stamina-.22f); staminaRegenDelay=.20f; }
  else if (staminaRegenDelay>0.0f) staminaRegenDelay=fmaxf(0.0f,staminaRegenDelay-dt);
  else if (stamina<100.0f) stamina=fminf(100.0f,stamina+.14f);
  if (bossHP<=50.0f) phase=2;
  if (bossHP<=0.0f) { bossHP=0.0f; victory=1; targetLocked=0; }
  vx=px-bx; vz=pz-bz; distance=sqrtf(vx*vx+vz*vz)+.001f;
  if (distance>1.60f) { float chase=phase==1?.018f:.030f; move_boss(vx/distance*chase,vz/distance*chase); }
  /* Yor telegraphs each close-range strike before the hit check. The warning
     gives the player a readable dodge/guard window instead of instant damage. */
  if (bossStrikePending) {
   if (distance<2.55f && dodgeTimer<=0.0f && hurtTimer<=0.0f) {
    if (guarding && stamina>=12.0f && player_faces_boss()) { stamina-=12.0f; hitFlash=.10f; }
    else { playerHP-=phase==1?9.0f:14.0f; hitFlash=.16f; hurtTimer=.48f; }
   }
   bossStrikePending=0;
   bossAttackTimer=phase==1?2.1f:1.25f;
  } else if (bossAttackTimer<=0.0f && bossWindup<=0.0f) {
   if (distance<2.75f) bossWindup=phase==1?.48f:.36f;
   else bossAttackTimer=.22f;
  }
  if (playerHP<0.0f) playerHP=0.0f;
  if (playerHP<=0.0f) { defeated=1; guarding=0; targetLocked=0; }
 }
}
int main(void) {
 setup_callbacks(); sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG); init_gu();
 while (running) { update(); frame(); }
 sceGuTerm(); sceKernelExitGame(); return 0;
}
