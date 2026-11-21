// Original compatibility adapter. SoLoud handles voices/mixing; miniaudio owns device output.
// Existing Atlas DSP is reused; no new filtering/limiting algorithm. Signed: Mech.
#include "soloud_backend.h"
#include "spatial_processor.hpp"
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
namespace {
constexpr unsigned voice_limit=32;
constexpr unsigned master_slot=FILTERS_PER_STREAM-1;
constexpr size_t encoded_limit=16*1024*1024;
constexpr uint64_t decoded_limit=64*1024*1024;
BslCachedVoiceProbe counters{};
uint64_t next_voice=1;
// SoLoud buffers are planar with an explicit stride. Bounded stack conversion preserves stereo.
template<class Processor> void planar(Processor& processor,float* data,unsigned frames,unsigned stride,unsigned channels){
 if(channels!=2)return;
 float block[128];
 for(unsigned offset=0;offset<frames;offset+=64){const unsigned count=std::min(64u,frames-offset);
  for(unsigned i=0;i<count;++i){block[2*i]=data[offset+i];block[2*i+1]=data[stride+offset+i];}
  processor.process(block,count);
  for(unsigned i=0;i<count;++i){data[offset+i]=block[2*i];data[stride+offset+i]=block[2*i+1];}
 }
}
class SpatialInstance final:public SoLoud::FilterInstance {
 bsl::SpatialProcessor processor;double gain=0,balance=0,cutoff=20000;
public:
 explicit SpatialInstance(unsigned rate):processor(rate){}
 void setFilterParameter(unsigned id,float value)override{if(id==1)gain=value;else if(id==2)balance=value;else if(id==3)cutoff=value;processor.set(gain,balance,cutoff);}
 void filter(float* data,unsigned frames,unsigned stride,unsigned channels,float,SoLoud::time)override{planar(processor,data,frames,stride,channels);}
};
class SpatialFilter final:public SoLoud::Filter {
 unsigned rate;
public:
 explicit SpatialFilter(unsigned frequency):rate(frequency){}
 SoLoud::FilterInstance* createInstance()override{return new SpatialInstance(rate);}
};
struct Levels {std::atomic<float> rms[2]{},peak[2]{};std::atomic<uint64_t> frames{0};Levels(){for(unsigned c=0;c<2;c++){rms[c].store(0);peak[c].store(0);}}};
class MasterInstance final:public SoLoud::FilterInstance {
 bsl::MasterLimiter& processor;std::atomic<float>& gain;Levels& levels;
public:
 MasterInstance(bsl::MasterLimiter& limiter,std::atomic<float>& level,Levels& meter):processor(limiter),gain(level),levels(meter){}
 void filter(float* data,unsigned frames,unsigned stride,unsigned channels,float,SoLoud::time)override{const float level=gain.load(std::memory_order_relaxed);for(unsigned c=0;c<channels;c++)for(unsigned i=0;i<frames;i++)data[c*stride+i]*=level;planar(processor,data,frames,stride,channels);
  for(unsigned c=0;c<std::min(channels,2u);c++){double sum=0;float peak=0;for(unsigned i=0;i<frames;i++){const float x=data[c*stride+i];sum+=double(x)*x;peak=std::max(peak,std::abs(x));}levels.rms[c].store(frames?float(std::sqrt(sum/frames)):0,std::memory_order_relaxed);levels.peak[c].store(peak,std::memory_order_relaxed);}levels.frames.fetch_add(frames,std::memory_order_relaxed);
 }
};
class MasterFilter final:public SoLoud::Filter {
public:
 bsl::MasterLimiter limiter;std::atomic<float> gain{1};Levels levels;
 SoLoud::FilterInstance* createInstance()override{return new MasterInstance(limiter,gain,levels);}
};
struct Sample {
 SoLoud::Wav wave;std::unique_ptr<SoLoud::WavStream> stream;uint64_t bytes=0;
 SoLoud::AudioSource& source(){return stream?static_cast<SoLoud::AudioSource&>(*stream):wave;}
 double duration(){return stream?stream->getLength():wave.getLength();}
 unsigned frames(){return stream?stream->mSampleCount:wave.mSampleCount;}
};
struct Voice {
 SoLoud::Soloud& engine;std::shared_ptr<Sample> sample;SpatialFilter filter;SoLoud::Bus bus;
 SoLoud::handle source=0,output=0,group=0;uint64_t owner=0;double gain=0,rate=1,balance=0,cutoff=20000;bool requested=false,looping=false;unsigned scope=0;bool monitored=true;
 Voice(SoLoud::Soloud& core,std::shared_ptr<Sample> data,unsigned frequency):engine(core),sample(std::move(data)),filter(frequency){}
 ~Voice(){if(source)engine.stop(source);if(output)engine.stop(output);if(group)engine.destroyVoiceGroup(group);if(scope==BSL_AUDIO_OWNED){counters.released++;counters.active--;}}
};
struct Context {
 SoLoud::Soloud engine;MasterFilter master;BslAudioAssets assets{};bool offline=false,paused=false;unsigned rate=44100,bank=BSL_AUDIO_BANK_A,monitor_mask=15;double sound_gain=1,music_gain=1,sound_cutoff=20000,music_cutoff=20000;bool suspended=false;
 std::map<std::string,std::shared_ptr<Sample>> samples;
 std::map<uint64_t,std::unique_ptr<Voice>> voices;
 ~Context(){voices.clear();samples.clear();engine.setGlobalFilter(master_slot,nullptr);engine.deinit();}
};
std::unique_ptr<Context> context;
bool valid_path(const char* path){return path&&(!std::strncmp(path,"samples/",8)||!std::strncmp(path,"musics/",7))&&std::strlen(path)<1024&&!std::strstr(path,"..")&&!std::strchr(path,'\\')&&!std::strchr(path,':');}
uint64_t resident_bytes(){if(!context)return 0;std::set<const Sample*> seen;uint64_t sum=0;for(const auto& p:context->samples)if(seen.insert(p.second.get()).second)sum+=p.second->bytes;for(const auto& p:context->voices)if(seen.insert(p.second->sample.get()).second)sum+=p.second->sample->bytes;return sum;}
unsigned owned_count(){if(!context)return 0;unsigned count=0;for(const auto& p:context->voices)if(p.second->scope==BSL_AUDIO_OWNED)count++;return count;}
void apply(Voice& voice){
 auto& core=context->engine;const bool music=voice.scope==BSL_AUDIO_MUSIC;
 const bool audible=(context->monitor_mask&(1u<<voice.scope))!=0;if(audible!=voice.monitored){voice.monitored=audible;core.fadeVolume(voice.output,audible?1.f:0.f,.03);}
 const double category=music?context->music_gain:context->sound_gain;
 const double cutoff=std::min(voice.cutoff,music?context->music_cutoff:context->sound_cutoff);
 core.setVolume(voice.source,float(std::max(1.0,voice.gain)));
 core.setFilterParameter(voice.output,0,1,float(std::min(1.0,voice.gain)*category));core.setFilterParameter(voice.output,0,2,float(voice.balance));core.setFilterParameter(voice.output,0,3,float(cutoff));
 const bool held=context->suspended||(voice.scope==BSL_AUDIO_OWNED?context->paused:voice.scope!=BSL_AUDIO_MUSIC&&voice.scope!=bsl_soloud_current_bank());
 core.setPause(voice.group,!voice.requested||held);
}
bool preload_source(const char* path,bool streaming);
std::string source_key(const char* path,bool stream){return stream?std::string("stream:")+path:path;}
uint64_t create(const char* path,uint64_t owner,bool looping,bool cached,unsigned scope=BSL_AUDIO_OWNED,bool streaming=false){
 if(!context||!valid_path(path)||!owner||scope>BSL_AUDIO_MUSIC||context->voices.size()>=120||(scope==BSL_AUDIO_OWNED&&owned_count()>=voice_limit))return 0;
 const auto key=source_key(path,streaming);auto found=context->samples.find(key);
 if(found==context->samples.end()){if(cached||!preload_source(path,streaming))return 0;found=context->samples.find(key);}
 auto voice=std::make_unique<Voice>(context->engine,found->second,context->rate);voice->scope=scope;if(scope==BSL_AUDIO_OWNED){counters.acquired++;counters.active++;}
 auto& core=context->engine;voice->owner=owner;voice->looping=looping;voice->scope=scope;voice->bus.mBaseSamplerate=float(context->rate);voice->bus.setChannels(2);voice->bus.setFilter(0,&voice->filter);
 voice->output=core.playBackground(voice->bus,1,true);if(!core.isValidVoiceHandle(voice->output))return 0;
 voice->monitored=(context->monitor_mask&(1u<<scope))!=0;core.setVolume(voice->output,voice->monitored?1.f:0.f);core.setProtectVoice(voice->output,true);core.setInaudibleBehavior(voice->output,true,false);
 voice->source=voice->bus.play(voice->sample->source(),1,0,true);if(!core.isValidVoiceHandle(voice->source))return 0;
 core.setPanAbsolute(voice->source,1,1);
 core.setLooping(voice->source,looping);core.setProtectVoice(voice->source,true);core.setInaudibleBehavior(voice->source,true,false);
 voice->group=core.createVoiceGroup();if(!voice->group||core.addVoiceToGroup(voice->group,voice->source)!=SoLoud::SO_NO_ERROR||core.addVoiceToGroup(voice->group,voice->output)!=SoLoud::SO_NO_ERROR)return 0;
 const auto id=next_voice++;context->voices.emplace(id,std::move(voice));return id;
}
}
extern "C" bool bsl_soloud_init(BslAudioAssets assets,unsigned rate,bool offline,double ceiling,double release){
 if(context||!assets.read||!assets.release||rate<22050||rate>96000||!std::isfinite(ceiling)||ceiling<.1||ceiling>1||!std::isfinite(release)||release<.01||release>2)return false;
 auto next=std::make_unique<Context>();next->assets=assets;next->offline=offline;
 if(next->engine.init(0,offline?SoLoud::Soloud::NULLDRIVER:SoLoud::Soloud::MINIAUDIO,rate,512,2)!=SoLoud::SO_NO_ERROR)return false;
 next->rate=next->engine.getBackendSamplerate();next->engine.setMaxActiveVoiceCount(240);next->engine.setPostClipScaler(1);next->master.limiter.prepare(next->rate,ceiling,release);next->engine.setGlobalFilter(master_slot,&next->master);
 // Engine-global volume stays1; future volume controls belong before the final limiter.
 context=std::move(next);return true;
}
extern "C" void bsl_soloud_shutdown(){context.reset();}
namespace {
bool preload_source(const char* path,bool streaming){
 if(!context||!valid_path(path))return false;
 const auto key=source_key(path,streaming);if(context->samples.count(key))return true;
 if(context->samples.size()>=256)return false;
 size_t size=0;const auto* bytes=context->assets.read(context->assets.context,path,&size);if(!bytes)return false;
 if(!size||size>(streaming?128*1024*1024:encoded_limit)){context->assets.release(context->assets.context,bytes);return false;}
 auto data=std::make_shared<Sample>();counters.decoded++;SoLoud::result result;
 if(streaming){data->stream=std::make_unique<SoLoud::WavStream>();result=data->stream->loadMem(bytes,unsigned(size),true,false);data->bytes=size;}
 else {result=data->wave.loadMem(bytes,unsigned(size),false,false);data->bytes=uint64_t(data->wave.mSampleCount)*data->wave.mChannels*sizeof(float);}
 context->assets.release(context->assets.context,bytes);
 if(result!=SoLoud::SO_NO_ERROR||data->source().mChannels<1||data->source().mChannels>2||!data->frames())return false;
 if(data->bytes>decoded_limit||resident_bytes()>decoded_limit-data->bytes)return false;
 context->samples.emplace(key,std::move(data));return true;
}
}
extern "C" bool bsl_soloud_preload(const char* path){return preload_source(path,false);}
extern "C" bool bsl_soloud_stream_preload(const char* path){return preload_source(path,true);}
extern "C" bool bsl_soloud_unload(const char* path){return context&&valid_path(path)&&context->samples.erase(source_key(path,false))>0;}
extern "C" bool bsl_soloud_stream_unload(const char* path){return context&&valid_path(path)&&context->samples.erase(source_key(path,true))>0;}
extern "C" double bsl_soloud_source_duration(const char* path,bool streaming){if(!context||!valid_path(path))return 0;auto p=context->samples.find(source_key(path,streaming));return p==context->samples.end()?0:p->second->duration();}
extern "C" uint64_t bsl_soloud_play_scoped(const char* path,uint64_t owner,bool looping,unsigned scope,bool streaming){return create(path,owner,looping,true,scope,streaming);}
extern "C" bool bsl_soloud_control(uint64_t id,double gain,double rate,double balance,bool playing){
 if(!context||!std::isfinite(gain)||!std::isfinite(rate)||!std::isfinite(balance)||gain<0||rate<1.0/64||rate>64||balance<-1||balance>1)return false;
 auto p=context->voices.find(id);if(p==context->voices.end())return false;auto& voice=*p->second;if(!context->engine.isValidVoiceHandle(voice.source))return false;
 voice.gain=gain;voice.rate=rate;voice.balance=balance;voice.requested=playing;if(context->engine.setRelativePlaySpeed(voice.source,float(rate))!=SoLoud::SO_NO_ERROR)return false;apply(voice);return true;
}
extern "C" bool bsl_soloud_repeat_from(uint64_t id,double seconds){if(!context||!std::isfinite(seconds)||seconds<0)return false;auto p=context->voices.find(id);if(p==context->voices.end()||seconds>=p->second->sample->duration())return false;context->engine.setLoopPoint(p->second->source,seconds);return true;}
extern "C" unsigned bsl_soloud_current_bank(){return !context?0:context->paused?unsigned(BSL_AUDIO_BANK_B):context->bank;}
extern "C" void bsl_soloud_active_bank(unsigned bank){if(!context||(bank!=BSL_AUDIO_BANK_A&&bank!=BSL_AUDIO_BANK_B))return;context->bank=bank;for(auto& p:context->voices)apply(*p.second);}
extern "C" void bsl_soloud_mix(double master,double sound,double music,bool muted){if(!context||!std::isfinite(master)||!std::isfinite(sound)||!std::isfinite(music))return;context->master.gain.store(muted?0:float(std::clamp(master,0.0,1.0)),std::memory_order_relaxed);context->sound_gain=std::clamp(sound,0.0,1.0);context->music_gain=std::clamp(music,0.0,1.0);for(auto& p:context->voices)apply(*p.second);}
extern "C" void bsl_soloud_muffle(double sound,double music){if(!context||!std::isfinite(sound)||!std::isfinite(music))return;context->sound_cutoff=std::clamp(sound,350.0,20000.0);context->music_cutoff=std::clamp(music,350.0,20000.0);for(auto& p:context->voices)apply(*p.second);}
extern "C" void bsl_soloud_suspend(bool suspended){if(!context)return;context->suspended=suspended;for(auto& p:context->voices)apply(*p.second);}
extern "C" bool bsl_soloud_monitor(unsigned mask){if(!context||mask>15)return false;context->monitor_mask=mask;for(auto& p:context->voices)apply(*p.second);return true;}
extern "C" unsigned bsl_soloud_monitor_mask(){return context?context->monitor_mask:0;}
extern "C" void bsl_soloud_collect(){if(!context)return;for(auto p=context->voices.begin();p!=context->voices.end();)if(p->second->scope!=BSL_AUDIO_OWNED&&!context->engine.isValidVoiceHandle(p->second->source))p=context->voices.erase(p);else ++p;}
extern "C" bool bsl_soloud_info(BslSoloudInfo* out){if(!out)return false;*out={};if(!context)return false;*out={context->rate,context->engine.getBackendBufferSize(),unsigned(context->samples.size()),unsigned(context->voices.size()),resident_bytes(),context->offline,context->engine.getBackendString()};return true;}
extern "C" bool bsl_soloud_levels(BslAudioLevels* out){if(!out)return false;*out={};if(!context)return false;auto& m=context->master.levels;*out={m.rms[0].load(std::memory_order_relaxed),m.rms[1].load(std::memory_order_relaxed),m.peak[0].load(std::memory_order_relaxed),m.peak[1].load(std::memory_order_relaxed),m.frames.load(std::memory_order_relaxed)};return true;}
extern "C" bool bsl_soloud_source_info(const char* path,bool streaming,BslAudioSourceInfo* out){if(!out)return false;*out={};if(!context||!valid_path(path))return false;auto p=context->samples.find(source_key(path,streaming));if(p==context->samples.end())return false;*out={p->second->source().mChannels,unsigned(p->second->source().mBaseSamplerate),p->second->duration()};return true;}
extern "C" bool bsl_soloud_render(float* data,unsigned frames){if(!context||!context->offline||!data||!frames||frames>44100*30)return false;for(unsigned offset=0;offset<frames;offset+=512)context->engine.mix(data+offset*2,std::min(512u,frames-offset));return true;}
extern "C" uint64_t bsl_voice_create(const char* path,uint64_t owner,bool looping){return create(path,owner,looping,false);}
extern "C" uint64_t bsl_voice_create_cached(const char* path,uint64_t owner,bool looping){return create(path,owner,looping,true);}
extern "C" bool bsl_voice_set(uint64_t id,double gain,double rate,double balance,double cutoff,bool playing,double dt){
 if(!context||!std::isfinite(gain)||!std::isfinite(rate)||!std::isfinite(balance)||!std::isfinite(cutoff)||!std::isfinite(dt)||gain<0||gain>1||rate<.1||rate>4||balance<-1||balance>1||cutoff<350||cutoff>20000||dt<0)return false;
 auto p=context->voices.find(id);if(p==context->voices.end())return false;auto& voice=*p->second;auto& core=context->engine;if(!core.isValidVoiceHandle(voice.source))return false;
 voice.gain=gain;voice.balance=balance;voice.cutoff=cutoff;voice.rate+=(rate-voice.rate)*(1-std::exp(-std::min(dt,.1)/.045));voice.requested=playing;
 
 if(core.setRelativePlaySpeed(voice.source,float(voice.rate))!=SoLoud::SO_NO_ERROR)return false;
 apply(voice);return true;
}
extern "C" bool bsl_voice_probe(uint64_t id,BslVoiceProbe* out){if(!out)return false;*out={};if(!context)return false;auto p=context->voices.find(id);if(p==context->voices.end())return false;auto& v=*p->second;auto& core=context->engine;double position=core.getStreamPosition(v.source)*v.sample->source().mBaseSamplerate;const auto frames=v.sample->frames();if(v.looping&&frames)position=std::fmod(position,frames);*out={true,core.isValidVoiceHandle(v.source)&&!core.getPause(v.source),unsigned(std::max(0.0,position)),v.gain,v.rate,v.balance,v.cutoff};return true;}
extern "C" double bsl_voice_duration(uint64_t id){if(!context)return 0;auto p=context->voices.find(id);return p==context->voices.end()?0:p->second->sample->duration();}
extern "C" void bsl_voice_destroy(uint64_t id){if(context)context->voices.erase(id);}
extern "C" void bsl_voice_cancel_owner(uint64_t owner){if(!context)return;for(auto p=context->voices.begin();p!=context->voices.end();)if(p->second->scope==BSL_AUDIO_OWNED&&p->second->owner==owner)p=context->voices.erase(p);else ++p;}
extern "C" void bsl_voice_release(){if(context){for(auto p=context->voices.begin();p!=context->voices.end();)if(p->second->scope==BSL_AUDIO_OWNED)p=context->voices.erase(p);else ++p;context->paused=false;for(auto& p:context->voices)apply(*p.second);}}
extern "C" void bsl_voice_pause(bool paused){if(!context)return;context->paused=paused;for(const auto& p:context->voices)apply(*p.second);}
extern "C" bool bsl_voice_paused(){return context&&context->paused;}
extern "C" unsigned bsl_voice_snapshot(BslOwnedVoiceSnapshot* out,unsigned capacity){if(!context||!out)return 0;unsigned count=0;for(const auto& p:context->voices){if(p.second->scope!=BSL_AUDIO_OWNED)continue;if(count>=capacity)break;auto& row=out[count++];row={};row.id=p.first;row.owner=p.second->owner;bsl_voice_probe(row.id,&row.voice);}return count;}
extern "C" void bsl_voice_cached_probe(BslCachedVoiceProbe* out){if(out)*out=counters;}
extern "C" void bsl_master_probe(float* reduction,float* peak){if(reduction)*reduction=context?context->master.limiter.reduction():0;if(peak)*peak=context?context->master.limiter.peak():0;}
