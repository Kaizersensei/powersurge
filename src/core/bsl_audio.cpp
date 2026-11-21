// Native Surge audio facade on the selected SoLoud/miniaudio host. Signed: Mech.
// Uses the existing public audio.h contract; graphics/input remain Allegro-owned.
#include "soloud_backend.h"
#include "acoustic_records.hpp"
extern "C" {
#include "audio.h"
#include "asset.h"
#include "engine.h"
#include "resourcemanager.h"
#include "logfile.h"
#include "../util/util.h"
}
#undef min
#undef max
// Production has no Allegro audio dependency; audited DLL closure. Signed: Mech.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <new>
#include <iomanip>
#include <map>
#include <string>
struct sound_t {std::string path;};
struct music_t {std::string path;uint64_t voice=0;double volume=1,repeat=0;bool paused=false,loop=true;};
struct bsl_audio_loop_t {uint64_t voice=0;};
namespace {
struct Slot {uint64_t voice=0;};
Slot pool[32];
std::map<uint64_t,bsl_audio_loop_t*> loops;
music_t* current=nullptr;
float master=1,percentage=.5f;bool muted=false;
unsigned bank=BSL_AUDIO_BANK_A,loaded=0;uint64_t serial=1,last_legacy=0,legacy_starts=0,stream_starts=0;
mufflerprofile_t profile=MUFFLER_MEDIUM;int muffle_flags=0;
BslVoiceProbe view(uint64_t id){BslVoiceProbe out{};bsl_voice_probe(id,&out);return out;}
const unsigned char* read_asset(void*,const char* path,size_t* size){
 // Read the engine VFS, independent of the process working directory. Signed: President.
 if(!size||!asset_exists(path))return nullptr;
 ALLEGRO_FILE* file=al_fopen(asset_path(path),"rb");if(!file)return nullptr;
 const auto count=al_fsize(file);
 if(count<=0||count>128*1024*1024){al_fclose(file);return nullptr;}
 auto* bytes=new(std::nothrow) unsigned char[size_t(count)];
 if(!bytes){al_fclose(file);return nullptr;}
 const auto read=al_fread(file,bytes,size_t(count));const bool closed=al_fclose(file);
 if(read!=size_t(count)||!closed){delete[] bytes;return nullptr;}
 *size=size_t(count);return bytes;
}
void release_asset(void*,const unsigned char* bytes){delete[] bytes;}
void mix(){const double alpha=2*(percentage-.5);bsl_soloud_mix(master,1-std::max(0.0,alpha),1+std::min(0.0,alpha),muted);}
void muffler(){const double cutoff=profile==MUFFLER_LOW?6000:profile==MUFFLER_MEDIUM?2500:profile==MUFFLER_HIGH?1200:20000;bsl_soloud_muffle(muffle_flags&MUFFLE_SOUNDS?cutoff:20000,muffle_flags&MUFFLE_MUSICS?cutoff:20000);}
void halt_resume(const ALLEGRO_EVENT* event,void*){bsl_soloud_suspend(event->type==ALLEGRO_EVENT_DISPLAY_HALT_DRAWING);}
int preload(const char* path,void*){sound_load(path);return 0;}
void write_voice(std::ostream& out,uint64_t id){const auto p=view(id);out<<"{\"id\":"<<id<<",\"present\":"<<p.present<<",\"playing\":"<<p.playing<<",\"position\":"<<p.position<<",\"gain\":"<<p.gain<<",\"rate\":"<<p.rate<<"}";}
void trace(){
 const char* path=std::getenv("BSL_SELECTED_AUDIO_TRACE");if(!path||!*path)return;
 BslSoloudInfo info{};bsl_soloud_info(&info);std::ofstream out(path);if(!out)return;
 out<<std::setprecision(12)<<std::boolalpha<<"{\"backend\":\""<<(info.backend?info.backend:"")<<"\",\"allegro\":"<<false<<",\"sample_rate\":"<<info.sample_rate<<",\"buffer_frames\":"<<info.buffer_frames<<",\"voices\":"<<info.voices<<",\"loaded_samples\":"<<loaded<<",\"legacy_starts\":"<<legacy_starts<<",\"stream_starts\":"<<stream_starts<<",\"bank\":"<<bsl_soloud_current_bank()<<",\"master\":"<<master<<",\"muted\":"<<muted<<",\"percentage\":"<<percentage<<",\"world_paused\":"<<bsl_voice_paused()<<",\"music\":";write_voice(out,current?current->voice:0);out<<",\"last_legacy\":";write_voice(out,last_legacy);out<<",\"loops\":[";bool comma=false;for(const auto& entry:loops){if(comma)out<<',';comma=true;write_voice(out,entry.first);}out<<"]}\n";
}
}
extern "C" void audio_init(){
 logfile_message("audio_init(): selected SoLoud/miniaudio; Allegro audio remains uninstalled");
 if(!bsl_soloud_init({nullptr,read_asset,release_asset},44100,false,compiled_limiter_ceiling(),compiled_limiter_release()))fatal_error("Can't start selected SoLoud/miniaudio audio output");
 bank=BSL_AUDIO_BANK_A;master=1;percentage=.5f;muted=false;loaded=0;legacy_starts=stream_starts=0;last_legacy=0;profile=MUFFLER_MEDIUM;muffle_flags=0;for(auto& slot:pool)slot.voice=0;mix();muffler();
 engine_add_event_listener(ALLEGRO_EVENT_DISPLAY_HALT_DRAWING,nullptr,halt_resume);engine_add_event_listener(ALLEGRO_EVENT_DISPLAY_RESUME_DRAWING,nullptr,halt_resume);
}
extern "C" void audio_release(){engine_remove_event_listener(ALLEGRO_EVENT_DISPLAY_RESUME_DRAWING,nullptr,halt_resume);engine_remove_event_listener(ALLEGRO_EVENT_DISPLAY_HALT_DRAWING,nullptr,halt_resume);music_stop();for(auto& entry:loops)delete entry.second;loops.clear();bsl_soloud_shutdown();logfile_message("audio_release(): selected output closed");}
extern "C" void audio_update(){bsl_soloud_collect();for(auto& slot:pool)if(slot.voice&&!view(slot.voice).present)slot.voice=0;if(current&&!current->paused&&!view(current->voice).present)current=nullptr;trace();}
extern "C" void audio_preload(){asset_foreach_file("samples/",".wav",preload,nullptr,true);}
extern "C" bool audio_is_muted(){return muted;}
extern "C" void audio_set_muted(bool value){muted=value;mix();}
extern "C" float audio_get_master_volume(){return master;}
extern "C" void audio_set_master_volume(float value){if(std::isfinite(value)){master=std::clamp(value,0.f,1.f);mix();}}
extern "C" float audio_get_mixer_percentage(){return percentage;}
extern "C" void audio_set_mixer_percentage(float value){if(std::isfinite(value)){percentage=std::clamp(value,0.f,1.f);mix();}}
extern "C" void audio_muffler_set_profile(mufflerprofile_t value){if(value>=MUFFLER_OFF&&value<=MUFFLER_HIGH){profile=value;muffler();}}
extern "C" mufflerprofile_t audio_muffler_profile(){return profile;}
extern "C" void audio_muffler_activate(int flags){muffle_flags=flags&MUFFLE_EVERYTHING;muffler();}
extern "C" bool audio_muffler_is_activated(){return profile!=MUFFLER_OFF&&muffle_flags!=0;}
extern "C" sound_t* sound_load(const char* path){
 if(!path||!*path)return nullptr;
 auto* sound=resourcemanager_find_sample(path);if(!sound){if(!bsl_soloud_preload(path))fatal_error("Can't load selected sound \"%s\"",path);sound=new sound_t{path};resourcemanager_add_sample(path,sound);loaded++;}resourcemanager_ref_sample(path);return sound;
}
extern "C" void sound_destroy(sound_t* sound){if(sound){bsl_soloud_unload(sound->path.c_str());delete sound;}}
extern "C" int sound_unref(sound_t* sound){return sound?resourcemanager_unref_sample(sound->path.c_str()):0;}
extern "C" samplehandle_t sound_play(const sound_t* sound){return sound_play_ex(sound,1,0,1);}
extern "C" samplehandle_t sound_play_ex(const sound_t* sound,float gain,float pan,float speed){
 if(!sound||!std::isfinite(gain)||!std::isfinite(pan)||!std::isfinite(speed))return 0;
 const unsigned active=bsl_soloud_current_bank(),begin=active==BSL_AUDIO_BANK_A?0:16;
 for(unsigned i=begin;i<begin+16;i++){auto& slot=pool[i];const auto p=view(slot.voice);if(p.present&&p.playing)continue;if(p.present)bsl_voice_destroy(slot.voice);slot.voice=bsl_soloud_play_scoped(sound->path.c_str(),(uint64_t(1)<<62)|serial++,false,active,false);if(!slot.voice)return 0;
 if(!bsl_soloud_control(slot.voice,std::max(0.f,gain),std::clamp(speed,1.f/64,64.f),std::clamp(pan,-1.f,1.f),true)){bsl_voice_destroy(slot.voice);slot.voice=0;return 0;}last_legacy=slot.voice;legacy_starts++;return slot.voice;}return 0;
}
extern "C" void sound_stop(samplehandle_t id){bsl_voice_destroy(id);for(auto& slot:pool)if(slot.voice==id)slot.voice=0;}
extern "C" bool sound_is_playing(samplehandle_t id){return view(id).playing;}
extern "C" float sound_get_volume(samplehandle_t id){return float(view(id).gain);}
extern "C" void sound_set_volume(samplehandle_t id,float value){const auto p=view(id);if(p.present&&std::isfinite(value))bsl_soloud_control(id,std::max(0.f,value),p.rate,p.balance,true);}
extern "C" void sound_stop_all(){const unsigned begin=bsl_soloud_current_bank()==BSL_AUDIO_BANK_A?0:16;for(unsigned i=begin;i<begin+16;i++)sound_stop(pool[i].voice);}
extern "C" void sound_swap_mixers(){bank=bank==BSL_AUDIO_BANK_A?BSL_AUDIO_BANK_B:BSL_AUDIO_BANK_A;bsl_soloud_active_bank(bank);}
extern "C" music_t* music_load(const char* path){if(!path||!*path)return nullptr;auto* music=resourcemanager_find_music(path);if(!music){if(!bsl_soloud_stream_preload(path))fatal_error("Can't load selected music \"%s\"",path);music=new music_t;music->path=path;resourcemanager_add_music(path,music);}resourcemanager_ref_music(path);return music;}
extern "C" int music_unref(music_t* music){return music?resourcemanager_unref_music(music->path.c_str()):0;}
extern "C" void music_destroy(music_t* music){if(!music)return;if(music==current)music_stop();else bsl_voice_destroy(music->voice);bsl_soloud_stream_unload(music->path.c_str());delete music;}
extern "C" void music_play(music_t* music,bool loop){music_stop();if(!music)return;music->voice=bsl_soloud_play_scoped(music->path.c_str(),(uint64_t(1)<<61)|serial++,loop,BSL_AUDIO_MUSIC,true);if(!music->voice){logfile_message("Selected music voice unavailable: %s",music->path.c_str());return;}current=music;music->paused=false;music->loop=loop;music->volume=1;if(music->repeat>0)bsl_soloud_repeat_from(music->voice,music->repeat);bsl_soloud_control(music->voice,1,1,0,true);stream_starts++;}
extern "C" void music_set_repeat_point(music_t* music,double point){if(!music||!std::isfinite(point))return;const double duration=bsl_soloud_source_duration(music->path.c_str(),true);music->repeat=std::clamp(point,0.0,std::max(0.0,duration-.000001));if(music->voice)bsl_soloud_repeat_from(music->voice,music->repeat);}
extern "C" void music_stop(){if(current){bsl_voice_destroy(current->voice);current->voice=0;current->paused=false;current=nullptr;}}
extern "C" void music_pause(){if(current){current->paused=true;bsl_soloud_control(current->voice,current->volume,1,0,false);}}
extern "C" void music_resume(){if(current){current->paused=false;bsl_soloud_control(current->voice,current->volume,1,0,true);}}
extern "C" void music_set_volume(float value){if(current&&std::isfinite(value)){current->volume=std::max(value,0.f);bsl_soloud_control(current->voice,current->volume,1,0,!current->paused);}}
extern "C" float music_get_volume(){return current?float(current->volume):0;}
extern "C" bool music_is_playing(){return current&&view(current->voice).playing;}
extern "C" bool music_is_paused(){return current&&current->paused;}
extern "C" bool music_is_repeating(music_t* music){return music&&music->loop;}
extern "C" float music_duration(){return current?float(bsl_soloud_source_duration(current->path.c_str(),true)):0;}
extern "C" music_t* music_current(){return current;}
extern "C" const char* music_path(const music_t* music){return music?music->path.c_str():"";}
extern "C" bsl_audio_loop_t* bsl_audio_loop_create(const sound_t* sound){if(!sound)return nullptr;const auto id=bsl_soloud_play_scoped(sound->path.c_str(),(uint64_t(1)<<60)|serial++,true,bsl_soloud_current_bank(),false);if(!id)return nullptr;auto* loop=new bsl_audio_loop_t{id};loops[id]=loop;return loop;}
extern "C" void bsl_audio_loop_destroy(bsl_audio_loop_t* loop){if(!loop)return;bsl_voice_destroy(loop->voice);loops.erase(loop->voice);delete loop;}
extern "C" bool bsl_audio_loop_set(bsl_audio_loop_t* loop,float gain,bool playing){return loop&&std::isfinite(gain)&&gain>=0&&gain<=1&&bsl_soloud_control(loop->voice,gain,1,0,playing);}
extern "C" bool bsl_audio_loop_probe(const bsl_audio_loop_t* loop,unsigned* position,float* gain){if(!loop)return false;const auto p=view(loop->voice);if(position)*position=p.position;if(gain)*gain=float(p.gain);return p.playing;}
