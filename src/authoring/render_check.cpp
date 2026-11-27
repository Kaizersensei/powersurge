// Native rendering-record cases; only caller-owned fixture copies. Signed: President.
#include "render_records.hpp"
#include <cstdio>
#include <limits>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
namespace {
using namespace ps::authoring;
void render_png(const fs::path& path,int width,unsigned char alpha){auto flags=al_get_new_bitmap_flags();al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);auto* bitmap=al_create_bitmap(width,1);al_set_new_bitmap_flags(flags);if(!bitmap)throw std::runtime_error("Cannot allocate PNG fixture");auto* lock=al_lock_bitmap(bitmap,ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_WRITEONLY);if(!lock){al_destroy_bitmap(bitmap);throw std::runtime_error("Cannot lock PNG fixture");}auto* bytes=static_cast<unsigned char*>(lock->data);for(int x=0;x<width;++x){bytes[x*4]=40;bytes[x*4+1]=80;bytes[x*4+2]=120;bytes[x*4+3]=alpha;}al_unlock_bitmap(bitmap);auto* previous=al_get_new_file_interface();al_set_standard_file_interface();auto* file=al_fopen(path.u8string().c_str(),"wb");al_set_new_file_interface(previous);bool saved=file&&al_save_bitmap_f(file,".png",bitmap);if(file)al_fclose(file);al_destroy_bitmap(bitmap);if(!saved)throw std::runtime_error("Cannot save PNG fixture");}
Json& row(std::vector<Json>& records,const char* id){for(auto& j:records)if(j["id"]==id)return j;throw std::runtime_error("Missing render fixture");}
}
extern "C" int ps_render_record_check(const char* source,const char* owned,char* error,unsigned capacity) {
 int checks=0;try {
  auto check=[&](bool okay,const char* why){if(!okay)throw std::runtime_error(why);++checks;};
  auto refuses=[&](auto action,const char* why){bool refused=false;try{action();}catch(...){refused=true;}check(refused,why);};
  auto input=fs::u8path(source),unit=fs::u8path(owned)/"render-record-unit";check(fs::create_directory(unit),"Fresh unit root required");fs::copy(input/"database",unit/"database",fs::copy_options::recursive);
  auto read=[&](const std::string& p){return read_file(unit/"database"/p);};auto records=read_records(read);auto registry=compile_render(records);
  check(registry.lights.count("light:sun")==1&&registry.lights.count("light:bevel-default")==1&&registry.shaders.size()==2&&registry.profiles.size()==static_cast<size_t>(std::count_if(records.begin(),records.end(),[](const auto& j){return j.value("kind",std::string{})=="lighting_profile";}))&&registry.libraries.size()==static_cast<size_t>(std::count_if(records.begin(),records.end(),[](const auto& j){return j.value("kind",std::string{})=="terrain_material_library";})),"Rendering fixture inventory mismatch");
  auto legacy=read_file(input/"terrain/cell-materials.json");auto projected=registry.legacy_materials("terrain/cell-materials.json");check(parse(projected)==parse(legacy),"Legacy color projection changed existing appearance");check(registry.legacy_materials("terrain/CELL-materials.json")==projected,"Case alias bypassed material authority");check(registry.level_profiles.at("level:cell-lab")=="lighting:outdoor","Level profile reference missing");check(compile_render({}).libraries.empty(),"Legacy project fallback added an implicit material owner");
  for(auto& lib:registry.libraries)for(auto& m:lib.second.materials){auto path=fs::u8path(m["texture"].get<std::string>());fs::create_directories((unit/path).parent_path());if(!fs::exists(unit/path))fs::copy_file(input/path,unit/path);if(m.contains("maps"))for(auto& map:m["maps"])if(map.is_string()){auto p=fs::u8path(map.get<std::string>());fs::create_directories((unit/p).parent_path());if(!fs::exists(unit/p))fs::copy_file(input/p,unit/p);}}
  auto bad_case=[&](const std::function<void(std::vector<Json>&)>& change,const char* why){auto bad=records;change(bad);refuses([&]{compile_render(bad);},why);};
  bad_case([](auto& r){row(r,"light:sun")["energy"]=-1;},"Negative light energy accepted");
  bad_case([](auto& r){row(r,"light:sun")["energy"]=std::numeric_limits<double>::infinity();},"Infinite light energy accepted");
  bad_case([](auto& r){row(r,"light:sun")["color"]=Json::array({1,0});},"Wrong color dimensions accepted");
  bad_case([](auto& r){row(r,"light:sun")["color"]=Json::array({1,0,1.1});},"Color out of range accepted");
  bad_case([](auto& r){row(r,"light:sun")["angle_degrees"]=360;},"Aliased angle accepted");
  bad_case([](auto& r){row(r,"light:spot")["inner_degrees"]=80;row(r,"light:spot")["outer_degrees"]=20;},"Inverted spot cone accepted");
  bad_case([](auto& r){row(r,"lighting:outdoor")["sun"]="light:missing";},"Missing light reference accepted");
  bad_case([](auto& r){row(r,"lighting:outdoor")["sun"]="light:torch";},"Point light accepted for sun");
  bad_case([](auto& r){row(r,"level:cell-lab")["properties"]["lighting"]["profile"]="lighting:missing";},"Missing level profile accepted");
  bad_case([](auto& r){r.push_back(row(r,"light:sun"));},"Duplicate light accepted");
  bad_case([](auto& r){auto copy=row(r,"material-library:terrain");copy["id"]="material-library:second";copy["legacy_path"]="terrain/CELL-materials.json";r.push_back(copy);},"Case-aliased material owner accepted");
  bad_case([](auto& r){row(r,"material-library:terrain")["materials"][0]["texture"]="textures/../stone.png";},"Texture traversal accepted");
  bad_case([](auto& r){row(r,"material-library:terrain")["materials"][0]["maps"]["normal"]="outside/normal.png";},"Out-of-root normal map accepted");
  bad_case([](auto& r){row(r,"material-library:terrain")["materials"][0]["maps"]["unknown"]="textures/stone.png";},"Unknown map role accepted");
  bad_case([](auto& r){row(r,"material-library:terrain")["materials"][0]["lighting"]["transmission"]=1.1;},"Excess transmission accepted");
  bad_case([](auto& r){row(r,"material-library:terrain")["materials"][0]["uv"]["scale"]=Json::array({0,1});},"Zero UV scale accepted");
  bad_case([](auto& r){row(r,"material-library:terrain")["materials"][0]["bevel"]["contact"]="group-only";},"Unknown bevel mode accepted");
  bad_case([](auto& r){row(r,"material-library:terrain")["materials"][0]["parameters"]={{"roughness",.2}};},"Unregistered material uniforms accepted");
  bad_case([](auto& r){auto& m=row(r,"material-library:terrain")["materials"][0];m["shader"]="shader:terrain-lit";m["parameters"]={{"roughness",2}};},"Out-of-range material override accepted");
  bad_case([](auto& r){row(r,"shader:terrain-lit")["parameters"]["roughness"]["type"]="bool";},"Builtin shader ABI altered");
  bad_case([](auto& r){row(r,"shader:terrain-lit")["parameters"]["roughness"]["maximum"]=100;},"Builtin uniform bounds widened incompatibly");
  bad_case([](auto& r){auto& sh=row(r,"shader:terrain-unlit");sh["program"]="trusted-source";sh["source"]="shaders/../unsafe.glsl";},"Shader source traversal accepted");
  for(auto* name:{"bad-name","a.b","1first","gl_reserved"})bad_case([&](auto& r){auto& p=row(r,"shader:terrain-unlit");p["program"]="trusted-source";p["source"]="shaders/test.glsl";p["parameters"]={{name,{{"type","bool"},{"default",true}}}};},"Invalid uniform name accepted");
  bad_case([](auto& r){row(r,"level:lighting-study")["properties"]["surface_study"]["library"]="material-library:missing";},"Missing study library accepted");
  bad_case([](auto& r){row(r,"level:lighting-study")["properties"]["surface_study"]["material"]="material:stone";},"Foreign study member accepted");
  bad_case([](auto& r){row(r,"level:lighting-study")["properties"]["surface_study"]["light"]="light:torch";},"Non-directional study light accepted");
  auto mirrored=records;row(mirrored,"material-library:terrain")["materials"][0]["uv"]["scale"]=Json::array({-1,1});check(compile_render(mirrored).libraries.size()==registry.libraries.size(),"Mirrored UV metadata refused");
  auto good=records;auto& m=row(good,"material-library:terrain")["materials"][0];m["shader"]="shader:terrain-lit";m["parameters"]={{"roughness",.4},{"normal_strength",2}};m["maps"]["normal"]="generated/stone/revision/normal.png";check(compile_render(good).libraries.size()==registry.libraries.size(),"Valid generated-map metadata refused");
  Workspace workspace(unit,unit/"recovery");auto doc=workspace.open("material-library:terrain");check(doc==workspace.open(doc->id),"Material surface forked its document");auto original=parse(doc->source),changed=original;changed["materials"][0]["tint"][0]=128;changed["materials"][0]["extension"]={{"keep",true}};changed["extension"]={{"keep",true}};
  workspace.edit(*doc,changed.dump(2));check(workspace.undo(*doc)&&parse(doc->source)==original,"Material undo failed");check(workspace.redo(*doc)&&parse(doc->source)==changed,"Material redo failed");workspace.save(*doc);auto saved=parse(read_file(unit/"database"/doc->file));check(saved==changed,"Material save lost unsupported fields");check(compile_render(read_records(read)).libraries.at(doc->id).materials[0]["tint"][0]==128,"Saved material did not compile");check(read_file(input/"terrain/cell-materials.json")==legacy,"Material editing rewrote compatibility source");auto uses=workspace.usages(doc->id);check(std::find(uses.begin(),uses.end(),"level:cell-lab")!=uses.end(),"Compatibility attachment absent from usages");
  render_png(unit/"textures/native-transparent.png",1,128);render_png(unit/"textures/native-oversize.png",1025,255);for(auto* file:{"textures/native-transparent.png","textures/native-oversize.png"}){auto bad=changed;bad["materials"][0]["texture"]=file;workspace.edit(*doc,bad.dump());refuses([&]{workspace.save(*doc);},"Runtime-incompatible color image was saved");check(read_file(unit/"database"/doc->file)==doc->baseline,"Rejected image changed material source");workspace.reload(*doc,true);}
  auto rejected=changed;rejected["materials"][0]["maps"]["normal"]="textures/missing-normal.png";workspace.edit(*doc,rejected.dump());refuses([&]{workspace.save(*doc);},"Missing map saved");check(read_file(unit/"database"/doc->file)==doc->baseline&&doc->dirty(),"Failed map save damaged baseline/draft");workspace.reload(*doc,true);
  rejected=changed;rejected["palette"]=true;workspace.edit(*doc,rejected.dump());refuses([&]{workspace.save(*doc);},"Shared palette membership changed without migration");workspace.reload(*doc,true);
  rejected=changed;rejected["legacy_path"]="terrain/other.json";workspace.edit(*doc,rejected.dump());refuses([&]{workspace.save(*doc);},"Material attachment migrated silently");workspace.reload(*doc,true);rejected=changed;rejected["materials"][0]["id"]="material:renamed";workspace.edit(*doc,rejected.dump());refuses([&]{workspace.save(*doc);},"Material identity changed silently");workspace.reload(*doc,true);
  auto external=changed;external["name"]="External material change";atomic_file(unit/"database"/doc->file,external.dump());workspace.edit(*doc,original.dump());refuses([&]{workspace.save(*doc);},"External library overwritten");check(parse(read_file(unit/"database"/doc->file))==external,"Conflict refusal changed disk");
  return checks;
 }catch(const std::exception& e){if(error&&capacity)snprintf(error,capacity,"%s",e.what());return -1;}
}
