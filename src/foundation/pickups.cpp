// Prepared inventory plus flag commit; does not cancel existing event continuations. Signed: Atlas.
#include "pickups.hpp"
#include <utility>
namespace bsl {
Result PickupClaims::claim(Inventory& inventory,Events& events,const PickupDef& def,const std::string& player){
 if(inventory.busy_)return {Code::busy};
 if(!inventory.enabled_)return {Code::disabled};
 const auto* flag=events.catalog_.flag(def.flag);const auto* item=inventory.catalog_.item(def.item);
 if(&events.inventory_!=&inventory||events.catalog_.signature()!=inventory.state_.catalog_signature||!flag||flag->initial||!item||item->storage!=Storage::stack||def.quantity<1||def.quantity>item->max_stack||def.id.empty()||def.id.size()>128||def.id.find('\0')!=std::string::npos||player.empty()||player.size()>256||player.find('\0')!=std::string::npos)return {Code::invalid};
 if(events.flag(def.flag,player))return {Code::ok,true};
 auto values=events.values_;values[{def.flag,flag->scope==Scope::session?"":player}]=1;
 Inventory next(inventory.catalog_,inventory.enabled_);next.restore(inventory.state_);
 auto result=next.execute({"pickup/"+def.id,player,Op::grant,def.item,"","",def.quantity});
 if(result.code!=Code::ok)return result;
 // A manually cleared flag cannot replay a still-committed inventory receipt.
 if(result.replay)return {Code::conflict};
 using std::swap;swap(inventory.state_,next.state_);events.values_.swap(values);return {Code::ok};
}
}
