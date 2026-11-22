#include "../labs/lab.h"
#include "../editor_shell/shell.h"
/* Minimal native bindings for stock Surge hosting source-defined BOR actors.
 * Does not modify terrain, editor, renderer or level format. Signed: Atlas. */
#include "bor_core.h"
#include "bor_editor.h"
#include "bor_models.h"
#include <surgescript.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bor_preview.inc"
#include "../scripting/scripting.h"
#include "../physics/physicsactor.h"
#include "../physics/obstaclemap.h"
#include "../physics/obstacle.h"
#include "../scenes/level.h"
#include "../entities/actor.h"
#include "../entities/player.h"
#include "../entities/camera.h"
#include "../core/input.h"
#include "../core/timer.h"
#include "../core/sprite.h"
#include "../core/audio.h"
#include "../core/resourcemanager.h"
#include "../core/engine.h"
#include "../core/logfile.h"
#include "../core/video.h"
#include "../core/image.h"
#include "bor_profile.h"
#include "bor_pose.h"
#include "input_trigger.h"
#include "../foundation/foundation_host.h"
#include "../foundation/spatial_audio.h"
#include "../foundation/audio_demo_records.h"

typedef struct {
 BorActor a; physicsactor_t* body; actor_t* visual; surgescript_object_t* entity; input_t* drive_input; int steering; bool running;
 double surface_angle;
 double lie_time,think_time,retreat_origin,grab_release; int ai_phase,landing_damage,reaction_tick; bool placed,was_ground,frozen_tick,grab_finisher; int input_direction;
 BorPhysics physics,job;
 int action_channel,progress_damage_bonus;uint64_t progress_action;char registered_action_tag[129];
 bool defense_held;uint64_t lock_target;v2d_t aim_position;
 int jumps_used,job_tick,dash_ready_tick,dash_started_tick,ledge_ready_tick,job_direction,guard_kind,guard_age,hold_ticks[2],press_ticks[2];
 bool jump_held,jump_cut_active,down_held,channel_down[2],channel_captured[2],press_captured[2];int tap_tick[2],tap_direction;
 bool contact_x_blocked;int guard_facing,locomotion;uint64_t lifetime,actor_support,ignored_support,contact_lifetime;int actor_support_ticks,support_block_tick,contact_slot,contact_ticks;double grip_distance;v2d_t actor_support_position;
 bool listening,investigating,hearing_pending;double hearing_threshold,search_until,provoked_until;v2d_t hearing_at,pending_hearing;
 unsigned char agency; /* 0 legacy,1 passive,2 model AI. Signed: Mech. */
 char template_id[129]; /* Definition identity belongs to the body, not a save session. */
 bool suspended;bool enemy_awake;double wake_distance;int training_hits;double training_damage;int motion_step,motion_facing,motion_deadline,motion_octant;bool armed_motion,armed_up,armed_down;int armed_direction;double region_distance;double roll_entry_speed,roll_angle;v2d_t ledge,hang_start;uint64_t ledge_id;point2d_t support_position;v2d_t support_velocity;bool support_solid;int support_sample_tick;uint64_t region_id;int climb_direction,climb_ready_tick;bool climb_needs_release;v2d_t region_position;
} HostActor;
static HostActor* slots[64];
static bool controlled(const HostActor* h){return h&&(bsl_gameplay_enabled()?bsl_host_controlled(h->lifetime):bor_is_player(&h->a));}
static void gameplay_test_tick(void);
static void registry_character_test_tick(void);
static void registry_lifecycle_test_tick(void);
static void registry_factory_test_tick(void);
static void registry_event_test_tick(void);
static void registry_start_test_tick(void);
static void registry_placement_test_tick(void);
static void registry_dormant_test_tick(void);
static void registry_conversation_test_tick(void);
static void registry_interaction_test_tick(void);
static void registry_placement_binding_test_tick(void);
static void registry_actor_factory_test_tick(void);
static void registry_actor_event_test_tick(void);
static void registry_reconstruct_test_tick(void);
static void progression_test_tick(void);
static void learning_test_tick(void);
static void checkpoint_test_tick(void);
static void progress_save_test_tick(void);
static void progress_boot_test_tick(void);
static void pickup_test_tick(void);
static void loot_test_tick(void);
static void equipment_test_tick(void);
static void event_reward_test_tick(void);
static void quest_test_tick(void);
static void commerce_test_tick(void);
static void slope_test_tick(void);
static void pose_test_tick(void);
static void teleport_test_tick(void);
static uint64_t actor_lifetime;
static input_t* controls;
static sound_t** sounds;
static bool previous_keys[ALLEGRO_KEY_MAX],reset_requested;
static double world_time,last_tap[2]={-1000,-1000};
static unsigned random_state=0x6d696368;
static int total_impact_events,last_impact_material,last_impact_kind;
static int total_hits,total_frames,checks,failures,test_hits,total_sound_events;
static bool test_mode,bor_tick_requested;
static unsigned debug_mask=63;static bool debug_keys[6];
static void load_sounds(void){if(sounds)return;sounds=calloc(bor_sound_count,sizeof(*sounds));for(int i=0;i<bor_sound_count;i++)sounds[i]=sound_load(bor_sound_paths[i]);}
static void release_sounds(void){if(!sounds)return;for(int i=0;i<bor_sound_count;i++)if(sounds[i])sound_unref(sounds[i]);free(sounds);sounds=NULL;}
static void hit_segment(BorActor*,const BorFrame*,bool,void*);
static const obstaclemap_t* test_terrain;
static const obstaclemap_t* terrain_map(void){return test_terrain?test_terrain:level_obstaclemap();}
static unsigned rng(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static void commit_pose(HostActor* h){scripting_util_set_world_position(h->entity,v2d_new(h->a.x,h->a.y));}
static void traversal_clear(HostActor* h);
static void prepare_combat_poses(void);
static bool region_acquire(HostActor*,int);
static void region_player(HostActor*,int,bool);
static void region_step(HostActor*,const obstaclemap_t*);
static bool climb_state(BorState);
static bool encounter_ready(HostActor*);
static void update_actor_activity(void);
static void apply_physics(HostActor* h);
static bool guard_hit(HostActor*,HostActor*,const BorFrame*);
static bool hold_begin(HostActor*,HostActor*,int);
static bool soft_path_clear(HostActor*,v2d_t,v2d_t);
static bool body_sweep(HostActor*,const obstaclemap_t*,v2d_t,v2d_t);
static v2d_t soft_clip(HostActor*,v2d_t,v2d_t,bool);
static void contact_clear(HostActor*);
static void locomotion_update(HostActor*);
static void rebase(HostActor* h){
 v2d_t pos=scripting_util_world_position(h->entity);h->a.x=pos.x;h->a.y=pos.y;
 physicsactor_set_position(h->body,v2d_new(pos.x,pos.y-20));h->placed=true;h->surface_angle=0;
 if(!h->a.initialized){h->a.spawn_x=pos.x;h->a.spawn_y=pos.y;h->a.initialized=true;}
}
static void update_surface_rotation(HostActor* h){
 if(h->suspended||h->frozen_tick||h->a.freeze>0)return;
 double target=h->a.grounded&&!h->actor_support?physicsactor_get_angle(h->body)*3.141592653589793/180:0;
 h->surface_angle=bor_surface_angle_step(h->surface_angle,target,1.0/60);
}
static void render_frame(HostActor* h){
 const BorFrame* f=bor_frame(&h->a);
 char sprite_name[160];snprintf(sprite_name,sizeof(sprite_name),"%s",h->a.model->sprite);if(f->sprite>=256)snprintf(sprite_name,sizeof(sprite_name),"%s@%d",h->a.model->sprite,f->sprite/256);
 if(f->cw>0)snprintf(sprite_name,sizeof(sprite_name),"%s#%d",h->a.model->sprite,f->sprite);
 actor_change_animation(h->visual,sprite_get_animation(sprite_name,f->cw>0?0:f->sprite%256));
 h->a.pose_x=h->a.pose_y=0;h->a.pose_scale=1;
 double base=h->surface_angle;
 h->a.pose_angle=base;
 if(h->a.state==BOR_LEDGE||h->a.state==BOR_CLIMB){
  double progress=h->a.state==BOR_CLIMB?fmin(1,(double)h->job_tick/h->job.climb_ticks):0;
  double over=fmax(0,(progress-.8)/.2);v2d_t grip=v2d_new(h->ledge.x+h->a.facing*h->job.climb_inset*over,h->ledge.y-2*progress);
  int gx=h->a.state==BOR_LEDGE?65:60,gy=h->a.state==BOR_LEDGE?40:42;
  h->a.pose_x=grip.x-h->a.x+h->a.facing*(f->ox-gx);h->a.pose_y=grip.y-h->a.y+f->oy-gy;
 }
 if(climb_state(h->a.state))h->a.pose_y=f->oy-128;
 if(h->a.state==BOR_BARCLIMB){h->a.pose_angle=0;h->a.pose_x=h->a.facing*(f->ox-65);h->a.pose_y=f->oy-40-72;}
 if(h->a.state==BOR_ROLL||h->a.state==BOR_EVADE){double radius=f->ch>0?f->ch*.5:16;h->a.pose_angle=base+h->roll_angle;h->a.pose_x=-radius*sin(base);h->a.pose_y=-radius*cos(base);}
 h->visual->angle=h->a.pose_angle;
 surgescript_object_t* sprite=surgescript_objectmanager_get(surgescript_object_manager(h->entity),surgescript_object_child(h->entity,"Actor"));
 surgescript_transform_setposition2d(surgescript_object_transform(sprite),h->a.pose_x,h->a.pose_y);
 surgescript_transform_setscale2d(surgescript_object_transform(sprite),h->a.pose_scale,h->a.pose_scale);
 surgescript_transform_setrotation2d(surgescript_object_transform(sprite),h->visual->angle*180/3.141592653589793);
 h->visual->scale=v2d_new(h->a.pose_scale,h->a.pose_scale);
 h->visual->hot_spot=v2d_new((h->a.facing>0?f->ox:image_width(actor_image(h->visual))-f->ox),f->oy);h->visual->mirror=h->a.facing>0?IF_NONE:IF_HFLIP;
 h->visual->visible=h->a.state!=BOR_DEAD||h->lie_time<1.5;
}
static BorVelocity motion_query(HostActor* h){
 double angle=physicsactor_get_angle(h->body)*3.141592653589793/180.0;
 double gsp=physicsactor_get_gsp(h->body);
 BorVelocity v=h->a.grounded&&!h->actor_support?(BorVelocity){gsp*cos(angle),-gsp*sin(angle)}:(BorVelocity){physicsactor_get_xsp(h->body),physicsactor_get_ysp(h->body)};
 h->a.vx=v.x;h->a.vy=v.y;return v;
}
static bool motion_apply(HostActor* h,BorMotionMode mode,unsigned axes,double x,double y){
 BorVelocity before=motion_query(h),next;
 if(!bor_resolve_motion(before,mode,axes,x,y,&next))return false;
 if(mode==BOR_MOTION_PRESERVE)return true;
 double tangent=physicsactor_get_gsp(h->body);
 bool on_tangent=bor_ground_tangent(next,physicsactor_get_angle(h->body)*3.141592653589793/180.0,&tangent);
 bool launch=h->a.grounded&&(axes&BOR_AXIS_Y)&&next.y!=0&&!on_tangent;
 if(h->a.grounded&&!h->actor_support&&!launch&&!on_tangent)return false;
 if(launch){
  h->ignored_support=h->actor_support;h->actor_support=0;
  physicsactor_detach_from_ground(h->body);physicsactor_set_position(h->body,v2d_new(h->a.x,h->a.y-20-.5));h->a.grounded=false;
  physicsactor_set_xsp(h->body,next.x);physicsactor_set_ysp(h->body,next.y);
 }else{
  if(h->a.grounded&&!h->actor_support)physicsactor_set_gsp(h->body,tangent);
  if(axes&BOR_AXIS_X)physicsactor_set_xsp(h->body,next.x);
  if(axes&BOR_AXIS_Y)physicsactor_set_ysp(h->body,next.y);
 }
 h->a.vx=next.x;h->a.vy=next.y;return true;
}
static bool motion_tangent(HostActor* h,double speed){
 if(!h->a.grounded||h->actor_support)return motion_apply(h,BOR_MOTION_SET,BOR_AXIS_X,speed,0);
 double angle=physicsactor_get_angle(h->body)*3.141592653589793/180.0;
 return motion_apply(h,BOR_MOTION_SET,3,speed*cos(angle),-speed*sin(angle));
}
static void drive(HostActor* h,int direction,double speed){h->steering=direction;h->input_direction=direction;physicsactor_set_topspeed(h->body,h->physics.enabled&&!h->a.grounded?fmin(speed,h->physics.air_speed):speed);}
static bool start(HostActor* h,const char* animation,BorState state){return bor_start(&h->a,animation,state,hit_segment,h);}
static void clear_intent(HostActor* h){
 contact_clear(h);
 h->ai_phase=0;h->think_time=0;h->retreat_origin=0;h->input_direction=0;h->grab_finisher=false;h->steering=0;h->running=false;h->a.combo=0;h->a.combo_until=0;traversal_clear(h);
}
static HostActor* linked_partner(HostActor* h){
 int index=h->a.partner;if(index<0||index>=64||!slots[index])return NULL;
 HostActor* other=slots[index];return other!=h&&other->a.partner==(int)h->a.serial?other:NULL;
}
static void unlink_actor(HostActor* h){
 HostActor* other=linked_partner(h);h->a.partner=-1;h->grab_release=0;h->grab_finisher=false;
 if(other){
  other->a.partner=-1;other->grab_release=0;other->grab_finisher=false;
  if(other->a.state==BOR_GRABBED){clear_intent(other);other->reaction_tick=total_frames;motion_tangent(other,0);start(other,"pain",BOR_PAIN);}
 }
}
static void die(HostActor* h){
 if(controlled(h))bsl_host_hit(h->lifetime);
 if(h->a.model->training_dummy){unlink_actor(h);clear_intent(h);h->landing_damage=0;h->a.health=2147483647;h->a.x=h->a.spawn_x;h->a.y=h->a.spawn_y;commit_pose(h);rebase(h);physicsactor_restore_state(h->body);physicsactor_set_xsp(h->body,0);physicsactor_set_ysp(h->body,0);physicsactor_set_gsp(h->body,0);start(h,"idle",BOR_FREE);return;}
 if(h->a.state==BOR_DEAD)return;
 unlink_actor(h);clear_intent(h);h->landing_damage=0;h->lie_time=0;h->reaction_tick=total_frames;h->a.health=0;motion_tangent(h,0);
 start(h,"fall",BOR_DEAD);
 if(h->a.model->death_sound>=0&&sounds&&sounds[h->a.model->death_sound])sound_play(sounds[h->a.model->death_sound]);
}
static int impact_index(const HostActor* source,const HostActor* target,const BorFrame* frame,bool blocked){
 int material=target->a.model->impact_material,kind=blocked?3:source->a.animation->impact_kind,strength=source->a.animation->impact_strength;
 if(material<0||material>=bor_impact_material_count||kind<0||kind>3)return -1;
 if(!strength)strength=frame->damage<=4?1:frame->damage<=9?2:3;
 return bor_impact_sounds[material][kind][strength-1];
}
#include "bor_impact_feed.inc"
static uint64_t impact_spatial_count,impact_fallback_count,impact_last_voice;
static void play_impact(HostActor* source,HostActor* target,const BorFrame* frame,bool blocked,int actual,bool defeated){
 struct BorImpactRecord* contact=impact_publish(source,target,frame,blocked,actual,defeated);
 int index=impact_index(source,target,frame,blocked);
 if(index>=0&&sounds&&sounds[index]){
  /* Finite impact owns its lifetime independently of either actor. No duplicate fallback. Signed: Mech. */
  impact_last_voice=bsl_spatial_play_cached_path(bor_sound_paths[index],0x6900000000000000ULL|contact->id,contact->x,contact->y);
  if(impact_last_voice)impact_spatial_count++;else {sound_play(sounds[index]);impact_fallback_count++;}
  total_impact_events++;last_impact_material=target->a.model->impact_material;last_impact_kind=blocked?3:source->a.animation->impact_kind;
 }
}
/* Read-only cache/route counters and last finite spatial voice. Signed: Mech. */
static surgescript_var_t* impact_audio_view(surgescript_object_t* obj,const surgescript_var_t** args,int count){return NULL;}
#define AUDIO_PROBE(name,expr) static surgescript_var_t* impact_audio_##name(surgescript_object_t* obj,const surgescript_var_t** args,int count){BslCachedVoiceProbe cache={0};BslVoiceProbe voice={0};BslAcousticProbe acoustic={0};bsl_voice_cached_probe(&cache);bsl_spatial_probe(impact_last_voice,&voice);bsl_acoustic_probe(&acoustic);return surgescript_var_set_number(surgescript_var_create(),(double)(expr));}
AUDIO_PROBE(acquired,cache.acquired) AUDIO_PROBE(released,cache.released) AUDIO_PROBE(decoded,cache.decoded) AUDIO_PROBE(active,cache.active)
AUDIO_PROBE(spatial,impact_spatial_count) AUDIO_PROBE(fallback,impact_fallback_count) AUDIO_PROBE(position,voice.position)
AUDIO_PROBE(gain,voice.gain) AUDIO_PROBE(balance,voice.balance) AUDIO_PROBE(cutoff,voice.cutoff) AUDIO_PROBE(voices,acoustic.voices) AUDIO_PROBE(sources,acoustic.sources)
static surgescript_var_t* impact_audio_playing(surgescript_object_t* obj,const surgescript_var_t** args,int count){BslVoiceProbe p={0};bsl_spatial_probe(impact_last_voice,&p);return surgescript_var_set_bool(surgescript_var_create(),p.playing);}
static void register_impact_audio_view(surgescript_vm_t* vm){
 surgescript_vm_bind(vm,"BORImpactAudioView","state:main",impact_audio_view,0);
#define AUDIO_BIND(name) surgescript_vm_bind(vm,"BORImpactAudioView","get_"#name,impact_audio_##name,0)
 AUDIO_BIND(acquired);AUDIO_BIND(released);AUDIO_BIND(decoded);AUDIO_BIND(active);AUDIO_BIND(spatial);AUDIO_BIND(fallback);AUDIO_BIND(position);AUDIO_BIND(gain);AUDIO_BIND(balance);AUDIO_BIND(cutoff);AUDIO_BIND(playing);AUDIO_BIND(voices);AUDIO_BIND(sources);
#undef AUDIO_BIND
}
#undef AUDIO_PROBE

static void damage_reaction(HostActor* target,HostActor* source,const BorFrame* f){
 bsl_host_hit(target->lifetime);
 if(target->listening){target->provoked_until=world_time+5;bsl_acoustic_reaction(target->lifetime,target->a.health>0?3:-1,target->a.x,target->a.y-target->a.model->soft_height);}
 if(target->a.model->training_dummy){target->training_hits++;target->training_damage+=2147483647-target->a.health;target->a.health=2147483647;target->a.freeze=fmax(target->a.freeze,fmax(0,f->pause)/200.0);source->a.freeze=fmax(source->a.freeze,fmax(0,f->pause)/200.0);total_hits++;if(test_mode)test_hits++;return;}
 bool hold_link=linked_partner(target)==source&&f->drop<=0&&target->a.health>0;
 clear_intent(target);target->landing_damage=0;target->reaction_tick=total_frames;
 if(!hold_link)unlink_actor(target);
 target->a.facing=source->a.x>=target->a.x?1:-1;
 target->a.knockdown-=f->drop;target->a.knockdown_until=1;
 if(!target->a.health||target->a.knockdown<0){
  if(!target->a.health)die(target);else start(target,"fall",BOR_FALL);
  target->lie_time=0;target->a.knockdown=0;motion_apply(target,BOR_MOTION_SET,3,source->a.facing*120,-300);
 }else{start(target,"pain",hold_link?BOR_GRABBED:BOR_PAIN);motion_tangent(target,hold_link?0:source->a.facing*20);}
 target->a.freeze=fmax(target->a.freeze,fmax(0,f->pause)/200.0);source->a.freeze=fmax(source->a.freeze,fmax(0,f->pause)/200.0);
 total_hits++;if(test_mode)test_hits++;
}
extern void bsl_lab_attack(uint64_t,uint64_t,int,const double*);
static void hit_segment(BorActor* a,const BorFrame* f,bool entering,void* context){
 HostActor* h=(HostActor*)context;
 if(a->action_id!=h->progress_action){
  h->progress_action=a->action_id;
  const char* tag=a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_ATTACK)&&!(a->animation->flags&BOR_ANIM_RANGED)?(h->action_channel==0?"melee.primary":"melee.secondary"):"";
  h->progress_damage_bonus=bsl_host_action_begin(h->lifetime,a->action_id,h->registered_action_tag[0]?h->registered_action_tag:tag);
 }
 render_frame(h);
 if(entering){
  if(a->frame==0&&a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_RANGED))bsl_host_use_assigned(h->lifetime,a->action_id);
  /* Source frame motion is displacement, resolved before that frame's attack. */
  int direction=f->move*a->facing>0?1:-1;
  const struct obstaclemap_t* map=terrain_map();int width=19,height=39;
  physicsactor_bounding_box(h->body,&width,&height,NULL);
  for(int step=0;step<abs(f->move);step++){
   bool blocked=false;
   if(map)for(int y=3;y<height-2;y++)if(obstaclemap_solid_exists(map,(int)(a->x+direction*(width/2+1)),(int)(a->y-y),OL_DEFAULT)){blocked=true;break;}
   if(blocked||!soft_path_clear(h,v2d_new(a->x,a->y),v2d_new(a->x+direction,a->y)))break;
   a->x+=direction;
  }
  if(f->movea){v2d_t from=v2d_new(a->x,a->y),to=v2d_new(a->x,a->y-f->movea);if(body_sweep(h,terrain_map(),from,to))a->y=to.y;}physicsactor_set_position(h->body,v2d_new(a->x,a->y-20));commit_pose(h);
  if(a->animation->jump_frame==a->frame){
   motion_apply(h,BOR_MOTION_SET,3,a->facing*a->animation->jump_x*100,-a->animation->jump_y*100);
  }
  if(f->sound>=0&&sounds&&sounds[f->sound]){total_sound_events++;sound_play(sounds[f->sound]);}
 }
 if(!a->active||a->state==BOR_DEAD||a->state==BOR_FALL)return;
 double attack_corners[8];if(f->damage>0&&bor_corners(a,true,attack_corners))bsl_lab_attack(h->lifetime,a->epoch,a->facing,attack_corners);
 uint64_t action=a->action_id;
 for(int i=0;i<64;i++){
  if(a->action_id!=action||a->health<=0||!a->active||a->state==BOR_DEAD||a->state==BOR_FALL)break;
  HostActor* target=slots[i];if(!target||target==h||!target->placed||target->suspended)continue;
  if(a->partner>=0&&(unsigned)a->partner!=target->a.serial)continue;
  if(guard_hit(h,target,f))continue;
  BorHitResult result;
  if(bor_damage_with_bonus(a,&target->a,world_time,h->progress_damage_bonus,&result)){
   bsl_host_combat_result(h->lifetime,action,target->lifetime,a->epoch,result.actual,result.defeated&&!target->a.model->training_dummy,h->progress_damage_bonus);
   play_impact(h,target,f,false,result.actual,result.defeated&&!target->a.model->training_dummy);damage_reaction(target,h,f);
  }
 }
}
static HostActor* hero(void){for(int i=0;i<64;i++)if(slots[i]&&controlled(slots[i]))return slots[i];return NULL;}
static void reconstruct_actor(HostActor* h,double x,double y,int hp,bool preserve_ids){
 unsigned serial=h->a.serial;int model=h->a.model_index;double sx=h->a.spawn_x,sy=h->a.spawn_y;
 uint64_t action=h->a.action_id,epoch=h->a.epoch;
 bor_init(&h->a,bor_models+model,model);h->a.serial=serial;h->a.health=hp;h->a.x=x;h->a.y=y;h->a.spawn_x=sx;h->a.spawn_y=sy;h->a.initialized=true;
 if(preserve_ids){h->a.action_id=action;h->a.epoch=epoch;}
 h->progress_action=0;h->progress_damage_bonus=0;h->action_channel=0;h->registered_action_tag[0]=0;
 commit_pose(h);physicsactor_restore_state(h->body);physicsactor_set_xsp(h->body,0);physicsactor_set_ysp(h->body,0);physicsactor_set_gsp(h->body,0);rebase(h);clear_intent(h);
 bsl_acoustic_reaction(h->lifetime,-1,0,0);h->hearing_pending=h->investigating=false;h->search_until=h->provoked_until=0;
 h->suspended=false;h->enemy_awake=h->wake_distance<=0;h->training_hits=0;h->training_damage=0;h->lie_time=0;h->was_ground=false;h->landing_damage=0;h->reaction_tick=-1;h->frozen_tick=false;h->jumps_used=0;h->dash_ready_tick=h->ledge_ready_tick=0;h->tap_tick[0]=h->tap_tick[1]=-1000;h->tap_direction=0;h->grab_release=0;h->steering=0;h->running=false;h->job=h->physics;
 input_reset(h->drive_input);apply_physics(h);
 if(preserve_ids){bor_start(&h->a,hp>0?"idle":"fall",hp>0?BOR_FREE:BOR_DEAD,NULL,NULL);if(hp<=0){h->a.state=BOR_DEAD;h->a.active=false;}render_frame(h);}
 else start(h,"idle",BOR_FREE);
}
void bsl_bor_editor_origin(const surgescript_object_t* entity,float x,float y){
 if(!level_editmode()||!isfinite(x)||!isfinite(y))return;
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->entity==entity){slots[i]->a.spawn_x=x;slots[i]->a.spawn_y=y;return;}
}
bool bsl_bor_editor_origin_test(const surgescript_object_t* entity,float x,float y,bool reset){
 const char* mode=getenv("BSL_INSTANCE_TEST");if(!mode||strcmp(mode,"drag"))return false;
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->entity==entity){
  HostActor* h=slots[i];if(h->a.spawn_x!=x||h->a.spawn_y!=y)return false;
  if(reset){reconstruct_actor(h,h->a.spawn_x,h->a.spawn_y,h->a.model->health,false);return h->a.x==x&&h->a.y==y;}
  return true;
 }
 return false;
}
static void reset_all(void){
 bsl_level_sequence_exit_reprime();
 total_hits=0;total_impact_events=0;last_impact_material=last_impact_kind=-1;world_time=0;last_tap[0]=last_tap[1]=-1000;random_state=0x6d696368;
 for(int i=0;i<64;i++)if(slots[i])slots[i]->a.partner=-1;
 for(int i=0;i<64;i++)if(slots[i])reconstruct_actor(slots[i],slots[i]->a.spawn_x,slots[i]->a.spawn_y,slots[i]->a.model->health,false);
 bsl_host_reset();
}
static bool pressed(const ALLEGRO_KEYBOARD_STATE* keys,int code){return al_key_down(keys,code)&&!previous_keys[code];}
static bool down(const ALLEGRO_KEYBOARD_STATE* keys,int code){return al_key_down(keys,code);}
static const char* air_attack(const BorActor* a,bool crouch,bool running){
 const char* wanted=crouch?"jumpattack2":running?"runjumpattack":"jumpattack";
 return bor_animation(a->model,wanted)?wanted:bor_animation(a->model,"jumpattack")?"jumpattack":NULL;
}
static void player_actions(HostActor*,const ALLEGRO_KEYBOARD_STATE*);
static void legacy_player_actions(HostActor* h,const ALLEGRO_KEYBOARD_STATE* keys){
 BorActor* a=&h->a;if(h->frozen_tick||a->freeze>0)return;motion_query(h);int direction=(down(keys,ALLEGRO_KEY_D)||down(keys,ALLEGRO_KEY_RIGHT)||input_button_down(controls,IB_RIGHT))-(down(keys,ALLEGRO_KEY_A)||down(keys,ALLEGRO_KEY_LEFT)||input_button_down(controls,IB_LEFT));
 bool attack=pressed(keys,ALLEGRO_KEY_J)||input_button_pressed(controls,IB_FIRE2);
 bool jump=pressed(keys,ALLEGRO_KEY_SPACE)||input_button_pressed(controls,IB_FIRE1);
 bool crouch=down(keys,ALLEGRO_KEY_S)||down(keys,ALLEGRO_KEY_DOWN)||input_button_down(controls,IB_DOWN);
 bool special=pressed(keys,ALLEGRO_KEY_L)||input_button_pressed(controls,IB_FIRE5);
 bool roll=pressed(keys,ALLEGRO_KEY_K)||input_button_pressed(controls,IB_FIRE6);
 if(a->state==BOR_DEAD||a->state==BOR_PAIN||a->state==BOR_FALL||a->state==BOR_RISE||a->state==BOR_GRABBED)return;
 if(a->state==BOR_GRAB){
  HostActor* target=linked_partner(h);
  if(!target){a->partner=-1;start(h,"idle",BOR_FREE);return;}
  bool away=direction==-a->facing;
  if(!away)h->grab_release=world_time+.5;
  if((jump||(attack&&away))&&bor_animation(a->model,"throw")){
   unlink_actor(h);target->a.facing=a->facing;motion_apply(target,BOR_MOTION_SET,3,-a->facing*target->a.model->throw_distance,-target->a.model->throw_height);
   target->landing_damage=a->model->throw_damage;target->lie_time=0;target->reaction_tick=total_frames;start(target,"fall",BOR_FALL);
   start(h,"throw",BOR_ACTION);
  }else if(attack){
   bool finishing=crouch||a->grabs>=2;const char* move=finishing?"grabattack2":"grabattack";
   if(bor_animation(a->model,move)){a->grabs++;h->grab_finisher=finishing;start(h,move,BOR_ACTION);}
  }else if(away&&world_time>h->grab_release){unlink_actor(h);start(h,"idle",BOR_FREE);}
  return;
 }
 if(a->state==BOR_ACTION){
  if(!a->grounded&&(a->animation->flags&BOR_ANIM_AIR_CONTROL)){if(direction)a->facing=direction;drive(h,direction,(a->animation->flags&BOR_ANIM_RUN_JUMP)?a->model->run_speed:a->model->speed);}
  if(bor_try_cancel(a,(jump?BOR_INPUT_JUMP:0)|(special?BOR_INPUT_SPECIAL:0)|(attack?BOR_INPUT_ATTACK:0),hit_segment,h)){h->steering=0;return;}
  if(!a->grounded&&(a->animation->flags&BOR_ANIM_AIR_ATTACK)&&attack){
   const char* wanted=air_attack(a,crouch,(a->animation->flags&BOR_ANIM_RUN_JUMP)!=0);if(wanted)start(h,wanted,BOR_ACTION);
  }
  return;
 }
 if(!bor_can_act(a))return;
 bool running=down(keys,ALLEGRO_KEY_LSHIFT)||down(keys,ALLEGRO_KEY_RSHIFT)||(h->running&&direction&&direction==h->input_direction);
 if(direction&&direction!=h->input_direction){int side=direction>0;running=running||(world_time>=last_tap[side]&&world_time-last_tap[side]<.25);last_tap[side]=world_time;}
 if(direction==h->input_direction&&a->vx!=0&&fabs(a->vx)>a->model->speed+1)running=true;
 running=running&&bor_animation(a->model,"run")!=NULL;
 h->input_direction=direction;h->running=running&&direction;
 if(direction)a->facing=direction;
 drive(h,crouch&&a->grounded?0:direction,running?a->model->run_speed:a->model->speed);
 if(attack){
  if(!a->grounded){const char* wanted=air_attack(a,crouch,running);if(wanted)start(h,wanted,BOR_ACTION);}
  else if(running&&direction&&bor_animation(a->model,"runattack"))start(h,"runattack",BOR_ACTION);
  else bor_chain_attack(a,hit_segment,h);
 }else if(special){start(h,"special",BOR_ACTION);}
 else if(roll&&a->grounded){start(h,"dodge",BOR_ACTION);}
 else if(jump&&a->grounded){
  bool runjump=running&&bor_animation(a->model,"runjump")!=NULL;
  const char* wanted=runjump?"runjump":direction&&bor_animation(a->model,"forwardjump")?"forwardjump":"jump";
  if(bor_animation(a->model,wanted)){
   motion_apply(h,BOR_MOTION_SET,BOR_AXIS_Y,0,-(runjump?a->model->run_jump:a->model->jump));start(h,wanted,BOR_ACTION);
  }
 }
 else{
  const char* wanted=crouch&&a->grounded?"duck":(!a->grounded?"walkoff":direction?(running?"run":"walk"):"idle");
  if(!bor_animation(a->model,wanted))wanted="idle";
  if(strcmp(wanted,a->animation->name))start(h,wanted,a->grounded?BOR_FREE:BOR_ACTION);
  if(a->grounded&&direction&&bor_animation(a->model,"grab")){
   for(int i=0;i<64;i++)if(slots[i]&&slots[i]!=h&&slots[i]->a.model->grabbable&&bor_can_target(a,&slots[i]->a)&&slots[i]->a.health>0&&slots[i]->a.state==BOR_FREE&&slots[i]->a.grounded&&fabs(slots[i]->a.x-a->x)<a->model->grab_distance&&fabs(slots[i]->a.y-a->y)<8){
    if(hold_begin(h,slots[i],direction))break;
   }
  }
 }
}

