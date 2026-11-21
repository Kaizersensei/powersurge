// Derived non-roster actor templates. Signed: Mech.
#pragma once
#include <string>
#include <vector>
struct BslActorTemplateDef {std::string id,name,model,interaction;bool passive;};
inline const std::vector<BslActorTemplateDef>& compiled_actor_templates(){static const std::vector<BslActorTemplateDef> defs={};return defs;}
