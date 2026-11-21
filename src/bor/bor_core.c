/* Selected OpenBOR7555 semantics adapted behind a host-neutral boundary.
 * Preserve BSD notices in OPENBOR_BSD.txt. Signed: Atlas. */
#include "bor_core.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static uint64_t attack_sequence=1,action_sequence=1;
static unsigned callback_depth;
static bool valid_animation(const BorAnimation* anim){
 if(!anim||!anim->frames||anim->count<=0||anim->count>4096)return false;
 if(anim->loop&&(anim->loop_from<0||anim->loop_from>=anim->count))return false;
 for(int i=0;i<anim->count;i++)if(!isfinite(anim->frames[i].duration)||anim->frames[i].duration<=0||anim->frames[i].move< -4096||anim->frames[i].move>4096)return false;
 return true;
}
const BorModel* bor_find_model(const BorModel* models,int count,const char* name){
 if(!models||!name)return NULL;
 for(int i=0;i<count;i++)if(models[i].name&&!strcmp(models[i].name,name))return models+i;
 return NULL;
}
const BorAnimation* bor_animation(const BorModel* model,const char* name){
 if(!model||!name||!model->animations)return NULL;
 for(int i=0;i<model->count;i++)if(model->animations[i].name&&!strcmp(model->animations[i].name,name))return model->animations+i;
 return NULL;
}
const BorFrame* bor_frame(const BorActor* a){return a&&a->animation&&a->animation->frames&&a->frame>=0&&a->frame<a->animation->count?a->animation->frames+a->frame:NULL;}
bool bor_init(BorActor* a,const BorModel* m,int index){
 if(!a)return false;
 memset(a,0,sizeof(*a));a->partner=-1;a->state=BOR_DEAD;a->action_id=++action_sequence;
 const BorAnimation* idle=bor_animation(m,"idle");if(!valid_animation(idle))return false;
 a->pose_scale=1;a->model=m;a->model_index=index;a->health=m->health;a->facing=1;
 a->animation=idle;a->remaining=bor_frame(a)->duration;a->state=BOR_FREE;return true;
}
static void notify(BorActor* a,const BorFrame* f,bool entering,BorEvent event,void* context){
 if(event){callback_depth++;event(a,f,entering,context);callback_depth--;}
}
static void enter_frame(BorActor* a,BorEvent event,void* context){
 const BorFrame* f=bor_frame(a);bool active=f->aw>0&&f->ah>0;
 if(!active)a->epoch=0;else if(!a->epoch)a->epoch=++attack_sequence;
 a->active=active;a->remaining=f->duration;notify(a,f,true,event,context);
}
bool bor_start(BorActor* a,const char* name,BorState state,BorEvent event,void* context){
 if(!a||state<BOR_FREE||state>=BOR_STATE_COUNT||callback_depth>=16)return false;
 if((a->state==BOR_DEAD||a->health<=0)&&state!=BOR_DEAD)return false;
 const BorAnimation* anim=bor_animation(a->model,name);if(!valid_animation(anim))return false;
 a->animation=anim;a->frame=0;a->age=0;a->finished=false;a->state=state;a->pose_x=a->pose_y=a->pose_angle=0;a->pose_scale=1;a->hit_confirm=false;a->combo_hits=0;a->epoch=0;a->timeline_fault=false;a->action_id=++action_sequence;
 a->combo_owner=a->combo_arming?a->action_id:0;a->combo_arming=false;
 if(state==BOR_ACTION&&(anim->flags&BOR_ANIM_ATTACK))a->combo_until=0;
 enter_frame(a,event,context);return true;
}
bool bor_tick_status(BorActor* a,double dt){
 if(!a||!isfinite(dt)||dt<=0||a->timeline_fault)return true;
 bool frozen=a->freeze>0;
 a->invulnerable=fmax(0,a->invulnerable-dt);a->combo_until=fmax(0,a->combo_until-dt);a->ai_wait=fmax(0,a->ai_wait-dt);
 if(a->knockdown_until>0){a->knockdown_until=fmax(0,a->knockdown_until-dt);if(a->knockdown_until==0)a->knockdown=0;}
 a->freeze=fmax(0,a->freeze-dt);return frozen;
}
void bor_advance_animation(BorActor* a,double dt,BorEvent event,void* context){
 if(!a||callback_depth||!isfinite(dt)||dt<=0||!bor_frame(a)||a->freeze>0||a->timeline_fault)return;
 uint64_t action=a->action_id;
 if(a->animation->flags&BOR_ANIM_MANUAL){a->age+=dt;notify(a,bor_frame(a),false,event,context);return;}
 if(a->finished){notify(a,bor_frame(a),false,event,context);return;}
 unsigned steps=0;
 while(dt>1e-9&&!a->finished&&steps++<65536){
  double span=fmin(dt,a->remaining);notify(a,bor_frame(a),false,event,context);
  if(a->action_id!=action||a->freeze>0)return;
  a->remaining-=span;a->age+=span;dt-=span;
  if(a->remaining>1e-9)break;
  if(a->frame+1<a->animation->count)a->frame++;
  else if(a->animation->loop)a->frame=a->animation->loop_from;
  else {a->finished=true;break;}
  enter_frame(a,event,context);if(a->action_id!=action||a->freeze>0)return;
 }
 if(dt>1e-9&&!a->finished){a->timeline_fault=true;a->active=false;a->epoch=0;}
}
void bor_advance(BorActor* a,double dt,BorEvent event,void* context){
 if(!a||callback_depth||!isfinite(dt)||dt<=0)return;
 if(!bor_tick_status(a,dt))bor_advance_animation(a,dt,event,context);
}
bool bor_is_player(const BorActor* a){return a&&a->model&&(a->model->type&BOR_TYPE_PLAYER)!=0;}
bool bor_can_target(const BorActor* a,const BorActor* b){return a&&b&&a!=b&&a->model&&b->model&&b->health>0&&b->state!=BOR_DEAD&&(a->model->hostility&b->model->type)!=0;}
bool bor_can_act(const BorActor* a){return a&&a->health>0&&a->freeze<=0&&!a->timeline_fault&&a->state==BOR_FREE;}
bool bor_try_cancel(BorActor* a,unsigned input,BorEvent event,void* context){
 if(!a||callback_depth>=16||a->timeline_fault||a->health<=0||a->freeze>0||a->state!=BOR_ACTION||!a->animation)return false;
 for(int i=0;i<a->animation->cancel_count;i++){
  const BorCancel* c=a->animation->cancels+i;
  if((input&c->input)!=c->input||a->frame<c->first||a->frame>c->last||a->combo_hits<c->hits)continue;
  if(!valid_animation(bor_animation(a->model,c->target)))continue;
  return bor_start(a,c->target,BOR_ACTION,event,context);
 }
 return false;
}
const char* bor_combo_select(const BorActor* a,int channel,int context,int axis){
 if(!a||!a->model)return NULL;
 int stage=a->combo_until>0?a->combo:0;const char* selected=NULL;int score=-1;
 for(int i=0;i<a->model->combo_count;i++){const BorComboRule* r=a->model->combos+i;if(r->channel!=channel||r->context!=context||(r->axis>=0&&r->axis!=axis)||(r->stage>=0&&r->stage!=stage))continue;int priority=(r->axis>=0?1:0)+(r->stage>=0?2:0);if(priority>score){selected=r->animation;score=priority;}}
 return selected;
}
bool bor_combo_attack(BorActor* a,const char* name,BorEvent event,void* context){
 if(callback_depth>=16||!a||a->health<=0||a->freeze>0||a->timeline_fault||!valid_animation(bor_animation(a->model,name)))return false;
 bool air=a->state==BOR_ACTION&&!(a->animation->flags&BOR_ANIM_ATTACK)&&(a->animation->flags&BOR_ANIM_AIR_ATTACK);
 if(!bor_can_act(a)&&!air&&a->state!=BOR_DASH&&a->state!=BOR_ROLL)return false;
 int stage=a->combo_until>0?a->combo:0;if(stage<0||stage>=a->model->chain_count)stage=0;
 a->combo_stage=stage;a->combo=stage;a->combo_arming=true;return bor_start(a,name,BOR_ACTION,event,context);
}
bool bor_chain_attack(BorActor* a,BorEvent event,void* context){
 if(!bor_can_act(a)||!a->model->chain||a->model->chain_count<=0)return false;
 int index=a->combo_until>0?a->combo:0;if(index<0||index>=a->model->chain_count)index=0;
 char name[24];snprintf(name,sizeof(name),"attack%d",a->model->chain[index]);return bor_combo_attack(a,name,event,context);
}
/* Authored boxes rotate around the sprite origin. Positive Surge angles
 * rotate screen coordinates counterclockwise. Signed: Atlas. */
