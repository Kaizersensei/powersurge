// Packed native cosmetic weather layer; registry and simulation are renderer independent. Signed: Atlas.
#include "particles.hpp"
#include "particle_records.hpp"
#include "foundation_host.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <cstdio>
#include <map>
extern "C" {
#include "../core/image.h"
#include "../core/video.h"
#include "../entities/camera.h"
}
namespace {
const bsl::ParticleRegistry& registry(){static const bsl::ParticleRegistry r(compiled_particles());return r;}
bsl::ParticleWorld& world(){static bsl::ParticleWorld w(registry());return w;}
std::map<std::string,image_t*> images;
std::vector<ALLEGRO_VERTEX> vertices;
BslParticleProbe stats{};std::string weather_effect;uint64_t weather_handle=0;
constexpr uint64_t WEATHER_OWNER=UINT64_MAX;
void quad(float x,float y,float w,float h,const bsl::ParticleEmitter& e,ALLEGRO_COLOR color){
 ALLEGRO_VERTEX a={x,y,0,float(e.sx),float(e.sy),color},b={x+w,y,0,float(e.sx+e.sw),float(e.sy),color},c={x+w,y+h,0,float(e.sx+e.sw),float(e.sy+e.sh),color},d={x,y+h,0,float(e.sx),float(e.sy+e.sh),color};
 vertices.push_back(a);vertices.push_back(b);vertices.push_back(c);vertices.push_back(a);vertices.push_back(c);vertices.push_back(d);
}
}
extern "C" uint64_t bsl_host_particle_start(const char* effect,const BslParticleSpawn* p){
 if(!effect||!p||!p->owner||p->owner==WEATHER_OWNER||bsl_host_boot_busy()||bsl_engine_editor())return 0;
 try{return world().start(effect,{p->owner,p->seed,p->x,p->y,p->width,p->height,p->multiplier,p->looping});}catch(...){return 0;}
}
extern "C" bool bsl_host_particle_stop(uint64_t handle,bool cancel){if(!handle||handle==weather_handle)return false;try{return world().stop(handle,cancel);}catch(...){return false;}}
extern "C" void bsl_host_particle_cancel_owner(uint64_t owner){if(owner&&owner!=WEATHER_OWNER)world().cancel_owner(owner);}
extern "C" void bsl_host_particle_probe(BslParticleProbe* out){if(!out)return;*out=stats;out->instances=unsigned(world().instances().size());out->particles=unsigned(world().count());out->dropped=world().dropped();out->weather_handle=weather_handle;std::snprintf(out->weather_effect,sizeof(out->weather_effect),"%s",weather_effect.c_str());}
extern "C" void bsl_particles_clear(){world().clear();weather_handle=0;weather_effect.clear();stats={};}
extern "C" void bsl_particles_release(){bsl_particles_clear();for(auto& p:images)image_unload(p.second);images.clear();std::vector<ALLEGRO_VERTEX>().swap(vertices);}
extern "C" void bsl_particles_tick(double dt){
 const double start=al_get_time();BslWeatherProbe weather{};bsl_host_weather_probe(&weather);
 int kind=std::string(weather.kind)=="rain"?0:std::string(weather.kind)=="snow"?1:std::string(weather.kind)=="ash"?2:-1;
 std::string next=weather.enabled&&!weather.indoor&&weather.precipitation>0&&kind>=0?registry().catalog().weather[size_t(kind)]:"";
 if(next!=weather_effect){if(weather_handle)world().stop(weather_handle,true);weather_handle=0;weather_effect=next;}
 if(!weather_effect.empty()){
  const auto screen=video_get_screen_size();const auto camera=camera_get_position();
  bsl::ParticleSpawn p{WEATHER_OWNER,0x42534c5745415448ULL,camera.x-screen.x*.5-128,camera.y-screen.y*.5-128,screen.x+256,screen.y+256,weather.precipitation,true};
  if(!weather_handle)weather_handle=world().start(weather_effect,p);else world().configure(weather_handle,p);
 }
 constexpr double PI=3.141592653589793;const double angle=weather.direction*PI/180;
 world().update(dt,std::cos(angle)*weather.particle_speed,std::sin(angle)*weather.particle_speed);
 stats.update_ms=(al_get_time()-start)*1000;
}
extern "C" void bsl_particles_render(){
 stats.batches=stats.vertices=0;if(bsl_engine_editor())return;
 const double start=al_get_time();const auto camera=camera_get_position(),screen=video_get_screen_size();
 const float left=camera.x-screen.x*.5f,top=camera.y-screen.y*.5f;
 const bool held=al_is_bitmap_drawing_held();if(held)al_hold_bitmap_drawing(false);
 ALLEGRO_TRANSFORM saved=*al_get_current_transform(),identity;al_identity_transform(&identity);al_use_transform(&identity);
 int op,src,dst,aop,asrc,adst;al_get_separate_blender(&op,&src,&dst,&aop,&asrc,&adst);
 al_set_blender(ALLEGRO_ADD,ALLEGRO_ONE,ALLEGRO_INVERSE_ALPHA);
 if(vertices.capacity()<registry().catalog().capacity*6)vertices.reserve(registry().catalog().capacity*6);
 for(const auto& instance:world().instances())for(size_t emitter=0;emitter<instance.definition->emitters.size();++emitter){
  const auto& e=instance.definition->emitters[emitter];vertices.clear();
  for(const auto& p:instance.particles)if(p.emitter==emitter){
   double t=p.age/p.life,scale=1+(e.end_scale-1)*t;
   float w=float(e.width*scale),h=float(e.height*scale),x=float(p.x-left)-w*.5f,y=float(p.y-top)-h*.5f;
   if(x+w<0||y+h<0||x>screen.x||y>screen.y)continue;
   double alpha=e.color[3]*(e.fade>0?std::min(1.0,(1-t)/e.fade):1);
   auto c=al_map_rgba_f(float(e.color[0]*alpha),float(e.color[1]*alpha),float(e.color[2]*alpha),float(alpha));quad(x,y,w,h,e,c);
  }
  if(vertices.empty())continue;
  auto at=images.find(e.texture);if(at==images.end())at=images.emplace(e.texture,image_load(e.texture.c_str())).first;
  al_draw_prim(vertices.data(),nullptr,IMAGE2BITMAP(at->second),0,int(vertices.size()),ALLEGRO_PRIM_TRIANGLE_LIST);++stats.batches;stats.vertices+=unsigned(vertices.size());
 }
 al_set_separate_blender(op,src,dst,aop,asrc,adst);al_use_transform(&saved);if(held)al_hold_bitmap_drawing(true);
 stats.submit_ms=(al_get_time()-start)*1000;
}
