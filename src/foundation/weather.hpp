// Seasonal weather projection and validated temporary modifiers. Signed: Atlas.
#pragma once
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace bsl {
struct WeatherPreset {std::string id,name,precipitation;int icon=-1;double amount=0,wind=0,direction=0;};
struct WeatherSeason {std::string name;double start=0;std::string preset;};
struct WeatherStep {int second=0;std::string preset;};
struct WeatherRegion {std::string id,hemisphere;int cycle=86400;std::vector<WeatherStep> steps;};
struct WeatherContext {std::string region,location;bool indoor=false;std::string preset;double wind_factor=1,amount_factor=1;std::string hemisphere;};
struct WeatherDef {std::string default_region;std::vector<WeatherPreset> presets;std::vector<WeatherSeason> seasons;std::vector<WeatherRegion> regions;};
struct WeatherOverride {std::string preset,hemisphere;double wind_factor=1,amount_factor=1;bool wind_override=false,amount_override=false;double wind=0,direction=0,amount=0;};
struct WeatherView {std::string region,location,preset,description,precipitation,season,hemisphere;int icon=-1;bool indoor=false;double wind=0,direction=0,amount=0,dust_rate=0,particle_speed=0,wind_speed=0;};
class Weather {
 WeatherDef def_;
 const WeatherPreset& preset(const std::string& id) const {for(const auto& p:def_.presets)if(p.id==id)return p;throw std::invalid_argument("unknown weather preset");}
 const WeatherRegion& region(const std::string& id) const {for(const auto& r:def_.regions)if(r.id==id)return r;throw std::invalid_argument("unknown weather region");}
 static bool label(const std::string& s){return !s.empty()&&s.size()<=128&&s.find('\0')==std::string::npos;}
 static bool bounded(double v,double hi){return std::isfinite(v)&&v>=0&&v<=hi;}
 static bool hemisphere(const std::string& s){return s=="north"||s=="south";}
public:
 explicit Weather(WeatherDef d):def_(std::move(d)) {
  if(def_.presets.empty()||def_.presets.size()>64||def_.seasons.empty()||def_.seasons.size()>24||def_.regions.empty()||def_.regions.size()>64)throw std::invalid_argument("invalid weather catalog");
  std::set<std::string> ids;
  for(const auto& p:def_.presets){if(!label(p.id)||!ids.insert(p.id).second||!label(p.name)||p.icon< -1||p.icon>20||(p.precipitation!="none"&&p.precipitation!="rain"&&p.precipitation!="snow"&&p.precipitation!="ash")||!bounded(p.amount,10)||!bounded(p.wind,10)||!bounded(p.direction,360)||p.direction==360||(p.precipitation=="none"&&p.amount!=0))throw std::invalid_argument("invalid weather preset");}
  double previous=-1;for(const auto& s:def_.seasons){if(!label(s.name)||!bounded(s.start,1)||s.start==1||s.start<=previous)throw std::invalid_argument("invalid weather season");previous=s.start;preset(s.preset);}if(def_.seasons.front().start!=0)throw std::invalid_argument("season bands must begin at zero");
  ids.clear();for(const auto& r:def_.regions){if(!label(r.id)||!ids.insert(r.id).second||!hemisphere(r.hemisphere)||r.cycle<1||r.cycle>31536000||r.steps.size()>64||(!r.steps.empty()&&r.steps.front().second!=0))throw std::invalid_argument("invalid weather region");int last=-1;for(const auto& s:r.steps){if(s.second<=last||s.second>=r.cycle)throw std::invalid_argument("invalid weather sequence");last=s.second;preset(s.preset);}}
  region(def_.default_region);
 }
 void validate(const WeatherOverride& o) const {
  if(!o.preset.empty())preset(o.preset);
  if((!o.hemisphere.empty()&&!hemisphere(o.hemisphere))||!bounded(o.wind_factor,10)||!bounded(o.amount_factor,10)||!bounded(o.wind,10)||!bounded(o.direction,360)||o.direction==360||!bounded(o.amount,10))throw std::invalid_argument("invalid weather override");
 }
 WeatherView project(double game_seconds,double year_fraction,const WeatherContext& context,const WeatherOverride* o=nullptr) const {
  if(!bounded(game_seconds,1e15)||!bounded(year_fraction,1)||year_fraction==1)throw std::invalid_argument("invalid weather clock");
  if(o)validate(*o);
  const auto* place=&context;
  if(!label(place->location)||!bounded(place->wind_factor,10)||!bounded(place->amount_factor,10)||(!place->hemisphere.empty()&&!hemisphere(place->hemisphere)))throw std::invalid_argument("invalid level weather context");
  const auto& r=region(place->region.empty()?def_.default_region:place->region);std::string hemi=o&&!o->hemisphere.empty()?o->hemisphere:place->hemisphere.empty()?r.hemisphere:place->hemisphere;double annual=std::fmod(year_fraction+(hemi=="south"?.5:0),1.0);const WeatherSeason* season=&def_.seasons.front();for(const auto& s:def_.seasons){if(s.start>annual)break;season=&s;}
  std::string id=season->preset;double phase=std::fmod(game_seconds,r.cycle);for(const auto& step:r.steps){if(step.second>phase)break;id=step.preset;}if(place&&!place->preset.empty())id=place->preset;
  const auto& base=preset(id);const auto& p=o&&!o->preset.empty()?preset(o->preset):base;
  WeatherView v;v.region=r.id;v.location=place->location;v.season=season->name;v.hemisphere=hemi;v.preset=p.id;v.description=p.name;v.precipitation=p.precipitation;v.icon=p.icon;v.indoor=place&&place->indoor;
  // An explicit external preset replaces level weather modifiers; otherwise they compose.
  v.wind=p.wind*(o&&!o->preset.empty()?1:place?place->wind_factor:1);v.amount=p.amount*(o&&!o->preset.empty()?1:place?place->amount_factor:1);v.direction=p.direction;
  if(o){v.wind*=o->wind_factor;v.amount*=o->amount_factor;if(o->wind_override){v.wind=o->wind;v.direction=o->direction;}if(o->amount_override)v.amount=o->amount;}
  v.wind=std::min(10.0,v.wind);v.amount=std::min(10.0,v.amount);if(v.precipitation=="none")v.amount=0;if(v.indoor)v.wind=v.amount=0;
  v.dust_rate=v.wind;v.particle_speed=v.wind*32;v.wind_speed=v.wind*120;return v;
 }
};
}
