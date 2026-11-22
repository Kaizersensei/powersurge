// Experimental definition-backed rigid-body factory. Signed: Mech.
#include "object_factory.h"
#include <cmath>
#include <cstring>
#include <map>
#include <string>
namespace {
bool valid_id(const char* p){
 if(!p)return false;
 for(unsigned i=0;i<=128;i++){unsigned char c=static_cast<unsigned char>(p[i]);if(!c)return i>0;if(c<32||c==127)return false;}
 return false;
}
bool finite(float x){return std::isfinite(x)&&std::abs(x)<=10000.f;}
struct Definition {BslPhysicsShape shape;float hw,hh,density,friction,restitution;uint64_t category,mask;};
struct Placement {uint64_t body;std::string definition;};
}
struct BslObjectFactory {
 BslPhysicsWorld* world;uint64_t next,last;std::map<std::string,Definition> definitions;std::map<std::string,Placement> placements;
};
extern "C" BslObjectFactory* bsl_object_factory_create(BslPhysicsWorld* w,uint64_t first,uint64_t last){
 if(!w||!first||first>last)return nullptr;
 try{return new BslObjectFactory{w,first,last,{},{}};}catch(...){return nullptr;}
}
extern "C" void bsl_object_factory_destroy(BslObjectFactory* f){
 if(!f)return;
 for(const auto& p:f->placements)bsl_physics_body_destroy(f->world,p.second.body);
 delete f;
}
extern "C" bool bsl_object_factory_define(BslObjectFactory* f,const BslObjectDefinition* d){
 if(!f||!d||!valid_id(d->id)||f->definitions.size()>=4096||d->shape<BSL_SHAPE_CIRCLE||d->shape>BSL_SHAPE_STAR5||!d->category)return false;
 if(!finite(d->half_width)||!finite(d->half_height)||!finite(d->density)||!finite(d->friction)||!finite(d->restitution)||d->half_width<.02f||d->half_height<0||d->density<=0||d->friction<0||d->restitution<0||d->restitution>1||(d->shape!=BSL_SHAPE_CIRCLE&&d->half_height<.02f)||(d->shape==BSL_SHAPE_STAR5&&(d->half_width<.1f||d->half_height<.1f)))return false;
 try{return f->definitions.emplace(d->id,Definition{d->shape,d->half_width,d->half_height,d->density,d->friction,d->restitution,d->category,d->mask}).second;}catch(...){return false;}
}
extern "C" bool bsl_object_factory_spawn(BslObjectFactory* f,const char* definition,const char* placement,float x,float y,float angle){
 if(!f||!valid_id(definition)||!valid_id(placement)||!finite(x)||!finite(y)||!finite(angle)||!f->next||f->next>f->last)return false;
 auto d=f->definitions.find(definition);if(d==f->definitions.end())return false;
 try{
  auto added=f->placements.emplace(placement,Placement{f->next,definition});if(!added.second)return false;
  const auto& v=d->second;BslPhysicsBodyDef body{};body.id=f->next;body.mode=BSL_BODY_DYNAMIC;body.shape=v.shape;body.x=x;body.y=y;body.angle=angle;body.half_width=v.hw;body.half_height=v.hh;body.density=v.density;body.friction=v.friction;body.restitution=v.restitution;body.category=v.category;body.mask=v.mask;
  if(!bsl_physics_body_create(f->world,&body)){f->placements.erase(added.first);return false;}
  f->next=f->next==UINT64_MAX?0:f->next+1;return true;
 }catch(...){return false;}
}
extern "C" bool bsl_object_factory_remove(BslObjectFactory* f,const char* placement){
 if(!f||!valid_id(placement))return false;
 auto i=f->placements.find(placement);
 if(i==f->placements.end()||!bsl_physics_body_destroy(f->world,i->second.body))return false;
 f->placements.erase(i);return true;
}
extern "C" bool bsl_object_factory_lookup(BslObjectFactory* f,const char* placement,uint64_t* out){
 if(!f||!out||!valid_id(placement))return false;
 auto i=f->placements.find(placement);if(i==f->placements.end())return false;*out=i->second.body;return true;
}
