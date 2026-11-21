// Derived stable actor placement references. Signed: Mech.
#pragma once
#include <string>
#include <vector>
struct BslActorPlacementDef {std::string id,level,placement,object,interaction;};
inline const std::vector<BslActorPlacementDef>& compiled_actor_placements(){static const std::vector<BslActorPlacementDef> defs={};return defs;}
