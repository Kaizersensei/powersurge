// Focused contract/render/device journey using the exact public owned-voice API. Signed: Mech.
#include "soloud_backend.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>
struct Assets {std::string root;unsigned reads=0;};
const unsigned char* read_asset(void* data,const char* path,size_t* size){auto& assets=*static_cast<Assets*>(data);assets.reads++;std::ifstream f(assets.root+"/"+path,std::ios::binary|std::ios::ate);if(!f)return nullptr;const auto length=f.tellg();if(length<=0||length>16*1024*1024)return nullptr;auto* bytes=new unsigned char[size_t(length)];f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes),length)){delete[] bytes;return nullptr;}*size=size_t(length);return bytes;}
void release_asset(void*,const unsigned char* data){delete[] data;}
unsigned checks=0,failures=0;
void check(bool ok,const char* message){checks++;if(!ok)failures++;std::cout<<(ok?"PASS":"FAIL")<<",\""<<message<<"\",Mech\n";}
BslVoiceProbe probe(uint64_t voice){BslVoiceProbe out{};bsl_voice_probe(voice,&out);return out;}
BslCachedVoiceProbe cache(){BslCachedVoiceProbe out{};bsl_voice_cached_probe(&out);return out;}
std::vector<float> render(unsigned frames){std::vector<float> pcm(frames*2);if(!bsl_soloud_render(pcm.data(),frames)){std::cerr<<"render refused\n";std::exit(3);}return pcm;}
double peak(const std::vector<float>& data){double value=0;for(float x:data)value=std::max(value,double(std::abs(x)));return value;}
double rms(const std::vector<float>& data,unsigned channel){double sum=0;const size_t begin=data.size()/4;for(size_t i=begin;i<data.size()/2;i++)sum+=double(data[2*i+channel])*data[2*i+channel];return std::sqrt(sum/std::max<size_t>(1,data.size()/2-begin));}
double tone(const std::vector<float>& data,unsigned channel,double frequency,unsigned rate){double real=0,imag=0;const size_t begin=data.size()/4,end=data.size()/2;for(size_t i=begin;i<end;i++){const double phase=6.283185307179586*frequency*i/rate;real+=data[2*i+channel]*std::cos(phase);imag+=data[2*i+channel]*std::sin(phase);}return 2*std::hypot(real,imag)/std::max<size_t>(1,end-begin);}
void device(Assets& assets,bool audition){
 BslAudioAssets reader{&assets,read_asset,release_asset};check(bsl_soloud_init(reader,44100,false,.95,.12),"miniaudio device starts through SoLoud");BslSoloudInfo info{};if(!bsl_soloud_info(&info)){check(false,"device reports initialized backend");return;}
 check(!info.offline&&std::string(info.backend)=="MiniAudio"&&info.sample_rate>=22050,"reported output backend is miniaudio with a real sample rate");
 const auto voice=bsl_voice_create("samples/stereo.wav",700,true);check(voice!=0&&bsl_voice_set(voice,audition?.18:0,1,0,20000,true,1.0/60),"device voice opens through existing C ABI");
 std::this_thread::sleep_for(std::chrono::milliseconds(180));const auto before=probe(voice);check(before.playing&&before.position>0,"device callback advances playback without offline pumping");
 bsl_voice_pause(true);const auto held=probe(voice);std::cerr<<"MEASURE,device_pause_position,"<<held.position<<"\nMEASURE,device_sample_rate,"<<info.sample_rate<<"\nMEASURE,device_buffer_frames,"<<info.buffer_frames<<"\n";std::this_thread::sleep_for(std::chrono::milliseconds(160));check(!probe(voice).playing&&probe(voice).position==held.position,"device pause retains exact sample position");
 bsl_voice_pause(false);std::this_thread::sleep_for(std::chrono::milliseconds(150));check(probe(voice).playing&&probe(voice).position!=held.position,"same voice resumes on device");float sample[2]{};check(!bsl_soloud_render(sample,1),"device mode refuses a second offline mixer consumer");
 if(audition){
  struct Phase {const char* name;double balance,cutoff,rate;};
  const Phase phases[]={{"Centre / independent L and R tones",0,20000,1},{"Right balance",1,20000,1},{"Left balance",-1,20000,1},{"Muffled",0,500,1},{"Faster and higher",0,20000,1.5}};
  for(const auto& p:phases){std::cout<<"AUDITION: "<<p.name<<std::endl;for(unsigned i=0;i<90;i++){bsl_voice_set(voice,.18,p.rate,p.balance,p.cutoff,true,1.0/60);std::this_thread::sleep_for(std::chrono::milliseconds(16));}}
 }
 bsl_voice_destroy(voice);check(cache().active==0&&cache().acquired==cache().released,"device voice releases its independent sample lease");bsl_soloud_shutdown();check(!bsl_soloud_info(&info),"device teardown releases its context");
}
void offline(Assets& assets){
 BslAudioAssets reader{&assets,read_asset,release_asset};check(!bsl_soloud_init({},44100,true,.95,.12),"invalid asset provider refuses without partial setup");
 check(bsl_soloud_init(reader,44100,true,.95,.12),"offline SoLoud initialization succeeds");BslSoloudInfo info{};bsl_soloud_info(&info);const unsigned rate=info.sample_rate;
 check(info.offline&&std::string(info.backend)=="null driver","offline run owns no hardware output device");check(!bsl_soloud_init(reader,44100,true,.95,.12),"second context refuses without replacing the first");
 const auto initial=cache();check(!bsl_voice_create_cached("samples/stereo.wav",1,true)&&assets.reads==0,"uncached opening performs no asset read or decode");
 check(bsl_soloud_preload("samples/stereo.wav")&&bsl_soloud_preload("samples/stereo.wav")&&assets.reads==1&&cache().decoded==initial.decoded+1,"preload decodes once and shares the cached sample");
 check(!bsl_soloud_preload("../samples/stereo.wav")&&!bsl_soloud_preload("samples/../secret.wav")&&assets.reads==1,"unsafe asset paths refuse before provider access");
 const auto a=bsl_voice_create_cached("samples/stereo.wav",100,true);check(a!=0&&!bsl_voice_create_cached("samples/stereo.wav",0,true),"voice identity requires an explicit nonzero owner");
 auto pcm=render(rate/4);check(peak(pcm)==0&&probe(a).position==0&&!probe(a).playing,"new voice is silent and retains position until requested playing");
 check(bsl_voice_set(a,.4,1,0,20000,true,1.0/60),"owned ABI accepts live gain rate balance and cutoff");pcm=render(rate/3);const auto p1=probe(a);
 check(p1.playing&&p1.position>0,"offline mix advances the original voice");
 check(tone(pcm,0,440,rate)>.05&&tone(pcm,0,1760,rate)<.005&&tone(pcm,1,1760,rate)>.05&&tone(pcm,1,440,rate)<.005,"stereo content retains separate frequencies without channel folding");
 const double old_rms=rms(pcm,0);bsl_voice_set(a,.1,1,0,20000,true,.05);pcm=render(rate/3);check(rms(pcm,0)<old_rms*.5&&probe(a).position>p1.position,"gain changes smoothly without restarting the voice");
 const auto old_position=probe(a).position;bsl_voice_set(a,.3,1.5,0,20000,true,.1);pcm=render(rate/3);check(probe(a).rate>1.4&&probe(a).position-old_position>rate*.4,"rate changes preserve playhead while changing playback speed");
 bsl_voice_set(a,.3,1,1,20000,true,.1);pcm=render(rate/2);std::cerr<<"MEASURE,right_balance_ratio,"<<rms(pcm,0)/rms(pcm,1)<<"\n";check(rms(pcm,0)<rms(pcm,1)*.02,"right balance attenuates left while preserving right content");
 bsl_voice_set(a,.3,1,-1,20000,true,.1);pcm=render(rate/2);std::cerr<<"MEASURE,left_balance_ratio,"<<rms(pcm,1)/rms(pcm,0)<<"\n";check(rms(pcm,1)<rms(pcm,0)*.02,"left balance attenuates right while preserving left content");
 bsl_voice_set(a,.3,1,0,350,true,.1);pcm=render(rate/2);std::cerr<<"MEASURE,lowpass_high_low_ratio,"<<rms(pcm,1)/rms(pcm,0)<<"\n";check(rms(pcm,1)<rms(pcm,0)*.15,"per-voice low-pass reduces high-frequency content");
 const auto saved=probe(a);check(!bsl_voice_set(a,std::numeric_limits<double>::quiet_NaN(),1,0,20000,true,.01)&&probe(a).gain==saved.gain,"invalid live parameters leave the valid voice unchanged");
 bsl_voice_pause(true);const auto frozen=probe(a);pcm=render(rate/4);check(bsl_voice_paused()&&!probe(a).playing&&probe(a).position==frozen.position&&peak(pcm)==0,"world pause freezes source and filtered bus atomically");
 const auto stopped=bsl_voice_create_cached("samples/stereo.wav",200,true);bsl_voice_set(stopped,.1,1,0,20000,false,.02);const auto pausedNew=bsl_voice_create_cached("samples/stereo.wav",300,true);bsl_voice_set(pausedNew,.1,1,0,20000,true,.02);render(rate/8);
 check(probe(pausedNew).position==0&&!probe(pausedNew).playing,"voice created during world pause cannot start early");bsl_voice_pause(false);render(rate/8);
 check(!probe(stopped).playing&&probe(stopped).position==0&&probe(pausedNew).playing,"resume respects each voice requested state");
 check(bsl_soloud_unload("samples/stereo.wav")&&!bsl_voice_create_cached("samples/stereo.wav",400,true),"cache eviction prevents new cached openings");const auto held=probe(a).position;render(rate/8);check(probe(a).playing&&probe(a).position!=held,"eviction preserves decoded data held by existing voices");
 check(bsl_soloud_preload("samples/stereo.wav")&&cache().decoded==initial.decoded+2,"reload creates a fresh cached generation without mutating old sources");
 bsl_voice_cancel_owner(100);check(!probe(a).present&&probe(stopped).present&&probe(pausedNew).playing,"owner cancellation preserves other owners");
 BslOwnedVoiceSnapshot snapshot[32]{};check(bsl_voice_snapshot(snapshot,32)==2,"existing snapshot API reports only retained public voices");bsl_voice_release();check(cache().active==0&&cache().acquired==cache().released,"release balances voice sample leases");
 std::vector<uint64_t> voices;for(unsigned i=0;i<32;i++)voices.push_back(bsl_voice_create_cached("samples/stereo.wav",500+i,true));const auto before=cache();check(std::all_of(voices.begin(),voices.end(),[](auto v){return v!=0;})&&!bsl_voice_create_cached("samples/stereo.wav",999,true)&&cache().acquired==before.acquired,"public voice cap refuses cleanly without a partial lease");bsl_voice_release();
 check(bsl_soloud_preload("samples/constant.wav"),"limiter fixture preloads through the same asset API");for(unsigned i=0;i<8;i++){auto v=bsl_voice_create_cached("samples/constant.wav",1000+i,true);bsl_voice_set(v,1,1,0,20000,true,.02);}pcm=render(rate/2);float reduction=0,output=0;bsl_master_probe(&reduction,&output);
 std::cerr<<"MEASURE,master_output_peak,"<<peak(pcm)<<"\nMEASURE,master_gain_reduction,"<<reduction<<"\n";check(peak(pcm)>.8&&peak(pcm)<=.95001&&reduction>.5f&&output<=.95001f,"final stereo-linked limiter controls summed voices before output clipping");bsl_voice_release();
 const auto mono=bsl_voice_create("samples/mono.wav",1100,true);bsl_voice_set(mono,.2,1,0,20000,true,.02);pcm=render(rate/3);check(rms(pcm,0)>.02&&std::abs(rms(pcm,0)-rms(pcm,1))<.001,"mono sources enter the shared stereo processing path");bsl_voice_release();
 const auto ogg=bsl_voice_create("samples/material.ogg",1200,false);check(ogg!=0&&bsl_voice_duration(ogg)>0,"existing prototype Ogg material sound decodes through SoLoud");if(ogg){bsl_voice_set(ogg,.1,1,0,20000,true,.02);render(unsigned((bsl_voice_duration(ogg)+.25)*rate));check(!probe(ogg).playing,"finite material sound completes naturally");bsl_voice_destroy(ogg);}
 check(!bsl_voice_set(999999,.1,1,0,20000,true,.02)&&!probe(999999).present,"stale public handles refuse safely");
 bsl_soloud_shutdown();check(cache().active==0&&cache().acquired==cache().released&&!bsl_soloud_info(&info),"full teardown balances leases and releases decoded resources");
 check(bsl_soloud_init(reader,48000,true,.95,.12),"adapter reinitializes with a different output sample rate");
 const auto alternate=bsl_voice_create("samples/stereo.wav",1300,true);bsl_voice_set(alternate,.3,1,0,350,true,.02);pcm=render(24000);bsl_soloud_info(&info);
 check(info.sample_rate==48000&&tone(pcm,0,440,48000)>.02&&rms(pcm,1)<rms(pcm,0)*.15,"output-rate buses preserve source pitch and cutoff semantics at48kHz");
 bsl_soloud_shutdown();check(cache().active==0&&cache().acquired==cache().released,"reinitialization and alternate-rate teardown retain balanced ownership");
}

