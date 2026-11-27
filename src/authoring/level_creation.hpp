// Pure creation plan; registry publication is the commit point. Signed: President.
#pragma once
#include "world_records.hpp"
#include "../cell_terrain/terrain.hpp"
namespace ps::authoring {
struct LevelPlan {std::string id,sequence;Json index;std::map<std::string,std::string> files;};
inline LevelPlan level_plan(const Json& index,const std::string& slug,const std::string& name,const std::string& parent=""){
 if(slug.empty()||slug.size()>48||slug[0]<'a'||slug[0]>'z'||!std::all_of(slug.begin(),slug.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-';}))throw std::runtime_error("Key: use1–48 lowercase letters, digits or hyphens, beginning with a letter");
 if(name.empty()||name.size()>128||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;}))throw std::runtime_error("Name: use1–128 bytes without control characters");
 const std::string base="user-"+slug,scene="levels/"+base+".lev",terrain="terrain/"+base+".json",route="quests/"+base+".qst",materials="terrain/cell-materials.json";
 LevelPlan plan{"level:"+base,"sequence:"+base,index,{}};if(index.at("format")!=1||!index.at("records").is_array()||index["records"].size()>4094)throw std::runtime_error("Unsupported/full project index");
 for(auto& e:index["records"])if(e.at("id")==plan.id||e.at("id")==plan.sequence)throw std::runtime_error("Level or sequence key is already registered");
 auto encoded=[](const Json& value){return value.dump(2)+"\n";};auto add=[&](const std::string& id,const std::string& file,const Json& j){for(auto& e:plan.index["records"])if(e.at("file")==file)throw std::runtime_error("Record file is already indexed");plan.index["records"].push_back({{"id",id},{"file",file}});plan.files["database/"+file]=encoded(j);};
 add(plan.id,"records/level-"+base+".json",{{"version",1},{"kind","level"},{"id",plan.id},{"name",name},{"scene",scene},{"parent",parent},{"location",{{"country",""},{"locality",name}}},{"properties",{{"editable",true},{"weather",{{"region",""},{"hemisphere","inherit"},{"indoor",false},{"preset",""},{"wind_factor",1},{"precipitation_factor",1}}},{"terrain",{{"document",terrain},{"materials",materials}}}}}});
 add(plan.sequence,"records/sequence-"+base+".json",{{"version",1},{"kind","sequence"},{"id",plan.sequence},{"name",name},{"file",route},{"levels",Json::array({plan.id})}});
 cell::Document ground;ground.shapes.push_back({1,"material:stone","square",false,1,0});ground.strokes.push_back({1,false,-256,128,256,128,128,0,.5});plan.files[terrain]=cell::encode(ground);
 plan.files[scene]="// New source level; cell coordinates use native4096,4096 as world zero. Signed: President.\nname "+Json(name).dump()+"\nauthor \"PowerSurge\"\nversion \"0.1\"\nrequires \"0.6.1\"\ntheme \"themes/presentation.brk\"\nbgtheme \"themes/template.bg\"\nspawn_point 4096 4096\nsetup \"BSL Runtime Setup\" \"Default Camera\" \"BSL Pause and Quit\"\nplayers \"None\"\nbrick 0 0 0\nbrick 0 8192 8192\nentity \"BOR Michio\" 4096 4096\n";
 if(plan.index.dump(2).size()+1>1024*1024)throw std::runtime_error("Creation index exceeds1MiB runtime/recovery limit");
 plan.files[route]="// Compatibility fallback; indexed sequence is authoritative. Signed: President.\nname "+Json(name).dump()+"\nlevel \""+scene+"\"\n";return plan;
}
inline Json creation_journal(const LevelPlan& p,const std::string& before,const std::string& slug,const std::string& name,const std::string& parent){return {{"format",1},{"operation","create_level"},{"slug",slug},{"name",name},{"parent",parent},{"before",before},{"after",p.index.dump(2)+"\n"},{"files",p.files}};}
}
