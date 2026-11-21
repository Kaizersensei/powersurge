// Original cosmetic particle model. No gameplay effects, clocks or gameplay RNG. Signed: Atlas.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace bsl {
struct ParticleEmitter {
 std::string texture;int sx=0,sy=0,sw=1,sh=1;
 double rate=1,life_min=1,life_max=1,vx_min=0,vx_max=0,vy_min=0,vy_max=0,ax=0,ay=0,wind=0,width=1,height=1,end_scale=1,fade=.2;
 unsigned burst=0;
 std::array<double,4> color{{1,1,1,1}};
};
struct ParticleEffect {std::string id;unsigned capacity=1;std::vector<ParticleEmitter> emitters;};
struct ParticleCatalog {unsigned capacity=8192;std::vector<ParticleEffect> effects;std::array<std::string,3> weather;};
class ParticleRegistry {
 ParticleCatalog data_;
 static bool num(double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
public:
 explicit ParticleRegistry(ParticleCatalog d):data_(std::move(d)){
  if(!data_.capacity||data_.capacity>32768||data_.effects.empty()||data_.effects.size()>64)throw std::invalid_argument("invalid particle catalog");
  std::vector<std::string> ids;
  for(const auto& f:data_.effects){
   if(f.id.rfind("effect:",0)||f.id.size()>128||f.id.size()<8||f.id.find('\0')!=std::string::npos||std::find(ids.begin(),ids.end(),f.id)!=ids.end()||!f.capacity||f.capacity>data_.capacity||f.emitters.empty()||f.emitters.size()>8)throw std::invalid_argument("invalid particle effect");
   ids.push_back(f.id);
   for(const auto& e:f.emitters){
    if(e.texture.rfind("images/",0)||e.texture.size()>256||e.texture.find("..")!=std::string::npos||e.texture.find_first_of("\\:\n\r")!=std::string::npos||e.texture.find('\0')!=std::string::npos||e.sx<0||e.sy<0||e.sw<1||e.sh<1||e.sx>4096-e.sw||e.sy>4096-e.sh)throw std::invalid_argument("invalid particle texture");
    if(e.burst>8192||!num(e.rate,0,2000)||!num(e.life_min,.05,10)||!num(e.life_max,e.life_min,10)||!num(e.vx_min,-4000,4000)||!num(e.vx_max,e.vx_min,4000)||!num(e.vy_min,-4000,4000)||!num(e.vy_max,e.vy_min,4000)||!num(e.ax,-4000,4000)||!num(e.ay,-4000,4000)||!num(e.wind,-8,8)||!num(e.width,.1,1024)||!num(e.height,.1,1024)||!num(e.end_scale,0,4)||!num(e.fade,0,1))throw std::invalid_argument("invalid particle emitter");
    for(double v:e.color)if(!num(v,0,1))throw std::invalid_argument("invalid particle color");
   }
  }
  for(const auto& id:data_.weather)if(!find(id))throw std::invalid_argument("missing particle weather effect");
 }
 const ParticleEffect* find(const std::string& id)const noexcept {for(const auto& e:data_.effects)if(e.id==id)return &e;return nullptr;}
 const ParticleCatalog& catalog()const noexcept{return data_;}
};
struct ParticleSpawn {uint64_t owner=0,seed=1;double x=0,y=0,width=0,height=0,multiplier=1;bool looping=true;};
struct Particle {double x=0,y=0,vx=0,vy=0,age=0,life=1;unsigned emitter=0;};
struct ParticleInstance {uint64_t handle=0,owner=0,random=1;const ParticleEffect* definition=nullptr;ParticleSpawn spawn;bool emitting=true;std::array<double,8> credit{};std::vector<Particle> particles;};
class ParticleWorld {
 const ParticleRegistry& registry_;std::vector<ParticleInstance> instances_;uint64_t next_=1;uint64_t dropped_=0;
 static double random(ParticleInstance& i){i.random^=i.random<<13;i.random^=i.random>>7;i.random^=i.random<<17;return double(i.random>>11)*(1.0/9007199254740992.0);}
 static double range(ParticleInstance& i,double lo,double hi){return lo+(hi-lo)*random(i);}
 static bool valid(const ParticleSpawn& p){return p.owner&&std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1e9&&std::abs(p.y)<=1e9&&std::isfinite(p.width)&&p.width>=0&&p.width<=32768&&std::isfinite(p.height)&&p.height>=0&&p.height<=32768&&std::isfinite(p.multiplier)&&p.multiplier>=0&&p.multiplier<=10;}
public:
 explicit ParticleWorld(const ParticleRegistry& r):registry_(r){instances_.reserve(32);}
 uint64_t start(const std::string& id,const ParticleSpawn& p){
  const auto* f=registry_.find(id);if(!f||!valid(p)||instances_.size()>=32||next_==0)return 0;
  ParticleInstance i;i.handle=next_++;i.owner=p.owner;i.random=p.seed?p.seed:1;i.definition=f;i.spawn=p;i.particles.reserve(f->capacity);instances_.push_back(std::move(i));return instances_.back().handle;
 }
 bool configure(uint64_t handle,const ParticleSpawn& p){if(!valid(p))return false;for(auto& i:instances_)if(i.handle==handle&&i.owner==p.owner){i.spawn=p;return true;}return false;}
 bool stop(uint64_t handle,bool cancel){for(size_t n=0;n<instances_.size();++n)if(instances_[n].handle==handle){if(cancel){instances_[n]=std::move(instances_.back());instances_.pop_back();}else instances_[n].emitting=false;return true;}return false;}
 void cancel_owner(uint64_t owner){for(size_t n=0;n<instances_.size();)if(instances_[n].owner==owner){instances_[n]=std::move(instances_.back());instances_.pop_back();}else ++n;}
 void clear(){instances_.clear();dropped_=0;}
 void update(double dt,double wx=0,double wy=0){
  if(!std::isfinite(dt)||dt<0||!std::isfinite(wx)||!std::isfinite(wy)||std::abs(wx)>10000||std::abs(wy)>10000)throw std::invalid_argument("invalid particle tick");
  double remaining=std::min(dt,.25);
  while(remaining>1e-9){const double step=std::min(remaining,1.0/60);remaining-=step;
   for(auto& i:instances_){
    for(size_t n=0;n<i.particles.size();){auto& p=i.particles[n];const auto& e=i.definition->emitters[p.emitter];p.age+=step;
     if(p.age>=p.life){p=i.particles.back();i.particles.pop_back();continue;}
     p.vx+=e.ax*step;p.vy+=e.ay*step;p.x+=(p.vx+wx*e.wind)*step;p.y+=(p.vy+wy*e.wind)*step;++n;
    }
   }
   size_t total=count();
   for(auto& i:instances_)if(i.emitting){
    for(size_t e=0;e<i.definition->emitters.size();++e){const auto& d=i.definition->emitters[e];i.credit[e]+=d.rate*i.spawn.multiplier*step;const unsigned earned=unsigned(i.credit[e]);i.credit[e]-=earned;const unsigned wanted=i.spawn.looping?earned:d.burst;
     unsigned admitted=unsigned(std::min<size_t>(wanted,std::min<size_t>(i.definition->capacity-i.particles.size(),registry_.catalog().capacity-total)));dropped_+=wanted-admitted;
     while(admitted--){Particle p;p.emitter=unsigned(e);p.x=range(i,i.spawn.x,i.spawn.x+i.spawn.width);p.y=range(i,i.spawn.y,i.spawn.y+i.spawn.height);p.vx=range(i,d.vx_min,d.vx_max);p.vy=range(i,d.vy_min,d.vy_max);p.life=range(i,d.life_min,d.life_max);i.particles.push_back(p);++total;}
    }
    if(!i.spawn.looping)i.emitting=false;
   }
   for(size_t n=0;n<instances_.size();)if(!instances_[n].emitting&&instances_[n].particles.empty()){instances_[n]=std::move(instances_.back());instances_.pop_back();}else ++n;
  }
 }
 size_t count()const noexcept{size_t n=0;for(const auto& i:instances_)n+=i.particles.size();return n;}
 uint64_t dropped()const noexcept{return dropped_;}
 const std::vector<ParticleInstance>& instances()const noexcept{return instances_;}
};
}
