#include "terrain.hpp"
#include "render_util.hpp"
#include <allegro5/allegro_opengl.h>
namespace ps::lighting {
namespace {
const char* program="__ps_terrain_lit_v1";
const char kernel[]=FRAGMENT_SHADER_GLSL_PREFIX("highp") R"GLSL(
uniform sampler2D tex,normal_tex,properties_tex,bevel_tex;
uniform bool has_bevel;
uniform vec2 color_storage,normal_storage,properties_storage,bevel_storage;
uniform float light_energy,ambient_energy;
uniform vec3 light_direction,light_color,ambient_color;
)GLSL"
#include "shading.inc"
R"GLSL(
void main(){
 vec4 base=texture2D(tex,v_texcoord);if(base.a==0.0)discard;
 vec2 q=v_texcoord/color_storage;
 vec4 normal=texture2D(normal_tex,q*normal_storage);
 vec4 rules=texture2D(properties_tex,q*properties_storage);
 vec3 n=normalize(normal.rgb*2.0-1.0);
 vec4 bevel=has_bevel?texture2D(bevel_tex,q*bevel_storage):vec4(.5,.5,1.0,0.0);vec3 geometry=normalize(bevel.rgb*2.0-1.0);
 if(bevel.a>0.0){vec3 t=geometry+vec3(0.0,0.0,1.0),u=n*vec3(-1.0,-1.0,1.0);n=normalize(t*dot(t,u)/t.z-u);}
 float emission=(rules.g*65280.0+rules.b*255.0)/65535.0*100.0;
 vec3 lit=shade_surface(base.rgb,n,normal.a,rules.r,emission);
 vec3 unlit=base.rgb;
 if(bevel.a>0.0){vec3 flat_response=linearize(shade_surface(base.rgb,vec3(0.0,0.0,1.0),normal.a,rules.r,0.0));vec3 edge=linearize(shade_surface(base.rgb,geometry,normal.a,rules.r,0.0));unlit=mix(base.rgb,encode(linearize(base.rgb)*edge/max(flat_response,vec3(.005))),bevel.a);}
 gl_FragColor=vec4(mix(unlit,lit,rules.a)*base.a,base.a)*v_color;
}
)GLSL";
std::array<double,3> unit(std::array<double,3> n){double length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(length<.000001)return {0,0,1};for(auto& v:n)v/=length;return n;}
unsigned char byte(double x){return static_cast<unsigned char>(std::lround(std::clamp(x,0.,1.)*255));}
}
std::shared_ptr<TerrainMaterials> TerrainMaterials::compile(const authoring::RenderRegistry& registry,const std::string& level,const std::string& path,const std::vector<cell::Material>& definitions,std::string& diagnostic,bool force_bevel){
 auto attachment=registry.level_profiles.find(level);const typename decltype(registry.profiles)::mapped_type* profile=nullptr;
 if(attachment!=registry.level_profiles.end()&&!attachment->second.empty())profile=&registry.profiles.at(attachment->second);
 bool receives=profile&&profile->driver=="static";
 const authoring::TerrainMaterialLibrary* library=nullptr;for(auto& p:registry.libraries)if(authoring::render_fold(p.second.path)==authoring::render_fold(path))library=&p.second;
 if(!library)return {};
 auto result=std::make_shared<TerrainMaterials>();
 if(receives){if(!profile->sun.empty())result->light=detail::light_settings(registry,profile->sun,profile->id);else{result->light.energy=0;result->light.ambient_energy=profile->energy;for(int i=0;i<3;++i)result->light.ambient[i]=profile->ambient[i];}}
 std::map<std::string,const authoring::Json*> definitions_by_id;for(auto& m:library->materials)definitions_by_id[m.at("id").get<std::string>()]=&m;for(auto& shared:registry.libraries)if(shared.second.palette)for(auto& m:shared.second.materials)definitions_by_id[m.at("id").get<std::string>()]=&m;
 std::map<std::string,std::shared_ptr<cell::MaterialTexture>> maps;size_t pixels=0;
 for(auto& definition:definitions){
  auto found=definitions_by_id.find(definition.id);if(found==definitions_by_id.end())throw std::runtime_error("Terrain material identity mismatch");auto& m=*found->second;TerrainSlot slot;if(m.contains("bevel")&&!m["bevel"].is_null()){auto& b=m["bevel"];slot.bevel={b["enabled"],b["profile"],b["contact"],b["width_m"],b["depth_m"],b["smoothness"]};cell::validate_bevel(slot.bevel);force_bevel|=slot.bevel.enabled;}slot.values=detail::material_settings(registry,m);
  double radians=slot.values.rotation*3.141592653589793/180;slot.cosine=std::cos(radians);slot.sine=std::sin(radians);
  auto sh=m.value("shader",authoring::Json{});slot.receive=receives&&m.contains("lighting")&&m["lighting"].value("receive",false)&&sh.is_string()&&registry.shaders.at(sh.get<std::string>()).program=="builtin:terrain-lit";
  if(slot.receive)try{
   std::string file;if(m.contains("maps")&&m["maps"].value("normal",authoring::Json{}).is_string())file=m["maps"]["normal"].get<std::string>();auto cached=maps.find(file);
   if(cached!=maps.end())slot.normal=cached->second;else{auto normal=std::make_shared<cell::MaterialTexture>(detail::normal_data(file));if(pixels+normal->pixels.size()>8*1024*1024)throw std::runtime_error("Normal-map library exceeds8M unique pixels");pixels+=normal->pixels.size();maps.emplace(file,normal);slot.normal=std::move(normal);}
  }catch(const std::exception& e){slot.receive=false;diagnostic+=(diagnostic.empty()?"":"; ")+definition.id+": "+e.what();if(diagnostic.size()>2048)diagnostic.resize(2048);}
  result->slots.push_back(std::move(slot));
 }
 if(!receives&&!force_bevel){if(profile)diagnostic="Calendar light projection is pending; terrain retains unlit appearance";return {};}
 if(!receives){auto fallback=registry.profiles.find("lighting:bevel-default");if(fallback==registry.profiles.end())throw std::runtime_error("Bevel needs a static level lighting profile or lighting:bevel-default");auto& p=fallback->second;if(p.driver!="static"||p.sun.empty())throw std::runtime_error("Bevel fallback profile requires a static directional light");result->light=detail::light_settings(registry,p.sun,p.id);diagnostic="Bevel-only fallback: lighting:bevel-default. Calendar light projection remains pending; unlit interiors retain their color.";}
 result->diagnostics=diagnostic;return result;
}
MaterialSample TerrainMaterials::sample(size_t index,int x,int y,const cell::MaterialTexture& color)const{
 auto& slot=slots.at(index);auto& v=slot.values;double u=(x+.5)/color.w-.5,t=(y+.5)/color.h-.5;
 double qx=(slot.cosine*u+slot.sine*t)*v.scale_x+.5+v.offset_x,qy=(-slot.sine*u+slot.cosine*t)*v.scale_y+.5+v.offset_y;qx-=std::floor(qx);qy-=std::floor(qy);
 auto at=[&](const cell::MaterialTexture& tex){return tex.pixels[std::min(tex.h-1,int(qy*tex.h))*tex.w+std::min(tex.w-1,int(qx*tex.w))];};MaterialSample sample;sample.color=at(color);
 if(slot.receive){auto n=at(*slot.normal);double nx=(n.r/255.*2-1)*(v.scale_x<0?-1:1)*v.strength,ny=-(n.g/255.*2-1)*(v.directx?-1:1)*(v.scale_y<0?-1:1)*v.strength;sample.normal=v.strength==0?std::array<double,3>{0,0,1}:unit({slot.cosine*nx-slot.sine*ny,slot.sine*nx+slot.cosine*ny,n.b/255.*2-1});sample.receive=1;sample.roughness=v.roughness;sample.specular=v.specular;sample.emission=v.emission;}return sample;
}
MaterialSample mix_materials(MaterialSample a,MaterialSample b,double w){
 auto mix=[&](double x,double y){return x*(1-w)+y*w;};a.color={static_cast<uint8_t>(mix(a.color.r,b.color.r)+.5),static_cast<uint8_t>(mix(a.color.g,b.color.g)+.5),static_cast<uint8_t>(mix(a.color.b,b.color.b)+.5),255};for(int i=0;i<3;++i)a.normal[i]=mix(a.normal[i],b.normal[i]);a.normal=unit(a.normal);a.roughness=mix(a.roughness,b.roughness);a.specular=mix(a.specular,b.specular);a.emission=mix(a.emission,b.emission);a.receive=mix(a.receive,b.receive);return a;
}
std::pair<cell::MaterialPixel,cell::MaterialPixel> pack_material(const MaterialSample& m){
 unsigned emission=std::lround(std::clamp(m.emission,0.,100.)/100*65535);
 return {{byte(m.normal[0]*.5+.5),byte(m.normal[1]*.5+.5),byte(m.normal[2]*.5+.5),byte(m.roughness)},{byte(m.specular),uint8_t(emission>>8),uint8_t(emission&255),byte(m.receive)}};
}
void terrain_reset_shader(){if(shader_exists(program))shader_clear_uniforms(shader_get(program));}
struct TerrainPass::Impl {detail::State state;shader_t* shader=nullptr;};
TerrainPass::TerrainPass(const Settings& light):impl(std::make_unique<Impl>()){
 if(shader_exists(program))impl->shader=shader_get(program);else{char error[1024]="";impl->shader=shader_try_create(program,kernel,error,sizeof error);if(!impl->shader)throw std::runtime_error(std::string("Terrain lighting unavailable: ")+error);}detail::bind_light(impl->shader,light);
}
TerrainPass::~TerrainPass()=default;
void TerrainPass::draw(image_t* color,image_t* normal,image_t* properties,int x,int y,image_t* bevel){
 auto* sh=impl->shader;auto storage=[&](const char* name,image_t* bitmap){int w,h;al_get_opengl_texture_size(IMAGE2BITMAP(bitmap),&w,&h);if(w<=0||h<=0)throw std::runtime_error("Terrain texture storage unavailable");const float size[]={float(image_width(bitmap))/w,float(image_height(bitmap))/h};shader_set_float_vector(sh,name,2,size);};
 storage("color_storage",color);storage("normal_storage",normal);storage("properties_storage",properties);shader_set_sampler(sh,"normal_tex",normal);shader_set_sampler(sh,"properties_tex",properties);shader_set_bool(sh,"has_bevel",bevel!=nullptr);shader_set_sampler(sh,"bevel_tex",bevel?bevel:normal);if(bevel)storage("bevel_storage",bevel);
 if(!shader_set_active(sh))throw std::runtime_error("Cannot activate terrain lighting");image_draw(color,x,y,IF_NONE);
}
}
