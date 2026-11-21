// Derived actor-interaction recipes. Signed: Mech.
#pragma once
#include <string>
#include <vector>
struct BslActorInteractionDef {std::string id,dialogue;double range,calendar_factor;};
inline const std::vector<BslActorInteractionDef>& compiled_interactions(){static const std::vector<BslActorInteractionDef> defs={};return defs;}
