// Immutable level-property registry; geometry remains native Surge content. Signed: Atlas.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace bsl {
struct LevelWeather {std::string region,hemisphere,preset;bool indoor=false;double wind_factor=1,precipitation_factor=1;};
struct LevelTerrain {std::string document,materials;};
struct LevelDefinition {std::string id,scene,parent,name,country,locality;bool editable=true;LevelWeather weather;LevelTerrain terrain{};};
struct LevelCatalog {LevelDefinition defaults;std::vector<LevelDefinition> records;};
class LevelRegistry {
 LevelCatalog data_;
 static bool text(const std::string& v,bool empty=false){return (empty||!v.empty())&&v.size()<=128&&v.find('\0')==std::string::npos;}
 static bool path(const std::string& v){return v.size()>11&&v.size()<=256&&v.rfind("levels/",0)==0&&v.substr(v.size()-4)==".lev"&&v.find("..") ==std::string::npos&&v.find('\\')==std::string::npos&&v.find(':')==std::string::npos&&v.find("//")==std::string::npos&&std::all_of(v.begin(),v.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='/'||c=='_'||c=='-'||c=='.');});}
 static bool terrain_path(const std::string& v){return v.size()>13&&v.size()<=256&&v.rfind("terrain/",0)==0&&v.substr(v.size()-5)==".json"&&v.find("..")==std::string::npos&&v.find("/./")==std::string::npos&&v.find("//")==std::string::npos&&std::all_of(v.begin(),v.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='/'||c=='_'||c=='-'||c=='.');});}
 static void properties(const LevelDefinition& d){const auto& t=d.terrain;if((!t.document.empty()||!t.materials.empty())&&(!terrain_path(t.document)||!terrain_path(t.materials)))throw std::invalid_argument("invalid level terrain attachment");const auto& w=d.weather;if(!text(d.name)||!text(d.country,true)||!text(d.locality)||(d.country.empty()?d.locality.size():d.country.size()+2+d.locality.size())>128||!text(w.region,true)||!text(w.preset,true)||(w.hemisphere!="inherit"&&w.hemisphere!="north"&&w.hemisphere!="south")||!std::isfinite(w.wind_factor)||w.wind_factor<0||w.wind_factor>10||!std::isfinite(w.precipitation_factor)||w.precipitation_factor<0||w.precipitation_factor>10)throw std::invalid_argument("invalid level properties");}
public:
 explicit LevelRegistry(LevelCatalog d):data_(std::move(d)){
  properties(data_.defaults);if(!data_.defaults.terrain.document.empty())throw std::invalid_argument("default terrain documents cannot be shared implicitly");if(data_.records.size()>1024)throw std::invalid_argument("too many level definitions");std::set<std::string> ids,paths;
  for(const auto& r:data_.records){properties(r);std::string key=r.scene;std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(!text(r.id)||r.id.size()<=6||r.id.rfind("level:",0)!=0||!std::all_of(r.id.begin()+6,r.id.end(),[](unsigned char c){return c<128&&(std::isalnum(c)||c=='_'||c=='-'||c=='.');})||!text(r.parent,true)||!path(r.scene)||!ids.insert(r.id).second||!paths.insert(key).second)throw std::invalid_argument("invalid/duplicate level identity");}
  std::set<std::string> terrain_documents;
  for(const auto& r:data_.records)if(!r.terrain.document.empty()){auto path=r.terrain.document;std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(!terrain_documents.insert(path).second)throw std::invalid_argument("terrain document belongs to more than one level");}
  for(const auto& r:data_.records){std::set<std::string> chain;const LevelDefinition* at=&r;while(at){if(!chain.insert(at->id).second)throw std::invalid_argument("cyclic level hierarchy");if(chain.size()>65)throw std::invalid_argument("level hierarchy exceeds64 ancestors");if(at->parent.empty())break;at=by_id(at->parent);if(!at)throw std::invalid_argument("missing level parent");}}
 }
 const LevelDefinition* by_id(const std::string& id) const noexcept {for(const auto& r:data_.records)if(r.id==id)return &r;return nullptr;}
 const LevelDefinition* by_scene(const std::string& scene) const noexcept {for(const auto& r:data_.records)if(r.scene==scene)return &r;return nullptr;}
 const LevelDefinition& effective(const std::string& scene) const noexcept {const auto* r=by_scene(scene);return r?*r:data_.defaults;}
 const LevelCatalog& catalog() const noexcept{return data_;}
};
}