void routes(Assets& assets){
 BslAudioAssets reader{&assets,read_asset,release_asset};check(bsl_soloud_init(reader,44100,true,.95,.12),"shared native routes initialize on one context");
 check(bsl_soloud_preload("samples/stereo.wav")&&bsl_soloud_stream_preload("samples/material.ogg"),"resident sound and compressed streaming music preload separately");
 const auto a=bsl_soloud_play_scoped("samples/stereo.wav",901,true,BSL_AUDIO_BANK_A,false),b=bsl_soloud_play_scoped("samples/stereo.wav",902,true,BSL_AUDIO_BANK_B,false),m=bsl_soloud_play_scoped("samples/material.ogg",903,true,BSL_AUDIO_MUSIC,true),o=bsl_voice_create_cached("samples/stereo.wav",904,true);
 bsl_soloud_control(a,.15,1,0,true);bsl_soloud_control(b,.15,1,0,true);bsl_soloud_control(m,.1,1,0,true);bsl_voice_set(o,.1,1,0,20000,true,.02);render(4410);
 check(probe(a).playing&&!probe(b).playing&&probe(m).playing&&probe(o).playing,"initial bank plays alongside independent music and owned voices");
 bsl_voice_pause(true);auto pa=probe(a).position,pm=probe(m).position,po=probe(o).position;render(4410);
 check(bsl_soloud_current_bank()==BSL_AUDIO_BANK_B&&probe(a).position==pa&&probe(o).position==po&&probe(b).playing&&probe(m).playing&&probe(m).position!=pm,"world pause holds gameplay bank and owned voices while menu bank and music continue");
 bsl_soloud_control(a,.3,1,0,true);render(2205);check(!probe(a).playing&&probe(a).position==pa,"editing a paused bank voice cannot start it early");
 bsl_voice_pause(false);render(4410);check(probe(a).playing&&!probe(b).playing&&probe(o).playing&&probe(m).playing,"world resume restores bank ownership without pausing music");
 bsl_soloud_active_bank(BSL_AUDIO_BANK_B);render(2205);check(!probe(a).playing&&probe(b).playing&&probe(o).playing,"legacy explicit bank swap does not pause unrelated owned voices");
 bsl_soloud_suspend(true);pa=probe(a).position;pm=probe(m).position;po=probe(o).position;auto pb=probe(b).position;auto pcm=render(4410);check(peak(pcm)==0&&probe(a).position==pa&&probe(b).position==pb&&probe(m).position==pm&&probe(o).position==po,"display suspension freezes every route without losing positions");bsl_soloud_suspend(false);
 bsl_soloud_control(m,.1,1,0,false);pm=probe(m).position;render(2205);check(probe(m).position==pm&&!probe(m).playing&&probe(o).playing,"explicit music pause is independent of world-owned playback");bsl_soloud_control(m,.1,1,0,true);
 bsl_soloud_mix(1,1,1,true);pm=probe(m).position;pcm=render(4410);check(peak(pcm)==0&&probe(m).position!=pm,"final mute silences all routes without freezing source timelines");bsl_soloud_mix(1,1,1,false);

 bsl_soloud_control(m,.1,1,0,true);const auto monitor_position=probe(m).position;
 check(bsl_soloud_monitor(0)&&bsl_soloud_monitor_mask()==0,"scope monitor can isolate silence without changing requested playback");pcm=render(13230);check(rms(pcm,0)<.000001&&rms(pcm,1)<.000001&&probe(m).position!=monitor_position,"scope isolation fades output while music timeline keeps advancing");
 check(!bsl_soloud_monitor(16)&&bsl_soloud_monitor_mask()==0,"unsupported monitor bits refuse without changing the active mask");
 bsl_soloud_monitor(1u<<BSL_AUDIO_OWNED);pcm=render(13230);check(rms(pcm,0)>.01&&probe(o).playing&&probe(m).playing,"solo-owned monitoring leaves muted music alive on the shared output");bsl_soloud_monitor(15);
 bsl_voice_release();BslOwnedVoiceSnapshot snapshots[32]{};check(!probe(o).present&&probe(m).present&&probe(b).present&&bsl_voice_snapshot(snapshots,32)==0&&cache().active==0,"scene-owned cleanup preserves music and legacy handles without polluting owned counters");
 bsl_voice_destroy(a);bsl_voice_destroy(b);bsl_voice_destroy(m);
 const auto short_voice=bsl_soloud_play_scoped("samples/material.ogg",905,false,BSL_AUDIO_MUSIC,true);bsl_soloud_control(short_voice,.1,1,0,true);render(unsigned((bsl_voice_duration(short_voice)+.3)*44100));bsl_soloud_collect();check(!probe(short_voice).present,"finished compressed stream retires its processing bus during control-thread collection");
 bsl_soloud_shutdown();check(cache().active==0&&cache().acquired==cache().released,"native routes leave owned accounting balanced at teardown");
}

