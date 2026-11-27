// Versioned level/sequence records; spatial geometry remains Surge-owned. Signed: President.
#pragma once
#include "documents.hpp"
#include "../foundation/level_records.hpp"
#include "../foundation/weather_records.hpp"
#include <set>
namespace ps::authoring {
inline std::string world_text(const Json& j,const char* key){if(!j.contains(key)||!j[key].is_string())throw std::runtime_error(std::string(key)+": text required");auto value=j[key].get<std::string>();if(value.size()>256||value.find('\0')!=value.npos)throw std::runtime_error(std::string(key)+": invalid text");return value;}
inline bool world_bool(const Json& j,const char* key){if(!j.contains(key)||!j[key].is_boolean())throw std::runtime_error(std::string(key)+": boolean required");return j[key].get<bool>();}
inline double world_number(const Json& j,const char* key){if(!j.contains(key)||!j[key].is_number())throw std::runtime_error(std::string(key)+": number required");double n=j[key].get<double>();if(!std::isfinite(n))throw std::runtime_error(std::string(key)+": finite number required");return n;}
inline bsl::LevelDefinition level_definition(const Json& j){
 if(world_text(j,"kind")!="level"){throw std::runtime_error("Expected a level record");}
 auto& p=j.at("properties");auto& w=p.at("weather");auto& l=j.at("location");bsl::LevelDefinition d;
 d.id=world_text(j,"id");d.name=world_text(j,"name");d.scene=world_text(j,"scene");d.parent=world_text(j,"parent");d.country=world_text(l,"country");d.locality=world_text(l,"locality");d.editable=world_bool(p,"editable");d.weather={world_text(w,"region"),world_text(w,"hemisphere"),world_text(w,"preset"),world_bool(w,"indoor"),world_number(w,"wind_factor"),world_number(w,"precipitation_factor")};
 const auto& t=p.at("terrain");if(!t.is_null())d.terrain={world_text(t,"document"),world_text(t,"materials")};return d;
}
struct SequenceDefinition {std::string id,name,file;std::vector<std::string> levels;};
struct WorldRegistry {bsl::LevelRegistry levels;std::vector<SequenceDefinition> sequences;
 const SequenceDefinition* sequence(const std::string& id)const{for(auto& s:sequences)if(s.id==id)return &s;return nullptr;}
 const SequenceDefinition* route(const std::string& file)const{for(auto& s:sequences)if(s.file==file)return &s;return nullptr;}
};
inline SequenceDefinition sequence_definition(const Json& j){SequenceDefinition s{world_text(j,"id"),world_text(j,"name"),world_text(j,"file"),{}};if(world_text(j,"kind")!="sequence"||!j.at("levels").is_array()||j["levels"].empty()||j["levels"].size()>256)throw std::runtime_error("Sequence requires1–256 registered levels");for(auto& id:j["levels"]){if(!id.is_string()||id.get_ref<const std::string&>().size()>128)throw std::runtime_error("Sequence level ID required");s.levels.push_back(id.get<std::string>());}return s;}
inline WorldRegistry compile_world(const std::vector<Json>& records){
 auto catalog=compiled_levels();catalog.records.clear();for(auto& j:records)if(j.value("kind",std::string{})=="level")catalog.records.push_back(level_definition(j));if(catalog.records.empty())catalog=compiled_levels();WorldRegistry result{bsl::LevelRegistry(std::move(catalog)),{}};auto climate=compiled_weather();
 for(auto& d:result.levels.catalog().records){if(!d.weather.region.empty()&&std::none_of(climate.regions.begin(),climate.regions.end(),[&](auto& r){return r.id==d.weather.region;}))throw std::runtime_error("Unknown weather region: "+d.weather.region);if(!d.weather.preset.empty()&&std::none_of(climate.presets.begin(),climate.presets.end(),[&](auto& p){return p.id==d.weather.preset;}))throw std::runtime_error("Unknown weather preset: "+d.weather.preset);}
 std::set<std::string> ids,files;for(auto& j:records)if(j.value("kind",std::string{})=="sequence"){
 auto d=sequence_definition(j);auto ascii=[](unsigned char c){return c<128&&(std::isalnum(c)||c=='_'||c=='-'||c=='.');};auto file=d.file;std::transform(file.begin(),file.end(),file.begin(),[](unsigned char c){return char(std::tolower(c));});
 if(d.id.rfind("sequence:",0)!=0||d.id.size()<=9||d.id.size()>128||!std::all_of(d.id.begin()+9,d.id.end(),ascii)||d.name.empty()||d.name.size()>128||d.file.size()<12||d.file.size()>256||d.file.rfind("quests/",0)!=0||d.file.substr(d.file.size()-4)!=".qst"||d.file.find("..")!=file.npos||d.file.find("/./")!=file.npos||d.file.find("//")!=file.npos||!std::all_of(d.file.begin(),d.file.end(),[&](unsigned char c){return c=='/'||ascii(c);})||!ids.insert(d.id).second||!files.insert(file).second)throw std::runtime_error("Invalid/duplicate sequence identity or route");
 for(auto& id:d.levels){if(!result.levels.by_id(id))throw std::runtime_error("Missing sequence level: "+id);}
 result.sequences.push_back(std::move(d));if(result.sequences.size()>256)throw std::runtime_error("Sequence registry limit");
 }return result;
}
const WorldRegistry& world_snapshot();
}
