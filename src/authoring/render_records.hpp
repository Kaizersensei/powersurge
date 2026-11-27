// File-owned rendering contracts; no renderer or gameplay state. Signed: President.
#pragma once
#include "documents.hpp"
#include "../environment/sky_profile.hpp"
#include "../foundation/weather_records.hpp"
#include "../cell_terrain/materials.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
namespace ps::authoring {
inline bool render_kind(const std::string& kind) {
 return kind=="sky_profile"||kind=="terrain_material_library"||kind=="shader_profile"||kind=="light"||kind=="lighting_profile";
}
inline void render_require(bool okay,const std::string& why) {if(!okay)throw std::runtime_error(why);}
inline std::string render_text(const Json& j,const char* key) {
 render_require(j.contains(key)&&j[key].is_string(),std::string(key)+": text required");
 auto s=j[key].get<std::string>();render_require(!s.empty()&&s.size()<=256&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32;}),std::string(key)+": invalid text");return s;
}
inline bool render_flag(const Json& j,const char* key) {render_require(j.contains(key)&&j[key].is_boolean(),std::string(key)+": boolean required");return j[key].get<bool>();}
inline double render_number(const Json& j,const char* key,double low,double high) {
 render_require(j.contains(key)&&j[key].is_number(),std::string(key)+": number required");double n=j[key].get<double>();render_require(std::isfinite(n)&&n>=low&&n<=high,std::string(key)+": out of range");return n;
}
inline void render_id(const std::string& id,const std::string& prefix) {
 render_require(id.rfind(prefix,0)==0&&id.size()>prefix.size()&&id.size()<=128&&std::all_of(id.begin()+prefix.size(),id.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='_'||c=='-'||c=='.');}),"Invalid identity: "+id);
}
inline bool render_path(const std::string& path,const std::string& root,const std::string& suffix) {
 if(path.size()>256||path.size()<=root.size()+suffix.size()||path.rfind(root,0)!=0||path.substr(path.size()-suffix.size())!=suffix)return false;
 if(!std::all_of(path.begin(),path.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='_'||c=='-'||c=='.'||c=='/');}))return false;
 size_t at=0;while(at<path.size()){auto end=path.find('/',at);if(end==path.npos)end=path.size();auto part=path.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;at=end+1;}return true;
}
inline std::array<double,3> render_color(const Json& value) {
 render_require(value.is_array()&&value.size()==3,"Color requires three sRGB channels");std::array<double,3> out{};
 for(size_t i=0;i<3;++i){render_require(value[i].is_number(),"Numeric color required");out[i]=value[i].get<double>();render_require(std::isfinite(out[i])&&out[i]>=0&&out[i]<=1,"Color channels must be0–1");}return out;
}
inline void render_vector(const Json& v,size_t count,double low,double high) {
 render_require(v.is_array()&&v.size()==count,"Invalid vector length");for(auto& x:v){render_require(x.is_number(),"Numeric vector required");double n=x.get<double>();render_require(std::isfinite(n)&&n>=low&&n<=high,"Vector out of range");}
}
inline void render_parameter(const Json& specification,const Json& value) {
 auto type=render_text(specification,"type");
 if(type=="bool")render_require(value.is_boolean(),"Boolean parameter required");
 else if(type=="color")render_color(value);
 else if(type=="float"||type=="int") {
  render_require(value.is_number()&&(type!="int"||value.is_number_integer()),"Wrong numeric parameter type");
  double low=render_number(specification,"minimum",-1000000,1000000),high=render_number(specification,"maximum",-1000000,1000000),n=value.get<double>();
  render_require(low<=high&&std::isfinite(n)&&n>=low&&n<=high,"Parameter outside registered bounds");
 } else if(type=="vec2")render_vector(value,2,-1000000,1000000);
 else render_require(false,"Unsupported parameter type: "+type);
}
struct ShaderProfile {std::string id,program,source;Json parameters;};
struct LightDefinition {
 std::string id,type;std::array<double,3> color{};bool enabled=false,shadows=false;
 double energy=0,angle=0,elevation=0,height=0,range=0,falloff=1,inner=0,outer=0;
};
struct LightingProfile {std::string id,driver,sun,moon;std::array<double,3> ambient{};double energy=0;};
struct TerrainMaterialLibrary {std::string id,name,path,legacy;Json materials;bool palette=false;};
inline std::string render_fold(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});return value;}
struct SurfaceStudyDefinition {std::string library,material,light,profile;};
struct RenderRegistry {
 std::map<std::string,environment::SkyProfile> skies;std::map<std::string,environment::SkyAttachment> level_skies;
 std::map<std::string,SurfaceStudyDefinition> studies;
 std::map<std::string,ShaderProfile> shaders;std::map<std::string,LightDefinition> lights;std::map<std::string,LightingProfile> profiles;std::map<std::string,TerrainMaterialLibrary> libraries;
 std::map<std::string,std::string> level_profiles;
 std::string legacy_materials(const std::string& path)const {for(auto& p:libraries)if(render_fold(p.second.path)==render_fold(path))return p.second.legacy;return {};}
};
inline std::string render_reference(const Json& j,const char* key) {render_require(j.contains(key)&&(j[key].is_null()||j[key].is_string()),std::string(key)+": reference or null required");return j[key].is_null()?std::string{}:render_text(j,key);}
inline Json legacy_material_projection(const Json& materials) {
 Json result=Json::array();for(auto& material:materials){Json row;for(auto* key:{"id","name","texture","tint","shader","physical_profile","acoustic_profile","feedback","reactions"})row[key]=material.at(key);Json feedback;for(auto* key:{"impact","footstep","damage","destruction"})feedback[key]=material.at("feedback").at(key);row["feedback"]=feedback;result.push_back(std::move(row));}return {{"version",1},{"materials",result}};
}
inline RenderRegistry compile_render(const std::vector<Json>& records) {
 RenderRegistry registry;std::set<std::string> paths,material_ids;
 for(auto& j:records)if(render_kind(j.value("kind",std::string{}))) {
  auto kind=render_text(j,"kind"),id=render_text(j,"id");render_require(j.at("version")==1,"Unsupported rendering record version");render_text(j,"name");
  if(kind=="sky_profile") {
   render_id(id,"sky:");auto p=environment::sky_profile(j);auto climate=compiled_weather();for(auto& entry:p.weather)render_require(std::any_of(climate.presets.begin(),climate.presets.end(),[&](auto& preset){return preset.id==entry.first;}),"Sky references unknown weather preset");render_require(registry.skies.emplace(id,std::move(p)).second,"Duplicate sky profile");
  } else if(kind=="shader_profile") {
   render_id(id,"shader:");ShaderProfile p{id,render_text(j,"program"),render_reference(j,"source"),j.at("parameters")};
   render_require(p.program=="builtin:terrain-unlit"||p.program=="builtin:terrain-lit"||p.program=="trusted-source","Unknown shader program");
   render_require(p.program=="trusted-source"?render_path(p.source,"shaders/",".glsl"):p.source.empty(),"Invalid shader source declaration");
   render_require(p.parameters.is_object()&&p.parameters.size()<=32,"Shader parameters require a bounded object");
   for(auto it=p.parameters.begin();it!=p.parameters.end();++it){auto name=it.key();render_require(!name.empty()&&name.size()<=64&&(std::isalpha(static_cast<unsigned char>(name[0]))||name[0]=='_')&&name.rfind("gl_",0)!=0&&std::all_of(name.begin(),name.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='_');}),"Invalid uniform identifier");render_parameter(it.value(),it.value().at("default"));}
   if(p.program=="builtin:terrain-unlit")render_require(p.parameters.empty(),"Unlit builtin has no parameter uniforms");
   if(p.program=="builtin:terrain-lit"){render_require(p.parameters.size()==4,"Lit builtin requires its four declared uniforms");for(auto* uniform:{"normal_strength","roughness","specular","emission_energy"})render_require(p.parameters.contains(uniform)&&p.parameters[uniform].at("type")=="float","Lit builtin uniform type/name changed");for(auto* uniform:{"normal_strength","roughness","specular","emission_energy"}){auto& spec=p.parameters[uniform];double ceiling=std::string(uniform)=="normal_strength"?16:std::string(uniform)=="emission_energy"?100:1;render_require(render_number(spec,"minimum",0,ceiling)<=render_number(spec,"maximum",0,ceiling),"Lit builtin parameter bounds changed incompatibly");}}
   render_require(registry.shaders.emplace(id,std::move(p)).second,"Duplicate shader identity");
  } else if(kind=="light") {
   render_id(id,"light:");LightDefinition l;l.id=id;l.type=render_text(j,"type");render_require(l.type=="directional"||l.type=="point"||l.type=="spot","Unknown light type");
   l.color=render_color(j.at("color"));l.enabled=render_flag(j,"enabled");l.shadows=render_flag(j,"shadows");l.energy=render_number(j,"energy",0,100);
   l.angle=render_number(j,"angle_degrees",0,360);render_require(l.angle<360,"Angle360 aliases0; use0–359.999");l.elevation=render_number(j,"elevation_degrees",0,90);
   l.height=render_number(j,"height_m",0,100000);l.range=render_number(j,"range_m",.001,100000);l.falloff=render_number(j,"falloff",.01,16);l.inner=render_number(j,"inner_degrees",0,180);l.outer=render_number(j,"outer_degrees",.001,180);render_require(l.inner<=l.outer,"Spot inner angle exceeds outer angle");
   render_require(registry.lights.emplace(id,std::move(l)).second,"Duplicate light identity");
  } else if(kind=="lighting_profile") {
   render_id(id,"lighting:");LightingProfile p{id,render_text(j,"driver"),render_reference(j,"sun"),render_reference(j,"moon"),render_color(j.at("ambient").at("color")),render_number(j.at("ambient"),"energy",0,100)};
   render_require(p.driver=="static"||p.driver=="calendar","Unknown lighting driver");render_require(registry.profiles.emplace(id,std::move(p)).second,"Duplicate lighting profile");
  } else {
   render_id(id,"material-library:");auto path=render_text(j,"legacy_path");render_require(render_path(path,"terrain/",".json"),"Invalid material-library compatibility path");
   auto folded=path;std::transform(folded.begin(),folded.end(),folded.begin(),[](unsigned char c){return char(std::tolower(c));});render_require(paths.insert(folded).second,"Two libraries own one compatibility path");
   auto materials=j.at("materials");render_require(materials.is_array()&&!materials.empty()&&materials.size()<=4096,"Material library requires1–4096 materials");
   auto projection=legacy_material_projection(materials).dump();cell::decode_materials(projection);
   for(auto& m:materials){auto material=render_text(m,"id");render_id(material,"material:");render_require(material_ids.insert(material).second,"Duplicate material identity across libraries");}
   render_require(registry.libraries.emplace(id,TerrainMaterialLibrary{id,render_text(j,"name"),path,projection,materials,j.contains("palette")?render_flag(j,"palette"):false}).second,"Duplicate material library");
  }
 }
 for(auto& entry:registry.profiles)for(auto& ref:{entry.second.sun,entry.second.moon})if(!ref.empty()){auto light=registry.lights.find(ref);render_require(light!=registry.lights.end()&&light->second.type=="directional","Daylight reference requires a directional light: "+ref);}
 for(auto& entry:registry.libraries)for(auto& m:entry.second.materials) {
  auto shader=render_reference(m,"shader");if(!shader.empty())render_require(registry.shaders.count(shader)!=0,"Missing material shader profile: "+shader);
  if(m.contains("maps")){render_require(m["maps"].is_object(),"Material maps require an object");for(auto it=m["maps"].begin();it!=m["maps"].end();++it){render_require(it.key()=="normal"||it.key()=="height"||it.key()=="roughness"||it.key()=="ao"||it.key()=="emission","Unknown texture role");if(!it.value().is_null()){render_require(it.value().is_string(),"Texture path required");auto file=it.value().get<std::string>();render_require(render_path(file,"textures/",".png")||render_path(file,"generated/",".png"),"Invalid companion-map path");}}}
  if(m.contains("normal_convention"))render_require(m["normal_convention"]=="opengl"||m["normal_convention"]=="directx","Unknown normal-map convention");
  if(m.contains("uv")){auto& uv=m["uv"];render_vector(uv.at("scale"),2,-100000,100000);for(auto& axis:uv["scale"])render_require(std::abs(axis.get<double>())>=.0001,"Zero UV scale");render_vector(uv.at("offset"),2,-100000,100000);render_number(uv,"rotation_degrees",-360,360);}
  if(m.contains("lighting")){auto& l=m["lighting"];render_flag(l,"receive");render_flag(l,"block");render_number(l,"transmission",0,1);}
  if(m.contains("parameters")){render_require(m["parameters"].is_object(),"Material parameters require an object");render_require(!shader.empty()||m["parameters"].empty(),"Material parameters need a shader profile");for(auto it=m["parameters"].begin();it!=m["parameters"].end();++it){auto& defs=registry.shaders.at(shader).parameters;render_require(defs.contains(it.key()),"Unregistered material parameter: "+it.key());render_parameter(defs.at(it.key()),it.value());}}
  if(m.contains("bevel")&&!m["bevel"].is_null()){auto& b=m["bevel"];render_flag(b,"enabled");auto profile=render_text(b,"profile");render_require(profile=="linear"||profile=="round"||profile=="concave","Unknown bevel profile");render_number(b,"width_m",0,32);render_number(b,"depth_m",0,32);render_number(b,"smoothness",0,1);auto mode=render_text(b,"contact");render_require(mode=="separate"||mode=="continuous"||mode=="soft_seam","Unknown bevel contact mode");}
 }
 for(auto& j:records)if(j.value("kind",std::string{})=="level"&&j.at("properties").contains("sky")){
  auto& s=j["properties"]["sky"];if(s.is_null())continue;render_require(s.is_object(),"Sky attachment requires an object or null");auto ref=render_reference(s,"profile");if(ref.empty())continue;render_require(registry.skies.count(ref),"Missing level sky profile: "+ref);environment::SkyAttachment a{ref,render_number(s,"origin_y_px",-10000000,10000000),render_number(s,"px_per_m",.001,100000),render_flag(s,"visible_indoors")};render_require(registry.level_skies.emplace(render_text(j,"id"),std::move(a)).second,"Duplicate level sky attachment");
 }
 for(auto& j:records)if(j.value("kind",std::string{})=="level"&&j.at("properties").contains("lighting")){auto ref=render_reference(j["properties"]["lighting"],"profile");if(!ref.empty())render_require(registry.profiles.count(ref)!=0,"Missing level lighting profile: "+ref);registry.level_profiles.emplace(render_text(j,"id"),ref);}
 for(auto& j:records)if(j.value("kind",std::string{})=="level"&&j.at("properties").contains("surface_study")){
  auto& s=j["properties"]["surface_study"];SurfaceStudyDefinition d{render_text(s,"library"),render_text(s,"material"),render_text(s,"light"),render_text(s,"profile")};
  render_require(registry.libraries.count(d.library)&&registry.lights.count(d.light)&&registry.profiles.count(d.profile),"Missing surface-study definition");
  render_require(registry.lights.at(d.light).type=="directional","Study requires a directional light");bool found=false;
  for(auto& m:registry.libraries.at(d.library).materials)if(m["id"]==d.material){auto sh=render_reference(m,"shader");found=!sh.empty()&&registry.shaders.at(sh).program=="builtin:terrain-lit"&&m.contains("lighting")&&render_flag(m["lighting"],"receive");}
  render_require(found,"Study requires a receiving builtin-lit material from its library");registry.studies.emplace(render_text(j,"id"),std::move(d));
 }
 render_require(registry.skies.size()<=128,"Sky profile budget exceeded");
 render_require(registry.lights.size()<=1024&&registry.profiles.size()<=128&&registry.shaders.size()<=128&&registry.libraries.size()<=64,"Rendering registry budget exceeded");return registry;
}
const RenderRegistry& render_snapshot();
}