void solo_stream(Assets& assets){
 BslAudioAssets reader{&assets,read_asset,release_asset};check(bsl_soloud_init(reader,44100,true,.95,.12),"isolated music waveform context initializes");
 for(const char* path:{"samples/stereo.wav","samples/material.ogg"}){
  check(bsl_soloud_stream_preload(path),"isolated stream fixture preloads");const auto m=bsl_soloud_play_scoped(path,5001,true,BSL_AUDIO_MUSIC,true);bsl_soloud_control(m,.5,1,0,true);bsl_soloud_monitor(1u<<BSL_AUDIO_BANK_A);render(13230);const auto position=probe(m).position;
  bsl_soloud_monitor(1u<<BSL_AUDIO_MUSIC);auto pcm=render(22050);std::cerr<<"MEASURE,solo_stream_"<<path<<","<<rms(pcm,0)<<"\n";check(rms(pcm,0)>.005&&probe(m).position!=position,"solo streamed music produces nonzero PCM after monitoring switch");render(unsigned((bsl_voice_duration(m)+.3)*44100));pcm=render(22050);std::cerr<<"MEASURE,loop_stream_"<<path<<","<<rms(pcm,0)<<"\n";check(rms(pcm,0)>.005,"solo streamed music remains nonzero after its first loop");bsl_voice_destroy(m);
 }
 bsl_soloud_shutdown();
}

int main(int argc,char** argv){if(argc<3){std::cerr<<"usage: audio_backend_check --check|--device|--audition content-root\n";return 2;}std::cout<<std::unitbuf;Assets assets{argv[2],0};const std::string mode=argv[1];std::cout<<"result,check,signed\n";if(mode=="--check"){offline(assets);routes(assets);solo_stream(assets);}else if(mode=="--device"||mode=="--audition")device(assets,mode=="--audition");else return 2;std::cerr<<"checks="<<checks<<" failures="<<failures<<"\n";return failures?1:0;}
