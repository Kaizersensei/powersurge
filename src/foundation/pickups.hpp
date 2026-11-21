// Atomic authored pickup -> player inventory and selected claim flag. Signed: Atlas.
#pragma once
#include "events.hpp"
namespace bsl {
struct PickupDef {std::string id,level,item,flag;std::uint64_t placement=0;int quantity=1;double range=36;};
class PickupClaims {
public:
 // Single simulation writer; spatial/input/actor authorization belongs to the host.
 static Result claim(Inventory&,Events&,const PickupDef&,const std::string& player);
};
}
