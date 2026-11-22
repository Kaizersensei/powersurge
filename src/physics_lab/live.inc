// Isolated player-facing physics laboratory. Experimental, signed: Mech.
#include "../physics_lab/world.h"
#include "../physics_lab/bor_hurtbox.h"
#include "../physics_lab/object_factory.h"
#include "fixture_profiles.h"
#include <map>
extern "C" {
#include "../core/engine.h"
}
namespace {
struct LabWorld {BslPhysicsWorld* world=nullptr;BslObjectFactory* factory=nullptr;uint64_t proxy=0,next_proxy=0,actor=0;BslHurtboxPose previous{};std::map<std::pair<uint64_t,uint64_t>,uint64_t> hits;};
static LabWorld lab;
static bool lab_failed=false;

static bool lab_enabled(){return true;}
static void lab_end(){
 if(lab.factory)bsl_object_factory_destroy(lab.factory);
 if(lab.world)bsl_physics_world_destroy(lab.world);
 lab={};lab_failed=false;
}
static bool lab_fail(const char* why){lab_end();lab_failed=true;logfile_message("Physics world unavailable: %s",why);return false;}
static bool lab_begin(){
 if(!lab_enabled()||!active||lab_failed)return false;
 if(lab.world)return true;
 lab.world=bsl_physics_world_create(0,10);if(!lab.world)return lab_fail("world allocation");
 uint64_t id=1;
 for(int route=0;route<3;route++){
  auto contours=boundary(field,route);if(!contours.valid)return lab_fail("invalid terrain contour");
  for(const auto& loop:contours.loops){
   std::vector<BslPhysicsPoint> points;points.reserve(loop.size());for(auto p:loop)points.push_back({p.first/16.f,p.second/16.f});
   BslPhysicsContourDef d{};d.id=id++;d.points=points.data();d.count=unsigned(points.size());d.category=uint64_t(1)<<route;d.mask=~uint64_t(0);
   if(!bsl_physics_contour_create(lab.world,&d))return lab_fail("terrain contour unsupported or capacity exceeded");
  }
 }
 if(id+3072>4096)return lab_fail("terrain identity capacity exceeded");
 lab.factory=bsl_object_factory_create(lab.world,id,id+2047);lab.next_proxy=id+2048;
 if(!lab.factory)return lab_fail("factory allocation");
 for(const auto& d:fixture_profiles)if(!bsl_object_factory_define(lab.factory,&d))return lab_fail("invalid object definition");
 return true;
}
}
extern "C" bool bsl_lab_object(const char* placement,const char* definition,float* x,float* y,float* angle){
 if(!lab.world||!lab.factory||!placement||!definition||!x||!y||!angle)return false;
 uint64_t id=0;
 if(!bsl_object_factory_lookup(lab.factory,placement,&id)){
  if(!bsl_object_factory_spawn(lab.factory,definition,placement,(*x-4096.f)/32.f,(*y-4096.f)/32.f,0))return false;
  if(!bsl_object_factory_lookup(lab.factory,placement,&id))return false;
  logfile_message("Physics object instantiated: %s (%s)",definition,placement);
 }
 BslPhysicsBodyState state{};if(!bsl_physics_body_state(lab.world,id,&state))return false;
 *x=state.x*32.f+4096.f;*y=state.y*32.f+4096.f;*angle=state.angle;
 return true;
}
extern "C" void bsl_lab_remove(const char* placement){if(lab.factory&&placement)bsl_object_factory_remove(lab.factory,placement);}
extern "C" void bsl_lab_actor(uint64_t lifetime,const double* corners){
 if(!lab.world)return;
 BslHurtboxPose pose{};
 if(!lifetime||!corners||!bsl_hurtbox_pose_from_corners(corners,4096,4096,32,&pose)){
  if(lab.proxy)bsl_physics_body_enabled(lab.world,lab.proxy,false);
  lab.actor=0;return;
 }
 if(!lab.proxy){
  BslPhysicsBodyDef d{};d.id=lab.next_proxy;d.mode=BSL_BODY_DYNAMIC;d.gravity_free=true;d.shape=BSL_SHAPE_BOX;d.x=pose.x;d.y=pose.y;d.angle=pose.angle;d.half_width=pose.half_width;d.half_height=pose.half_height;d.density=12;d.friction=.4f;d.category=16;d.mask=8;
  if(!bsl_physics_body_create(lab.world,&d))return;
  lab.proxy=d.id;
 }
 if(lab.actor!=lifetime){
  // Position a disabled proxy before it re-enters the solver; never sweep between actors.
  if(!bsl_physics_body_enabled(lab.world,lab.proxy,false))return;
  if(!bsl_physics_kinematic_box_pose(lab.world,lab.proxy,pose.x,pose.y,pose.angle,pose.half_width,pose.half_height,0,0,0))return;
  if(!bsl_physics_body_enabled(lab.world,lab.proxy,true))return;
  lab.actor=lifetime;lab.previous=pose;return;
 }
 float dx=pose.x-lab.previous.x,dy=pose.y-lab.previous.y,da=std::remainder(pose.angle-lab.previous.angle,6.283185307f);
 if(std::abs(dx)>8||std::abs(dy)>8){lab.previous=pose;dx=dy=da=0;}
 if(bsl_physics_kinematic_box_pose(lab.world,lab.proxy,lab.previous.x,lab.previous.y,lab.previous.angle,pose.half_width,pose.half_height,dx*60,dy*60,da*60))lab.previous=pose;
}
extern "C" void bsl_lab_attack(uint64_t actor,uint64_t epoch,int facing,const double* corners){
 if(!lab.world||!actor||!epoch||!corners||(facing!=1&&facing!=-1))return;
 BslHurtboxPose p{};if(!bsl_hurtbox_pose_from_corners(corners,4096,4096,32,&p))return;
 float radius=std::min(p.half_width,p.half_height),length=std::abs(p.half_width-p.half_height);
 float ux=p.half_width>=p.half_height?std::cos(p.angle):-std::sin(p.angle);
 float uy=p.half_width>=p.half_height?std::sin(p.angle):std::cos(p.angle);
 BslPhysicsHit hits[256];unsigned n=0;
 if(!bsl_physics_query_capsule(lab.world,p.x-ux*length,p.y-uy*length,p.x+ux*length,p.y+uy*length,radius,hits,256,&n))return;
 for(unsigned i=0;i<n;i++){
  auto key=std::make_pair(actor,hits[i].id);auto it=lab.hits.find(key);
  if(it!=lab.hits.end()&&it->second==epoch)continue;
  if(it==lab.hits.end()&&lab.hits.size()>=4096)continue;
  float mass=0;if(!bsl_physics_body_mass(lab.world,hits[i].id,&mass))continue;
  float impulse=std::min(300.f,mass*12.f);
  if(bsl_physics_body_impulse_at(lab.world,hits[i].id,facing*impulse,-impulse*.2f,hits[i].x,hits[i].y))lab.hits[key]=epoch;
 }
}
extern "C" void bsl_lab_tick(){if(lab_begin())bsl_physics_step(lab.world,1.f/60,4);}
extern "C" void bsl_lab_suspend(){lab_end();}
