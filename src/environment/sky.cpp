// Stateless drawing plus level-owned motion phase; no clock/weather authority. Signed: President.
#include "sky.hpp"
#include "sky.h"
#include "../authoring/render_records.hpp"
#include "../authoring/world_records.hpp"
extern "C" {
#include "../core/shader.h"
#include "../core/logfile.h"
#include "../foundation/foundation_host.h"
}
#include <allegro5/allegro_primitives.h>
#include <cstring>
#include <memory>
namespace ps::environment {
namespace {
const char* program="__ps_sky_v1";
const char fragment[]=FRAGMENT_SHADER_GLSL_PREFIX("highp") R"GLSL(
uniform vec3 top_color,horizon_color,bottom_color,cloud_color,sun_color,moon_color;
uniform float horizon,aspect,cloud_scale,cloud_coverage,cloud_softness,cloud_opacity,seed;
uniform vec2 cloud_offset;
uniform vec4 sun,moon;
float random_cell(vec2 p){p=mod(p,256.0);return fract(sin(dot(p,vec2(127.1,311.7))+seed)*43758.5453);}
float sky_noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(random_cell(i),random_cell(i+vec2(1.0,0.0)),f.x),mix(random_cell(i+vec2(0.0,1.0)),random_cell(i+vec2(1.0,1.0)),f.x),f.y);}
float cloud_noise(vec2 p){return sky_noise(p)*.533333+sky_noise(p*2.0)*.266667+sky_noise(p*4.0)*.133333+sky_noise(p*8.0)*.066667;}
vec3 disc(vec2 uv,vec4 body,vec3 tint){vec2 d=(uv-body.xy)*vec2(aspect,1.0);return tint*body.w*(1.0-smoothstep(body.z*.86,body.z,length(d)))*(1.0-smoothstep(horizon,horizon+.003,uv.y));}
void main(){
 vec2 uv=v_texcoord;vec3 color=uv.y<horizon?mix(top_color,horizon_color,clamp(uv.y/max(horizon,.0001),0.0,1.0)):mix(horizon_color,bottom_color,clamp((uv.y-horizon)/max(1.0-horizon,.0001),0.0,1.0));
 color+=disc(uv,sun,sun_color)+disc(uv,moon,moon_color);
 if(cloud_opacity>0.0&&cloud_coverage>0.0){vec2 p=uv*vec2(aspect,1.0)*cloud_scale+cloud_offset;float n=cloud_noise(p);float mask=smoothstep(1.0-cloud_coverage-cloud_softness,1.0-cloud_coverage+cloud_softness,n);mask*=1.0-smoothstep(horizon-.2,horizon+.05,uv.y);color=mix(color,cloud_color,mask*cloud_opacity);}
 gl_FragColor=vec4(clamp(color,0.0,1.0),1.0)*v_color;
}
)GLSL";
struct State {
 ALLEGRO_STATE state;image_t* target=image_drawing_target();const shader_t* managed=shader_get_active();ALLEGRO_SHADER* actual=al_get_current_shader();bool held=al_is_bitmap_drawing_held();int depth=al_get_render_state(ALLEGRO_DEPTH_TEST),mask=al_get_render_state(ALLEGRO_WRITE_MASK);
 State(){al_store_state(&state,ALLEGRO_STATE_ALL);al_hold_bitmap_drawing(false);}
 ~State(){image_set_drawing_target(target);al_restore_state(&state);al_set_render_state(ALLEGRO_DEPTH_TEST,depth);al_set_render_state(ALLEGRO_WRITE_MASK,mask);shader_set_active(managed);al_use_shader(actual);al_hold_bitmap_drawing(held);}
};
void quad(float y0,float y1,int width,ALLEGRO_COLOR top,ALLEGRO_COLOR bottom){ALLEGRO_VERTEX v[4]={{0,y0,0,0,0,top},{float(width),y0,0,1,0,top},{float(width),y1,0,1,1,bottom},{0,y1,0,0,1,bottom}};al_draw_prim(v,nullptr,nullptr,0,4,ALLEGRO_PRIM_TRIANGLE_FAN);}
Color gradient(const SkyFrame& f,double y){Color a,b;double t;if(y<f.horizon){a=f.top;b=f.horizon_color;t=std::clamp(y/std::max(f.horizon,.0001),0.,1.);}else{a=f.horizon_color;b=f.bottom;t=std::clamp((y-f.horizon)/std::max(1-f.horizon,.0001),0.,1.);}for(int i=0;i<3;++i)a[i]+=(b[i]-a[i])*t;return a;}
ALLEGRO_COLOR color(Color v){return al_map_rgb_f(v[0],v[1],v[2]);}
std::string failed_shader;
}
void reset_sky_resources(){failed_shader.clear();if(shader_exists(program))shader_clear_uniforms(shader_get(program));}
bool draw_sky(image_t* output,const SkyFrame& frame,const SkyProfile&,std::string& diagnostic){
 if(!output)throw std::runtime_error("Sky drawing target unavailable");State state;int width=image_width(output),height=image_height(output);image_set_drawing_target(output);ALLEGRO_TRANSFORM transform;al_identity_transform(&transform);al_use_transform(&transform);al_orthographic_transform(&transform,0,0,-1,width,height,1);al_use_projection_transform(&transform);al_set_clipping_rectangle(0,0,width,height);al_set_render_state(ALLEGRO_DEPTH_TEST,0);al_set_render_state(ALLEGRO_WRITE_MASK,ALLEGRO_MASK_RGBA);al_set_blender(ALLEGRO_ADD,ALLEGRO_ONE,ALLEGRO_ZERO);
 shader_t* shader=shader_exists(program)?shader_get(program):nullptr;
 if(!shader&&failed_shader.empty()){char error[1024]="";shader=shader_try_create(program,fragment,error,sizeof error);if(!shader)failed_shader=error;}
 auto fallback=[&]{shader_set_active(shader_get_default());float at=float(std::clamp(frame.horizon,0.,1.))*height;quad(0,at,width,color(gradient(frame,0)),color(gradient(frame,at/height)));quad(at,float(height),width,color(gradient(frame,at/height)),color(gradient(frame,1)));};
 if(!shader){fallback();diagnostic=failed_shader;return false;}
 auto vector=[&](const char* name,const auto& values){float data[4];for(size_t i=0;i<values.size();++i)data[i]=float(values[i]);shader_set_float_vector(shader,name,int(values.size()),data);};
 vector("top_color",frame.top);vector("horizon_color",frame.horizon_color);vector("bottom_color",frame.bottom);vector("cloud_color",frame.cloud_color);vector("sun_color",frame.sun_color);vector("moon_color",frame.moon_color);vector("sun",frame.sun);vector("moon",frame.moon);vector("cloud_offset",frame.cloud_offset);
 shader_set_float(shader,"horizon",frame.horizon);shader_set_float(shader,"aspect",double(width)/height);shader_set_float(shader,"cloud_scale",frame.cloud_scale);shader_set_float(shader,"cloud_coverage",frame.cloud_coverage);shader_set_float(shader,"cloud_softness",frame.cloud_softness);shader_set_float(shader,"cloud_opacity",frame.cloud_opacity);shader_set_float(shader,"seed",frame.seed);
 if(!shader_set_active(shader)){fallback();diagnostic="Sky shader activation failed; gradient fallback active";return false;}quad(0,float(height),width,al_map_rgb(255,255,255),al_map_rgb(255,255,255));diagnostic.clear();return true;
}
}
namespace {
std::string level_id,last_diagnostic;std::optional<ps::environment::SkyProfile> profile;ps::environment::SkyAttachment attachment;
ps::environment::CloudMotion motion;
void note(const std::string& text){if(text!=last_diagnostic){last_diagnostic=text;if(!text.empty())logfile_message("Sky: %s",text.c_str());}}
}
extern "C" bool ps_sky_active(){return profile.has_value();}
extern "C" void ps_sky_detach(){ps::environment::reset_sky_resources();profile.reset();level_id.clear();last_diagnostic.clear();motion={};}
extern "C" void ps_sky_render(double camera_x,double camera_y){
 (void)camera_x;
 try{
  auto* level=ps::authoring::world_snapshot().levels.by_scene(bsl_engine_level_file());auto& registry=ps::authoring::render_snapshot();auto id=level?level->id:std::string{};auto found=registry.level_skies.find(id);
  if(found==registry.level_skies.end()){if(profile)ps_sky_detach();return;}
  if(level_id!=id||!profile){ps_sky_detach();attachment=found->second;profile=registry.skies.at(attachment.profile);level_id=id;}
  BslEnvironmentProbe input{};bsl_host_environment_probe(&input);if(!input.available){note(input.diagnostic[0]?input.diagnostic:"Waiting for an available calendar/weather snapshot");return;}if(input.indoor&&!attachment.visible_indoors)return;
  auto* output=image_drawing_target();auto frame=ps::environment::sky_frame(*profile,attachment,input,camera_y,image_width(output),image_height(output));frame.cloud_offset=motion.advance(*profile,input);std::string error;ps::environment::draw_sky(output,frame,*profile,error);note(error);
 }catch(const std::exception& e){note(e.what());}
}
