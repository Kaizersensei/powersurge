// Original per-voice pre-mix DSP adapter. Signed: Atlas.
#include "spatial_audio.h"
#include "spatial_processor.hpp"
#include "acoustic_records.hpp"
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <cstring>
#include <map>
#include <memory>
extern "C" {
#include "../core/audio.h"
#include "../core/asset.h"
void* bsl_audio_parent(void);
void* bsl_audio_cached_acquire(const char* path);
void* bsl_audio_cached_sample(void* lease);
void bsl_audio_cached_release(void* lease);
}
namespace {
BslCachedVoiceProbe cached_probe{};
struct Voice {
 void* lease=nullptr;
 ALLEGRO_SAMPLE* sample=nullptr;ALLEGRO_SAMPLE_INSTANCE* instance=nullptr;ALLEGRO_MIXER* mixer=nullptr;ALLEGRO_MIXER* parent=nullptr;
 std::unique_ptr<bsl::SpatialProcessor> processor;uint64_t owner=0;double gain=0,rate=1,balance=0,cutoff=20000;bool requested=false,started=false;
 ~Voice(){
  // Allegro detachment synchronizes with the parent's mixer mutex. Callback/userdata remain alive until detached.
  if(mixer)al_detach_mixer(mixer);
  if(instance)al_destroy_sample_instance(instance);
  if(mixer){al_set_mixer_postprocess_callback(mixer,nullptr,nullptr);al_destroy_mixer(mixer);}
  if(lease){bsl_audio_cached_release(lease);cached_probe.released++;cached_probe.active--;}else if(sample)al_destroy_sample(sample);
 }
};
std::map<uint64_t,std::unique_ptr<Voice>> voices;uint64_t next=1;bool world_paused=false;
void filter(void* buffer,unsigned frames,void* data){static_cast<bsl::SpatialProcessor*>(data)->process(static_cast<float*>(buffer),frames);}
}
static uint64_t create_voice(const char* path,uint64_t owner,bool looping,bool cached){
 if(!path||std::strncmp(path,"samples/",8)||std::strstr(path,"..")||std::strchr(path,'\\')||!owner||voices.size()>=32)return 0;
 auto v=std::make_unique<Voice>();v->owner=owner;
 if(cached){v->lease=bsl_audio_cached_acquire(path);if(!v->lease)return 0;cached_probe.acquired++;cached_probe.active++;v->sample=static_cast<ALLEGRO_SAMPLE*>(bsl_audio_cached_sample(v->lease));}
 else {if(!asset_exists(path))return 0;v->sample=al_load_sample(asset_path(path));cached_probe.decoded++;}
 if(!v->sample)return 0;
 auto* sample=v->sample;v->parent=static_cast<ALLEGRO_MIXER*>(bsl_audio_parent());if(!sample||!v->parent)return 0;
 unsigned frequency=al_get_mixer_frequency(v->parent);v->processor=std::make_unique<bsl::SpatialProcessor>(frequency);
 v->mixer=al_create_mixer(frequency,ALLEGRO_AUDIO_DEPTH_FLOAT32,ALLEGRO_CHANNEL_CONF_2);v->instance=al_create_sample_instance(sample);
 if(!v->mixer||!v->instance||!al_set_mixer_playing(v->mixer,false))return 0;
 // PAN_NONE retains authored stereo. Balance is diagonal multiplication in our callback.
 if(!al_set_sample_instance_pan(v->instance,ALLEGRO_AUDIO_PAN_NONE)||!al_set_sample_instance_playmode(v->instance,looping?ALLEGRO_PLAYMODE_LOOP:ALLEGRO_PLAYMODE_ONCE)||!al_attach_sample_instance_to_mixer(v->instance,v->mixer)||!al_set_mixer_postprocess_callback(v->mixer,filter,v->processor.get())||!al_attach_mixer_to_mixer(v->mixer,v->parent))return 0;
 auto id=next++;voices.emplace(id,std::move(v));return id;
}
extern "C" uint64_t bsl_voice_create(const char* path,uint64_t owner,bool looping){return create_voice(path,owner,looping,false);}
extern "C" uint64_t bsl_voice_create_cached(const char* path,uint64_t owner,bool looping){return create_voice(path,owner,looping,true);}
extern "C" void bsl_voice_cached_probe(BslCachedVoiceProbe* out){if(out)*out=cached_probe;}
extern "C" bool bsl_voice_set(uint64_t id,double gain,double rate,double balance,double cutoff,bool playing,double dt){
 auto p=voices.find(id);if(p==voices.end()||!std::isfinite(gain)||!std::isfinite(rate)||!std::isfinite(balance)||!std::isfinite(cutoff)||!std::isfinite(dt)||dt<0||gain<0||gain>1||rate<.1||rate>4||balance<-1||balance>1||cutoff<350||cutoff>20000)return false;
 auto& v=*p->second;v.gain=gain;v.balance=balance;v.cutoff=cutoff;v.rate+=(rate-v.rate)*(1-std::exp(-std::min(dt,.1)/.045));v.processor->set(gain,balance,cutoff);
 if(!al_set_sample_instance_speed(v.instance,float(v.rate)))return false;
 if(!v.started&&playing){if(!al_set_sample_instance_playing(v.instance,true))return false;v.started=true;}
 v.requested=playing;const bool effective=playing&&!world_paused;if(al_get_mixer_playing(v.mixer)!=effective)return al_set_mixer_playing(v.mixer,effective);return true;
}
extern "C" bool bsl_voice_probe(uint64_t id,BslVoiceProbe* out){if(!out)return false;*out={};auto p=voices.find(id);if(p==voices.end())return false;auto& v=*p->second;*out={true,al_get_sample_instance_playing(v.instance)&&al_get_mixer_playing(v.mixer)&&al_get_mixer_playing(v.parent),al_get_sample_instance_position(v.instance),v.gain,v.rate,v.balance,v.cutoff};return true;}
extern "C" double bsl_voice_duration(uint64_t id){auto p=voices.find(id);if(p==voices.end())return 0;auto* sample=p->second->sample;const auto rate=al_get_sample_frequency(sample);return rate?double(al_get_sample_length(sample))/rate:0;}
extern "C" void bsl_voice_destroy(uint64_t id){voices.erase(id);}
extern "C" void bsl_voice_cancel_owner(uint64_t owner){for(auto i=voices.begin();i!=voices.end();)if(i->second->owner==owner)i=voices.erase(i);else ++i;}
// Main-thread pause propagation still runs when the script VM is suspended. Signed: Mech.
extern "C" void bsl_voice_pause(bool pause){world_paused=pause;for(auto& item:voices){auto& v=*item.second;const bool effective=v.requested&&!pause;if(al_get_mixer_playing(v.mixer)!=effective)al_set_mixer_playing(v.mixer,effective);}}
extern "C" bool bsl_voice_paused(){return world_paused;}
extern "C" unsigned bsl_voice_snapshot(BslOwnedVoiceSnapshot* out,unsigned capacity){if(!out)return 0;unsigned count=0;for(const auto& item:voices){if(count>=capacity)break;auto& row=out[count++];row={};row.id=item.first;row.owner=item.second->owner;bsl_voice_probe(item.first,&row.voice);}return count;}
extern "C" void bsl_voice_release(){voices.clear();world_paused=false;}

namespace { bsl::MasterLimiter master_limiter; }
extern "C" void bsl_master_prepare(unsigned frequency){master_limiter.prepare(frequency,compiled_limiter_ceiling(),compiled_limiter_release());}
extern "C" void bsl_master_limit(void* buffer,unsigned frames){master_limiter.process(static_cast<float*>(buffer),frames);}
extern "C" void bsl_master_probe(float* reduction,float* peak){if(reduction)*reduction=master_limiter.reduction();if(peak)*peak=master_limiter.peak();}
