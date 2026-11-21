/* Native actor-kernel regression checks against supplied model data. Signed: Atlas. */
#include "bor_core.h"
#include "bor_models.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int checks,enters,activations,movement;static uint64_t last_epoch;
#define CHECK(value) do{checks++;if(!(value)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value);return 1;}}while(0)
static void observe(BorActor* a,const BorFrame* f,bool entering,void* ignored){
 if(entering){enters++;movement+=f->move;if(a->active&&a->epoch!=last_epoch){activations++;last_epoch=a->epoch;}}
}
static void freeze_on_segment(BorActor* a,const BorFrame* f,bool entering,void* state){int* flag=state;if(!entering&&!*flag){*flag=1;a->freeze=.1;}}
static void replace_on_segment(BorActor* a,const BorFrame* f,bool entering,void* state){
 int* fired=state;if(!entering&&!*fired){*fired=1;bor_start(a,"pain",BOR_PAIN,NULL,NULL);}
}
static void restart_on_segment(BorActor* a,const BorFrame* f,bool entering,void* state){
 int* fired=state;if(!entering&&!*fired){*fired=1;bor_start(a,a->animation->name,a->state,NULL,NULL);}
}
static void replace_on_entry(BorActor* a,const BorFrame* f,bool entering,void* state){
 int* flag=state;if(entering&&a->frame==1&&!*flag){*flag=1;bor_start(a,"pain",BOR_PAIN,NULL,NULL);}
}
static void recursive_entry(BorActor* a,const BorFrame* f,bool entering,void* state){
 int* count=state;if(entering){(*count)++;bor_start(a,a->animation->name,a->state,recursive_entry,state);}
}
static void damage_on_entry(BorActor* a,const BorFrame* f,bool entering,void* target){if(entering)bor_damage(a,target,0);}
int main(void){
 BorActor a,b;bor_init(&a,bor_models,0);bor_init(&b,bor_models+2,2);a.serial=0;b.serial=1;
 CHECK(a.health==100&&b.health==30);CHECK(bor_models[1].speed==50&&bor_models[3].speed==50&&bor_models[2].speed==120);
 const double durations[]={.30,.28,.27,.18,.43};
 for(int i=0;i<5;i++){char name[20];sprintf(name,"attack%d",i+1);const BorAnimation* an=bor_animation(bor_models,name);double total=0;for(int f=0;f<an->count;f++)total+=an->frames[f].duration;CHECK(fabs(total-durations[i])<1e-8);}
 CHECK(bor_animation(bor_models,"jumpattack2")->follow[0]==0);
 CHECK(bor_animation(bor_models,"dodge")->frames[0].bw==0);CHECK(bor_animation(bor_models+3,"rise")->frames[0].bw==0);
 enters=activations=movement=0;last_epoch=0;bor_start(&a,"runjumpattack",BOR_ACTION,observe,NULL);bor_advance(&a,.36,observe,NULL);
 CHECK(a.finished);CHECK(enters==12);CHECK(activations==6);
 bor_init(&a,bor_models+1,1);movement=0;bor_start(&a,"attack1",BOR_ACTION,observe,NULL);bor_advance(&a,1.42,observe,NULL);CHECK(a.finished&&movement==40);
 bor_init(&a,bor_models+2,2);movement=0;bor_start(&a,"attack2",BOR_ACTION,observe,NULL);bor_advance(&a,.5,observe,NULL);CHECK(a.finished&&movement==32);
 bor_init(&a,bor_models+4,4);bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&a,.071,NULL,NULL);CHECK(bor_frame(&a)->bw==0&&a.active);uint64_t epoch=a.epoch;bor_advance(&a,3,NULL,NULL);CHECK(a.epoch==epoch&&a.active);CHECK(bor_frame(&a)->drop==-3);
 bor_init(&a,bor_models+2,2);bor_init(&b,bor_models,0);a.serial=2;b.serial=0;a.x=0;a.y=100;b.x=20;b.y=100;
 bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&a,.071,NULL,NULL);CHECK(bor_damage(&a,&b,0));CHECK(b.health==98);
 b.invulnerable=0;bor_advance(&a,.075,NULL,NULL);CHECK(bor_frame(&a)->damage==5);CHECK(!bor_damage(&a,&b,.3));
 double x,y,w,h;bor_rect(&a,true,&x,&y,&w,&h);a.facing=-1;double lx,ly,lw,lh;bor_rect(&a,true,&lx,&ly,&lw,&lh);CHECK(fabs(lx+x+w)<1e-8&&ly==y&&lw==w&&lh==h);
 bor_init(&a,bor_models,0);bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&a,.299,NULL,NULL);CHECK(!a.finished);bor_advance(&a,.001,NULL,NULL);CHECK(a.finished);

 bor_init(&a,bor_models+2,2);bor_init(&b,bor_models,0);a.serial=2;b.serial=0;a.x=0;a.y=100;b.x=20;b.y=100;
 bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&a,.071,NULL,NULL);b.state=BOR_FALL;b.grounded=true;CHECK(!bor_damage(&a,&b,0));
 b.grounded=false;CHECK(bor_damage(&a,&b,0));
 bor_init(&a,bor_models,0);bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&a,.3,NULL,NULL);CHECK(a.finished);
 bor_start(&a,"idle",BOR_FREE,NULL,NULL);CHECK(!a.active&&a.epoch==0);
 a.knockdown=3;a.knockdown_until=1;bor_advance(&a,1,NULL,NULL);CHECK(a.knockdown==0);
 bor_init(&a,bor_models,0);bor_start(&a,"runjumpattack",BOR_ACTION,NULL,NULL);int trigger=0;bor_advance(&a,.1,freeze_on_segment,&trigger);CHECK(a.frame==0&&a.age==0&&trigger==1);
 bor_advance(&a,.05,freeze_on_segment,&trigger);CHECK(a.frame==0&&a.age==0);bor_advance(&a,.05,freeze_on_segment,&trigger);CHECK(a.frame==0&&a.age==0);
 bor_advance(&a,.03,freeze_on_segment,&trigger);CHECK(a.frame==1);
 bor_init(&a,bor_models,0);bor_init(&b,bor_models+1,1);a.serial=0;b.serial=1;a.x=0;a.y=100;b.x=20;b.y=100;
 bor_start(&a,"jumpattack2",BOR_ACTION,NULL,NULL);bor_advance(&a,.21,NULL,NULL);CHECK(a.finished&&a.active);uint64_t held=a.epoch;
 CHECK(bor_damage(&a,&b,.3));CHECK(a.epoch==held);CHECK(fabs(a.combo_until-2.0/3)<1e-12);
 CHECK(bor_models[0].jump_speed==100&&bor_models[0].run_jump==400&&bor_models[0].run_distance==3);
 CHECK(bor_models[1].throw_distance==250&&bor_models[1].throw_height==400);
 CHECK(bor_models[2].throw_distance==200&&bor_models[2].throw_height==300&&bor_models[3].throw_distance==100);
 BorVelocity velocity={200,-120},resolved={77,88};
 CHECK(bor_resolve_motion(velocity,BOR_MOTION_PRESERVE,3,0,0,&resolved)&&resolved.x==200&&resolved.y==-120);
 CHECK(bor_resolve_motion(velocity,BOR_MOTION_SET,BOR_AXIS_Y,0,-400,&resolved)&&resolved.x==200&&resolved.y==-400);
 CHECK(bor_resolve_motion(velocity,BOR_MOTION_ADD,3,25,40,&resolved)&&resolved.x==225&&resolved.y==-80);
 CHECK(bor_resolve_motion(velocity,BOR_MOTION_STOP,BOR_AXIS_X,0,0,&resolved)&&resolved.x==0&&resolved.y==-120);
 CHECK(bor_resolve_motion(velocity,BOR_MOTION_SET,BOR_AXIS_X,0,0,&resolved)&&resolved.x==0&&resolved.y==-120);
 double tangent=77,angle=3.141592653589793/6;BorVelocity slope={200*cos(angle),-200*sin(angle)};
 CHECK(bor_ground_tangent(slope,angle,&tangent)&&fabs(tangent-200)<1e-8);
 CHECK(bor_resolve_motion(slope,BOR_MOTION_STOP,BOR_AXIS_Y,0,0,&resolved));CHECK(!bor_ground_tangent(resolved,angle,&tangent)&&fabs(tangent-200)<1e-8);
 CHECK(bor_resolve_motion(slope,BOR_MOTION_SET,BOR_AXIS_X,30,0,&resolved));CHECK(!bor_ground_tangent(resolved,angle,&tangent));
 CHECK(bor_ground_tangent((BorVelocity){0,0},angle,&tangent)&&tangent==0);
 resolved=(BorVelocity){77,88};CHECK(!bor_resolve_motion(velocity,BOR_MOTION_SET,4,0,0,&resolved)&&resolved.x==77&&resolved.y==88);
 CHECK(!bor_resolve_motion(velocity,BOR_MOTION_SET,3,NAN,0,&resolved)&&resolved.x==77&&resolved.y==88);
 CHECK(!bor_resolve_motion((BorVelocity){1e308,0},BOR_MOTION_ADD,3,1e308,0,&resolved)&&resolved.x==77&&resolved.y==88);
 CHECK(!bor_resolve_motion(velocity,99,3,0,0,&resolved));CHECK(!bor_resolve_motion(velocity,BOR_MOTION_SET,0,0,0,&resolved));
 bor_init(&a,bor_models,0);bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);int replaced=0;bor_advance(&a,.1,replace_on_segment,&replaced);
 CHECK(replaced&&a.state==BOR_PAIN&&a.frame==0&&a.age==0&&fabs(a.remaining-bor_frame(&a)->duration)<1e-9);
 bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);replaced=0;bor_advance(&a,.1,restart_on_segment,&replaced);
 CHECK(replaced&&a.frame==0&&a.age==0&&fabs(a.remaining-bor_frame(&a)->duration)<1e-9);
 bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);replaced=0;double first_duration=bor_frame(&a)->duration;
 bor_advance(&a,first_duration+.001,replace_on_entry,&replaced);CHECK(replaced&&a.age==0&&a.state==BOR_PAIN);
 int recursion=0;CHECK(bor_start(&a,"idle",BOR_FREE,recursive_entry,&recursion));CHECK(recursion==16&&a.age==0);
 uint64_t unchanged=a.action_id;double remain=a.remaining;bor_advance(&a,NAN,observe,NULL);bor_advance(&a,-1,observe,NULL);bor_advance(&a,0,observe,NULL);CHECK(a.action_id==unchanged&&a.remaining==remain);
 CHECK(!bor_start(&a,"absent",BOR_ACTION,NULL,NULL)&&a.action_id==unchanged);
 BorModel altered=bor_models[0];BorAnimation clips[128];memcpy(clips,altered.animations,sizeof(*clips)*altered.count);altered.animations=clips;
 int jump_index=(int)(bor_animation(&altered,"jump")-clips);BorCancel gate={1,2,1,BOR_INPUT_JUMP,"freespecial3"};clips[jump_index].cancels=&gate;clips[jump_index].cancel_count=1;
 CHECK(bor_init(&a,&altered,77));bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);a.combo_hits=1;bor_start(&a,"jump",BOR_ACTION,NULL,NULL);a.frame=1;a.vx=73;
 unchanged=a.action_id;CHECK(a.combo_hits==0&&!bor_try_cancel(&a,BOR_INPUT_JUMP,NULL,NULL));CHECK(a.vx==73&&a.action_id==unchanged);
 a.combo_hits=1;a.frame=0;CHECK(!bor_try_cancel(&a,BOR_INPUT_JUMP,NULL,NULL));a.frame=3;CHECK(!bor_try_cancel(&a,BOR_INPUT_JUMP,NULL,NULL));
 a.frame=1;a.freeze=.1;CHECK(!bor_try_cancel(&a,BOR_INPUT_JUMP,NULL,NULL));a.freeze=0;CHECK(!bor_try_cancel(&a,BOR_INPUT_SPECIAL,NULL,NULL));
 CHECK(bor_try_cancel(&a,BOR_INPUT_JUMP,NULL,NULL));CHECK(!strcmp(a.animation->name,"freespecial3")&&a.combo_hits==0&&a.vx==73);
 int order[]={1,3,2};altered.chain=order;altered.chain_count=3;CHECK(bor_init(&a,&altered,77));
 BorFrame target_frame=*bor_models[1].animations[0].frames;target_frame.ox=target_frame.oy=0;target_frame.bx=target_frame.by=-1000;target_frame.bw=target_frame.bh=2000;BorAnimation target_animation=*bor_models[1].animations;target_animation.frames=&target_frame;target_animation.count=1;bor_init(&b,bor_models+1,1);b.animation=&target_animation;b.frame=0;b.x=b.y=0;a.serial=0;b.serial=1;
 for(int i=0;i<3;i++){a.state=BOR_FREE;CHECK(bor_chain_attack(&a,NULL,NULL));char expected[20];sprintf(expected,"attack%d",order[i]);CHECK(!strcmp(a.animation->name,expected));CHECK(a.combo_until==0);
  for(int frame=0;frame<a.animation->count;frame++)if(a.animation->frames[frame].aw>0){a.frame=frame;a.active=true;a.epoch=1000+i;b.health=100;b.invulnerable=0;CHECK(bor_damage(&a,&b,0));break;}
  CHECK(a.combo==(i+1)%3&&a.combo_until==a.animation->connect_window);}
 a.state=BOR_FREE;CHECK(bor_chain_attack(&a,NULL,NULL));a.state=BOR_FREE;CHECK(bor_chain_attack(&a,NULL,NULL)&&!strcmp(a.animation->name,"attack1"));
 a.state=BOR_FREE;a.combo=0;a.vx=37;order[0]=999;unchanged=a.action_id;CHECK(!bor_chain_attack(&a,NULL,NULL)&&a.combo==0&&a.vx==37&&a.action_id==unchanged);
 BorModel reordered[]={bor_models[4],bor_models[1],bor_models[0]};const BorModel* found=bor_find_model(reordered,3,"michio_sword");CHECK(found==reordered+2);CHECK(bor_find_model(reordered,3,"absent")==NULL);
 bor_init(&a,found,2);CHECK(bor_is_player(&a));bor_init(&b,reordered,0);CHECK(!bor_is_player(&b)&&bor_can_target(&b,&a));
 clips[jump_index].flags=0;clips[jump_index].name="ledgejump_attack";order[0]=1;bor_init(&a,&altered,77);a.vx=200;bor_start(&a,"ledgejump_attack",BOR_ACTION,NULL,NULL);CHECK(a.vx==200&&a.animation->flags==0);
 BorModel invalid=bor_models[0];invalid.count=0;CHECK(!bor_init(&a,&invalid,0));CHECK(bor_frame(&a)==NULL);
 BorAnimation malformed=*bor_animation(bor_models,"idle");malformed.count=0;invalid=bor_models[0];invalid.animations=&malformed;invalid.count=1;CHECK(!bor_init(&a,&invalid,0));
 bor_init(&a,bor_models,0);bor_start(&a,"attack1",BOR_ACTION,NULL,NULL);bor_advance(&a,.07,NULL,NULL);bor_init(&b,bor_models+1,1);a.serial=0;b.serial=1;a.x=0;a.y=100;b.x=20;b.y=100;
 a.health=0;CHECK(!bor_damage(&a,&b,0));a.health=100;a.state=BOR_DEAD;CHECK(!bor_damage(&a,&b,0)&&!bor_start(&a,"idle",BOR_FREE,NULL,NULL));
 BorFrame tiny=*bor_models[0].animations[0].frames;tiny.duration=1e-12;BorAnimation runaway=*bor_animation(bor_models,"idle");runaway.frames=&tiny;runaway.count=1;runaway.loop=1;runaway.loop_from=0;invalid=bor_models[0];invalid.animations=&runaway;invalid.count=1;
 CHECK(bor_init(&a,&invalid,0));bor_start(&a,"idle",BOR_FREE,NULL,NULL);bor_advance(&a,.01,NULL,NULL);CHECK(a.timeline_fault&&!a.active&&!bor_can_act(&a));
 bor_init(&a,bor_models,0);CHECK(a.pose_scale==1);double px,py,pw,ph;bor_rect(&a,false,&px,&py,&pw,&ph);double base_x=px,base_y=py,base_w=pw,base_h=ph;
 a.pose_scale=.5;a.pose_x=7;a.pose_y=-3;bor_rect(&a,false,&px,&py,&pw,&ph);CHECK(fabs(px-(a.x+7+(base_x-a.x)*.5))<1e-8&&fabs(py-(a.y-3+(base_y-a.y)*.5))<1e-8&&pw==base_w*.5&&ph==base_h*.5);
 CHECK(bor_start(&a,"climb",BOR_CLIMB,NULL,NULL)&&a.pose_scale==1&&a.pose_x==0&&a.pose_y==0);CHECK(!bor_start(&a,"idle",BOR_STATE_COUNT,NULL,NULL));
 bor_init(&a,bor_models,0);CHECK(bor_start(&a,"c2_hurt",BOR_PAIN,NULL,NULL));double paused_remaining=a.remaining;bor_advance(&a,10,NULL,NULL);CHECK(a.frame==0&&!a.finished&&a.remaining==paused_remaining&&a.age==10);
 /* Rotated thin rectangles: their enclosing AABBs overlap while actual boxes do not. */
 BorFrame geometry=*bor_frame(&a);geometry.ox=geometry.oy=0;geometry.ax=geometry.bx=0;geometry.ay=geometry.by=0;geometry.aw=geometry.bw=20;geometry.ah=geometry.bh=2;
 BorAnimation rectangle=*a.animation;rectangle.frames=&geometry;rectangle.count=1;rectangle.loop=0;a.animation=b.animation=&rectangle;a.frame=b.frame=0;a.x=a.y=0;a.pose_x=a.pose_y=b.pose_x=b.pose_y=0;a.pose_scale=b.pose_scale=1;a.facing=b.facing=1;a.pose_angle=b.pose_angle=.7853981633974483;
 b.x=3;b.y=3;CHECK(!bor_overlaps(&a,&b));b.x=1;b.y=1;CHECK(bor_overlaps(&a,&b));
 double corners[8];CHECK(bor_corners(&a,true,corners)&&fabs(corners[2]-14.1421356237)<1e-8&&fabs(corners[3]+14.1421356237)<1e-8);
 a.pose_angle=1.5707963267948966;a.facing=-1;CHECK(bor_corners(&a,true,corners)&&fabs(corners[1]-20)<1e-8);geometry.aw=0;CHECK(!bor_overlaps(&a,&b));
 /* Immediate frame hits must register their stage before the callback executes. */
 geometry.aw=geometry.ah=20;geometry.ax=geometry.ay=0;geometry.damage=1;rectangle.name="attack1";rectangle.flags=BOR_ANIM_ATTACK;rectangle.connect_window=.1;rectangle.frames=&geometry;rectangle.loop=0;
 BorAnimation combo_clips[2]={*bor_animation(bor_models,"idle"),rectangle};int repeat_chain[]={1,1};BorModel combo_model=bor_models[0];combo_model.animations=combo_clips;combo_model.count=2;combo_model.chain=repeat_chain;combo_model.chain_count=2;
 bor_init(&a,&combo_model,0);bor_init(&b,bor_models+1,1);a.serial=0;b.serial=1;b.animation=&target_animation;b.frame=0;b.x=b.y=a.x=a.y=0;
 CHECK(bor_chain_attack(&a,damage_on_entry,&b)&&a.combo==1&&a.combo_until==.1);BorActor second=b;second.invulnerable=0;memset(second.last_hit,0,sizeof(second.last_hit));CHECK(bor_damage(&a,&second,0)&&a.combo==1);
 bor_start(&a,"idle",BOR_FREE,NULL,NULL);CHECK(a.combo==1&&a.combo_until==.1);bor_tick_status(&a,.099);CHECK(a.combo_until>0);bor_tick_status(&a,.002);CHECK(a.combo_until==0);
 CHECK(bor_chain_attack(&a,NULL,NULL)&&a.combo_stage==0);bor_start(&a,"idle",BOR_FREE,NULL,NULL);CHECK(bor_chain_attack(&a,NULL,NULL)&&a.combo_stage==0&&a.combo_until==0);
 for(int i=0;i<bor_sound_count;i++)CHECK(strncmp(bor_sound_paths[i],"samples/bor/",12)==0);
 printf("BOR_CORE_TESTS %d checks; 0 failures\n",checks);return 0;
}
