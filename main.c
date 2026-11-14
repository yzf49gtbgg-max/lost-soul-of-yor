#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspaudio.h>
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
static float attackTimer = 0.0f, bossAttackTimer = 1.0f, bossWindup = 0.0f, dodgeTimer = 0.0f, hitFlash = 0.0f, bossFlash = 0.0f;
static float cameraYaw = 0.0f, cameraPitch = 0.24f, gait = 0.0f, worldTime = 0.0f;
static float dodgeX = 0.0f, dodgeZ = -1.0f;
static float hurtTimer = 0.0f;
static int phase = 1, victory = 0, defeated = 0, targetLocked = 0, playerMoving = 0, guarding = 0, bossStrikePending = 0;
static int flaskCharges = 3;
static int paused = 0;
static int bossAwake = 0;
static int titleScreen = 1;
static int lockedGuard=-1;
static int tutorialScreen=0;
static int storyScreen=0;
static int audioChannel=-1;
static float audioTime=0.0f;
static short audioBuffer[512] __attribute__((aligned(64)));
static float guardX[2]={-1.65f,1.65f}, guardZ[2]={0.25f,-2.45f};
static float guardHP[2]={42.0f,52.0f}, guardAttack[2]={1.1f,1.8f}, guardWindup[2]={0.0f,0.0f};
static int guardAlive[2]={1,1};
static float shardX[3]={-3.45f,3.55f,-.15f}, shardZ[3]={3.15f,-.55f,-3.55f};
static int shardTaken[3]={0,0,0};
static int shardCount=0;

struct V3C { unsigned int color; float x, y, z; };
struct V2C { unsigned int color; float x, y; };
#define SPHERE_LAT 6
#define SPHERE_LON 10
static ScePspFVector3 sphereVerts[(SPHERE_LAT+1)*(SPHERE_LON+1)];
static unsigned short sphereIndices[SPHERE_LAT*SPHERE_LON*6];
struct Collider { float x, y, z, hx, hy, hz; };
struct CheckpointData {
 unsigned int magic, version;
 float px,pz,playerHP,stamina,bossHP;
 int flaskCharges,phase,bossAwake,shardCount;
 int shardTaken[3],guardAlive[2];
 float guardX[2],guardZ[2],guardHP[2];
};
static const char savePath[]="ms0:/PSP/SAVEDATA/LOSTSOULYOR/DATA.BIN";


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

