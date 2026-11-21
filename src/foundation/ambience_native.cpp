// Owned weather ambience, routed through the engine SFX/master mixer. Signed: Atlas.
#include "ambience.hpp"
#include "ambient_records.hpp"
#include "foundation_host.h"
#include <vector>
extern "C" {
#include "../core/audio.h"
}
namespace {
bsl::AmbientMix& mix(){static bsl::AmbientMix m(compiled_ambience());return m;}
struct Voice {sound_t* sound=nullptr;bsl_audio_loop_t* loop=nullptr;uint64_t id=0;bool failed=false;};
std::vector<Voice>& voices(){static std::vector<Voice> v(mix().definition().layers.size());return v;}
uint64_t next_voice=1;unsigned starts=0,stops=0,failures=0;bool paused=false;
void stop(Voice& v){if(v.loop){bsl_audio_loop_destroy(v.loop);v.loop=nullptr;v.id=0;++stops;}}
}
extern "C" void bsl_ambient_clear(){for(auto& v:voices()){stop(v);v.failed=false;}mix().clear();paused=false;}
extern "C" void bsl_ambient_release(){bsl_ambient_clear();for(auto& v:voices())if(v.sound){sound_unref(v.sound);v.sound=nullptr;}}
extern "C" void bsl_ambient_tick(double dt,bool pause){
 BslWeatherProbe w{};bsl_host_weather_probe(&w);paused=pause;
 mix().update(dt,pause,!w.enabled||w.indoor,w.wind,w.kind,w.precipitation);
 for(size_t i=0;i<voices().size();++i){auto& v=voices()[i];const double gain=mix().gains()[i];
  if(gain<=1e-9){stop(v);continue;}
  if(!v.loop&&!pause&&!v.failed){
   if(!v.sound)v.sound=sound_load(mix().definition().layers[i].path.c_str());
   v.loop=bsl_audio_loop_create(v.sound);
   if(v.loop){v.id=next_voice++;++starts;}else{v.failed=true;++failures;}
  }
  if(v.loop&&!bsl_audio_loop_set(v.loop,float(gain),!pause)){stop(v);v.failed=true;++failures;}
 }
}
extern "C" void bsl_host_ambient_probe(BslAmbientProbe* out){
 if(!out)return;
 *out={};out->paused=paused;out->starts=starts;out->stops=stops;out->failures=failures;
 for(const auto& v:voices())if(v.loop){unsigned position=0;float gain=0;bool playing=bsl_audio_loop_probe(v.loop,&position,&gain);++out->voices;if(playing)++out->playing;out->gain+=gain;
  if(!out->first_voice){out->first_voice=v.id;out->position=position;}
 }
}
