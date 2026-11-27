#include "surface.hpp"
#include "render_util.hpp"
extern "C" {
#include "../core/asset.h"
#include "../core/shader.h"
}
#include <allegro5/allegro_opengl.h>
#include <allegro5/allegro_image.h>
#include <cstring>
#include <stdexcept>
namespace ps::lighting {
namespace {
#include "kernel.inc"
constexpr const char* program="__ps_surface_lit_v1";
using detail::State;
image_t* upload(const cell::MaterialTexture& pixels){
 State state;auto* result=image_create_ex(pixels.w,pixels.h,IC_DEFAULT);if(!result)throw std::runtime_error("Cannot allocate lighting texture");image_disable_linear_filtering(result);
 auto* lock=al_lock_bitmap(IMAGE2BITMAP(result),ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_WRITEONLY);
 if(!lock){image_destroy(result);throw std::runtime_error("Cannot upload lighting texture");}
 for(int y=0;y<pixels.h;++y)std::memcpy(static_cast<unsigned char*>(lock->data)+lock->pitch*y,pixels.pixels.data()+y*pixels.w,size_t(pixels.w)*4);
 al_unlock_bitmap(IMAGE2BITMAP(result));return result;
}
cell::MaterialTexture read_normal_data(const std::string& path){
 // Role-aware: bounded raw PNG, no tint, sRGB conversion or alpha masking.
 if(path.empty())return {1,1,{{128,128,255,255}}};
 auto* file=al_fopen(asset_path(path.c_str()),"rb");if(!file)throw std::runtime_error("Cannot open normal map: "+path);
 std::unique_ptr<ALLEGRO_FILE,decltype(&al_fclose)> owner(file,al_fclose);
 unsigned char h[24];const unsigned char magic[]={137,80,78,71,13,10,26,10};
 if(al_fsize(file)>16*1024*1024||al_fread(file,h,24)!=24||std::memcmp(h,magic,8)||std::memcmp(h+12,"IHDR",4))throw std::runtime_error("Invalid normal PNG");
 auto number=[&](int i){uint32_t n=0;for(int k=0;k<4;++k)n=n*256+h[i+k];return n;};auto w=number(16),height=number(20);
 if(!w||!height||w>1024||height>1024||!al_fseek(file,0,ALLEGRO_SEEK_SET))throw std::runtime_error("Normal-map dimension budget exceeded");
 std::unique_ptr<ALLEGRO_BITMAP,decltype(&al_destroy_bitmap)> bitmap(al_load_bitmap_flags_f(file,".png",ALLEGRO_MEMORY_BITMAP|ALLEGRO_NO_PREMULTIPLIED_ALPHA),al_destroy_bitmap);
 if(!bitmap||al_get_bitmap_width(bitmap.get())!=int(w)||al_get_bitmap_height(bitmap.get())!=int(height))throw std::runtime_error("Cannot decode normal map");
 auto* lock=al_lock_bitmap(bitmap.get(),ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_READONLY);if(!lock)throw std::runtime_error("Cannot read normal map");
 cell::MaterialTexture result{int(w),int(height),{}};result.pixels.resize(size_t(w)*height);
 for(unsigned y=0;y<height;++y){auto* row=static_cast<const unsigned char*>(lock->data)+std::ptrdiff_t(y)*lock->pitch;for(unsigned x=0;x<w;++x)result.pixels[y*w+x]={row[x*4],row[x*4+1],row[x*4+2],255};}al_unlock_bitmap(bitmap.get());return result;
}
void vector(shader_t* sh,const char* key,float x,float y){const float v[]={x,y};shader_set_float_vector(sh,key,2,v);}
}
namespace detail {
cell::MaterialTexture normal_data(const std::string& path){return read_normal_data(path);}
Settings material_settings(const authoring::RenderRegistry& registry,const authoring::Json& m){
 Settings v;auto shader=m.value("shader",authoring::Json{});
 if(shader.is_string()&&registry.shaders.at(shader.get<std::string>()).program=="builtin:terrain-lit"){
  auto& defs=registry.shaders.at(shader.get<std::string>()).parameters;
  auto parameter=[&](const char* key){return m.value("parameters",authoring::Json::object()).value(key,defs.at(key).at("default")).get<float>();};
  v.strength=parameter("normal_strength");v.roughness=parameter("roughness");v.specular=parameter("specular");v.emission=parameter("emission_energy");
 }
 v.directx=m.value("normal_convention",std::string("opengl"))=="directx";
 if(m.contains("uv")){auto& uv=m["uv"];v.rotation=uv["rotation_degrees"];v.scale_x=uv["scale"][0];v.scale_y=uv["scale"][1];v.offset_x=uv["offset"][0];v.offset_y=uv["offset"][1];}return v;
}
Settings light_settings(const authoring::RenderRegistry& registry,const std::string& light,const std::string& profile){
 auto& l=registry.lights.at(light);if(l.type!="directional")throw std::runtime_error("Surface lighting requires a directional light");auto& p=registry.profiles.at(profile);Settings v;
 v.angle=l.angle;v.elevation=l.elevation;v.energy=l.enabled?l.energy:0;v.ambient_energy=p.energy;for(int i=0;i<3;++i){v.color[i]=l.color[i];v.ambient[i]=p.ambient[i];}return v;
}
void bind_light(shader_t* sh,const Settings& v){
 shader_set_float(sh,"light_energy",v.energy);shader_set_float(sh,"ambient_energy",v.ambient_energy);
 float a=v.angle*float(3.141592653589793/180),e=v.elevation*float(3.141592653589793/180);float direction[]={std::cos(a)*std::cos(e),std::sin(a)*std::cos(e),std::sin(e)};
 shader_set_float_vector(sh,"light_direction",3,direction);shader_set_float_vector(sh,"light_color",3,v.color.data());shader_set_float_vector(sh,"ambient_color",3,v.ambient.data());
}
}
void reset_shader(){if(shader_exists(program))shader_clear_uniforms(shader_get(program));}
Surface::~Surface(){reset_shader();if(color_)image_destroy(color_);if(normal_)image_destroy(normal_);}
void Surface::load(const authoring::RenderRegistry& registry,const std::string& library,const std::string& material,const std::string& light,const std::string& profile){
 if(color_)throw std::runtime_error("Surface already loaded");auto& lib=registry.libraries.at(library);const authoring::Json* record=nullptr;for(auto& m:lib.materials)if(m.at("id")==material)record=&m;
 if(!record)throw std::runtime_error("Study material missing from library");auto& m=*record;if(!m.contains("lighting")||!m["lighting"].value("receive",false))throw std::runtime_error("Study material does not receive light");
 auto shader=m.value("shader",authoring::Json{});if(!shader.is_string()||registry.shaders.at(shader.get<std::string>()).program!="builtin:terrain-lit")throw std::runtime_error("Study requires the builtin terrain-lit profile; trusted source is not executed");
 auto definitions=cell::decode_materials(lib.legacy);auto definition=std::find_if(definitions.begin(),definitions.end(),[&](const auto& d){return d.id==material;});
 color_pixels_=cell::load_material_images({*definition},[](const std::string& path){return al_fopen(asset_path(path.c_str()),"rb");}).front();
 normal_pixels_=detail::normal_data(m.contains("maps")&&!m["maps"].value("normal",authoring::Json{}).is_null()?m["maps"]["normal"].get<std::string>():"");
 defaults=detail::material_settings(registry,m);auto illumination=detail::light_settings(registry,light,profile);defaults.angle=illumination.angle;defaults.elevation=illumination.elevation;defaults.energy=illumination.energy;defaults.color=illumination.color;defaults.ambient=illumination.ambient;defaults.ambient_energy=illumination.ambient_energy;
 color_=upload(color_pixels_);normal_=upload(normal_pixels_);
}
void Surface::draw(image_t* output,const Settings& v)const{
 if(!color_||!normal_||!output)throw std::runtime_error("Surface resources unavailable");State state;shader_t* sh=nullptr;
 if(shader_exists(program))sh=shader_get(program);else{char error[1024]="";sh=shader_try_create(program,kernel,error,sizeof error);if(!sh)throw std::runtime_error(std::string("Lighting shader unavailable: ")+error);}
 image_set_drawing_target(output);ALLEGRO_TRANSFORM transform;al_identity_transform(&transform);al_use_transform(&transform);al_orthographic_transform(&transform,0,0,-1,image_width(output),image_height(output),1);al_use_projection_transform(&transform);al_set_clipping_rectangle(0,0,image_width(output),image_height(output));al_set_blender(ALLEGRO_ADD,ALLEGRO_ONE,ALLEGRO_ZERO);
 int cw,ch,nw,nh;al_get_opengl_texture_size(IMAGE2BITMAP(color_),&cw,&ch);al_get_opengl_texture_size(IMAGE2BITMAP(normal_),&nw,&nh);if(cw<=0||ch<=0||nw<=0||nh<=0)throw std::runtime_error("OpenGL texture storage unavailable");
 vector(sh,"color_storage",float(image_width(color_))/cw,float(image_height(color_))/ch);vector(sh,"normal_storage",float(image_width(normal_))/nw,float(image_height(normal_))/nh);
 vector(sh,"uv_scale",v.scale_x,v.scale_y);vector(sh,"uv_offset",v.offset_x,v.offset_y);shader_set_float(sh,"rotation",v.rotation*float(3.141592653589793/180));
 shader_set_sampler(sh,"normal_tex",normal_);shader_set_int(sh,"mode",v.mode);shader_set_bool(sh,"directx",v.directx);
 shader_set_float(sh,"normal_strength",v.strength);shader_set_float(sh,"roughness",v.roughness);shader_set_float(sh,"specular",v.specular);shader_set_float(sh,"emission_energy",v.emission);detail::bind_light(sh,v);
 if(!shader_set_active(sh))throw std::runtime_error("Cannot activate surface shader");al_draw_scaled_bitmap(IMAGE2BITMAP(color_),0,0,image_width(color_),image_height(color_),0,0,image_width(output),image_height(output),0);
}
}