#include "bor_traversal.inc"
#include "bor_regions.inc"
#include "bor_contact.inc"

/* Selected 7555 normal_attack/normal_prepare/attack_finish branches, with Z removed. */
static HostActor* enemy_target(HostActor* h){
 HostActor* target=NULL;double best=1e30;
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]!=h){
  HostActor* other=slots[i];BorActor* a=&other->a;
  if(other->suspended||a->model->training_dummy||!bor_can_target(&h->a,a))continue;
  double distance=fabs(a->x-h->a.x)+fabs(a->y-h->a.y);
  if(distance<best){best=distance;target=other;}
 }
 return target;
}
static bool attack_range(HostActor* h,HostActor* target,const BorAnimation* animation){
 if(!target||!animation)return false;
 BorActor* t=&target->a;const BorFrame* frame=bor_frame(t);
 if(frame->bw<=0&&(t->state==BOR_ACTION||t->state==BOR_FALL||t->state==BOR_RISE))return false;
 double dx=fabs(t->x-h->a.x),dy=t->y-h->a.y;
 return dx>=animation->range_min&&dx<=animation->range_max&&dy>=-80&&dy<=40;
}
static const char* normal_choice(HostActor* h,HostActor* target){
 int total=0;
 for(int i=0;i<h->a.model->count;i++){
  const BorAnimation* a=h->a.model->animations+i;char* end=NULL;
  if(strncmp(a->name,"attack",6))continue;
  long number=strtol(a->name+6,&end,10);if(number<=0||*end||!attack_range(h,target,a))continue;
  total+=number<=6?7-number:1;
 }
 if(!total)return NULL;
 int selected=rng()%total;
 for(int i=0;i<h->a.model->count;i++){
  const BorAnimation* a=h->a.model->animations+i;char* end=NULL;
  if(strncmp(a->name,"attack",6))continue;
  long number=strtol(a->name+6,&end,10);if(number<=0||*end||!attack_range(h,target,a))continue;
  selected-=number<=6?7-number:1;if(selected<0)return a->name;
 }
 return NULL;
}
/* Hearing queues intent across committed BOR actions. It never assigns a combat target. */
static void hearing_sense(HostActor* h){
 if(!h->listening||h->a.health<=0)return;
 BslAcousticResult sound;
 if(bsl_acoustic_hear(h->lifetime,h->a.x,h->a.y-24,1,h->hearing_threshold,&sound)){
  h->pending_hearing=v2d_new(sound.x,sound.y);h->hearing_pending=true;bsl_acoustic_reaction(h->lifetime,1,h->a.x,h->a.y-h->a.model->soft_height);
 }
}
static bool hearing_actions(HostActor* h,double dt){
 if(!h->listening||h->provoked_until>world_time)return false;
 if(!bor_can_act(&h->a))return true;
 if(h->hearing_pending){h->hearing_at=h->pending_hearing;h->hearing_pending=false;h->investigating=true;h->search_until=0;bsl_acoustic_reaction(h->lifetime,2,h->a.x,h->a.y-h->a.model->soft_height);}
 if(!h->investigating){drive(h,0,h->a.model->speed);if(strcmp(h->a.animation->name,"idle"))start(h,"idle",BOR_FREE);return true;}
 double dx=h->hearing_at.x-h->a.x;int direction=dx>0?1:-1;
 const obstaclemap_t* map=terrain_map();int ahead=(int)(h->a.x+direction*24),feet=(int)h->a.y;
 bool blocked=obstaclemap_solid_exists(map,ahead,feet-20,OL_DEFAULT);
 bool floor=obstaclemap_solid_exists(map,ahead,feet+12,OL_DEFAULT);
 bool no_route=blocked||!floor||h->contact_x_blocked||(fabs(dx)<18&&fabs(h->hearing_at.y-(h->a.y-24))>64);
 if(fabs(dx)<18||no_route||h->search_until>0){
  drive(h,0,h->a.model->speed);if(strcmp(h->a.animation->name,"idle"))start(h,"idle",BOR_FREE);
  if(!h->search_until){if(no_route)bsl_acoustic_reaction(h->lifetime,1,h->a.x,h->a.y-h->a.model->soft_height);h->search_until=world_time+1.5;}
  if(world_time>=h->search_until){h->investigating=false;h->search_until=0;}
 }else{h->a.facing=direction;drive(h,direction,h->a.model->speed*.65);if(strcmp(h->a.animation->name,"walk"))start(h,"walk",BOR_FREE);}
 (void)dt;return true;
}
static void enemy_actions(HostActor* h,double dt){
 motion_query(h);BorActor* a=&h->a;if(h->frozen_tick||a->freeze>0)return;
 if(a->state==BOR_GRAB){
  HostActor* victim=linked_partner(h);if(!victim){a->partner=-1;start(h,"idle",BOR_FREE);return;}
  if(world_time>=h->grab_release){
   const char* move=a->grabs<2?"grabattack":"grabattack2";
   if(bor_animation(a->model,move)){h->grab_finisher=a->grabs>=2;a->grabs++;start(h,move,BOR_ACTION);}
   else{unlink_actor(h);start(h,"idle",BOR_FREE);}
  }
  return;
 }
 if(hearing_actions(h,dt))return;
 if(!bor_can_act(a))return;
 HostActor* target=enemy_target(h);
 if(!target)return;
 double dx=target->a.x-a->x;int direction=dx>0?1:-1;
 if(h->ai_phase==2){
  h->think_time=fmax(0,h->think_time-dt);
  if(!a->grounded)return; /* retain authored backwalk jump impulse */
  if(fabs(a->x-h->retreat_origin)>1&&h->think_time>0){drive(h,h->retreat_origin>a->x?1:-1,a->model->speed);return;}
  h->ai_phase=3;start(h,"idle",BOR_FREE);return;
 }
 if(h->ai_phase==3){h->think_time-=dt;if(h->think_time<=0)h->ai_phase=0;return;}
 if(h->ai_phase==1){
  h->think_time-=dt;if(h->think_time>0)return;
  h->ai_phase=0;if(!a->grounded)return;
  const char* chosen=normal_choice(h,target);if(!chosen)return;
  a->facing=direction;start(h,chosen,BOR_ACTION);return;
 }
 /* OpenBOR suppresses every fourth nominal 100 Hz think phase, not random 25%. */
 bool think_phase=((int)floor(world_time*100+1e-7)%4)!=0;
 const char* candidate=normal_choice(h,target);
 if(a->grounded&&think_phase&&candidate){
  h->ai_phase=1;h->think_time=(rng()%(unsigned)fmax(1,150.0-a->model->aggression))/200.0;
  if(strcmp(a->animation->name,"idle"))start(h,"idle",BOR_FREE);
 }else{
  a->facing=direction;drive(h,direction,a->model->speed);
  if(strcmp(a->animation->name,"walk"))start(h,"walk",BOR_FREE);
 }
}
static void finish_action(HostActor* h){
 BorActor* a=&h->a;
 if(a->state==BOR_FALL||a->state==BOR_DEAD){
  if(a->grounded)h->lie_time+=1.0/60;
  if(a->state==BOR_FALL&&a->grounded&&h->lie_time>=(controlled(h)?0:1)){start(h,"rise",BOR_RISE);}
  return;
 }
 if(a->state==BOR_GRABBED)return;
 if(a->state==BOR_GRAB){if(!linked_partner(h)){a->partner=-1;start(h,"idle",BOR_FREE);}return;}
 if(a->state==BOR_FREE||a->state>=BOR_DASH)return;
 if(a->state==BOR_PAIN&&!a->grounded)return;
 if(a->state==BOR_ACTION&&!a->grounded)return;
 if(!a->finished)return;
 if(a->partner>=0){
  if(h->grab_finisher)unlink_actor(h);
  else if(linked_partner(h)){h->grab_release=world_time+.5;start(h,"grab",BOR_GRAB);return;}
  else a->partner=-1;
 }
 bool attack=a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_ATTACK)!=0;
 start(h,a->grounded?"idle":"walkoff",BOR_FREE);
 if(!controlled(h)&&attack){
  HostActor* target=enemy_target(h);
  if(target&&fabs(target->a.x-a->x)<80&&(rng()%4)!=0){h->ai_phase=2;h->retreat_origin=a->x>target->a.x?fmin(a->x+40,target->a.x+80):fmax(a->x-40,target->a.x-80);h->think_time=fmax(.5,(200.0-a->model->aggression)/200.0);start(h,bor_animation(a->model,"backwalk")?"backwalk":"walk",BOR_FREE);}
  else{h->ai_phase=3;h->think_time=fmax(.5,(200.0-a->model->aggression)/200.0);}
 }
}
static void move_actor(HostActor* h){
 BorActor* a=&h->a;if(h->suspended||h->frozen_tick||a->freeze>0||a->state==BOR_GRABBED)return;
 if(h->physics.enabled&&climb_state(a->state)){region_step(h,terrain_map());render_frame(h);commit_pose(h);return;}
 if(h->physics.enabled&&(a->state==BOR_LEDGE||a->state==BOR_CLIMB)){traversal_step(h,terrain_map());render_frame(h);commit_pose(h);return;}
 if(a->state==BOR_DEAD&&a->y>11000){h->lie_time=1.5;a->vx=a->vy=0;return;}
 
 actor_support_step(h);
 v2d_t from=v2d_new(a->x,a->y);
 input_reset(h->drive_input);
 bool ranged=a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_RANGED);
 bool steering=(a->state==BOR_FREE)||ranged||(a->state==BOR_ACTION&&!a->grounded&&(a->animation->flags&BOR_ANIM_AIR_CONTROL));
 if(h->physics.enabled){
  physicsactor_set_frc(h->body,terrain_friction(h,a->state==BOR_ROLL?h->job.roll_friction:a->state==BOR_DASH||a->state==BOR_EVADE||ranged?0:h->physics.deceleration));
  physicsactor_set_air(h->body,h->physics.acceleration*h->physics.air_control+(h->steering&&h->a.vx*h->steering<0?h->physics.deceleration:0));
  if(steering&&!a->grounded&&!ranged){double vx=h->a.vx*h->physics.air_drag;if(!h->steering)vx=copysign(fmax(0,fabs(vx)-h->physics.air_brake/60),vx);motion_apply(h,BOR_MOTION_SET,1,vx,0);}
  if(a->state==BOR_DASH)motion_tangent(h,h->job_direction*h->job.dash_speed);
  if(a->state==BOR_WALL_JUMP)motion_apply(h,BOR_MOTION_SET,3,h->job_direction*h->job.wall_jump_x,-h->job.wall_jump_y);
 }
 if(steering&&h->steering)input_simulate_button_down(h->drive_input,h->steering>0?IB_RIGHT:IB_LEFT);
 physicsactor_capture_input(h->body,h->drive_input);h->steering=0;
 const struct obstaclemap_t* map=terrain_map();if(map)physicsactor_update(h->body,map);
 v2d_t p=physicsactor_get_position(h->body);a->x=p.x;a->y=p.y+20;a->vy=physicsactor_get_ysp(h->body);a->grounded=!physicsactor_is_midair(h->body);
 if(a->vy>(h->physics.enabled?h->physics.max_fall:600))physicsactor_set_ysp(h->body,h->physics.enabled?h->physics.max_fall:600);
 v2d_t candidate=v2d_new(a->x,a->y),resolved=contact_resolve(h,from,candidate);
 if(h->contact_x_blocked){if(!physicsactor_is_midair(h->body))physicsactor_set_gsp(h->body,0);physicsactor_set_xsp(h->body,0);}
 a->x=resolved.x;a->y=resolved.y;physicsactor_set_position(h->body,v2d_new(a->x,a->y-20));
 if(h->actor_support){HostActor* support=contact_actor(h->actor_support);if(support&&fabs(a->y-(support->a.y-support->a.model->soft_height))<1&&fabs(a->x-support->a.x)<a->model->soft_radius+support->a.model->soft_radius-.5){a->grounded=true;physicsactor_set_ysp(h->body,0);}else{h->ignored_support=h->actor_support;h->actor_support=0;}}
 motion_query(h);
 if(a->grounded&&!h->was_ground&&h->landing_damage>0){
  if(a->model->training_dummy)a->health=2147483647;else a->health-=h->landing_damage;h->landing_damage=0;if(a->health<=0)die(h);
 }
 if(a->grounded&&!h->was_ground&&a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_JUMP_LAND)){
  if(!start(h,"jumpland",BOR_ACTION))start(h,"idle",BOR_FREE);
 }
 if(h->physics.enabled){
  if(a->grounded)h->jumps_used=0;
  if(a->state==BOR_ROLL||a->state==BOR_EVADE){h->roll_angle-=(a->x-h->roll_entry_speed)*8*3.141592653589793/180.0;h->roll_entry_speed=a->x;}
  traversal_after_motion(h);
 }
 pressure_hold(h);locomotion_update(h);grip_follow(h);
 h->was_ground=a->grounded;commit_pose(h);
 if(a->y>11000&&a->state!=BOR_DEAD)die(h);
}
static void advance_host(HostActor* h){
 if(h->suspended)return;
 if(!h->frozen_tick&&h->a.freeze<=0&&h->reaction_tick!=total_frames){
  bool fault=h->a.timeline_fault;
  if(climb_state(h->a.state)&&!h->climb_direction&&!(h->a.state==BOR_BARCLIMB&&h->input_direction)){render_frame(h);return;}
  if(climb_state(h->a.state)&&h->a.finished){h->a.frame=0;h->a.remaining=bor_frame(&h->a)->duration;h->a.finished=false;}
  bor_advance_animation(&h->a,1.0/60,hit_segment,h);
  if(!fault&&h->a.timeline_fault)logfile_message("BOR timeline budget exceeded: %s; reset required",h->a.model->name);
  if(!h->a.timeline_fault)finish_action(h);
 }
 render_frame(h);
}
static void assertion(bool value,const char* name){checks++;if(!value){failures++;logfile_message("BOR_CHECK FAIL %s",name);}else logfile_message("BOR_CHECK PASS %s",name);}
static void test_place(HostActor* h,double x,double y){
 h->a.x=x;h->a.y=y;h->a.grounded=true;commit_pose(h);rebase(h);
}
static void test_fixture(double floor){
 reset_all();for(int i=0;i<64;i++)if(slots[i])test_place(slots[i],1500+i*100,floor);
 HostActor* p=hero();test_place(p,100,floor);p->a.facing=1;test_place(slots[1],130,floor);slots[1]->a.facing=-1;
 input_reset(controls);
}
static void test_grab(HostActor* p,HostActor* target){
 target->a.x=p->a.x+12;target->a.y=p->a.y;commit_pose(target);rebase(target);
 ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));input_simulate_button_down(controls,IB_RIGHT);player_actions(p,&keys);input_reset(controls);
}
static void hardening_checks(double floor){
 HostActor* p=hero();HostActor* red=slots[1];int tick=total_frames;
 for(int reversed=0;reversed<2;reversed++){
  test_fixture(floor);
  if(reversed){slots[0]=red;slots[1]=p;red->a.serial=0;p->a.serial=1;}
  start(p,"attack1",BOR_ACTION);
  int active=2; /* First source slash that intersects the short ninja at equal feet. */
  p->a.frame=active-1;p->a.remaining=1.0/120;p->a.active=false;p->a.epoch=0;
  for(int i=0;i<64;i++)if(slots[i]&&!slots[i]->suspended)slots[i]->frozen_tick=bor_tick_status(&slots[i]->a,1.0/60);
  for(int i=0;i<64;i++)if(slots[i])advance_host(slots[i]);
  assertion(red->a.health<40&&red->a.age==0&&red->a.frame==0&&fabs(red->a.invulnerable-.2)<1e-9,"fresh reaction and hit protection are not aged by registration order");
  total_frames++;
  for(int i=0;i<64;i++)if(slots[i]&&!slots[i]->suspended)slots[i]->frozen_tick=bor_tick_status(&slots[i]->a,1.0/60);
  for(int i=0;i<64;i++)if(slots[i])advance_host(slots[i]);
  assertion(fabs(red->a.age-1.0/60)<1e-9,"new reaction begins aging on the next simulation tick");
  total_frames=tick;slots[0]=p;slots[1]=red;p->a.serial=0;red->a.serial=1;
 }
 test_fixture(floor);red->ai_phase=1;red->think_time=.7;red->retreat_origin=555;
 start(p,"attack1",BOR_ACTION);bor_advance(&p->a,.15,hit_segment,p);
 assertion(red->a.health<40&&red->ai_phase==0&&red->think_time==0&&red->retreat_origin==0,"damage cancels stale enemy preparation and retreat intent");
 test_fixture(floor);test_grab(p,red);assertion(linked_partner(p)==red&&red->a.state==BOR_GRABBED,"actual contextual input links the grab pair");
 bor_advance(&p->a,10.1,hit_segment,p);finish_action(p);
 assertion(p->a.finished&&p->a.state==BOR_GRAB&&linked_partner(p)==red,"grab pose completion does not release live gameplay ownership");
 ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));input_simulate_button_down(controls,IB_LEFT);player_actions(p,&keys);
 assertion(linked_partner(p)==red&&red->landing_damage==0,"away alone does not throw or immediately release");
 world_time+=.51;player_actions(p,&keys);input_reset(controls);
 assertion(p->a.partner<0&&red->a.partner<0&&p->a.state==BOR_FREE,"holding away releases after the separate BOR release clock");
 test_fixture(floor);test_grab(p,red);input_simulate_button_press(controls,IB_FIRE1);player_actions(p,&keys);input_reset(controls);
 assertion(p->a.partner<0&&red->a.partner<0&&red->a.state==BOR_FALL&&red->a.vx<0&&red->landing_damage==21,"explicit throw creates the source launch profile and clears both links");
 red->a.health=10;red->a.y=floor-1;red->was_ground=false;red->a.grounded=false;commit_pose(red);rebase(red);physicsactor_detach_from_ground(red->body);physicsactor_set_ysp(red->body,120);move_actor(red);
 assertion(red->a.state==BOR_DEAD&&red->a.health==0&&red->landing_damage==0,"lethal throw landing uses the shared death cleanup");
 test_fixture(floor);HostActor* green=slots[2];p->a.partner=red->a.serial;red->a.partner=green->a.serial;green->a.partner=red->a.serial;
 unlink_actor(p);assertion(p->a.partner<0&&linked_partner(red)==green,"stale link cleanup preserves another reciprocal pair");
 test_fixture(floor);test_grab(p,red);p->a.health=0;die(p);finish_action(red);uint64_t death=p->a.action_id;die(p);
 assertion(p->a.state==BOR_DEAD&&p->a.partner<0&&red->a.partner<0&&red->a.state!=BOR_GRABBED&&p->a.action_id==death,"death releases both participants exactly once");
 for(int reversed=0;reversed<2;reversed++){
  test_fixture(floor);p->a.health=red->a.health=1;
  if(reversed){slots[0]=red;slots[1]=p;red->a.serial=0;p->a.serial=1;}
  bor_start(&p->a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&p->a,.11,NULL,NULL);
  bor_start(&red->a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&red->a,.13,NULL,NULL);
  for(int i=0;i<2;i++)hit_segment(&slots[i]->a,bor_frame(&slots[i]->a),false,slots[i]);
  assertion(reversed?(p->a.state==BOR_DEAD&&red->a.health==1):(red->a.state==BOR_DEAD&&p->a.health==1),"stable sequential lethal priority prevents dead actors completing an old attack");
  slots[0]=p;slots[1]=red;p->a.serial=0;red->a.serial=1;
 }
 test_fixture(floor);
 BorModel reduced=*p->a.model;BorAnimation clips[128];int count=0;
 for(int i=0;i<reduced.count;i++)if(strcmp(reduced.animations[i].name,"grab")&&strcmp(reduced.animations[i].name,"jump")&&strcmp(reduced.animations[i].name,"runjump")&&strcmp(reduced.animations[i].name,"forwardjump"))clips[count++]=reduced.animations[i];
 reduced.animations=clips;reduced.count=count;p->a.model=&reduced;start(p,"idle",BOR_FREE);test_grab(p,red);
 assertion(p->a.partner<0&&red->a.partner<0&&red->a.state==BOR_FREE,"missing optional grab cannot acquire or strand a victim");
 input_simulate_button_press(controls,IB_FIRE1);player_actions(p,&keys);input_reset(controls);
 assertion(p->a.grounded&&p->a.state==BOR_FREE&&p->a.vy==0&&!physicsactor_is_midair(p->body),"missing optional jump cannot detach or launch the body");
 reset_all();test_fixture(floor);
 BorAnimation jump=*bor_animation(p->a.model,"jump");BorCancel cancel={0,2,0,BOR_INPUT_ATTACK,"freespecial3"};jump.cancels=&cancel;jump.cancel_count=1;
 bor_start(&p->a,"jump",BOR_ACTION,NULL,NULL);p->a.animation=&jump;p->a.grounded=false;
 input_simulate_button_press(controls,IB_FIRE2);player_actions(p,&keys);input_reset(controls);
 assertion(!strcmp(p->a.animation->name,"freespecial3"),"actual attack input resolves authored cancel before generic aerial attack");
 total_frames=tick;test_fixture(floor);
}
static bool test_script_motion(HostActor* h,const char* mode,const char* axes,double x,double y){
 surgescript_object_t* object=surgescript_objectmanager_get(surgescript_object_manager(h->entity),surgescript_object_child(h->entity,"BORController"));
 surgescript_var_t* values[]={surgescript_var_set_string(surgescript_var_create(),mode),surgescript_var_set_string(surgescript_var_create(),axes),surgescript_var_set_number(surgescript_var_create(),x),surgescript_var_set_number(surgescript_var_create(),y)};
 surgescript_var_t* result=surgescript_var_create();surgescript_object_call_function(object,"applyMotion",(const surgescript_var_t**)values,4,result);bool ok=surgescript_var_get_bool(result);
 surgescript_var_destroy(result);for(int i=0;i<4;i++)surgescript_var_destroy(values[i]);return ok;
}
static void slope_physics_checks(double floor){
 test_fixture(floor);HostActor* p=hero();HostActor* red=slots[1];
 image_t* previous=image_drawing_target();image_t* image=image_create(256,128);image_set_drawing_target(image);image_clear(color_rgba(0,0,0,0));
 for(int x=0;x<256;x++)image_line(x,100-x/4,x,127,color_rgb(255,255,255));
 image_set_drawing_target(previous);
 collisionmask_t* mask=collisionmask_create(image,0,0,256,128,0);obstacle_t* ramp=obstacle_create(mask,(point2d_t){100,(int)floor-128},OL_DEFAULT,0);obstaclemap_t* map=obstaclemap_create();obstaclemap_add(map,ramp);obstaclemap_build(map);
 for(int who=0;who<2;who++){
  HostActor* h=who?red:p;h->a.x=200+who*12;h->a.y=floor-160;h->a.grounded=false;commit_pose(h);rebase(h);input_reset(h->drive_input);physicsactor_capture_input(h->body,h->drive_input);
  for(int step=0;step<90;step++)physicsactor_update(h->body,map);
  v2d_t at=physicsactor_get_position(h->body);h->a.x=at.x;h->a.y=at.y+20;h->a.grounded=!physicsactor_is_midair(h->body);commit_pose(h);motion_query(h);
 }
 double angle=physicsactor_get_angle(p->body)*3.141592653589793/180.0;
 assertion(p->a.grounded&&red->a.grounded&&fabs(sin(angle))>.1,"real Surge terrain solver acquires the isolated inclined collision fixture");
 for(int step=0;step<120;step++)update_surface_rotation(p);
 render_frame(p);double corners[8];bor_corners(&p->a,false,corners);
 assertion(fabs(p->a.pose_angle-angle)<1e-6&&fabs(p->visual->angle-angle)<1e-6&&fabs(corners[3]-corners[1])>1,"slope visual and combat-box edges settle together toward the actual terrain angle");
 motion_tangent(p,100);BorVelocity original=motion_query(p);
 assertion(!test_script_motion(p,"stop","y",0,0)&&fabs(motion_query(p).x-original.x)<1e-7&&fabs(motion_query(p).y-original.y)<1e-7,"incompatible grounded world-axis stop rejects without changing slope momentum");
 test_grab(p,red);
 assertion(linked_partner(p)==red&&physicsactor_get_gsp(p->body)==0&&p->a.vx==0&&p->a.vy==0,"actual contextual grab stops its owner coherently on inclined terrain");
 unlink_actor(p);start(red,"idle",BOR_FREE);motion_tangent(red,100);BorFrame impact=*bor_frame(&red->a);impact.drop=0;impact.pause=0;
 damage_reaction(red,p,&impact);
 assertion(red->a.grounded&&red->a.state==BOR_PAIN&&fabs(physicsactor_get_gsp(red->body)-20)<1e-6,"pain reaction applies its declared tangent impulse on inclined terrain");
 motion_tangent(p,100);p->a.health=0;die(p);
 assertion(p->a.state==BOR_DEAD&&physicsactor_get_gsp(p->body)==0&&p->a.vx==0&&p->a.vy==0,"death stops ground motion coherently on inclined terrain");
 obstaclemap_destroy(map);obstacle_destroy(ramp);collisionmask_destroy(mask);image_destroy(image);test_fixture(floor);
}
static void preliminary_physics_checks(double floor){
 HostActor* p=hero();ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));
 for(int i=0;i<5;i++)for(int facing=-1;facing<=1;facing+=2){
  HostActor* h=slots[i];h->a.facing=facing;render_frame(h);const BorFrame* frame=bor_frame(&h->a);double x,y,w,hh;bor_rect(&h->a,false,&x,&y,&w,&hh);
  double left=h->a.x-h->visual->hot_spot.x+(facing>0?frame->bx:image_width(actor_image(h->visual))-frame->bx-frame->bw);
  assertion(fabs(left-x)<1e-7&&fabs(h->a.y-h->visual->hot_spot.y+frame->by-y)<1e-7,"rendered source bounds and combat rectangles share the origin in both facings");
 }

 const double vertical[]={-120,0,120};
 for(int phase=0;phase<3;phase++){
  test_fixture(floor);start(p,"jump",BOR_ACTION);p->a.y=floor-200;p->a.grounded=false;p->a.vx=200;p->a.vy=vertical[phase];commit_pose(p);rebase(p);
  physicsactor_detach_from_ground(p->body);physicsactor_set_xsp(p->body,200);physicsactor_set_ysp(p->body,vertical[phase]);double x=p->a.x;
  for(int i=0;i<10;i++)move_actor(p);
  logfile_message("BOR_PHYSICS phase=%d distance=%.6f vx=%.6f body_xsp=%.6f",phase,p->a.x-x,p->a.vx,physicsactor_get_xsp(p->body));
  assertion(fabs(p->a.x-x-200.0/6)<.05&&fabs(p->a.vx-200)<1e-6,"neutral airborne travel preserves momentum across ascent apex and descent");
 }
 test_fixture(floor);p->a.vx=200;p->input_direction=1;physicsactor_set_xsp(p->body,200);physicsactor_set_gsp(p->body,200);
 input_simulate_button_down(controls,IB_RIGHT);input_simulate_button_press(controls,IB_FIRE1);player_actions(p,&keys);input_reset(controls);
 assertion(!p->a.grounded&&!strcmp(p->a.animation->name,"runjump")&&fabs(p->a.vx-200)<1e-6,"actual running jump inherits takeoff speed rather than replacing it");
 p->a.frame=1;input_simulate_button_press(controls,IB_FIRE5);player_actions(p,&keys);input_reset(controls);
 assertion(!strcmp(p->a.animation->name,"freespecial3")&&fabs(p->a.vx-200)<1e-6,"actual airborne cancel preserves momentum without an authored replacement");
 test_fixture(floor);start(p,"jump",BOR_ACTION);p->a.y=floor-200;p->a.grounded=false;p->a.vx=200;commit_pose(p);rebase(p);physicsactor_detach_from_ground(p->body);physicsactor_set_xsp(p->body,200);physicsactor_set_ysp(p->body,0);
 input_simulate_button_press(controls,IB_FIRE2);player_actions(p,&keys);input_reset(controls);double x=p->a.x;move_actor(p);
 assertion(!strcmp(p->a.animation->name,"jumpattack")&&p->a.x>x&&fabs(p->a.vx-200)<1e-6,"ordinary aerial attack commits without stopping inherited motion");
 physicsactor_set_xsp(p->body,0);x=p->a.x;move_actor(p);
 assertion(fabs(p->a.x-x)<.001&&fabs(p->a.vx)<1e-6,"solver horizontal stop is read back and never overwritten by stale actor velocity");
 motion_apply(p,BOR_MOTION_SET,3,200,0);start(p,"jump",BOR_ACTION);input_simulate_button_down(controls,IB_LEFT);player_actions(p,&keys);input_reset(controls);move_actor(p);
 assertion(p->a.vx>0&&p->a.vx<200,"allowed air steering uses Surge acceleration rather than instant BOR velocity reversal");
 motion_apply(p,BOR_MOTION_SET,3,200,0);start(p,"freespecial3",BOR_ACTION);input_simulate_button_down(controls,IB_LEFT);player_actions(p,&keys);input_reset(controls);move_actor(p);
 assertion(fabs(p->a.vx-200)<1e-6,"opposite input cannot steer a committed special without air-control permission");
 start(p,"freespecial1",BOR_ACTION);
 assertion(p->a.vx==0&&p->a.vy==-400&&physicsactor_get_xsp(p->body)==0,"authored jumpframe zero-X remains an explicit stop");
 p->a.facing=1;start(p,"freespecial2",BOR_ACTION);
 assertion(p->a.vx==300&&p->a.vy==-400,"authored jumpframe replacement applies once to the authoritative body");
 assertion(test_script_motion(p,"set","xy",200,0)&&p->a.vx==200&&p->a.vy==0,"SurgeScript motion binding sets the actual physics body");
 assertion(test_script_motion(p,"add","x",25,0)&&p->a.vx==225,"SurgeScript additive impulse preserves the unselected axis");
 surgescript_object_t* controller=surgescript_objectmanager_get(surgescript_object_manager(p->entity),surgescript_object_child(p->entity,"BORController"));surgescript_var_t* query=surgescript_var_create();
 surgescript_object_call_function(controller,"get_velocityX",NULL,0,query);double queried_x=surgescript_var_get_number(query);surgescript_object_call_function(controller,"get_velocityY",NULL,0,query);
 assertion(queried_x==225&&surgescript_var_get_number(query)==0,"SurgeScript velocity queries return the committed physical result immediately");surgescript_var_destroy(query);

 assertion(!test_script_motion(p,"set","bad",0,0)&&p->a.vx==225&&physicsactor_get_xsp(p->body)==225,"unknown motion axis is rejected without mutating physics");
 assertion(!test_script_motion(p,"set","x",NAN,0)&&p->a.vx==225&&physicsactor_get_xsp(p->body)==225,"nonfinite script motion is rejected without mutating physics");
 assertion(test_script_motion(p,"stop","x",0,0)&&p->a.vx==0,"explicit script stop is honored during a committed action");
 physicsactor_set_xsp(p->body,1e308);
 assertion(!motion_apply(p,BOR_MOTION_ADD,1,1e308,0)&&physicsactor_get_xsp(p->body)==1e308,"overflowing additive motion is rejected before any body write");
 test_fixture(floor);test_place(slots[1],500,floor);start(p,"jumpattack",BOR_ACTION);p->a.y=floor-40;p->a.grounded=false;commit_pose(p);rebase(p);physicsactor_detach_from_ground(p->body);physicsactor_set_xsp(p->body,200);physicsactor_set_ysp(p->body,120);
 for(int i=0;i<30&&!p->a.grounded;i++)move_actor(p);
 logfile_message("BOR_PHYSICS landing ground=%d x=%.6f y=%.6f vx=%.6f gsp=%.6f vy=%.6f",p->a.grounded,p->a.x,p->a.y,p->a.vx,physicsactor_get_gsp(p->body),p->a.vy);
 assertion(p->a.grounded&&fabs(p->a.vx-physicsactor_get_gsp(p->body))<1e-6&&p->a.vx>190,"landing transfers velocity to ground speed without a false zero or landing reset");
 test_fixture(floor);motion_apply(p,BOR_MOTION_SET,BOR_AXIS_X,200,0);drive(p,1,200);BorFrame impact=*bor_frame(&p->a);impact.drop=0;impact.pause=0;
 damage_reaction(p,slots[1],&impact);move_actor(p);
 assertion(p->a.state==BOR_PAIN&&fabs(p->a.vx)>0&&fabs(p->a.vx)<20&&p->steering==0,"same-tick reaction overrides locomotion and clears stale directional intent");
 test_fixture(floor);
}
#include "bor_movement_test.inc"
#include "bor_contact_test.inc"
#include "bor_tuning_test.inc"
static void integration_test(void){
 HostActor* p=hero();if(!p)return;
 if(total_frames==100){
  int count=0;for(int i=0;i<64;i++)if(slots[i])count++;
  assertion(count==6,"five BOR actors and the C2 training log registered");assertion(p->a.grounded,"Michio contacts stock terrain");
  assertion(bor_models[0].health==100&&bor_models[1].health==40&&bor_models[2].health==30&&bor_models[3].health==120&&bor_models[4].health==25,"source health values");
  assertion(!strcmp(bor_animation(bor_models,"jumpattack2")->follow,""),"invalid historical follow remains disabled");
  double floor=p->a.y;
  for(int model=1;model<5;model++){
   reset_all();for(int i=0;i<64;i++)if(slots[i]){slots[i]->a.x=1000+i*100;slots[i]->a.y=floor;slots[i]->a.grounded=true;commit_pose(slots[i]);rebase(slots[i]);}
   HostActor* enemy=slots[model];p->a.x=100;enemy->a.x=80;p->a.facing=-1;enemy->a.facing=1;commit_pose(p);rebase(p);commit_pose(enemy);rebase(enemy);
   start(enemy,"attack1",BOR_ACTION);bor_advance(&enemy->a,model==3?.23:model==1?.15:.08,hit_segment,enemy);
   assertion(p->a.health<100,"each source enemy damages Michio through native hit resolution");
  }
  reset_all();for(int i=0;i<64;i++)if(slots[i]){slots[i]->a.x=1000+i*100;slots[i]->a.y=floor;slots[i]->a.grounded=true;commit_pose(slots[i]);rebase(slots[i]);}
  HostActor* red=slots[1];HostActor* hedge=slots[4];red->a.x=100;hedge->a.x=80;hedge->a.facing=1;start(hedge,"attack1",BOR_ACTION);bor_advance(&hedge->a,.08,hit_segment,hedge);
  assertion(red->a.health<40,"hedgehog retains source hostility to other enemies");
  reset_all();p->a.x=100;p->a.y=floor;p->a.grounded=true;red->a.x=118;red->a.y=floor;red->a.grounded=true;p->a.facing=1;red->a.facing=-1;p->a.partner=red->a.serial;red->a.partner=p->a.serial;
  start(red,"pain",BOR_GRABBED);start(p,"grabattack",BOR_ACTION);bor_advance(&p->a,.41,hit_segment,p);
  assertion(red->a.health<40&&red->a.state==BOR_GRABBED&&red->a.partner==(int)p->a.serial&&p->a.partner==(int)red->a.serial,"nonfinishing grab attack retains linked participants");
  unlink_actor(p);assertion(red->a.partner==-1&&p->a.partner==-1&&red->a.state==BOR_PAIN,"interruption clears both grapple links");
  reset_all();red->a.x=300;red->a.y=floor;red->a.grounded=true;start(red,"attack1",BOR_ACTION);bor_advance(&red->a,1.42,hit_segment,red);
  assertion(fabs(red->a.x-340)<.001,"native frame root displacement occurs once at source boundaries");
  hardening_checks(floor);preliminary_physics_checks(floor);slope_physics_checks(floor);c2_movement_checks(floor);c2_region_checks(floor);continuous_combat_checks(floor);continuous_region_checks(floor);contact_checks(floor);control_tuning_checks(floor);modifier_checks(floor);reset_all();
 }
 if(total_frames==120){for(int i=0;i<64;i++)if(slots[i])slots[i]->a.ai_wait=100;
  HostActor* target=slots[1];target->a.x=p->a.x+40;target->a.y=p->a.y;commit_pose(target);rebase(target);start(p,"attack1",BOR_ACTION);p->a.vx=0;p->a.facing=1;
 }
 if(total_frames==165){assertion(total_hits>0,"real native actor hit on stock level");assertion(slots[1]->a.health<40,"source hit damages enemy");assertion(p->a.x<2000,"depth disabled and world position remains finite");
  reset_all();
 }
 if(total_frames==180){for(int i=0;i<64;i++)if(slots[i])slots[i]->a.ai_wait=100;start(p,"freespecial2",BOR_ACTION);}
 if(total_frames==218)assertion(p->a.finished&&p->a.state==BOR_ACTION&&!p->a.grounded,"finished freespecial2 remains committed until stock terrain landing");
 if(total_frames==250)assertion(p->a.grounded&&p->a.state==BOR_FREE,"freespecial2 returns control after landing");
 if(total_frames==260){p->a.y-=100;p->a.grounded=false;commit_pose(p);rebase(p);physicsactor_detach_from_ground(p->body);physicsactor_set_ysp(p->body,0);start(p,"freespecial3",BOR_ACTION);}
 if(total_frames==268)assertion(p->a.finished&&p->a.state==BOR_ACTION&&!p->a.grounded,"short freespecial3 retains airborne action ownership");
 if(total_frames==310){
  assertion(p->a.grounded&&p->a.state==BOR_FREE,"short special eventually releases safely on terrain");
  const char* path=getenv("BOR_POC_REPORT");FILE* report=path?fopen(path,"w"):NULL;
  if(report){fprintf(report,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"hits\":%d,\"depth_movement_enabled\":false,\"host\":\"stock Surge terrain/editor\"}\n",failures?"false":"true",checks,failures,test_hits);fclose(report);}engine_quit();
 }
}
static void write_trace(void){
 const char* path=getenv("BOR_POC_TRACE");if(!path||total_frames%6)return;
 FILE* out=fopen(path,"w");if(!out)return;
 unsigned held_mask=0;for(int i=0;i<6;i++)if(debug_keys[i])held_mask|=1u<<i;
 fprintf(out,"{\"impact_events\":%d,\"impact_material\":%d,\"impact_kind\":%d,\"debug_held\":%u,\"tick\":%d,\"depth_movement_enabled\":false,\"hits\":%d,\"debug_mask\":%u,\"entry_sounds\":%d,\"actors\":[",total_impact_events,last_impact_material,last_impact_kind,held_mask,total_frames,total_hits,debug_mask,total_sound_events);
 bool first=true;
 for(int i=0;i<64;i++)if(slots[i]){
  HostActor* h=slots[i];BorActor* a=&h->a;
  fprintf(out,"%s{\"name\":\"%s\",\"x\":%.6f,\"y\":%.6f,\"vx\":%.6f,\"vy\":%.6f,\"gsp\":%.6f,\"ground_angle\":%d,\"surface_angle\":%.6f,\"pose_angle\":%.6f,\"control_lock\":%.6f,\"ground_friction\":%.6f,\"contact_blocked\":%s,\"depth\":0,\"hp\":%d,\"action\":\"%s\",\"frame\":%d,\"state\":%d,\"grounded\":%s,\"handle\":%u,\"ledge_id\":%llu,\"ledge_x\":%.6f,\"ledge_y\":%.6f,\"jumps_used\":%d,\"combo_next\":%d,\"combo_window\":%.6f,\"training_hits\":%d,\"training_damage\":%.3f,\"suspended\":%s,\"facing\":%d,\"locomotion\":%d,\"actor_support\":%llu,\"guard_kind\":%d,\"defense_held\":%s,\"lock_target\":%llu,\"aim_x\":%.6f,\"aim_y\":%.6f,\"evading\":%s}",first?"":",",a->model->name,a->x,a->y,a->vx,a->vy,physicsactor_get_gsp(h->body),physicsactor_get_angle(h->body),h->surface_angle*180/3.141592653589793,a->pose_angle*180/3.141592653589793,physicsactor_hlock_timer(h->body),physicsactor_get_frc(h->body),h->contact_x_blocked?"true":"false",a->health,a->animation->name,a->frame,a->state,a->grounded?"true":"false",surgescript_object_handle(h->entity),(unsigned long long)h->ledge_id,h->ledge.x,h->ledge.y,h->jumps_used,a->combo,a->combo_until,h->training_hits,h->training_damage,h->suspended?"true":"false",a->facing,h->locomotion,(unsigned long long)h->actor_support,h->guard_kind,h->defense_held?"true":"false",(unsigned long long)h->lock_target,h->aim_position.x,h->aim_position.y,a->state==BOR_EVADE?"true":"false");first=false;
 }
 fprintf(out,"],\"platform\":");surgescript_object_t* fixture=moving_fixture();if(fixture){v2d_t pos=scripting_util_world_position(fixture);fprintf(out,"{\"x\":%.6f,\"y\":%.6f}",pos.x,pos.y);}else fprintf(out,"null");fprintf(out,"}\n");fclose(out);
}
static void prepare_combat_poses(void){for(int i=0;i<64;i++)if(slots[i])render_frame(slots[i]);}
static bool encounter_ready(HostActor* h){HostActor* player=hero();if(h->listening||h->wake_distance<=0)h->enemy_awake=true;else if(player&&fabs(player->a.x-h->a.x)<=h->wake_distance&&fabs(player->a.y-h->a.y)<=480)h->enemy_awake=true;return h->enemy_awake;}
/* Stock brick updates cover screen half-extents +320px (pinned host). Use
 * a smaller128px simulation margin and96px wake margin, retaining terrain slack.
 * Persist actor identities/state; stop all nominal clocks and physics outside. */
