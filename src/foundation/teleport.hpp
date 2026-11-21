// Original door registry and spatial activation contract. Signed: Atlas.
#pragma once
#include "foundation.hpp"
#include <cmath>
#include <optional>
#include <set>
#include <tuple>
namespace bsl {
struct TravelPoint {std::string level,door;double x=0,y=0;int facing=1;};
struct TravelContext {
 TravelPoint source,target;
 bool operator==(const TravelContext& o) const {return std::tie(source.level,source.door,source.x,source.y,source.facing,target.level,target.door,target.x,target.y,target.facing)==std::tie(o.source.level,o.source.door,o.source.x,o.source.y,o.source.facing,o.target.level,o.target.door,o.target.x,o.target.y,o.target.facing);}
};
inline bool valid_travel_context(const TravelContext& c){
 for(const auto* p:{&c.source,&c.target})if(p->level.empty()||p->level.size()>200||p->level.find('\0')!=std::string::npos||p->door.size()>128||p->door.find('\0')!=std::string::npos||!std::isfinite(p->x)||!std::isfinite(p->y)||std::abs(p->x)>1e7||std::abs(p->y)>1e7||(p->facing!=1&&p->facing!=-1))return false;
 return true;
}
enum class DoorDestination {point,door,origin};
struct DoorDef {
 std::string id,level;std::uint64_t placement=0;double x=0,y=0,width=64,height=96,origin_x=32,origin_y=96;
 DoorDestination destination=DoorDestination::point;std::string target;double offset_x=0,offset_y=0;
 bool action=true,preserve_velocity=false;std::string condition,complete;int facing=0;
};
class DoorRegistry {
public:
 explicit DoorRegistry(std::vector<DoorDef>);
 const std::vector<DoorDef>& all() const noexcept{return doors_;}
 const DoorDef* door(const std::string&) const noexcept;
 bool contains(const DoorDef&,const std::string& level,double x,double y) const noexcept;
 TravelPoint resolve(const DoorDef&,int facing,const std::optional<TravelContext>& previous={}) const;
private:std::vector<DoorDef> doors_;
};
// Contact-only triggers fire on entry. Arrival suppresses every overlapped door until exit.
class DoorContacts {
public:
 bool request(const DoorDef&,bool inside,bool fresh_action);
 void arrive(const DoorRegistry&,const TravelPoint&);
 void clear() noexcept{inside_.clear();}
private:std::set<std::string> inside_;
};
}
