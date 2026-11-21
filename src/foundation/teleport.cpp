// Original generic teleporter definitions; no scene or save serialization. Signed: Atlas.
#include "teleport.hpp"
#include <stdexcept>
namespace bsl {
namespace {
bool identity(const std::string& s){return !s.empty()&&s.size()<=128&&s.find('\0')==std::string::npos;}
bool level(const std::string& s){if(s.size()>200||s.rfind("levels/",0)!=0||s.size()<11||s.substr(s.size()-4)!=".lev"||s.find('\\')!=std::string::npos||s.find('\0')!=std::string::npos)return false;std::size_t start=0;while(start<s.size()){auto end=s.find('/',start);auto part=s.substr(start,end-start);if(part.empty()||part=="."||part=="..")return false;if(end==std::string::npos)break;start=end+1;}return true;}
bool number(double n){return std::isfinite(n)&&std::abs(n)<=1e7;}
}
DoorRegistry::DoorRegistry(std::vector<DoorDef> defs):doors_(std::move(defs)){
 if(doors_.size()>1024)throw std::invalid_argument("too many doors");
 std::set<std::string> ids;std::set<std::pair<std::string,std::uint64_t>> placements;
 for(const auto& d:doors_){
  if(!identity(d.id)||!ids.insert(d.id).second||!level(d.level)||!d.placement||!placements.emplace(d.level,d.placement).second||!number(d.x)||!number(d.y)||!number(d.origin_x)||!number(d.origin_y)||!number(d.offset_x)||!number(d.offset_y)||!std::isfinite(d.width)||!std::isfinite(d.height)||d.width<1||d.width>1024||d.height<1||d.height>1024||(d.facing!=0&&d.facing!=1&&d.facing!=-1)||(!d.condition.empty()&&!identity(d.condition))||(!d.complete.empty()&&!identity(d.complete)))throw std::invalid_argument("invalid door definition");
  if(d.destination==DoorDestination::point){if(!level(d.target))throw std::invalid_argument("invalid point destination");}
  else if(d.destination==DoorDestination::door){if(!identity(d.target))throw std::invalid_argument("invalid door destination");}
  else if(d.destination!=DoorDestination::origin||!d.target.empty())throw std::invalid_argument("invalid origin destination");
 }
 for(const auto& d:doors_)if(d.destination==DoorDestination::door&&!door(d.target))throw std::invalid_argument("missing destination door");
}
const DoorDef* DoorRegistry::door(const std::string& id) const noexcept{for(const auto& d:doors_)if(d.id==id)return &d;return nullptr;}
bool DoorRegistry::contains(const DoorDef& d,const std::string& at,double x,double y) const noexcept{return at==d.level&&std::isfinite(x)&&std::isfinite(y)&&x>=d.x-d.origin_x&&x<=d.x-d.origin_x+d.width&&y>=d.y-d.origin_y&&y<=d.y-d.origin_y+d.height;}
TravelPoint DoorRegistry::resolve(const DoorDef& d,int facing,const std::optional<TravelContext>& previous) const {
 TravelPoint p;p.facing=d.facing?d.facing:facing;
 if(d.destination==DoorDestination::point){p.level=d.target;p.x=d.offset_x;p.y=d.offset_y;}
 else if(d.destination==DoorDestination::door){const auto* dest=door(d.target);if(!dest)throw std::invalid_argument("missing door");p.level=dest->level;p.door=dest->id;p.x=dest->x+d.offset_x;p.y=dest->y+d.offset_y;}
 else {if(!previous||!valid_travel_context(*previous))throw std::invalid_argument("no departure context");p=previous->source;p.x+=d.offset_x;p.y+=d.offset_y;if(d.facing)p.facing=d.facing;}
 TravelContext check{p,p};if(!valid_travel_context(check))throw std::invalid_argument("invalid resolved destination");return p;
}
bool DoorContacts::request(const DoorDef& d,bool inside,bool fresh){if(!inside){inside_.erase(d.id);return false;}bool entered=inside_.insert(d.id).second;return d.action?fresh:entered;}
void DoorContacts::arrive(const DoorRegistry& r,const TravelPoint& p){std::set<std::string> next;for(const auto& d:r.all())if(r.contains(d,p.level,p.x,p.y))next.insert(d.id);inside_.swap(next);}
}