static void audio_init(void) {
 audioChannel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,256,PSP_AUDIO_FORMAT_STEREO);
}
static void audio_update(void) {
 static const float notes[8]={110.00f,164.81f,146.83f,196.00f,130.81f,164.81f,110.00f,98.00f};
 int i,step;
 float f, t, sample;
 if(audioChannel<0) return;
 for(i=0;i<256;i++) {
  t=audioTime+(float)i/44100.0f;
  step=((int)(t*1.65f))&7;
  f=notes[step];
  sample=sinf(2.0f*PI*f*t)*.075f+sinf(2.0f*PI*(f*.5f)*t)*.045f;
  if(bossAwake) sample+=sinf(2.0f*PI*(f*1.5f)*t)*.025f;
  audioBuffer[i*2]=(short)(sample*32767.0f);
  audioBuffer[i*2+1]=(short)(sample*32767.0f);
 }
 sceAudioOutputBlocking(audioChannel,PSP_AUDIO_VOLUME_MAX,audioBuffer);
 audioTime+=256.0f/44100.0f;
}
static int audio_thread(SceSize args, void *argp) {
 (void)args; (void)argp;
 audio_init();
 if(audioChannel<0) return 0;
 while(running) audio_update();
 sceAudioChRelease(audioChannel);
 audioChannel=-1;
 return 0;
}
static void audio_start(void) {
 SceUID tid=sceKernelCreateThread("yor_audio",audio_thread,0x20,0x4000,THREAD_ATTR_USER,0);
 if(tid>=0) sceKernelStartThread(tid,0,0);
}
static void save_checkpoint(void) {
 struct CheckpointData d;
 SceUID fd;
 memset(&d,0,sizeof(d));
 d.magic=0x594F5253; d.version=1;
 d.px=px; d.pz=pz; d.playerHP=playerHP; d.stamina=stamina; d.bossHP=bossHP;
 d.flaskCharges=flaskCharges; d.phase=phase; d.bossAwake=bossAwake; d.shardCount=shardCount;
 memcpy(d.shardTaken,shardTaken,sizeof(shardTaken));
 memcpy(d.guardAlive,guardAlive,sizeof(guardAlive));
 memcpy(d.guardX,guardX,sizeof(guardX)); memcpy(d.guardZ,guardZ,sizeof(guardZ));
 memcpy(d.guardHP,guardHP,sizeof(guardHP));
 sceIoMkdir("ms0:/PSP/SAVEDATA",0777);
 sceIoMkdir("ms0:/PSP/SAVEDATA/LOSTSOULYOR",0777);
 fd=sceIoOpen(savePath,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
 if(fd>=0) { sceIoWrite(fd,&d,sizeof(d)); sceIoClose(fd); }
}
static int load_checkpoint(void) {
 struct CheckpointData d;
 SceUID fd=sceIoOpen(savePath,PSP_O_RDONLY,0777);
 int n;
 if(fd<0) return 0;
 n=sceIoRead(fd,&d,sizeof(d)); sceIoClose(fd);
 if(n!=(int)sizeof(d) || d.magic!=0x594F5253 || d.version!=1) return 0;
 px=d.px; pz=d.pz; playerHP=fmaxf(1.0f,fminf(100.0f,d.playerHP));
 stamina=fmaxf(0.0f,fminf(100.0f,d.stamina)); bossHP=fmaxf(0.0f,fminf(100.0f,d.bossHP));
 flaskCharges=d.flaskCharges<0?0:(d.flaskCharges>3?3:d.flaskCharges);
 phase=d.phase==2?2:1; bossAwake=d.bossAwake?1:0; victory=(bossHP<=0.0f); defeated=0;
 shardCount=d.shardCount<0?0:(d.shardCount>3?3:d.shardCount);
 memcpy(shardTaken,d.shardTaken,sizeof(shardTaken));
 memcpy(guardAlive,d.guardAlive,sizeof(guardAlive));
 memcpy(guardX,d.guardX,sizeof(guardX)); memcpy(guardZ,d.guardZ,sizeof(guardZ));
 memcpy(guardHP,d.guardHP,sizeof(guardHP));
 return 1;
}
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

static void sphere_init(void) {
 int i,j,k=0;
 for(i=0;i<=SPHERE_LAT;i++) {
  float lat=-PI*.5f+PI*(float)i/(float)SPHERE_LAT;
  float ring=cosf(lat), y=sinf(lat);
  for(j=0;j<=SPHERE_LON;j++) {
   float lon=2.0f*PI*(float)j/(float)SPHERE_LON;
   sphereVerts[i*(SPHERE_LON+1)+j].x=ring*cosf(lon);
   sphereVerts[i*(SPHERE_LON+1)+j].y=y;
   sphereVerts[i*(SPHERE_LON+1)+j].z=ring*sinf(lon);
  }
 }
 for(i=0;i<SPHERE_LAT;i++) for(j=0;j<SPHERE_LON;j++) {
  unsigned short a=(unsigned short)(i*(SPHERE_LON+1)+j);
  unsigned short b=(unsigned short)(a+SPHERE_LON+1);
  sphereIndices[k++]=a; sphereIndices[k++]=b; sphereIndices[k++]=a+1;
  sphereIndices[k++]=a+1; sphereIndices[k++]=b; sphereIndices[k++]=b+1;
 }
}
static void ellipsoid(float x,float y,float z,float sx,float sy,float sz,unsigned int color) {
 sceGumPushMatrix();
 sceGumTranslate(&(ScePspFVector3){x,y,z});
 sceGumScale(&(ScePspFVector3){sx,sy,sz});
 sceGuColor(color);
 sceGuDrawArray(GU_TRIANGLES,GU_VERTEX_32BITF|GU_TRANSFORM_3D,
  SPHERE_LAT*SPHERE_LON*6, sphereIndices, sphereVerts);
 sceGumPopMatrix();
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
 if (!bossAwake && fabsf(z+3.82f)<.22f && fabsf(x)<7.55f) return 1;
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
 int i, steps;
 if (distance>=range || facing<=distance*.35f) return 0;
 /* Ruin pillars and rubble block sword hits instead of allowing wall damage. */
 steps=(int)(distance/.30f);
 for (i=1;i<steps;i++) {
  float t=(float)i/(float)steps;
  if (point_in_collider(px+dx*t,1.05f,pz+dz*t,.04f)) return 0;
 }
 return 1;
}

static int player_faces_enemy(float ex,float ez) {
 float dx=ex-px, dz=ez-pz, d=sqrtf(dx*dx+dz*dz)+.001f;
 return dx*(-sinf(playerYaw))+dz*(-cosf(playerYaw))>d*.35f;
}
static int player_can_hit_enemy(float ex,float ez,float range) {
 float dx=ex-px, dz=ez-pz, distance=sqrtf(dx*dx+dz*dz)+.001f;
 float facing=dx*(-sinf(playerYaw))+dz*(-cosf(playerYaw));
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
 /* Violet seal bars Yor's gate until the guards fall and memories are recovered. */
 if (!bossAwake) {
  box(0.0f,1.42f,-3.82f,14.90f,2.84f,.18f,0xff3e285f);
  box(0.0f,1.42f,-3.70f,14.40f,.10f,.10f,0xffa36ad4);
  box(-1.55f,1.42f,-3.70f,.10f,2.55f,.10f,0xff8754bd);
  box(1.55f,1.42f,-3.70f,.10f,2.55f,.10f,0xff8754bd);
 }
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
 /* A small violet rest altar marks the start of the ruined approach. */
 box(0.0f,.18f,5.05f,1.05f,.36f,.82f,0xff292431);
 box(0.0f,.42f,5.05f,.72f,.16f,.58f,0xff51445f);
 box(0.0f,.88f+sinf(worldTime*2.0f)*.07f,5.05f,.20f,.76f,.20f,0xff7a4bb4);
 box(0.0f,1.30f+sinf(worldTime*2.0f)*.07f,5.05f,.12f,.22f,.12f,0xffc7a4f2);
}

static void draw_text(float x,float y,const char *str,int scale,unsigned int color);
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
 ellipsoid(0.0f,2.02f+bob,-.035f,.21f,.24f,.18f,0xffc8a99c);
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
static void draw_memory_shard(int n) {
 float bob=sinf(worldTime*2.6f+n*1.7f)*.12f;
 if (shardTaken[n]) return;
 box(shardX[n],.76f+bob,shardZ[n],.28f,.42f,.28f,0xff7042a8);
 box(shardX[n],.78f+bob,shardZ[n],.12f,.54f,.12f,0xffd4b5f2);
 box(shardX[n],.78f+bob,shardZ[n],.42f,.08f,.42f,0xff3e2a55);
}
static void draw_ruin_guard(int n) {
 float x=guardX[n], z=guardZ[n];
 float pulse=sinf(worldTime*3.0f+n)*.035f;
 float telegraph=guardWindup[n]>0.0f?sinf((.42f-guardWindup[n])*34.0f)*.10f:0.0f;
 unsigned int armor=n==0?0xff383b49:0xff493744;
 if (!guardAlive[n]) return;
 sceGumPushMatrix();
 sceGumTranslate(&(ScePspFVector3){x,0.0f,z});
 sceGumRotateY(atan2f(-(px-x),-(pz-z)));
 box(0.0f,1.02f+pulse,0.0f,.66f,.94f,.42f,armor);
 box(0.0f,1.12f+pulse,-.24f,.54f,.82f,.10f,0xff171720);
 ellipsoid(0.0f,1.72f+pulse,0.0f,.24f,.24f,.21f,0xff292631);
 box(0.0f,1.75f+pulse,-.23f,.32f,.13f,.035f,0xff9b5aaf);
 box(-.38f,1.48f+pulse,0.0f,.18f,.72f,.22f,armor);
 box(.38f,1.48f+pulse+telegraph,0.0f,.18f,.72f,.22f,armor);
 if(guardWindup[n]>0.0f) {
  box(x,.035f,z,1.35f,.035f,1.35f,0xff71365f);
  box(.53f,1.28f+pulse+telegraph,-.20f,.10f,.96f,.10f,0xffd49bce);
 }
 box(-.18f,.40f,0.0f,.24f,.76f,.27f,0xff24232d);
 box(.18f,.40f,0.0f,.24f,.76f,.27f,0xff24232d);
 box(-.18f,.08f,-.06f,.32f,.16f,.42f,0xff111117);
 box(.18f,.08f,-.06f,.32f,.16f,.42f,0xff111117);
 box(.53f,1.12f,-.20f,.09f,.92f,.09f,0xffa7a2b1);
 box(.53f,1.54f,-.20f,.24f,.09f,.14f,0xff51465e);
 sceGumPopMatrix();
}
static void draw_yor_veiled(void) {
 float sway=sinf(worldTime*1.1f)*.045f;
 /* Before the seal opens, Yor is completely concealed by a heavy black veil. */
 box(bx,1.05f,bz,.98f,1.72f,.72f,0xff09090f);
 box(bx,1.64f+sway,bz-.02f,.88f,.76f,.68f,0xff050509);
 box(bx,2.00f+sway,bz-.02f,.72f,.58f,.62f,0xff030307);
 box(bx-.38f-sway,.72f,bz,.22f,1.12f,.28f,0xff0c0b12);
 box(bx+.38f+sway,.72f,bz,.22f,1.12f,.28f,0xff0c0b12);
 box(bx, .22f,bz+.02f,1.02f,.42f,.76f,0xff08080e);
 /* Only a narrow glimpse of her mouth is visible; no eyes or nose. */
 box(bx,1.82f+sway,bz-.345f,.22f,.045f,.025f,0xff8d6268);
 box(bx,1.76f+sway,bz-.348f,.13f,.018f,.018f,0xff25131a);
}
static void draw_yor_spirit(void) {
 float hover=sinf(worldTime*2.1f)*.10f;
 float sway=sinf(worldTime*1.35f)*.075f;
 float hair=sinf(worldTime*1.8f)*.045f;
 unsigned int robe=bossFlash>0.0f?0xffffffff:(phase==1?0xffe5e1ec:0xffbdb1d2);
 unsigned int shade=phase==1?0xffc9c2d5:0xff9586b2;
 /* A warning sigil stays on the floor while Yor commits to her strike. */
 if (bossWindup>0.0f) { float warning=phase==1?2.25f:3.10f; box(bx,.035f,bz,warning,.045f,warning,0xff633b83); }
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
 ellipsoid(bx,1.92f+hover,bz,.24f,.25f,.22f,0xfff2eff8);
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
static void draw_yor_ending(void) {
 float pulse=sinf(worldTime*2.0f)*.06f;
 /* The spirit dissolves into a floating violet gem as Yor returns to her human form. */
 box(bx+1.05f,1.18f+pulse,bz,.34f,.52f,.34f,0xff7040a8);
 box(bx+1.05f,1.18f+pulse,bz,.16f,.72f,.16f,0xffc39aef);
 box(bx+1.05f,1.18f+pulse,bz,.72f,.08f,.72f,0xff352342);
 /* Natural Yor: long black dress, pale face and long hair falling over her eyes. */
 box(bx,0.88f,bz,.76f,1.48f,.50f,0xff17151d);
 box(bx,1.43f,bz-.015f,.62f,.62f,.46f,0xff211d28);
 ellipsoid(bx,1.92f,bz-.015f,.19f,.23f,.17f,0xffd8c2b8);
 box(bx,2.14f,bz-.04f,.46f,.20f,.38f,0xff101016);
 box(bx-.20f,1.72f,bz-.10f,.12f,.88f,.18f,0xff09090f);
 box(bx+.20f,1.70f,bz-.10f,.12f,.90f,.18f,0xff09090f);
 box(bx,1.96f,bz-.205f,.34f,.22f,.035f,0xff17131c);
 box(bx-.49f,1.02f,bz,.20f,.92f,.24f,0xff1b1821);
 box(bx+.49f,1.02f,bz,.20f,.92f,.24f,0xff1b1821);
 box(bx-.51f,.53f,bz,.16f,.42f,.18f,0xff0e0d13);
 box(bx+.51f,.53f,bz,.16f,.42f,.18f,0xff0e0d13);
 /* A soft aura frames the reunion without obscuring the characters. */
 box(bx,2.55f+pulse,bz,.08f,.08f,.08f,0xffd2b6f0);
 /* Final reunion dialogue. */
 rect(76,218,328,4,0xffbda9d4); rect(76,222,328,44,0xe51a1522); rect(76,266,328,3,0xffbda9d4);
 draw_text(156,230,"THANK YOU LLOYD",2,0xfff0e8fa);
 draw_text(190,248,"YOU FREED ME",1,0xffcdb5e8);
}
static int camera_blocked(float x, float y, float z) {
 if (x < WORLD_MIN_X || x > WORLD_MAX_X || z < WORLD_MIN_Z || z > WORLD_MAX_Z) return 1;
 return point_in_collider(x,y,z,.16f);
}
static void camera_pose(float *ex, float *ey, float *ez, float *tx, float *ty, float *tz) {
 float dist=5.6f, horiz, dx, dy, dz, t, desiredX, desiredY, desiredZ, targetX=px, targetZ=pz;
 if (targetLocked && lockedGuard>=0 && guardAlive[lockedGuard]) { targetX=guardX[lockedGuard]; targetZ=guardZ[lockedGuard]; }
 else if (targetLocked && bossHP>0.0f && bossAwake) { targetX=bx; targetZ=bz; }
 *tx=targetX; *ty=targetLocked?1.05f:1.18f; *tz=targetZ;
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
static const unsigned char *font_rows(char c) {
 static const unsigned char blank[7]={0,0,0,0,0,0,0};
 static const unsigned char A[7]={14,17,17,31,17,17,17}, E[7]={31,16,16,30,16,16,31};
 static const unsigned char F[7]={31,16,16,30,16,16,16}, I[7]={31,4,4,4,4,4,31};
 static const unsigned char L[7]={16,16,16,16,16,16,31}, N[7]={17,25,25,21,19,19,17};
 static const unsigned char O[7]={14,17,17,17,17,17,14}, P[7]={30,17,17,30,16,16,16};
 static const unsigned char R[7]={30,17,17,30,20,18,17}, S[7]={15,16,16,14,1,1,30};
 static const unsigned char T[7]={31,4,4,4,4,4,4}, U[7]={17,17,17,17,17,17,14};
 static const unsigned char Y[7]={17,17,10,4,4,4,4}, D[7]={30,17,17,17,17,17,30};
 static const unsigned char G[7]={14,17,16,23,17,17,14};
 static const unsigned char C[7]={14,17,16,16,16,17,14}, HGLYPH[7]={17,17,17,31,17,17,17};
 static const unsigned char K[7]={17,18,20,24,20,18,17}, M[7]={17,27,21,21,17,17,17};
 static const unsigned char V[7]={17,17,17,17,17,10,4};
 static const unsigned char Q[7]={14,17,17,17,21,18,13};
 switch(c) {
  case 'A': return A; case 'D': return D; case 'E': return E; case 'F': return F;
  case 'G': return G; case 'H': return HGLYPH; case 'I': return I; case 'K': return K; case 'M': return M; case 'L': return L; case 'N': return N;
  case 'O': return O; case 'P': return P; case 'Q': return Q; case 'R': return R; case 'S': return S;
  case 'T': return T; case 'U': return U; case 'V': return V; case 'C': return C; case 'Y': return Y; default: return blank;
 }
}
static void draw_text(float x,float y,const char *str,int scale,unsigned int color) {
 int i,row,col;
 for(i=0;str[i];i++) {
  const unsigned char *g=font_rows(str[i]);
  for(row=0;row<7;row++) for(col=0;col<5;col++)
   if(g[row]&(1<<(4-col))) rect(x+i*6*scale+col*scale,y+row*scale,scale,scale,color);
 }
}
static void draw_title_screen(void) {
 rect(0,0,480,272,0xb8100d18);
 rect(38,48,404,3,0xff665578);
 rect(38,51,404,170,0x9e211b2a);
 rect(38,221,404,3,0xff665578);
 draw_text(112,82,"LOST SOUL",4,0xffe5d9f0);
 draw_text(158,124,"OF YOR",4,0xffbca2dc);
 draw_text(158,174,"PRESS START",2,0xffffffff);
 rect(172,205,136,2,0xff806a98);
}
static void draw_tutorial_screen(void) {
 rect(0,0,480,272,0xc6100e18);
 rect(42,28,396,3,0xff665578); rect(42,31,396,210,0x9e211b2a); rect(42,241,396,3,0xff665578);
 draw_text(158,48,"CONTROLS",3,0xffe5d9f0);
 draw_text(78,78,"MOVE ANALOG",2,0xffffffff);
 draw_text(78,100,"ATTACK SQUARE",2,0xffffffff);
 draw_text(78,122,"DODGE CIRCLE",2,0xffffffff);
 draw_text(78,144,"GUARD R LOCK L",2,0xffffffff);
 draw_text(78,166,"HEAL SELECT",2,0xffc7a4f2);
 draw_text(78,192,"FIND SHARDS",2,0xffd7b8f2);
 draw_text(78,214,"DEFEAT GUARDS",2,0xffd7b8f2);
 draw_text(280,232,"PRESS CROSS",1,0xffe5d9f0);
}
static void draw_story_screen(void) {
 rect(0,0,480,272,0xc6100e18);
 rect(42,30,396,3,0xff665578); rect(42,33,396,204,0x9e211b2a); rect(42,237,396,3,0xff665578);
 draw_text(150,60,"YOR IS LOST",3,0xffe5d9f0);
 draw_text(92,116,"LLOYD MUST FIND HER",2,0xffffffff);
 draw_text(118,151,"ENTER THE RUINS",2,0xffcdb5e8);
 draw_text(280,215,"PRESS CROSS",1,0xffe5d9f0);
}
static void draw_hud(void) {
 float php=fmaxf(0.0f,fminf(1.0f,playerHP/100.0f));
 float st=fmaxf(0.0f,fminf(1.0f,stamina/100.0f));
 float bhp=fmaxf(0.0f,fminf(1.0f,bossHP/100.0f));
 rect(18,16,184,12,0xff24202a); rect(18,16,184*php,12,0xffa52d45);
 rect(18,34,184,7,0xff24202a); rect(18,34,184*st,7,0xff6a9b70);
 if (bossHP>0.0f && bossAwake) { rect(278,16,184,12,0xff24202a); rect(278,16,184*bhp,12,0xff8b315d); }
 /* Nearby ruin guard vitality bars. */
 { int gi; for(gi=0;gi<2;gi++) if(guardAlive[gi]) {
   float dx=guardX[gi]-px, dz=guardZ[gi]-pz, d=sqrtf(dx*dx+dz*dz);
   if(d<7.0f) { float x=220.0f+guardX[gi]*12.0f; float y=56.0f+guardZ[gi]*2.0f;
    rect(x-22,y,44,4,0xff24202a); rect(x-22,y,44.0f*(guardHP[gi]/52.0f),4,0xff8c477c);
   }
  } }
 /* Three recovered memory shards. */
 { int si; for(si=0;si<3;si++) { unsigned int c=si<shardCount?0xffc8a4f2:0xff4c4057; rect(410+si*17,42,9,9,c); } }
 /* Three healing flasks; filled markers show the remaining charges. */
 rect(18,222,54,5,0xff24202a);
 { int i; for (i=0;i<3;i++) { unsigned int c=i<flaskCharges?0xff65b7ad:0xff49434f; rect(20+i*17,211,10,10,c); rect(22+i*17,208,6,4,c); } }
 rect(18,246,444,2,targetLocked?0xffa36acb:0xff62566c);
 if (bossAwake) draw_text(204,48,"YOR GATE",1,0xffe1d1f0);
 else draw_text(177,48,"RUIN APPROACH",1,0xffc8b8dc);
 draw_text(365,28,"MEMORY SHARDS",1,0xffbba2d6);
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
 draw_world(); draw_lloyd(); draw_memory_shard(0); draw_memory_shard(1); draw_memory_shard(2); draw_ruin_guard(0); draw_ruin_guard(1); if (!victory) { if (bossAwake) draw_yor_spirit(); else draw_yor_veiled(); } else draw_yor_ending(); if (titleScreen) draw_title_screen(); else if (tutorialScreen) draw_tutorial_screen(); else if (storyScreen) draw_story_screen(); else draw_hud();
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
 if (titleScreen) { if (pressed & (PSP_CTRL_START|PSP_CTRL_CROSS)) { titleScreen=0; tutorialScreen=1; } return; }
 if (tutorialScreen) { if (pressed & (PSP_CTRL_START|PSP_CTRL_CROSS)) { tutorialScreen=0; storyScreen=1; } return; }
 if (storyScreen) { if (pressed & (PSP_CTRL_START|PSP_CTRL_CROSS)) storyScreen=0; return; }
 if (paused) { if (pressed & (PSP_CTRL_START|PSP_CTRL_CROSS)) paused=0; return; }
 if (pressed & PSP_CTRL_START) { paused=1; return; }
 if (c.Buttons & PSP_CTRL_LEFT) cameraYaw-=.025f;
 if (c.Buttons & PSP_CTRL_RIGHT) cameraYaw+=.025f;
 if (c.Buttons & PSP_CTRL_UP) cameraPitch=fminf(.68f,cameraPitch+.018f);
 if (c.Buttons & PSP_CTRL_DOWN) cameraPitch=fmaxf(.10f,cameraPitch-.018f);
 if (pressed & PSP_CTRL_LTRIGGER) {
  if (targetLocked) { targetLocked=0; lockedGuard=-1; }
  else {
   float best=10.0f, dx=bx-px, dz=bz-pz, d=sqrtf(dx*dx+dz*dz);
   int gi;
   lockedGuard=-1;
   if (bossHP>0.0f && bossAwake && d<best) best=d;
   for(gi=0;gi<2;gi++) if(guardAlive[gi]) {
    dx=guardX[gi]-px; dz=guardZ[gi]-pz; d=sqrtf(dx*dx+dz*dz);
    if(d<best) { best=d; lockedGuard=gi; }
   }
   if(best<10.0f) targetLocked=1;
  }
 }
 if (lockedGuard>=0 && !guardAlive[lockedGuard]) { targetLocked=0; lockedGuard=-1; }
 if (bossHP<=0.0f && lockedGuard<0) targetLocked=0;
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
 if (targetLocked) { float lx=lockedGuard>=0?guardX[lockedGuard]:bx, lz=lockedGuard>=0?guardZ[lockedGuard]:bz; playerYaw=atan2f(-(lx-px),-(lz-pz)); }
 if (dodgeTimer>0.0f) dodgeTimer-=dt;
 if (attackTimer>0.0f) attackTimer-=dt;
 if (bossAttackTimer>0.0f) bossAttackTimer-=dt;
 for (int gi=0;gi<2;gi++) {
  float gx,gz,gd;
  if (!guardAlive[gi]) continue;
  if (guardAttack[gi]>0.0f) guardAttack[gi]-=dt;
  if (guardWindup[gi]>0.0f) { guardWindup[gi]-=dt; if (guardWindup[gi]<=0.0f) {
   gx=px-guardX[gi]; gz=pz-guardZ[gi]; gd=sqrtf(gx*gx+gz*gz);
   if (gd<1.85f && dodgeTimer<=0.0f && hurtTimer<=0.0f) { if (guarding && stamina>=8.0f && player_faces_enemy(guardX[gi],guardZ[gi])) { stamina-=8.0f; hitFlash=.08f; } else { playerHP-=7.0f; hurtTimer=.38f; hitFlash=.13f; } }
   guardWindup[gi]=0.0f; guardAttack[gi]=1.75f;
  } }
 }
 if (bossWindup>0.0f) { bossWindup-=dt; if (bossWindup<=0.0f) { bossWindup=0.0f; bossStrikePending=1; } }
 if (hitFlash>0.0f) hitFlash-=dt;
  if (bossFlash>0.0f) bossFlash-=dt;
 if (hurtTimer>0.0f) hurtTimer-=dt;
 guarding = (!victory && !defeated && (c.Buttons & PSP_CTRL_RTRIGGER) && stamina>0.0f);
 if (defeated && (pressed & PSP_CTRL_CROSS)) {
  if (!load_checkpoint()) {
   px=0.0f; pz=6.0f; playerYaw=0.0f; bx=0.0f; bz=-5.7f;
   bossHP=100.0f; playerHP=100.0f; stamina=100.0f; staminaRegenDelay=0.0f; flaskCharges=3;
   bossAwake=0; guardX[0]=-1.65f; guardZ[0]=.25f; guardX[1]=1.65f; guardZ[1]=-2.45f;
   guardHP[0]=42.0f; guardHP[1]=52.0f; guardAlive[0]=guardAlive[1]=1;
   shardTaken[0]=shardTaken[1]=shardTaken[2]=0; shardCount=0; phase=1;
  }
  attackTimer=0.0f; bossAttackTimer=1.0f; bossWindup=0.0f; bossStrikePending=0; dodgeTimer=0.0f; hurtTimer=0.0f; staminaRegenDelay=0.0f;
  defeated=0; targetLocked=0; lockedGuard=-1; guarding=0; hitFlash=0.0f; bossFlash=0.0f;
 }
 if (victory && (pressed & PSP_CTRL_CROSS)) {
  px=0.0f; pz=6.0f; playerYaw=0.0f; bx=0.0f; bz=-5.7f;
  bossHP=100.0f; playerHP=100.0f; stamina=100.0f; staminaRegenDelay=0.0f; flaskCharges=3;
  attackTimer=0.0f; bossAttackTimer=1.0f; bossWindup=0.0f; bossStrikePending=0; dodgeTimer=0.0f; hurtTimer=0.0f; bossAwake=0;
  guardX[0]=-1.65f; guardZ[0]=.25f; guardX[1]=1.65f; guardZ[1]=-2.45f;
  guardHP[0]=42.0f; guardHP[1]=52.0f; guardAttack[0]=1.1f; guardAttack[1]=1.8f; guardWindup[0]=guardWindup[1]=0.0f; guardAlive[0]=guardAlive[1]=1;
  shardTaken[0]=shardTaken[1]=shardTaken[2]=0; shardCount=0; phase=1;
  victory=0; defeated=0; targetLocked=0; lockedGuard=-1; guarding=0; save_checkpoint();
 }
 if (!victory && !defeated) {
  for (int si=0;si<3;si++) if (!shardTaken[si]) {
   float sx=px-shardX[si], sz=pz-shardZ[si];
   if (sx*sx+sz*sz<1.05f) { shardTaken[si]=1; shardCount++; stamina=fminf(100.0f,stamina+18.0f); save_checkpoint(); }
  }
  for (int gi=0;gi<2;gi++) {
   float gx=px-guardX[gi], gz=pz-guardZ[gi], gd=sqrtf(gx*gx+gz*gz)+.001f;
   if (!guardAlive[gi]) continue;
   if (gd>1.05f) {
    float tx,tz,td,speed,nx,nz;
    if (gd<7.0f) { tx=px; tz=pz; speed=1.05f*dt; }
    else {
     float phaseOffset=gi*PI;
     tx=(gi==0?-1.65f:1.65f)+sinf(worldTime*.62f+phaseOffset)*1.05f;
     tz=(gi==0?.25f:-2.45f)+cosf(worldTime*.48f+phaseOffset)*.85f;
     speed=.42f*dt;
    }
    gx=tx-guardX[gi]; gz=tz-guardZ[gi]; td=sqrtf(gx*gx+gz*gz)+.001f;
    nx=guardX[gi]+gx/td*speed; nz=guardZ[gi]+gz/td*speed;
    if (!player_blocked(nx,guardZ[gi],.34f)) guardX[gi]=nx;
    if (!player_blocked(guardX[gi],nz,.34f)) guardZ[gi]=nz;
   }
   if (gd<1.75f && guardAttack[gi]<=0.0f && guardWindup[gi]<=0.0f) guardWindup[gi]=.42f;
  }
  /* Cross rests at the entrance altar: refill flasks, health and stamina. */
  if ((pressed&PSP_CTRL_CROSS) && fabsf(px)<1.45f && pz>4.15f && pz<6.25f) {
   playerHP=100.0f; stamina=100.0f; flaskCharges=3; staminaRegenDelay=0.0f; hitFlash=.18f; save_checkpoint();
  }
  /* Select consumes one of three flasks and restores health, never exceeding max HP. */
  if ((pressed&PSP_CTRL_SELECT) && flaskCharges>0 && playerHP<100.0f) {
   playerHP=fminf(100.0f,playerHP+35.0f);
   flaskCharges--;
   hitFlash=.12f; save_checkpoint();
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
   if (bossAwake && player_can_hit(2.35f)) { bossHP-=8.0f; bossFlash=.12f; save_checkpoint(); } else { int i; for(i=0;i<2;i++) if(guardAlive[i] && player_can_hit_enemy(guardX[i],guardZ[i],2.25f)) { guardHP[i]-=12.0f; if(guardHP[i]<=0.0f) guardAlive[i]=0; save_checkpoint(); break; } }
  }
  if ((pressed&PSP_CTRL_TRIANGLE) && attackTimer<=0.0f && stamina>=25.0f) {
   attackTimer=.68f; stamina-=25.0f; staminaRegenDelay=.90f;
   if (bossAwake && player_can_hit(2.85f)) { bossHP-=17.0f; bossFlash=.18f; save_checkpoint(); } else { int i; for(i=0;i<2;i++) if(guardAlive[i] && player_can_hit_enemy(guardX[i],guardZ[i],2.70f)) { guardHP[i]-=24.0f; if(guardHP[i]<=0.0f) guardAlive[i]=0; save_checkpoint(); break; } }
  }
  if (guarding) { stamina=fmaxf(0.0f,stamina-.22f); staminaRegenDelay=.20f; }
  else if (staminaRegenDelay>0.0f) staminaRegenDelay=fmaxf(0.0f,staminaRegenDelay-dt);
  else if (stamina<100.0f) stamina=fminf(100.0f,stamina+.14f);
  if (bossHP<=50.0f && phase==1) { phase=2; bossWindup=0.0f; bossStrikePending=0; bossAttackTimer=1.0f; bossFlash=.70f; }
  if (bossHP<=0.0f) { bossHP=0.0f; victory=1; targetLocked=0; save_checkpoint(); }
  vx=px-bx; vz=pz-bz; distance=sqrtf(vx*vx+vz*vz)+.001f;
  /* Yor waits beyond the ruined gate, then wakes when Lloyd enters her reach. */
  if (!bossAwake && !guardAlive[0] && !guardAlive[1] && shardCount>=2 && distance<8.0f) { bossAwake=1; bossAttackTimer=1.20f; }
  if (bossAwake) {
   if (distance>1.60f) { float chase=phase==1?.018f:.030f; move_boss(vx/distance*chase,vz/distance*chase); }
   /* Yor telegraphs each close-range strike before the hit check. The warning
      gives the player a readable dodge/guard window instead of instant damage. */
   if (bossStrikePending) {
    if (distance<(phase==1?2.55f:3.25f) && dodgeTimer<=0.0f && hurtTimer<=0.0f) {
     if (guarding && stamina>=12.0f && player_faces_boss()) { stamina-=12.0f; hitFlash=.10f; }
     else { playerHP-=phase==1?9.0f:14.0f; hitFlash=.16f; hurtTimer=.48f; }
    }
    bossStrikePending=0;
    bossAttackTimer=phase==1?2.1f:1.25f;
   } else if (bossAttackTimer<=0.0f && bossWindup<=0.0f) {
    if (distance<2.75f) bossWindup=phase==1?.48f:.36f;
    else bossAttackTimer=.22f;
   }
  }
  if (playerHP<0.0f) playerHP=0.0f;
  if (playerHP<=0.0f) { defeated=1; guarding=0; targetLocked=0; }
 }
}
int main(void) {
 setup_callbacks(); sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG); load_checkpoint(); sphere_init(); init_gu(); audio_start();
 while (running) { update(); frame(); }
 sceGuTerm(); sceKernelExitGame(); return 0;
}
