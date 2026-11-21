// Original world propagation / owned playback / demo diagnostics. Signed: Atlas.
#include "spatial_audio.h"
#include "foundation_host.h"
#include "acoustic_records.hpp"
#include "reaction_records.hpp"
#include "reaction_bubbles.hpp"
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <map>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../core/video.h"
#include "../core/image.h"
#include "../core/asset.h"
#include "../entities/camera.h"
void bsl_master_probe(float*,float*);
void bsl_bor_audio_render(void);
void bsl_bor_audio_reset(void);
}
namespace {
using namespace bsl::acoustic;
World& world(){static World value(compiled_acoustics());return value;}
const std::vector<BslSpatialRecipe>& recipes(){static const auto value=compiled_spatial_recipes();return value;}
struct Spatial {uint64_t voice=0,event=0,owner=0;const BslSpatialRecipe* recipe=nullptr;Point at;double speed=0,age=0,gain_scale=1,rate_scale=1;bool airborne=false;Result result;Cell listener;uint64_t revision=0;};
struct Blocker {double x,y,w,h;unsigned material;};
std::map<uint64_t,Spatial> voices;uint64_t next_handle=1;
std::map<Cell,uint8_t> terrain,pending,combined;
std::map<uint64_t,Blocker> blockers;bool dirty=true,paused=false;double schedule=0,update_ms=0,hearing_ms=0;
double terrain_ms=0,search_ms=0,playback_ms=0,last_hearing_ms=0;bool geometry_changed=false;
std::map<uint64_t,bsl::ReactionBubble> reactions;image_t* bubble_source=nullptr;image_t* bubble_image=nullptr;
const BslReactionDef& reaction_def(){static const auto d=compiled_reaction();return d;}
bsl::ReactionTuning reaction_tuning(){const auto& d=reaction_def();return d.sheet?bsl::ReactionTuning{d.lifetime,d.fade,d.inflate,d.peak,d.end,d.rise}:bsl::ReactionTuning{};}
std::map<uint64_t,Result> heard;unsigned debug_mask=254;ALLEGRO_FONT* debug_font=nullptr;
void export_result(const Result& r,BslAcousticResult* out){if(out)*out={r.resolved,r.source,r.revision,r.received.low,r.received.mid,r.received.high,r.arrival.x,r.arrival.y,r.length,r.crossings,r.bends};}
void compose(){
 auto next=terrain;
 for(const auto& e:blockers){const auto& b=e.second;for(int y=int(std::floor(b.y/16));y<=int(std::floor((b.y+b.h-1)/16));++y)for(int x=int(std::floor(b.x/16));x<=int(std::floor((b.x+b.w-1)/16));++x)next[{x,y}]=uint8_t(b.material);}
 for(auto& p:combined)if(!next.count(p.first))world().grid().set(p.first,0);
 for(auto& p:next)world().grid().set(p.first,p.second);
 combined=std::move(next);
}
void geometry(){if(!dirty)return;bsl_acoustic_scan_terrain();terrain=std::move(pending);compose();dirty=false;}
}
extern "C" void bsl_acoustic_reaction(uint64_t owner,int kind,double x,double y){
 if(kind<0){reactions.erase(owner);return;}if(!owner||!reaction_def().sheet||!Grid::valid({x,y})||kind>3||(!reactions.count(owner)&&reactions.size()>=64))return;
 auto& r=reactions[owner];r.x=x;r.y=y;r.emit(kind);
}
extern "C" void bsl_acoustic_reaction_move(uint64_t owner,double x,double y){if(!Grid::valid({x,y}))return;auto r=reactions.find(owner);if(r!=reactions.end()){r->second.x=x;r->second.y=y;}}
extern "C" bool bsl_acoustic_reaction_probe(uint64_t owner,BslReactionProbe* out){if(!out)return false;*out={};auto r=reactions.find(owner);if(r==reactions.end())return false;auto t=reaction_tuning();*out={r->second.kind,r->second.queued,r->second.age,r->second.alpha(t),r->second.scale(t),r->second.rise(t)};return true;}
namespace {
void render_reactions(){
 if(reactions.empty())return;
 const auto& d=reaction_def();if(!d.sheet||!asset_exists(d.sheet))return;
 const bool held=al_is_bitmap_drawing_held();if(held)al_hold_bitmap_drawing(false);
 if(!bubble_image){bubble_source=image_load(d.sheet);bubble_image=image_clone(bubble_source);al_convert_mask_to_alpha(IMAGE2BITMAP(bubble_image),al_map_rgb(d.key[0],d.key[1],d.key[2]));}
 ALLEGRO_TRANSFORM saved=*al_get_current_transform(),identity;al_identity_transform(&identity);al_use_transform(&identity);
 auto camera=camera_get_position(),screen=video_get_screen_size();auto t=reaction_tuning();
 for(const auto& e:reactions){const auto& r=e.second;if(r.kind<1||r.kind>3)continue;float scale=float(r.scale(t)),alpha=float(r.alpha(t));int frame=d.frames[r.kind-1];float x=float(r.x-camera.x+screen.x*.5-d.hotspot[0]*scale),y=float(r.y-camera.y+screen.y*.5-r.rise(t)-d.hotspot[1]*scale);
  al_draw_tinted_scaled_bitmap(IMAGE2BITMAP(bubble_image),al_map_rgba_f(alpha,alpha,alpha,alpha),float((frame%d.columns)*d.width),float((frame/d.columns)*d.height),float(d.width),float(d.height),x,y,d.width*scale,d.height*scale,0);
  // The supplied sheet already includes question/exclamation marks; its blank speech frame receives an ellipsis.
  if(r.kind==2)for(int dot=0;dot<3;++dot)al_draw_filled_rectangle(x+(9+dot*5)*scale,y+12*scale,x+(11+dot*5)*scale,y+14*scale,al_map_rgba_f(0,0,0,alpha));
 }
 al_use_transform(&saved);if(held)al_hold_bitmap_drawing(true);
}
}
extern "C" void bsl_acoustic_terrain_begin(){pending.clear();}
extern "C" void bsl_acoustic_cell(int x,int y,unsigned material){if(material&&material<world().settings().materials.size())pending[{x,y}]=uint8_t(material);}
extern "C" void bsl_acoustic_geometry_dirty(){dirty=true;}
extern "C" void bsl_acoustic_blocker(uint64_t id,double x,double y,double width,double height,unsigned material,bool enabled){
 if(!id||!Grid::valid({x,y})||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||width>4096||height>4096||!Grid::valid({x+width,y+height})||material>=world().settings().materials.size())return;
 auto it=blockers.find(id);if(enabled&&it==blockers.end()&&blockers.size()>=128)return;if(enabled&&material){if(it!=blockers.end()&&it->second.x==x&&it->second.y==y&&it->second.w==width&&it->second.h==height&&it->second.material==material)return;blockers[id]={x,y,width,height,material};}else if(it!=blockers.end())blockers.erase(it);else return;
 compose(); // revision changes synchronously, so old cached results fail closed immediately.
}
extern "C" uint64_t bsl_acoustic_emit(uint64_t owner,double x,double y,uint32_t category,double low,double mid,double high,double range,double lifetime,bool noise){
 if(!owner||!category||!std::isfinite(low)||!std::isfinite(mid)||!std::isfinite(high)||low<0||mid<0||high<0||low>1||mid>1||high>1)return 0;
 return world().emit({0,owner,0,{x,y},category,{low,mid,high},range,lifetime,noise});
}
extern "C" void bsl_acoustic_move(uint64_t event,double x,double y){world().move(event,{x,y});}
extern "C" void bsl_acoustic_remove(uint64_t event){world().remove(event);}
extern "C" bool bsl_acoustic_sample(uint64_t event,double x,double y,BslAcousticResult* out){auto r=dirty?Result{}:world().sample(event,{x,y});export_result(r,out);return r.resolved;}
extern "C" bool bsl_acoustic_hear(uint64_t listener,double x,double y,uint32_t categories,double threshold,BslAcousticResult* out){
 const double started=al_get_time();
 if(out)*out={};
 if(dirty||!listener||!std::isfinite(threshold)||threshold<0)return false;
 auto results=world().hear({listener,{x,y},{threshold,threshold,threshold},categories});if(results.empty()){hearing_ms+=(al_get_time()-started)*1000;return false;}
 auto r=*std::max_element(results.begin(),results.end(),[](const Result& a,const Result& b){return a.received.mid<b.received.mid;});heard[listener]=r;export_result(r,out);hearing_ms+=(al_get_time()-started)*1000;return true;
}
static uint64_t spatial_play(const char* recipe,uint64_t owner,double x,double y,bool cached){
 if(!recipe||!owner||!Grid::valid({x,y})||voices.size()>=24||bsl_engine_editor()||bsl_host_boot_busy())return 0;
 const BslSpatialRecipe* r=nullptr;for(const auto& p:recipes())if(!std::strcmp(recipe,p.id)){r=&p;break;}if(!r)return 0;
 auto voice=cached?bsl_voice_create_cached(r->path,owner,r->loop):bsl_voice_create(r->path,owner,r->loop);if(!voice)return 0;
 auto event=bsl_acoustic_emit(owner,x,y,r->category,r->bands.low,r->bands.mid,r->bands.high,r->range,r->loop?86400:r->noise?3:std::min(86400.0,bsl_voice_duration(voice)*10+1),r->noise);
 if(!event){bsl_voice_destroy(voice);return 0;}
 uint64_t id=next_handle++;Spatial s;s.voice=voice;s.event=event;s.owner=owner;s.recipe=r;s.at={x,y};voices.emplace(id,std::move(s));return id;
}
extern "C" uint64_t bsl_spatial_play(const char* recipe,uint64_t owner,double x,double y){return spatial_play(recipe,owner,x,y,false);}
extern "C" uint64_t bsl_spatial_play_cached(const char* recipe,uint64_t owner,double x,double y){return spatial_play(recipe,owner,x,y,true);}
extern "C" uint64_t bsl_spatial_play_cached_path(const char* path,uint64_t owner,double x,double y){
 if(!path)return 0;
 const BslSpatialRecipe* chosen=nullptr;
 for(const auto& recipe:recipes())if(!std::strcmp(path,recipe.path)){if(chosen)return 0;chosen=&recipe;}
 if(!chosen||chosen->loop||chosen->airflow||!chosen->spatial||chosen->noise)return 0;
 return spatial_play(chosen->id,owner,x,y,true);
}
// Live instance multipliers leave propagation balance/cutoff under acoustic ownership. Signed: Mech.
extern "C" bool bsl_spatial_configure(uint64_t handle,double gain,double rate){auto p=voices.find(handle);if(p==voices.end()||!std::isfinite(gain)||!std::isfinite(rate)||gain<0||gain>1||rate<.1||rate>4)return false;p->second.gain_scale=gain;p->second.rate_scale=rate;return true;}
extern "C" uint64_t bsl_spatial_voice_id(uint64_t handle){auto p=voices.find(handle);return p==voices.end()?0:p->second.voice;}
extern "C" void bsl_spatial_move(uint64_t handle,double x,double y,double speed,bool airborne){auto p=voices.find(handle);if(p==voices.end()||!Grid::valid({x,y})||!std::isfinite(speed))return;auto& v=p->second;if(Grid::cell(v.at)!=Grid::cell({x,y})){v.result={};v.revision=0;}v.at={x,y};v.speed=std::abs(speed);v.airborne=airborne;world().move(v.event,v.at);}
extern "C" void bsl_spatial_stop(uint64_t handle){auto p=voices.find(handle);if(p==voices.end())return;bsl_voice_destroy(p->second.voice);world().remove(p->second.event);voices.erase(p);}
extern "C" bool bsl_spatial_probe(uint64_t handle,BslVoiceProbe* out){auto p=voices.find(handle);if(p==voices.end()){if(out)*out={};return false;}return bsl_voice_probe(p->second.voice,out);}
extern "C" bool bsl_spatial_result(uint64_t handle,BslAcousticResult* out){if(!out)return false;*out={};auto p=voices.find(handle);if(p==voices.end()||dirty||!p->second.result.resolved||p->second.result.revision!=world().grid().revision())return false;export_result(p->second.result,out);return true;}
extern "C" void bsl_acoustic_cancel_owner(uint64_t owner){for(auto i=voices.begin();i!=voices.end();)if(i->second.owner==owner){auto id=i->first;++i;bsl_spatial_stop(id);}else ++i;world().cancel_owner(owner);bsl_voice_cancel_owner(owner);heard.erase(owner);reactions.erase(owner);}
extern "C" void bsl_acoustic_clear(){bsl_bor_audio_reset();reactions.clear();if(bubble_image){image_destroy(bubble_image);bubble_image=nullptr;}if(bubble_source){image_unload(bubble_source);bubble_source=nullptr;}voices.clear();bsl_voice_release();world().clear();heard.clear();terrain.clear();pending.clear();combined.clear();blockers.clear();dirty=true;paused=false;schedule=0;update_ms=hearing_ms=0;if(debug_font){al_destroy_font(debug_font);debug_font=nullptr;}}
extern "C" void bsl_acoustic_debug(unsigned mask){debug_mask=mask&255;}
extern "C" void bsl_acoustic_tick(double dt,bool pause){
 const double start=al_get_time();paused=pause;bsl_voice_pause(pause);double terrain_elapsed=0,search_elapsed=0;uint64_t before_geometry=world().grid().revision();
 if(!paused&&dt>0){for(auto i=reactions.begin();i!=reactions.end();){i->second.tick(dt,reaction_tuning());if(!i->second.kind)i=reactions.erase(i);else ++i;}schedule-=dt;if(schedule<=0){dirty=true;schedule=world().settings().schedule;}double phase=al_get_time();if(world().sources()||!blockers.empty())geometry();terrain_elapsed=(al_get_time()-phase)*1000;phase=al_get_time();world().tick(dt);search_elapsed=(al_get_time()-phase)*1000;}
 const double playback_started=al_get_time();
 auto camera=camera_get_position(),screen=video_get_screen_size();Point listener{camera.x,camera.y},half{screen.x*.5,screen.y*.5};
 for(auto i=voices.begin();i!=voices.end();){auto& v=i->second;v.age+=paused?0:dt;
  BslVoiceProbe probe{};bsl_voice_probe(v.voice,&probe);if(!v.recipe->loop&&v.age>.15&&!probe.playing&&!paused){bsl_voice_destroy(v.voice);world().remove(v.event);i=voices.erase(i);continue;}
  Cell cell=Grid::cell(listener);if(!pause&&dt>0&&(v.revision!=world().grid().revision()||v.listener!=cell||schedule==world().settings().schedule||!v.result.resolved)){v.result=dirty?Result{}:world().sample(v.event,listener);v.revision=world().grid().revision();v.listener=cell;}
  // Never reuse a now-invalid path while replacement work is queued.
  if(dirty||v.result.revision!=world().grid().revision())v.result={};
  if(v.result.resolved&&v.result.crossings==0&&v.result.bends==0)v.result.arrival=v.at;
  auto p=world().playback(v.result,listener,half);if(!v.recipe->spatial)p={1,0,20000};double mapping=1,rate=v.recipe->rate;
  if(v.recipe->airflow){mapping=v.airborne?std::clamp(v.speed/600,0.0,1.0):0;rate=std::clamp(v.speed*.0005,.35,1.6);}
  bsl_voice_set(v.voice,p.gain*v.recipe->gain*mapping*v.gain_scale,std::clamp(rate*v.rate_scale,.1,4.0),p.balance,p.cutoff,true,dt);++i;
 }
 if(dt>0){terrain_ms=terrain_elapsed;search_ms=search_elapsed;playback_ms=(al_get_time()-playback_started)*1000;last_hearing_ms=hearing_ms;geometry_changed=world().grid().revision()!=before_geometry;update_ms=(al_get_time()-start)*1000+hearing_ms;hearing_ms=0;}
}
extern "C" void bsl_acoustic_probe(BslAcousticProbe* out){if(!out)return;const auto& w=world().work();*out={unsigned(world().sources()),unsigned(voices.size()),unsigned(world().grid().chunks()),w.expansions,w.deferred,w.saturated,world().grid().revision(),w.cache_hits,w.invalidations,w.node_limit,update_ms,terrain_ms,search_ms,playback_ms,last_hearing_ms,geometry_changed};}
extern "C" void bsl_acoustic_render(){
 if(!std::getenv("BSL_AUDIO_DEMO")||bsl_engine_editor())return;
 bsl_bor_audio_render();render_reactions();if(!debug_mask)return;
 const auto camera=camera_get_position(),screen=video_get_screen_size();float left=camera.x-screen.x*.5f,top=camera.y-screen.y*.5f;
 const bool held=al_is_bitmap_drawing_held();if(held)al_hold_bitmap_drawing(false);
 ALLEGRO_TRANSFORM saved=*al_get_current_transform(),identity;al_identity_transform(&identity);al_use_transform(&identity);
 if(debug_mask&1)for(const auto& e:combined){float x=e.first.x*16.f-left,y=e.first.y*16.f-top;if(x+16<0||y+16<0||x>screen.x||y>screen.y)continue;al_draw_rectangle(x,y,x+16,y+16,al_map_rgba(255,170,0,130),1);}
 auto draw=[&](const Result& r,ALLEGRO_COLOR color){
  if(!r.resolved||r.revision!=world().grid().revision())return;
  if(debug_mask&2)for(size_t i=1;i<r.path.size();++i)al_draw_line(float(r.path[i-1].x-left),float(r.path[i-1].y-top),float(r.path[i].x-left),float(r.path[i].y-top),color,2);
  if(debug_mask&4)al_draw_circle(float(r.arrival.x-left),float(r.arrival.y-top),6,color,2);
 };
 for(const auto& e:voices)draw(e.second.result,al_map_rgb(80,220,255));
 for(const auto& e:heard)draw(e.second,al_map_rgb(255,120,90));
 if(!debug_font)debug_font=al_create_builtin_font();
 float reduction=0,peak=0;bsl_master_probe(&reduction,&peak);auto w=world().work();
 al_draw_filled_rectangle(4,32,std::min(screen.x-4,900.f),144,al_map_rgba(0,0,0,200));
 if(debug_mask&64)al_draw_textf(debug_font,al_map_rgb(255,220,140),12,40,0,"ACOUSTICS rev %llu | chunks %u | sources %u voices %u",(unsigned long long)world().grid().revision(),unsigned(world().grid().chunks()),unsigned(world().sources()),unsigned(voices.size()));
 if(debug_mask&128)al_draw_textf(debug_font,al_map_rgb(255,220,140),12,52,0,"Work %u/4096 | deferred %u saturated %u | cache %llu invalidations %llu | %.3f ms",w.expansions,w.deferred,w.saturated,(unsigned long long)w.cache_hits,(unsigned long long)w.invalidations,update_ms);
 al_draw_textf(debug_font,al_map_rgb(230,240,255),12,64,0,"1 cells | 2 paths | 3 arrivals | 4 bands | 5 hearing | 6 voices | 7 revisions | 8 work");
 al_draw_textf(debug_font,al_map_rgb(230,240,255),12,76,0,"E: nearby station/door/travel | N: loud noise | M: quiet noise | I: pause/menu");
 al_draw_textf(debug_font,al_map_rgb(230,240,255),12,88,0,"Master limiter ceiling %.2f | reduction %.1f%% | output peak %.3f",compiled_limiter_ceiling(),reduction*100,peak);
 int row=100;for(const auto& e:voices){if(row>screen.y-40)break;const auto& v=e.second;BslVoiceProbe p{};bsl_voice_probe(v.voice,&p);
  if(debug_mask&32){al_draw_textf(debug_font,al_map_rgb(180,230,255),12,float(row),0,"%s id%llu gain %.3f rate %.2f balance %+.2f cutoff %.0fHz",v.recipe->id,(unsigned long long)e.first,p.gain,p.rate,p.balance,p.cutoff);row+=12;}
  if(debug_mask&8){al_draw_textf(debug_font,al_map_rgb(200,220,200),12,float(row),0,"bands %.3f / %.3f / %.3f | walls %u bends %u | %s",v.result.received.low,v.result.received.mid,v.result.received.high,v.result.crossings,v.result.bends,v.result.resolved?"resolved":"pending (silent)");row+=12;}
 }
 if(debug_mask&16)for(const auto& e:heard){al_draw_textf(debug_font,al_map_rgb(255,160,140),12,float(row),0,"Listener %llu event %llu | received %.3f %.3f %.3f | arrival %.0f %.0f",(unsigned long long)e.first,(unsigned long long)e.second.source,e.second.received.low,e.second.received.mid,e.second.received.high,e.second.arrival.x,e.second.arrival.y);row+=12;}
 al_use_transform(&saved);if(held)al_hold_bitmap_drawing(true);
}
