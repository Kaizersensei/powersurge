/* BOR data-driven actor kernel. Source semantics: OpenBOR BSD b1676f9. Signed: Atlas. */
#ifndef BOR_CORE_H
#define BOR_CORE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
 double duration; int ox,oy,bx,by,bw,bh,ax,ay,aw,ah,damage,drop,no_block,no_flash,pause,depth;
 int move,movea,movez,sprite,sound; int cx,cy,cw,ch;
} BorFrame;
enum {BOR_ANIM_AIR_CONTROL=1,BOR_ANIM_AIR_ATTACK=2,BOR_ANIM_RUN_JUMP=4,BOR_ANIM_JUMP_LAND=8,BOR_ANIM_ATTACK=16,BOR_ANIM_MANUAL=32,BOR_ANIM_RANGED=64};
enum {BOR_INPUT_JUMP=1,BOR_INPUT_SPECIAL=2,BOR_INPUT_ATTACK=4};
enum {BOR_TYPE_PLAYER=1,BOR_TYPE_ENEMY=2,BOR_TYPE_NPC=4,BOR_TYPE_OBSTACLE=8,BOR_TYPE_SHOT=16,BOR_TYPE_TRAP=32,BOR_TYPE_ITEM=64,BOR_TYPE_PROJECTILE=128};
typedef enum {BOR_MOTION_PRESERVE,BOR_MOTION_SET,BOR_MOTION_ADD,BOR_MOTION_STOP} BorMotionMode;
enum {BOR_AXIS_X=1,BOR_AXIS_Y=2};
typedef struct {double x,y;} BorVelocity;
bool bor_resolve_motion(BorVelocity,BorMotionMode,unsigned,double,double,BorVelocity*);
bool bor_ground_tangent(BorVelocity,double,double*);
typedef struct {int first,last,hits;unsigned input;const char* target;} BorCancel;
typedef struct {
 const char* name; const BorFrame* frames; int count,loop,loop_from;
 int jump_frame; double jump_y,jump_x,jump_z; int land_frame,range_min,range_max;
 const char* follow; int fast;
 unsigned flags;const BorCancel* cancels;int cancel_count; double connect_window;int impact_kind,impact_strength;
} BorAnimation;
typedef struct {int channel,context,axis,stage;const char* animation;} BorComboRule;
typedef struct {
 const char* name; const char* sprite; int health; double speed,run_speed,jump; int hostility;
 const BorAnimation* animations; int count,death_sound;
 double jump_speed,run_jump,run_distance,throw_distance,throw_height; int throw_damage,jump_control;
 unsigned type,damage_mask;int aggression;const int* chain;int chain_count;double grab_distance;const BorComboRule* combos;int combo_count;bool training_dummy,grabbable;int impact_material;bool soft_solid;double soft_radius,soft_height;unsigned hold_mask;
} BorModel;
typedef enum {BOR_FREE,BOR_ACTION,BOR_PAIN,BOR_FALL,BOR_RISE,BOR_GRAB,BOR_GRABBED,BOR_DEAD,BOR_DASH,BOR_WALL_JUMP,BOR_LEDGE,BOR_CLIMB,BOR_ROLL,BOR_GUARD,BOR_POLE,BOR_LADDER,BOR_BARCLIMB,BOR_EVADE,BOR_STATE_COUNT} BorState;
typedef struct BorActor BorActor;
typedef void (*BorEvent)(BorActor*,const BorFrame*,bool,void*);
struct BorActor {
 const BorModel* model; const BorAnimation* animation; int model_index,frame,health,facing,combo,combo_hits,combo_stage;
 double age,remaining,invulnerable,freeze,combo_until,ai_wait,knockdown_until; BorState state;
 uint64_t combo_owner;bool combo_arming;uint64_t action_id,epoch,last_hit[64]; unsigned serial; bool active,finished,hit_confirm,grounded,initialized,timeline_fault;
 double x,y,vx,vy,spawn_x,spawn_y,air_time,pose_x,pose_y,pose_scale,pose_angle; int partner,grabs,knockdown; void* host;
};
const BorModel* bor_find_model(const BorModel*,int,const char*);
const BorAnimation* bor_animation(const BorModel*,const char*);
const BorFrame* bor_frame(const BorActor*);
bool bor_init(BorActor*,const BorModel*,int);
bool bor_is_player(const BorActor*);
bool bor_can_target(const BorActor*,const BorActor*);
bool bor_can_act(const BorActor*);
bool bor_try_cancel(BorActor*,unsigned,BorEvent,void*);
bool bor_combo_attack(BorActor*,const char*,BorEvent,void*);
const char* bor_combo_select(const BorActor*,int,int,int);
bool bor_chain_attack(BorActor*,BorEvent,void*);
bool bor_tick_status(BorActor*,double);
void bor_advance_animation(BorActor*,double,BorEvent,void*);
bool bor_start(BorActor*,const char*,BorState,BorEvent,void*);
void bor_advance(BorActor*,double,BorEvent,void*);
bool bor_overlaps(const BorActor*,const BorActor*);
bool bor_damage(BorActor*,BorActor*,double);
typedef struct {int actual;bool defeated;} BorHitResult;
bool bor_damage_with_bonus(BorActor*,BorActor*,double,int,BorHitResult*);
bool bor_corners(const BorActor*,bool,double[8]);
void bor_rect(const BorActor*,bool,double*,double*,double*,double*);
#endif