static void update_actor_activity(void){
 v2d_t camera=camera_get_position(),half=v2d_multiply(video_get_screen_size(),.5);
 for(int i=0;i<64;i++)if(slots[i]){HostActor* h=slots[i];bool old=h->suspended;double margin=old?96:128;bool owned=controlled(h);HostActor* partner=linked_partner(h);if(partner&&controlled(partner))owned=true;
  h->suspended=!owned&&(fabs(h->a.x-camera.x)>half.x+margin||fabs(h->a.y-20-camera.y)>half.y+margin);
  if(old&&!h->suspended){h->support_velocity=v2d_new(0,0);h->support_sample_tick=total_frames;}
 }
}
#include "audio_demo.inc"
extern void bsl_lab_tick(void);
extern void bsl_lab_suspend(void);
extern void bsl_lab_actor(uint64_t,const double*);
static void tick_world(void){
 if(level_editmode()){bsl_lab_suspend();return;}
 for(int i=0;i<64;i++)if(slots[i]&&!slots[i]->placed)rebase(slots[i]);
 if(bsl_host_boot_step())return;
 if(bsl_host_player_assembly_waiting())return;
 if(!controls){controls=input_create_user("bor");test_mode=getenv("BOR_POC_TEST")!=NULL;load_sounds();}
 ALLEGRO_KEYBOARD_STATE keys;if(ps_lab_automatic())memset(&keys,0,sizeof keys);else al_get_keyboard_state(&keys);
 if(pressed(&keys,ALLEGRO_KEY_R)&&!bsl_shell_keyboard())reset_requested=true;
 if(reset_requested){reset_requested=false;reset_all();}
 world_time+=1.0/60;total_frames++;
 bool ui_waiting=prepare_ledge_ui(&keys);
 for(int i=0;i<64;i++)if(slots[i]){hearing_sense(slots[i]);bsl_acoustic_reaction_move(slots[i]->lifetime,slots[i]->a.x,slots[i]->a.y-slots[i]->a.model->soft_height);}
 if(!test_mode)update_actor_activity();
 /* Age pre-existing status once, before any actor can create a new hit/reaction. */
 for(int i=0;i<64;i++)if(slots[i]&&!slots[i]->suspended)slots[i]->frozen_tick=bor_tick_status(&slots[i]->a,1.0/60);
 for(int i=0;i<64;i++)if(slots[i]){
  HostActor* h=slots[i];if(!h->placed)rebase(h);if(h->suspended)continue;
  if(bsl_host_conversation_locked(h->lifetime)){clear_intent(h);h->lock_target=0;}
  else if(controlled(h)){if(bsl_shell_keyboard()){h->steering=0;}else if(bsl_host_input_locked(h->lifetime)){h->lock_target=0;if(h->a.state==BOR_GUARD){h->guard_kind=0;start(h,"idle",BOR_FREE);}}else if(!test_mode&&!ui_waiting)player_actions(h,&keys);}
  else if(h->agency!=1&&(h->agency==2||!bor_is_player(&h->a))&&!h->a.model->training_dummy&&!test_mode&&encounter_ready(h)&&h->a.ai_wait<=0)enemy_actions(h,1.0/60);
 }
 for(int i=0;i<64;i++)if(slots[i]&&!(ui_waiting&&controlled(slots[i])))move_actor(slots[i]);
 for(int i=0;i<64;i++)if(slots[i]&&!(ui_waiting&&controlled(slots[i])))update_surface_rotation(slots[i]);
 prepare_combat_poses(); /* All targets need current movement/rotation before any slot attacks. */
 HostActor* lab_actor=hero();double lab_corners[8];if(lab_actor&&bor_corners(&lab_actor->a,false,lab_corners))bsl_lab_actor(lab_actor->lifetime,lab_corners);else bsl_lab_actor(0,NULL);bsl_lab_tick();
  for(int i=0;i<64;i++)if(slots[i]&&controlled(slots[i]))lock_update(slots[i],slots[i]->defense_held&&!bsl_host_input_locked(slots[i]->lifetime));
 for(int i=0;i<64;i++)if(slots[i]){
  advance_host(slots[i]);
 }
 for(int i=0;i<64;i++)if(slots[i]&&!slots[i]->suspended)evade_finish(slots[i]);
 audio_demo_tick(&keys);
 bsl_acoustic_tick(1.0/60,false);
 bsl_host_simulation_tick(); /* After hits: interruptions cancel pending event commands first. */
 HostActor* p=hero();if(p){struct player_t* anchor=level_get_player_by_id(0);if(anchor)player_set_position(anchor,v2d_new(p->a.x,p->a.y-20));}
 write_trace();
 if(test_mode)integration_test();
 registry_character_test_tick();registry_lifecycle_test_tick();registry_factory_test_tick();registry_event_test_tick();registry_start_test_tick();registry_placement_test_tick();registry_dormant_test_tick();registry_reconstruct_test_tick();registry_conversation_test_tick();registry_interaction_test_tick();registry_placement_binding_test_tick();registry_actor_factory_test_tick();registry_actor_event_test_tick();gameplay_test_tick();progression_test_tick();learning_test_tick();checkpoint_test_tick();progress_save_test_tick();progress_boot_test_tick();pickup_test_tick();loot_test_tick();equipment_test_tick();event_reward_test_tick();quest_test_tick();commerce_test_tick();teleport_test_tick();slope_test_tick();pose_test_tick();
 for(int k=0;k<ALLEGRO_KEY_MAX;k++)previous_keys[k]=down(&keys,k);
}
static surgescript_var_t* controller_motion(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);bool ok=false;
 if(h&&h->a.health>0&&h->a.state!=BOR_DEAD&&!h->a.timeline_fault&&surgescript_var_is_string(p[0])&&surgescript_var_is_string(p[1])&&surgescript_var_is_number(p[2])&&surgescript_var_is_number(p[3])){
  const char* mode=surgescript_var_fast_get_string(p[0]);const char* axis=surgescript_var_fast_get_string(p[1]);
  int m=!strcmp(mode,"preserve")?BOR_MOTION_PRESERVE:!strcmp(mode,"set")?BOR_MOTION_SET:!strcmp(mode,"add")?BOR_MOTION_ADD:!strcmp(mode,"stop")?BOR_MOTION_STOP:-1;
  unsigned axes=!strcmp(axis,"x")?1:!strcmp(axis,"y")?2:!strcmp(axis,"xy")?3:0;
  ok=motion_apply(h,m,axes,surgescript_var_get_number(p[2]),surgescript_var_get_number(p[3]));
 }
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
static surgescript_var_t* controller_vx(surgescript_object_t* obj,const surgescript_var_t** p,int n){HostActor* h=surgescript_object_userdata(obj);return surgescript_var_set_number(surgescript_var_create(),h?motion_query(h).x:0);}
static surgescript_var_t* controller_vy(surgescript_object_t* obj,const surgescript_var_t** p,int n){HostActor* h=surgescript_object_userdata(obj);return surgescript_var_set_number(surgescript_var_create(),h?motion_query(h).y:0);}
static surgescript_var_t* controller_lock_target(surgescript_object_t* obj,const surgescript_var_t** p,int n){HostActor* h=surgescript_object_userdata(obj);return surgescript_var_set_number(surgescript_var_create(),h?(double)h->lock_target:0);}
static surgescript_var_t* controller_aim_x(surgescript_object_t* obj,const surgescript_var_t** p,int n){HostActor* h=surgescript_object_userdata(obj);return surgescript_var_set_number(surgescript_var_create(),h?h->aim_position.x:0);}
static surgescript_var_t* controller_aim_y(surgescript_object_t* obj,const surgescript_var_t** p,int n){HostActor* h=surgescript_object_userdata(obj);return surgescript_var_set_number(surgescript_var_create(),h?h->aim_position.y:0);}
static surgescript_var_t* controller_zindex(surgescript_object_t* obj,const surgescript_var_t** p,int n){return surgescript_var_set_number(surgescript_var_create(),.99);}
static surgescript_var_t* controller_render(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 ALLEGRO_KEYBOARD_STATE keys;if(ps_lab_automatic())memset(&keys,0,sizeof keys);else al_get_keyboard_state(&keys);
 for(int i=0;i<6;i++){bool held=al_key_down(&keys,i<4?ALLEGRO_KEY_F3+i:i==4?ALLEGRO_KEY_F2:ALLEGRO_KEY_F7);if(held&&!debug_keys[i]&&!level_editmode()&&!bsl_shell_keyboard())debug_mask^=1u<<i;debug_keys[i]=held;}
 HostActor* h=surgescript_object_userdata(obj);if(!h||(!level_editmode()&&(!h->placed||!h->visual->visible)))return NULL;
 BorActor view=h->a;if(level_editmode()){
  const BorAnimation* idle=bor_animation(view.model,"idle");
  if(idle&&idle->count>0){double total=0;for(int i=0;i<idle->count;i++)total+=fmax(idle->frames[i].duration,1.0/600.0);double t=fmod(timer_get_now(),total);int frame=0;while(frame+1<idle->count&&t>=fmax(idle->frames[frame].duration,1.0/600.0)){t-=fmax(idle->frames[frame].duration,1.0/600.0);frame++;}view.animation=idle;view.frame=frame;}
  view.facing=1;view.pose_x=view.pose_y=view.pose_angle=0;view.pose_scale=1;view.active=bor_frame(&view)->aw>0&&bor_frame(&view)->ah>0;
 }
 v2d_t center=v2d_multiply(video_get_screen_size(),.5),position=scripting_util_world_position(h->entity);
 double dx=position.x-h->a.x-surgescript_var_get_number(p[0])+center.x,dy=position.y-h->a.y-surgescript_var_get_number(p[1])+center.y;
 double x,y;
 if(debug_mask&1){int bw,bh;v2d_t b;physicsactor_bounding_box(h->body,&bw,&bh,&b);if(!h->placed){v2d_t raw=physicsactor_get_position(h->body);b.x+=h->a.x-raw.x;b.y+=h->a.y-20-raw.y;}image_rect(b.x-bw*.5+dx,b.y-bh*.5+dy,b.x+bw*.5+dx,b.y+bh*.5+dy,(!level_editmode()&&h->a.state==BOR_EVADE)?color_rgb(180,100,255):color_rgb(0,220,255));}
 if((debug_mask&32)&&view.model->soft_solid){double radius=view.model->soft_radius,height=view.model->soft_height;image_rect(view.x-radius+dx,view.y-height+dy,view.x+radius+dx,view.y+dy,color_rgb(255,170,50));}
 if(debug_mask&2){double points[8];if(bor_corners(&view,false,points))for(int i=0;i<4;i++)image_line(points[2*i]+dx,points[2*i+1]+dy,points[2*((i+1)%4)]+dx,points[2*((i+1)%4)+1]+dy,color_rgb(80,255,100));}
 if((debug_mask&4)&&view.active){double points[8];if(bor_corners(&view,true,points))for(int i=0;i<4;i++)image_line(points[2*i]+dx,points[2*i+1]+dy,points[2*((i+1)%4)]+dx,points[2*((i+1)%4)+1]+dy,color_rgb(255,70,70));}
 if((debug_mask&16)&&h->physics.enabled){
  double cx=h->a.x+dx,cy=h->a.y-h->physics.chest_height+dy,ex=cx+h->a.facing*h->physics.ledge_reach;
  image_line(cx,cy,ex,cy,color_rgb(245,100,255));image_line(ex,cy,ex,cy-h->physics.ledge_scan,color_rgb(245,100,255));
  if(h->ledge_id){double lx=h->ledge.x+dx,ly=h->ledge.y+dy;image_line(lx-5,ly,lx+5,ly,color_rgb(255,255,255));image_line(lx,ly-5,lx,ly+5,color_rgb(255,255,255));}
 }
 if(!level_editmode()&&h->lock_target){double tx=h->aim_position.x+dx,ty=h->aim_position.y+dy;image_rect(tx-9,ty-9,tx+9,ty+9,color_rgb(255,230,60));image_line(tx-12,ty,tx+12,ty,color_rgb(255,230,60));}
 if(debug_mask&8){x=h->a.x+dx;y=h->a.y+dy;image_line(x-4,y,x+4,y,color_rgb(255,230,60));image_line(x,y-4,x,y+4,color_rgb(255,230,60));}
 return NULL;
}
static surgescript_var_t* controller_main(surgescript_object_t* obj,const surgescript_var_t** p,int n){return NULL;}
static surgescript_var_t* controller_init(surgescript_object_t* obj,const surgescript_var_t** params,int n){
 if(surgescript_object_userdata(obj))return NULL;
 int index=-1,count=(int)(sizeof(bor_models)/sizeof(*bor_models));
 if(surgescript_var_is_string(params[0])){
  const BorModel* definition=bor_find_model(bor_models,count,surgescript_var_fast_get_string(params[0]));
  if(definition)index=(int)(definition-bor_models);
 }else{
  double value=surgescript_var_get_number(params[0]);if(isfinite(value)&&floor(value)==value&&value>=0&&value<count)index=(int)value;
 }
 if(index<0){scripting_warning(obj,"Unknown BOR model ID");return NULL;}
 int slot=-1;for(int i=0;i<64;i++)if(!slots[i]){slot=i;break;}
 if(slot<0){scripting_warning(obj,"BOR PoC actor limit64 reached; object has no controller");return NULL;}
 HostActor* h=calloc(1,sizeof(*h));if(!h)return NULL;
 if(!bor_init(&h->a,bor_models+index,index)){scripting_warning(obj,"Invalid BOR actor definition");free(h);return NULL;}
 h->reaction_tick=-1;h->tap_tick[0]=h->tap_tick[1]=-1000;h->entity=surgescript_objectmanager_get(surgescript_object_manager(obj),surgescript_object_parent(obj));
 surgescript_object_t* actor=surgescript_objectmanager_get(surgescript_object_manager(obj),surgescript_object_child(h->entity,"Actor"));h->visual=scripting_actor_ptr(actor);
 h->drive_input=input_create_computer();h->body=physicsactor_create(v2d_new(0,0));
 int physics_result=load_physics(&h->physics,h->a.model->name);
 if(physics_result<0){char message[256];snprintf(message,sizeof message,"Invalid physics profile for %s; controller not activated",h->a.model->name);scripting_warning(obj,"%s",message);bsl_shell_note("movement",message,2);input_destroy(h->drive_input);physicsactor_destroy(h->body);free(h);return NULL;}
 if(physics_result==0&&bor_is_player(&h->a)){char message[256];snprintf(message,sizeof message,"Missing physics/%s.physics; player movement uses fallback",h->a.model->name);logfile_message("%s",message);scripting_warning(obj,"%s",message);bsl_shell_note("movement",message,1);}
 if(getenv("BOR_POC_TEST"))h->physics.enabled=false;
 apply_physics(h);
 slots[slot]=h;h->lifetime=++actor_lifetime;h->a.serial=slot;
 surgescript_object_set_userdata(obj,h);bsl_host_bind(h->lifetime,&h->a);render_frame(h);return NULL;
}
static surgescript_var_t* controller_destroy(surgescript_object_t*,const surgescript_var_t**,int);
static surgescript_var_t* controller_character_model(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslCharacterDefinition d={0};surgescript_var_t* result=surgescript_var_create();
 if(surgescript_var_is_string(p[0])&&bsl_host_character_definition(surgescript_var_fast_get_string(p[0]),&d)==0)return surgescript_var_set_string(result,d.model);
 return result;
}
static surgescript_var_t* controller_selected_character(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslCharacterDefinition d={0};surgescript_var_t* result=surgescript_var_create();
 for(unsigned i=0;bsl_host_character_at(i,&d);++i)if(d.selected)return surgescript_var_set_string(result,d.id);
 return result;
}
static surgescript_var_t* controller_character_at(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslCharacterDefinition d={0};double index=surgescript_var_get_number(p[0]);surgescript_var_t* result=surgescript_var_create();
 if(surgescript_var_is_number(p[0])&&isfinite(index)&&index>=0&&index<64&&floor(index)==index&&bsl_host_character_at((unsigned)index,&d))return surgescript_var_set_string(result,d.id);
 return result;
}
static surgescript_var_t* controller_character_position_valid(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 double x=surgescript_var_get_number(p[0]),y=surgescript_var_get_number(p[1]);
 bool ok=surgescript_var_is_number(p[0])&&surgescript_var_is_number(p[1])&&isfinite(x)&&isfinite(y)&&fabs(x)<=1e7&&fabs(y)<=1e7;
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
static surgescript_var_t* controller_character_entity(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslCharacterDefinition d={0};surgescript_var_t* result=surgescript_var_create();
 if(surgescript_var_is_string(p[0])&&bsl_host_character_definition(surgescript_var_fast_get_string(p[0]),&d)==0&&d.actor){
  for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==d.actor)return surgescript_var_set_objecthandle(result,surgescript_object_handle(slots[i]->entity));
 }
 return result;
}
bool bsl_bor_conversation_actor_live(uint64_t actor){
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor){HostActor* h=slots[i];return h->a.health>0&&h->a.state!=BOR_DEAD&&!surgescript_object_is_killed(h->entity);}
 return false;
}
bool bsl_bor_conversation_actor_ready(uint64_t actor){
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor){HostActor* h=slots[i];return bsl_bor_conversation_actor_live(actor)&&h->placed&&!h->suspended&&!h->frozen_tick&&h->a.grounded&&h->a.state==BOR_FREE&&h->a.partner<0&&h->a.freeze<=0&&!h->a.timeline_fault&&fabs(h->a.vx)<1&&fabs(h->a.vy)<1;}
 return false;
}
uint64_t bsl_bor_entity_actor(uint32_t handle){
 for(int i=0;i<64;i++)if(slots[i]&&surgescript_object_handle(slots[i]->entity)==handle&&!surgescript_object_is_killed(slots[i]->entity))return slots[i]->lifetime;
 return 0;
}
static surgescript_var_t* controller_reset_interaction(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);return surgescript_var_set_bool(surgescript_var_create(),h&&bsl_host_actor_placement_reset(h->lifetime)==0);
}
static surgescript_var_t* controller_set_interaction(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);bool ok=h&&surgescript_var_is_string(p[0])&&bsl_host_actor_interaction_bind(h->lifetime,surgescript_var_fast_get_string(p[0]))==0;
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
static surgescript_var_t* controller_interaction_definition(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);return surgescript_var_set_string(surgescript_var_create(),h?bsl_host_actor_interaction_definition(h->lifetime):"");
}
static surgescript_var_t* controller_begin_conversation(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* speaker=surgescript_object_userdata(obj);int code=2;surgescript_objectmanager_t* manager=surgescript_object_manager(obj);
 if(speaker&&surgescript_var_is_objecthandle(p[0])&&surgescript_var_is_string(p[1])&&surgescript_var_is_number(p[2])&&surgescript_var_is_number(p[3])){
  surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(p[0]);
  if(handle&&surgescript_objectmanager_exists(manager,handle)){
   surgescript_object_t* entity=surgescript_objectmanager_get(manager,handle);surgescript_objecthandle_t child=surgescript_object_child(entity,"BORController");
   HostActor* initiator=child&&surgescript_objectmanager_exists(manager,child)?surgescript_object_userdata(surgescript_objectmanager_get(manager,child)):NULL;
   if(initiator&&initiator->entity==entity)code=bsl_host_conversation_open(initiator->lifetime,speaker->lifetime,surgescript_var_fast_get_string(p[1]),surgescript_var_get_number(p[2]),surgescript_var_get_number(p[3]));
  }
 }
 return surgescript_var_set_bool(surgescript_var_create(),code==0);
}
static surgescript_var_t* controller_choose_conversation(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* initiator=surgescript_object_userdata(obj);double choice=surgescript_var_get_number(p[0]);
 bool ok=initiator&&surgescript_var_is_number(p[0])&&isfinite(choice)&&choice>=0&&choice<64&&floor(choice)==choice&&bsl_host_conversation_choose(initiator->lifetime,(int)choice)==0;
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
static surgescript_var_t* controller_choose_initial_character(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 bool ok=surgescript_var_is_string(p[0])&&bsl_host_choose_initial_character(surgescript_var_fast_get_string(p[0]))==0;
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
static surgescript_var_t* controller_select_character(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 bool ok=surgescript_var_is_string(p[0])&&bsl_host_select_character(surgescript_var_fast_get_string(p[0]))==0;
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
/* Only a synchronous native rewind preparation may defer attachment and placement checks. Signed: Mech. */
struct BslCharacterReconstruction {const char* definition;uint64_t lifetime;surgescript_objecthandle_t entity,controller;bool published;};
static BslCharacterReconstruction* preparing_character;
/* Event commands call the setup-retained factory, never a second actor implementation. Signed: Mech. */
static int registry_event_spawn(const char* registry,const char* definition,double x,double y){
 surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_objectmanager_t* manager=surgescript_object_manager(level);
 // Shared project setup takes precedence. A broken shared provider must not silently fall back.
 const char* setup_names[]={"BSL Runtime Setup","BOR Setup"};surgescript_objecthandle_t service=0;
 for(unsigned i=0;i<sizeof setup_names/sizeof *setup_names;i++){
  surgescript_var_t* name=surgescript_var_set_string(surgescript_var_create(),setup_names[i]);const surgescript_var_t* find_args[]={name};surgescript_var_t* found=surgescript_var_create();
  surgescript_object_call_function(level,"findEntity",find_args,1,found);surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(found);surgescript_var_destroy(name);surgescript_var_destroy(found);
  if(handle&&surgescript_objectmanager_exists(manager,handle)){
   service=surgescript_object_child(surgescript_objectmanager_get(manager,handle),registry);break;
  }
 }
 if(!service||!surgescript_objectmanager_exists(manager,service))return 1;
 surgescript_var_t* id=surgescript_var_set_string(surgescript_var_create(),definition);surgescript_var_t* px=surgescript_var_set_number(surgescript_var_create(),x);surgescript_var_t* py=surgescript_var_set_number(surgescript_var_create(),y);const surgescript_var_t* args[]={id,px,py};surgescript_var_t* result=surgescript_var_create();
 surgescript_object_call_function(surgescript_objectmanager_get(manager,service),"create",args,3,result);bool created=surgescript_var_get_objecthandle(result)!=0;
 surgescript_var_destroy(id);surgescript_var_destroy(px);surgescript_var_destroy(py);surgescript_var_destroy(result);return created?0:3;
}
int bsl_bor_event_spawn_character(const char* definition,double x,double y){
 BslCharacterDefinition d={0};if(!definition||!isfinite(x)||!isfinite(y)||fabs(x)>1e7||fabs(y)>1e7||bsl_host_character_definition(definition,&d)!=0)return 2;
 if(d.actor)return 4; /* Code::conflict; the existing body is retained. */
 return registry_event_spawn("BSL Character Registry",definition,x,y);
}
int bsl_bor_event_spawn_actor(const char* definition,double x,double y){
 BslActorTemplate d={0};if(!definition||!isfinite(x)||!isfinite(y)||fabs(x)>1e7||fabs(y)>1e7||!bsl_host_actor_template_definition(definition,&d))return 2;
 return registry_event_spawn("BSL Actor Registry",definition,x,y);
}
BslCharacterReconstruction* bsl_bor_checkpoint_character_prepare(const char* definition,double x,double y){
 if(preparing_character||level_editmode())return NULL;
 BslCharacterReconstruction* prepared=calloc(1,sizeof *prepared);if(!prepared)return NULL;
 prepared->definition=definition;preparing_character=prepared;
 int code=bsl_bor_event_spawn_character(definition,x,y);preparing_character=NULL;prepared->definition=NULL;
 if(code!=0||!prepared->lifetime){bsl_bor_checkpoint_character_discard(prepared);return NULL;}
 return prepared;
}
uint64_t bsl_bor_checkpoint_character_actor(const BslCharacterReconstruction* prepared){return prepared?prepared->lifetime:0;}
void bsl_bor_checkpoint_character_publish(BslCharacterReconstruction* prepared){if(prepared)prepared->published=true;}
void bsl_bor_checkpoint_character_discard(BslCharacterReconstruction* prepared){
 if(!prepared)return;
 if(!prepared->published&&prepared->lifetime){
  surgescript_objectmanager_t* manager=surgescript_vm_objectmanager(surgescript_vm());
  if(surgescript_objectmanager_exists(manager,prepared->controller)){
   surgescript_object_t* controller=surgescript_objectmanager_get(manager,prepared->controller);HostActor* h=surgescript_object_userdata(controller);
   if(h&&h->lifetime==prepared->lifetime)controller_destroy(controller,NULL,0);
  }
  if(surgescript_objectmanager_exists(manager,prepared->entity))surgescript_object_call_function(surgescript_objectmanager_get(manager,prepared->entity),"destroy",NULL,0,NULL);
 }
 free(prepared);
}
/* Exact placement preflight precedes roster binding/control and uses the existing body. Signed: Mech. */
static bool character_spawn_clear(HostActor* h){
 if(!h||!terrain_map())return false;
 v2d_t at=scripting_util_world_position(h->entity);if(!isfinite(at.x)||!isfinite(at.y)||fabs(at.x)>1e7||fabs(at.y)>1e7)return false;
 struct checkpoint_terrain_t* query=test_terrain?NULL:level_checkpoint_terrain_create(at);
 if(!test_terrain&&!query)return false;
 const obstaclemap_t* terrain=test_terrain?test_terrain:level_checkpoint_terrain_map(query);
 bool clear=terrain&&body_clear(h,terrain,at.x,at.y)&&body_clear(h,terrain_map(),at.x,at.y);
 if(query)level_checkpoint_terrain_destroy(query);
 if(clear&&h->a.model->soft_solid)for(int i=0;i<64;i++)if(slots[i]!=h&&slots[i]&&contact_live(slots[i])&&contact_overlap(h,slots[i],at.x,at.y)){clear=false;break;}
 return clear;
}
const char* bsl_bor_actor_template_id(uint64_t actor){for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor)return slots[i]->template_id;return "";}
bool bsl_bor_actor_template_record(uint64_t actor,const char* id,bool passive){
 if(!id||!*id||strlen(id)>=129)return false;
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor){HostActor* h=slots[i];if(*h->template_id)return !strcmp(h->template_id,id);snprintf(h->template_id,sizeof h->template_id,"%s",id);h->agency=passive?1:2;return true;}
 return false;
}
static surgescript_var_t* controller_actor_model(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslActorTemplate d={0};surgescript_var_t* result=surgescript_var_create();if(surgescript_var_is_string(p[0])&&bsl_host_actor_template_definition(surgescript_var_fast_get_string(p[0]),&d))return surgescript_var_set_string(result,d.model);return result;
}
static surgescript_var_t* controller_actor_at(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslActorTemplate d={0};surgescript_var_t* result=surgescript_var_create();double index=surgescript_var_get_number(p[0]);if(surgescript_var_is_number(p[0])&&isfinite(index)&&floor(index)==index&&index>=0&&index<256&&bsl_host_actor_template_at((unsigned)index,&d))return surgescript_var_set_string(result,d.id);return result;
}
static surgescript_var_t* controller_init_actor(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslActorTemplate d={0};HostActor* h=surgescript_object_userdata(obj);bool ok=false;
 if(!bsl_gameplay_enabled()||level_editmode())return surgescript_var_set_bool(surgescript_var_create(),false);
 if(surgescript_var_is_string(p[0])&&bsl_host_actor_template_definition(surgescript_var_fast_get_string(p[0]),&d)){
  if(h)ok=!strcmp(bsl_host_actor_template_for(h->lifetime),d.id);
  else{
   surgescript_object_t* entity=surgescript_objectmanager_get(surgescript_object_manager(obj),surgescript_object_parent(obj));
   if(surgescript_object_child(entity,"Actor")){
    surgescript_var_t* model=surgescript_var_set_string(surgescript_var_create(),d.model);const surgescript_var_t* args[]={model};controller_init(obj,args,1);surgescript_var_destroy(model);h=surgescript_object_userdata(obj);
    if(h){rebase(h);ok=character_spawn_clear(h)&&bsl_host_actor_template_bind(h->lifetime,d.id)==0;if(!ok)controller_destroy(obj,NULL,0);}
   }
  }
 }
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
static surgescript_var_t* controller_init_character(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 BslCharacterDefinition d={0};HostActor* h=surgescript_object_userdata(obj);bool ok=false;
 if(surgescript_var_is_string(p[0])&&bsl_host_character_definition(surgescript_var_fast_get_string(p[0]),&d)==0){
  if(h){ok=d.actor==h->lifetime&&bsl_host_bind_character(h->lifetime,d.id)==0;}
  else if(!d.actor){
   surgescript_object_t* entity=surgescript_objectmanager_get(surgescript_object_manager(obj),surgescript_object_parent(obj));
   if(surgescript_object_child(entity,"Actor")){
    surgescript_var_t* model=surgescript_var_set_string(surgescript_var_create(),d.model);const surgescript_var_t* args[]={model};
    controller_init(obj,args,1);surgescript_var_destroy(model);h=surgescript_object_userdata(obj);
    if(h){
     rebase(h);
     if(preparing_character&&preparing_character->definition&&!strcmp(preparing_character->definition,d.id)){
      preparing_character->lifetime=h->lifetime;preparing_character->entity=surgescript_object_handle(h->entity);preparing_character->controller=surgescript_object_handle(obj);ok=true;
     }else ok=character_spawn_clear(h)&&bsl_host_bind_character(h->lifetime,d.id)==0;
     if(!ok)controller_destroy(obj,NULL,0);
    }
   }
  }
 }
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
static surgescript_var_t* controller_character(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);int code=2;if(h&&surgescript_var_is_string(p[0]))code=bsl_host_bind_character(h->lifetime,surgescript_var_fast_get_string(p[0]));return surgescript_var_set_bool(surgescript_var_create(),code==0);
}
bool bsl_bor_pickup_site(uint64_t id,double* x,double* y){char key[17];snprintf(key,sizeof key,"%016llx",(unsigned long long)id);surgescript_object_t* entity=level_get_entity_by_id(key);if(!entity||strcmp(surgescript_object_name(entity),"BSL Pickup"))return false;v2d_t p=scripting_util_world_position(entity);*x=p.x;*y=p.y;return true;}
bool bsl_bor_pickup_ready(uint64_t actor){for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor){HostActor* h=slots[i];return h->placed&&!h->suspended&&h->down_held&&h->a.grounded&&h->a.state==BOR_FREE&&h->a.partner<0&&h->a.freeze<=0&&!h->a.timeline_fault&&fabs(h->a.vx)<1&&fabs(h->a.vy)<1;}return false;}
bool bsl_bor_checkpoint_site(uint64_t id,bool permanent,double* x,double* y){
 char key[17];snprintf(key,sizeof key,"%016llx",(unsigned long long)id);surgescript_object_t* entity=level_get_entity_by_id(key);
 if(!entity||strcmp(surgescript_object_name(entity),permanent?"BSL Permanent Checkpoint":"BSL Temporary Checkpoint"))return false;
 v2d_t at=scripting_util_world_position(entity);*x=at.x;*y=at.y;return true;
}
bool bsl_bor_checkpoint_spot(uint64_t actor,double x,double y,bool require_support,double* safe_x,double* safe_y){
 if(!safe_x||!safe_y||!isfinite(x)||!isfinite(y)||fabs(x)>1e7||fabs(y)>1e7)return false;
 HostActor* h=NULL;for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor)h=slots[i];if(!h||!terrain_map())return false;
 struct checkpoint_terrain_t* query=test_terrain?NULL:level_checkpoint_terrain_create(v2d_new(x,y));
 if(!test_terrain&&!query)return false;
 const obstaclemap_t* terrain=test_terrain?test_terrain:level_checkpoint_terrain_map(query);
 HostActor probe=*h;probe.body=physicsactor_create(v2d_new(x,y-20));physicsactor_restore_state(probe.body);
 int width,height;v2d_t center,raw=physicsactor_get_position(probe.body);physicsactor_bounding_box(probe.body,&width,&height,&center);
 const double bottom=center.y+height*.5-(raw.y+20),middle=center.x-raw.x;
 bool found=false;
 for(int distance=0;distance<=256&&!found;distance++)for(int direction=0;direction<4&&!found;direction++){
  if(distance>64&&direction!=3)continue;
  double px=x+(direction==1?-distance:direction==2?distance:0),py=y-(direction==0?distance:direction==3?-distance:0);
  if(!body_clear(&probe,terrain,px,py))continue;
  int support=(int)ceil(py+bottom);bool supported=false;for(int gap=0;gap<4;gap++)supported|=obstaclemap_obstacle_exists(terrain,(int)floor(px+middle),support+gap,OL_DEFAULT);
  if(require_support&&!supported)continue;
  // Live script obstacles near the current camera also veto a return point.
  if(!test_terrain&&!body_clear(&probe,terrain_map(),px,py))continue;
  *safe_x=px;*safe_y=py;found=true;
 }
 if(!found&&getenv("BSL_CHECKPOINT_TEST")){
  int first=-1;for(int i=-64;i<=512;i++)if(obstaclemap_obstacle_exists(terrain,(int)x,(int)y+i,OL_DEFAULT)){first=i;break;}
  double candidate=first==-1?y:y+first-ceil(bottom)-1;
  fprintf(stderr,"CHECKPOINT SPOT %s at %.1f,%.1f bbox=%dx%d bottom=%.1f first=%d candidate=%.1f queryclear=%d liveclear=%d\n",h->a.model->name,x,y,width,height,bottom,first,candidate,body_clear(&probe,terrain,x,candidate),body_clear(&probe,terrain_map(),x,candidate));
 }
 physicsactor_destroy(probe.body);if(query)level_checkpoint_terrain_destroy(query);return found;
}
static void checkpoint_rebase_camera(HostActor* player);
bool bsl_bor_teleport_site(uint64_t id,double* x,double* y){char key[17];snprintf(key,sizeof key,"%016llx",(unsigned long long)id);surgescript_object_t* e=level_get_entity_by_id(key);if(!e||strcmp(surgescript_object_name(e),"BSL Teleporter"))return false;v2d_t p=scripting_util_world_position(e);*x=p.x;*y=p.y;return true;}
/* A validated snapshot: bad trusted-script values refuse travel instead of changing native geometry. */
bool bsl_bor_teleport_live(uint64_t id,BslDoorLive* out){
 if(!out)return false;
 char key[17];snprintf(key,sizeof key,"%016llx",(unsigned long long)id);surgescript_object_t* e=level_get_entity_by_id(key);
 if(!e||surgescript_object_is_killed(e)||strcmp(surgescript_object_name(e),"BSL Teleporter"))return false;
 static const char* fields[]={"width","height","originX","originY","requireAction","preserveVelocity","enabled","facing"};
 double values[8];surgescript_var_t* v=surgescript_var_create();bool valid=true;
 for(int i=0;i<8&&valid;i++){
  char getter[64];snprintf(getter,sizeof getter,"get_%s",fields[i]);
  if(!surgescript_object_has_function(e,getter)){valid=false;break;}
  surgescript_object_call_function(e,getter,NULL,0,v);
  if(i>=4&&i<=6){valid=surgescript_var_is_bool(v);values[i]=surgescript_var_get_bool(v)?1:0;}
  else{valid=surgescript_var_is_number(v);values[i]=surgescript_var_get_number(v);}
  valid=valid&&isfinite(values[i]);
 }
 surgescript_var_destroy(v);if(!valid)return false;
 v2d_t at=scripting_util_world_position(e);
 if(!isfinite(at.x)||!isfinite(at.y)||fabs(at.x)>1e7||fabs(at.y)>1e7||values[0]<1||values[0]>1024||values[1]<1||values[1]>1024||fabs(values[2])>1e7||fabs(values[3])>1e7||values[7]<-1||values[7]>1||floor(values[7])!=values[7])return false;
 *out=(BslDoorLive){at.x,at.y,values[0],values[1],values[2],values[3],values[4]!=0,values[5]!=0,values[6]!=0,(int)values[7]};return true;
}
bool bsl_bor_teleport_action(void){ALLEGRO_KEYBOARD_STATE keys;if(ps_lab_automatic())memset(&keys,0,sizeof keys);else al_get_keyboard_state(&keys);return pressed(&keys,ALLEGRO_KEY_W)||pressed(&keys,ALLEGRO_KEY_UP)||(controls&&input_button_pressed(controls,IB_UP));}
bool bsl_bor_teleport(uint64_t actor,double x,double y,double vx,double vy,int facing){
 HostActor* h=NULL;for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor)h=slots[i];
 if(!h||!isfinite(x)||!isfinite(y)||fabs(x)>1e7||fabs(y)>1e7||!isfinite(vx)||!isfinite(vy)||fabs(vx)>1e5||fabs(vy)>1e5||(facing!=1&&facing!=-1)||h->a.action_id==UINT64_MAX||h->a.epoch==UINT64_MAX)return false;
 physicsactor_t* body=physicsactor_create(v2d_new(x,y-20));if(!body)return false;
 HostActor probe=*h;probe.body=body;apply_physics(&probe);physicsactor_set_layer(body,physicsactor_get_layer(h->body));
 struct checkpoint_terrain_t* q=test_terrain?NULL:level_checkpoint_terrain_create(v2d_new(x,y));if(!test_terrain&&!q){physicsactor_destroy(body);return false;}
 const obstaclemap_t* map=test_terrain?test_terrain:level_checkpoint_terrain_map(q);bool clear=body_clear(&probe,map,x,y);if(q)level_checkpoint_terrain_destroy(q);if(!clear){physicsactor_destroy(body);return false;}
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]!=h&&slots[i]->a.health>0&&h->a.model->soft_solid&&slots[i]->a.model->soft_solid){BorActor* b=&slots[i]->a;if(fabs(x-b->x)<h->a.model->soft_radius+b->model->soft_radius-.01&&y>b->y-b->model->soft_height+.5&&y-h->a.model->soft_height<b->y-.5){physicsactor_destroy(body);return false;}}
 unlink_actor(h);physicsactor_t* old=h->body;h->body=body;physicsactor_destroy(old);
 reconstruct_actor(h,x,y,h->a.health,true);h->a.facing=facing;h->a.vx=vx;h->a.vy=vy;h->a.grounded=false;
 physicsactor_detach_from_ground(h->body);physicsactor_set_xsp(h->body,vx);physicsactor_set_ysp(h->body,vy);
 h->lock_target=0;h->defense_held=false;h->actor_support=0;h->ignored_support=0;h->contact_lifetime=0;
 render_frame(h);checkpoint_rebase_camera(h);return true;
}
struct BslRewindPlan {unsigned count;HostActor* actors[64];physicsactor_t* bodies[64];};
void bsl_bor_rewind_discard(BslRewindPlan* plan){if(!plan)return;for(unsigned i=0;i<plan->count;i++)if(plan->bodies[i])physicsactor_destroy(plan->bodies[i]);free(plan);}
BslRewindPlan* bsl_bor_rewind_prepare(const BslActorRewind* actors,unsigned count,uint64_t selected){
 if(!actors||count<1||count>64)return NULL;
 unsigned actual=0;bool selected_found=false;
 for(int i=0;i<64;i++)if(slots[i]){
  ++actual;HostActor* h=slots[i];unsigned matches=0;
  if(h->a.action_id==UINT64_MAX||h->a.epoch==UINT64_MAX||!bor_animation(h->a.model,"idle"))return NULL;
  for(unsigned j=0;j<count;j++)if(actors[j].lifetime==h->lifetime){if(actors[j].health<0||actors[j].health>h->a.model->health||!isfinite(actors[j].x)||!isfinite(actors[j].y))return NULL;++matches;if(h->lifetime==selected)selected_found=actors[j].health>0;}
  if(matches!=1)return NULL;
 }
 if(actual!=count||!selected_found)return NULL;
 // Clearance concerns final planned positions, after terrain adjustment, for every living soft body.
 for(unsigned i=0;i<count;i++)for(unsigned j=i+1;j<count;j++)if(actors[i].health>0&&actors[j].health>0){
  HostActor *a=NULL,*b=NULL;for(int k=0;k<64;k++)if(slots[k]){if(slots[k]->lifetime==actors[i].lifetime)a=slots[k];if(slots[k]->lifetime==actors[j].lifetime)b=slots[k];}
  if(a->a.model->soft_solid&&b->a.model->soft_solid&&fabs(actors[i].x-actors[j].x)<a->a.model->soft_radius+b->a.model->soft_radius-.01&&actors[i].y>actors[j].y-b->a.model->soft_height+.5&&actors[i].y-a->a.model->soft_height<actors[j].y-.5)return NULL;
 }

 BslRewindPlan* plan=calloc(1,sizeof *plan);if(!plan)return NULL;
 for(unsigned j=0;j<count;j++)for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actors[j].lifetime){
  unsigned at=plan->count++;plan->actors[at]=slots[i];plan->bodies[at]=physicsactor_create(v2d_new(actors[j].x,actors[j].y-20));HostActor probe=*slots[i];probe.body=plan->bodies[at];apply_physics(&probe);
 }
 return plan;
}
static void checkpoint_rebase_camera(HostActor* player){
 if(player){
  v2d_t at=v2d_new(player->a.x,player->a.y-20);struct player_t* anchor=level_get_player_by_id(0);if(anchor)player_set_position(anchor,at);
  surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_var_t* name=surgescript_var_set_string(surgescript_var_create(),"Default Camera");const surgescript_var_t* args[]={name};surgescript_var_t* result=surgescript_var_create();surgescript_object_call_function(level,"findEntity",args,1,result);
  surgescript_objectmanager_t* manager=surgescript_object_manager(level);surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(result);if(handle&&surgescript_objectmanager_exists(manager,handle))scripting_util_set_world_position(surgescript_objectmanager_get(manager,handle),at);
  surgescript_var_destroy(name);surgescript_var_destroy(result);level_checkpoint_rebase(at);
 }
}
void bsl_bor_rewind_apply(BslRewindPlan* plan,const BslActorRewind* actors,unsigned count,uint64_t selected,double x,double y,int facing){
 (void)x;(void)y;
 for(unsigned i=0;i<plan->count;i++){physicsactor_t* previous=plan->actors[i]->body;plan->actors[i]->body=plan->bodies[i];plan->bodies[i]=NULL;physicsactor_destroy(previous);}
 for(int i=0;i<64;i++)if(slots[i])slots[i]->a.partner=-1;
 for(int i=0;i<64;i++)if(slots[i])for(unsigned j=0;j<count;j++)if(actors[j].lifetime==slots[i]->lifetime){
  HostActor* h=slots[i];reconstruct_actor(h,actors[j].x,actors[j].y,actors[j].health,true);if(h->lifetime==selected)h->a.facing=facing;
 }
 last_tap[0]=last_tap[1]=-1000;reset_requested=false;
 bsl_level_sequence_exit_reprime();
 checkpoint_rebase_camera(hero());
}
/* Permission is checked by the host, execution remains with the existing BOR FSM and Surge body. */
bool bsl_bor_ability(uint64_t actor,const char* animation,const char* tag){
 if(!animation||!tag||strlen(tag)>128||level_editmode())return false;
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==actor){
  HostActor* h=slots[i];BorActor* a=&h->a;
  if(!controlled(h)||!h->placed||h->suspended||h->frozen_tick||!a->grounded||a->partner>=0||!bor_can_act(a)||bsl_host_input_locked(actor))return false;
  const BorAnimation* clip=bor_animation(a->model,animation);if(!clip||!(clip->flags&BOR_ANIM_ATTACK))return false;
  snprintf(h->registered_action_tag,sizeof h->registered_action_tag,"%s",tag);
  bool accepted=start(h,animation,BOR_ACTION);h->registered_action_tag[0]=0;
  if(accepted){h->steering=0;h->jump_cut_active=false;h->armed_motion=false;h->motion_step=0;}
  return accepted;
 }
 return false;
}
static surgescript_var_t* controller_reward(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);int code=2;
 if(h&&surgescript_var_is_string(p[0])&&surgescript_var_is_string(p[1]))code=bsl_host_reward(h->lifetime,surgescript_var_fast_get_string(p[0]),surgescript_var_fast_get_string(p[1]));
 return surgescript_var_set_bool(surgescript_var_create(),code==0);
}
static surgescript_var_t* controller_ability(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);double index=surgescript_var_get_number(p[0]);int code=2;
 if(h&&isfinite(index)&&index>=0&&index<64&&index==floor(index))code=bsl_host_ability_use(h->lifetime,(unsigned)index);
 return surgescript_var_set_bool(surgescript_var_create(),code==0);
}
static surgescript_var_t* controller_wakerange(surgescript_object_t* obj,const surgescript_var_t** args,int n){HostActor* h=surgescript_object_userdata(obj);double distance=surgescript_var_get_number(args[0]);if(h&&isfinite(distance)&&distance>=0&&distance<=4096){h->wake_distance=distance;h->enemy_awake=distance==0;}return NULL;}
static surgescript_var_t* controller_rebase(surgescript_object_t* obj,const surgescript_var_t** p,int n){HostActor* h=surgescript_object_userdata(obj);if(h){bool attached=h->a.state>=BOR_DASH;traversal_clear(h);rebase(h);if(attached){physicsactor_detach_from_ground(h->body);h->a.grounded=false;motion_apply(h,BOR_MOTION_STOP,3,0,0);start(h,"walkoff",BOR_FREE);}}return NULL;}
static surgescript_var_t* controller_destroy(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 HostActor* h=surgescript_object_userdata(obj);if(h){bsl_host_unbind(h->lifetime);unlink_actor(h);if(h->a.serial<64&&slots[h->a.serial]==h){slots[h->a.serial]=NULL;}input_destroy(h->drive_input);physicsactor_destroy(h->body);free(h);surgescript_object_set_userdata(obj,NULL);}return NULL;
}
bool scripting_bor_tick_pending(void){return bor_tick_requested&&!level_editmode();}
void scripting_update_bor(void){if(bor_tick_requested){bor_tick_requested=false;tick_world();}}
static surgescript_var_t* bridge_tick(surgescript_object_t* obj,const surgescript_var_t** p,int n){bor_tick_requested=true;return NULL;}
static surgescript_var_t* bridge_destroy(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 audio_demo_clear();bsl_host_shutdown();
 if(controls){input_destroy(controls);}
 controls=NULL;bor_tick_requested=false;ledge_ui_started=false;impact_clear();release_sounds();memset(previous_keys,0,sizeof(previous_keys));world_time=0;total_frames=0;last_tap[0]=last_tap[1]=-1000;checks=failures=total_hits=test_hits=total_sound_events=total_impact_events=0;return NULL;
}
static surgescript_var_t* bridge_status(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 if(audio_enabled())return surgescript_var_set_string(surgescript_var_create(),"Audio Propagation Demo | WASD move H sword J unarmed K ranged L/Space jump | Ctrl/LT guard | F12 level editor");
 char text[1024];int used=snprintf(text,sizeof(text),"BOR actors / Surge terrain | C2 movement profile | depth OFF\nArrows/WASD move; L/Space jump; double-tap direction or Shift dash\nH/X sword; J/Y unarmed; K/B ranged; Ctrl/LT + attack: guard; Ctrl/LT + double-tap: evade\nUp/Down pole/ladder; Down duck/roll; press against actor to hold; Up/L climb; Down/away drop; R reset\nF12 level editor | F3 body %s F4 hurt %s F5 hit %s F6 origin %s | F2 ledge sensors / F7 soft contacts\n",debug_mask&1?"ON":"OFF",debug_mask&2?"ON":"OFF",debug_mask&4?"ON":"OFF",debug_mask&8?"ON":"OFF");
 for(int i=0;i<64;i++)if(slots[i]){BorActor* a=&slots[i]->a;if(a->model->training_dummy)used+=snprintf(text+used,sizeof(text)-used,"TRAINING LOG: HP INF | hits %d | damage %.0f  ",slots[i]->training_hits,slots[i]->training_damage);else used+=snprintf(text+used,sizeof(text)-used,"%s %d/%d [%s:%d]  ",a->model->name,a->health,a->model->health,a->animation->name,a->frame);if(used>800)break;}
 bsl_shell_status("actors",text);
 return surgescript_var_set_string(surgescript_var_create(),"");
}
/* UI capture consumes input history, never actor motion or committed jobs. Signed: Atlas. */
bool bsl_bor_input_held(void){
 ALLEGRO_KEYBOARD_STATE keys;if(ps_lab_automatic())memset(&keys,0,sizeof keys);else al_get_keyboard_state(&keys);
 const int codes[]={ALLEGRO_KEY_ENTER,ALLEGRO_KEY_ESCAPE,ALLEGRO_KEY_LCTRL,ALLEGRO_KEY_A,ALLEGRO_KEY_D,ALLEGRO_KEY_W,ALLEGRO_KEY_S,ALLEGRO_KEY_UP,ALLEGRO_KEY_DOWN,ALLEGRO_KEY_LEFT,ALLEGRO_KEY_RIGHT,ALLEGRO_KEY_C,ALLEGRO_KEY_BACKSPACE,ALLEGRO_KEY_U,ALLEGRO_KEY_V,ALLEGRO_KEY_H,ALLEGRO_KEY_J,ALLEGRO_KEY_K,ALLEGRO_KEY_L,ALLEGRO_KEY_SPACE,ALLEGRO_KEY_LSHIFT,ALLEGRO_KEY_RSHIFT};
 for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);i++)if(al_key_down(&keys,codes[i]))return true;
 if(controls)for(int i=0;i<IB_MAX;i++)if(input_button_down(controls,(inputbutton_t)i))return true;
 return false;
}
bool bsl_bor_sequence_exit_action_held(void){
 ALLEGRO_KEYBOARD_STATE keys;if(ps_lab_automatic())memset(&keys,0,sizeof keys);else al_get_keyboard_state(&keys);return al_key_down(&keys,ALLEGRO_KEY_W)||al_key_down(&keys,ALLEGRO_KEY_UP)||(controls&&input_button_down(controls,IB_UP));
}
bool bsl_bor_sequence_exit_ready(uint64_t lifetime){
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==lifetime){HostActor* h=slots[i];
  return h->placed&&!h->suspended&&!h->frozen_tick&&h->a.health>0&&h->a.state==BOR_FREE&&h->a.freeze<=0&&!h->a.timeline_fault&&h->a.partner<0;
 }
 return false;
}
bool bsl_bor_switch_ready(uint64_t lifetime){
 v2d_t camera=camera_get_position(),half=v2d_multiply(video_get_screen_size(),.5);
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==lifetime){HostActor* h=slots[i];return h->placed&&!h->suspended&&terrain_map()!=NULL&&fabs(h->a.x-camera.x)<=fmax(0,half.x-32)&&fabs(h->a.y-20-camera.y)<=fmax(0,half.y-32);}
 return false;
}
void bsl_bor_capture_input(void){
 ALLEGRO_KEYBOARD_STATE keys;if(ps_lab_automatic())memset(&keys,0,sizeof keys);else al_get_keyboard_state(&keys);
 for(int i=0;i<64;i++)if(slots[i]&&bsl_host_input_locked(slots[i]->lifetime)){
  HostActor* h=slots[i];h->lock_target=0;h->steering=h->input_direction=h->climb_direction=0;h->running=false;h->down_held=false;h->jump_cut_active=false;
  h->jump_held=al_key_down(&keys,ALLEGRO_KEY_L)||al_key_down(&keys,ALLEGRO_KEY_SPACE)||(controls&&input_button_down(controls,IB_FIRE1));
  h->tap_tick[0]=h->tap_tick[1]=-1000;h->tap_direction=0;h->armed_motion=false;h->motion_step=h->motion_deadline=h->motion_octant=0;
  for(int k=0;k<2;k++){h->press_captured[k]=false;h->press_ticks[k]=0;h->hold_ticks[k]=0;h->channel_captured[k]=true;h->channel_down[k]=al_key_down(&keys,k?ALLEGRO_KEY_J:ALLEGRO_KEY_H)||(controls&&input_button_down(controls,k?IB_FIRE6:IB_FIRE2));}
 }
 for(int k=0;k<ALLEGRO_KEY_MAX;k++)previous_keys[k]=al_key_down(&keys,k);
}
void bsl_bor_probe(BslWorldProbe* out){
 memset(out,0,sizeof(*out));out->tick=(uint64_t)total_frames;
 for(int i=0;i<64;i++)if(slots[i])out->actor_count++;
 surgescript_object_t* fixture=moving_fixture();if(fixture){v2d_t p=scripting_util_world_position(fixture);out->platform_x=p.x;out->platform_y=p.y;}
}
extern void scripting_register_foundation(surgescript_vm_t* vm);
static surgescript_var_t* bridge_ui_captures(surgescript_object_t* obj,const surgescript_var_t** p,int n){return surgescript_var_set_bool(surgescript_var_create(),bsl_host_captures_commands()||bsl_shell_keyboard());}
extern void scripting_register_spatial_audio(surgescript_vm_t* vm);
static surgescript_var_t* controller_hearing(surgescript_object_t* obj,const surgescript_var_t** p,int n){HostActor* h=surgescript_object_userdata(obj);if(h){h->listening=surgescript_var_get_bool(p[0]);h->hearing_threshold=fmax(.001,surgescript_var_get_number(p[1]));}return NULL;}
#include "../physics_lab/prop_controller.inc"
void scripting_register_bor(surgescript_vm_t* vm){
 surgescript_vm_bind(vm,"BSLPhysicsPropController","state:main",controller_main,0);
 surgescript_vm_bind(vm,"BSLPhysicsPropController","update",bsl_lab_prop_update,1);
 surgescript_vm_bind(vm,"BSLPhysicsPropController","destructor",bsl_lab_prop_destroy,0);
 scripting_register_foundation(vm);scripting_register_spatial_audio(vm);register_impact_feed(vm);register_impact_audio_view(vm);
 surgescript_vm_bind(vm,"AudioDemoDoorView","state:main",controller_main,0);
 surgescript_vm_bind(vm,"AudioDemoDoorView","get_open",audio_door_view,0);
 surgescript_vm_bind(vm,"BORController","setHearing",controller_hearing,2);
 register_climb_regions(vm);
 surgescript_tagsystem_add_tag(surgescript_vm_tagsystem(vm),"BORController","renderable");
 surgescript_tagsystem_add_tag(surgescript_vm_tagsystem(vm),"BORController","gizmo");
 surgescript_vm_bind(vm,"BORController","onRender",controller_render,2);
 surgescript_vm_bind(vm,"BORController","onRenderGizmos",controller_render,2);
 surgescript_vm_bind(vm,"BORController","get_zindex",controller_zindex,0);
 surgescript_vm_bind(vm,"BORController","get_lockTarget",controller_lock_target,0);
 surgescript_vm_bind(vm,"BORController","get_aimX",controller_aim_x,0);
 surgescript_vm_bind(vm,"BORController","get_aimY",controller_aim_y,0);
 surgescript_vm_bind(vm,"BORController","applyMotion",controller_motion,4);
 surgescript_vm_bind(vm,"BORController","get_velocityX",controller_vx,0);
 surgescript_vm_bind(vm,"BORController","get_velocityY",controller_vy,0);
 surgescript_vm_bind(vm,"BORController","state:main",controller_main,0);
 surgescript_vm_bind(vm,"BORController","init",controller_init,1);
 surgescript_vm_bind(vm,"BORController","rebase",controller_rebase,0);
 surgescript_vm_bind(vm,"BORController","setWakeRange",controller_wakerange,1);
 surgescript_vm_bind(vm,"BORController","bindCharacter",controller_character,1);
 surgescript_vm_bind(vm,"BORController","characterModel",controller_character_model,1);
 surgescript_vm_bind(vm,"BORController","characterAt",controller_character_at,1);
 surgescript_vm_bind(vm,"BORController","beginConversation",controller_begin_conversation,4);
 surgescript_vm_bind(vm,"BORController","chooseConversation",controller_choose_conversation,1);
 surgescript_vm_bind(vm,"BORController","selectedCharacter",controller_selected_character,0);
 surgescript_vm_bind(vm,"BORController","initCharacter",controller_init_character,1);
 surgescript_vm_bind(vm,"BORController","actorModel",controller_actor_model,1);
 surgescript_vm_bind(vm,"BORController","actorAt",controller_actor_at,1);
 surgescript_vm_bind(vm,"BORController","initActor",controller_init_actor,1);
 surgescript_vm_bind(vm,"BORController","validActorPosition",controller_character_position_valid,2);
 surgescript_vm_bind(vm,"BORController","setInteraction",controller_set_interaction,1);
 surgescript_vm_bind(vm,"BORController","resetInteractionBinding",controller_reset_interaction,0);
 surgescript_vm_bind(vm,"BORController","interactionDefinition",controller_interaction_definition,0);
 surgescript_vm_bind(vm,"BORController","validCharacterPosition",controller_character_position_valid,2);
 surgescript_vm_bind(vm,"BORController","characterEntity",controller_character_entity,1);
 surgescript_vm_bind(vm,"BORController","selectCharacter",controller_select_character,1);
 surgescript_vm_bind(vm,"BORController","chooseInitialCharacter",controller_choose_initial_character,1);

 surgescript_vm_bind(vm,"BORController","awardReward",controller_reward,2);
 surgescript_vm_bind(vm,"BORController","useAbility",controller_ability,1);
 surgescript_vm_bind(vm,"BORController","destructor",controller_destroy,0);
 surgescript_vm_bind(vm,"BORBridge","state:main",controller_main,0);
 surgescript_vm_bind(vm,"BORBridge","tick",bridge_tick,0);
 surgescript_vm_bind(vm,"BORBridge","status",bridge_status,0);
 surgescript_vm_bind(vm,"BORBridge","get_uiCapturesInput",bridge_ui_captures,0);
 surgescript_vm_bind(vm,"BORBridge","destructor",bridge_destroy,0);
}