bool bor_corners(const BorActor* a,bool attack,double out[8]){
 const BorFrame* f=bor_frame(a);if(!f)return false;
 double rx=attack?f->ax:f->bx,ry=attack?f->ay:f->by,w=attack?f->aw:f->bw,h=attack?f->ah:f->bh;
 if(w<=0||h<=0)return false;
 double left=(a->facing>0?rx-f->ox:f->ox-rx-w)*a->pose_scale,top=(ry-f->oy)*a->pose_scale;
 double c=cos(a->pose_angle),s=sin(a->pose_angle);
 for(int i=0;i<4;i++){double x=left+((i==1||i==2)?w*a->pose_scale:0),y=top+(i>=2?h*a->pose_scale:0);out[2*i]=a->x+a->pose_x+c*x+s*y;out[2*i+1]=a->y+a->pose_y-s*x+c*y;}
 return true;
}
void bor_rect(const BorActor* a,bool attack,double* x,double* y,double* w,double* h){
 double p[8];if(!bor_corners(a,attack,p)){*x=*y=*w=*h=0;return;}
 double right=p[0],bottom=p[1];*x=p[0];*y=p[1];
 for(int i=1;i<4;i++){*x=fmin(*x,p[2*i]);*y=fmin(*y,p[2*i+1]);right=fmax(right,p[2*i]);bottom=fmax(bottom,p[2*i+1]);}*w=right-*x;*h=bottom-*y;
}
bool bor_overlaps(const BorActor* a,const BorActor* b){
 double p[8],q[8];if(!bor_corners(a,true,p)||!bor_corners(b,false,q))return false;
 /* Separating axes from both rectangles avoid rotated AABB corner hits. */
 for(int box=0;box<2;box++)for(int edge=0;edge<2;edge++){
  const double* v=box?q:p;double nx=-(v[2*(edge+1)+1]-v[2*edge+1]),ny=v[2*(edge+1)]-v[2*edge];
  double lo=p[0]*nx+p[1]*ny,hi=lo,ql=q[0]*nx+q[1]*ny,qh=ql;
  for(int i=1;i<4;i++){double x=p[2*i]*nx+p[2*i+1]*ny,y=q[2*i]*nx+q[2*i+1]*ny;lo=fmin(lo,x);hi=fmax(hi,x);ql=fmin(ql,y);qh=fmax(qh,y);}
  if(hi<=ql||qh<=lo)return false;
 }return true;
}
bool bor_damage_with_bonus(BorActor* source,BorActor* target,double now,int bonus,BorHitResult* result){
 (void)now;if(result){result->actual=0;result->defeated=false;}if(bonus<0)return false;
 if(!source||!target||source==target||!source->model||!target->model||source->health<=0||source->state==BOR_DEAD||source->state==BOR_FALL)return false;
 if(!(source->model->damage_mask&target->model->type)||!source->active||!source->epoch||target->health<=0||target->state==BOR_DEAD||target->state==BOR_EVADE||target->invulnerable>0)return false;
 if(target->state==BOR_FALL&&target->grounded)return false;
 if(source->serial>=64||target->last_hit[source->serial]==source->epoch||!bor_overlaps(source,target))return false;
 target->last_hit[source->serial]=source->epoch;const BorFrame* f=bor_frame(source);
 int before=target->health;int64_t damage=(int64_t)f->damage+(f->damage>0?bonus:0);
 int64_t health=(int64_t)target->health-damage;target->health=health<0?0:health>INT_MAX?INT_MAX:(int)health;
 if(result){result->actual=target->health<before?before-target->health:0;result->defeated=before>0&&target->health==0;}
 target->invulnerable=source->animation->fast?.05:.2;
 source->hit_confirm=true;source->combo_hits++;source->combo_until=source->animation->connect_window;
 if(source->combo_owner==source->action_id&&source->model->chain_count>0)source->combo=(source->combo_stage+1)%source->model->chain_count;
 return true;
}
bool bor_damage(BorActor* source,BorActor* target,double now){return bor_damage_with_bonus(source,target,now,0,NULL);}
bool bor_resolve_motion(BorVelocity current,BorMotionMode mode,unsigned axes,double x,double y,BorVelocity* result){
 if(!result||mode<BOR_MOTION_PRESERVE||mode>BOR_MOTION_STOP||!axes||(axes&~(BOR_AXIS_X|BOR_AXIS_Y))||!isfinite(current.x)||!isfinite(current.y)||!isfinite(x)||!isfinite(y))return false;
 BorVelocity next=current;
 if(mode!=BOR_MOTION_PRESERVE){
  if(axes&BOR_AXIS_X)next.x=mode==BOR_MOTION_SET?x:mode==BOR_MOTION_ADD?current.x+x:0;
  if(axes&BOR_AXIS_Y)next.y=mode==BOR_MOTION_SET?y:mode==BOR_MOTION_ADD?current.y+y:0;
 }
 if(!isfinite(next.x)||!isfinite(next.y))return false;
 *result=next;return true;
}
bool bor_ground_tangent(BorVelocity velocity,double angle,double* tangent){
 if(!tangent||!isfinite(velocity.x)||!isfinite(velocity.y)||!isfinite(angle))return false;
 double cosine=cos(angle),sine=sin(angle),speed=velocity.x*cosine-velocity.y*sine;
 if(!isfinite(speed)||fabs(speed*cosine-velocity.x)>1e-7||fabs(-speed*sine-velocity.y)>1e-7)return false;
 *tangent=speed;return true;
}