#include "gameplay_test.inc"
#include "progression_test.inc"

#include "learning_test.inc"

#include "checkpoint_test.inc"
#include "progress_save_test.inc"
#include "progress_boot_test.inc"
#include "pickup_test.inc"
#include "loot_test.inc"
#include "equipment_test.inc"
#include "event_reward_test.inc"
#include "quest_test.inc"
#include "commerce_test.inc"

#include "teleport_test.inc"

#include "calendar_test.inc"

#include "slope_test.inc"

#include "weather_test.inc"

#include "pose_test.inc"

#include "registry_character_test.inc"

#include "registry_lifecycle_test.inc"

#include "registry_factory_test.inc"

#include "registry_event_test.inc"

#include "registry_start_test.inc"

#include "registry_placement_test.inc"

#include "registry_assembly_test.inc"

#include "registry_dormant_test.inc"

#include "registry_reconstruct_test.inc"

#include "registry_conversation_test.inc"
#include "registry_interaction_test.inc"
#include "registry_placement_binding_test.inc"
#include "registry_actor_factory_test.inc"
#include "registry_actor_event_test.inc"

#include "../cell_terrain/bridge.inc"

unsigned bsl_shell_debug_get(void){return debug_mask;}
void bsl_shell_debug_set(unsigned value){debug_mask=value&63;}
