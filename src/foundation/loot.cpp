// Original bounded single-writer service. Independent of rendering and native actor addresses. Signed: Atlas.
#include "loot.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>
namespace bsl {
namespace {
bool id(const std::string& value){return !value.empty()&&value.size()<=128&&value.find('\0')==std::string::npos;}
bool position(double v){return std::isfinite(v)&&std::abs(v)<=1e7;}
void require(bool value){if(!value)throw std::invalid_argument("invalid loot catalog/state");}
std::uint32_t draw(std::uint32_t& state,std::uint32_t bound){
 // Rejection sampling avoids modulo bias; nonzero xorshift state has a fixed portable sequence.
 const auto limit=UINT32_MAX-(UINT32_MAX%bound);std::uint32_t value;
 do{state^=state<<13;state^=state>>17;state^=state<<5;value=state-1;}while(value>=limit);
 return value%bound;
}
}
LootDrops::LootDrops(const Catalog& catalog,std::vector<LootTable> tables,std::uint64_t epoch,std::uint32_t seed):catalog_(&catalog){
 require(epoch&&seed&&tables.size()<=64);state_.epoch=epoch;state_.random=seed;
 for(const auto& t:tables){require(id(t.id)&&id(t.model)&&std::isfinite(t.range)&&t.range>=8&&t.range<=128&&!t.entries.empty()&&t.entries.size()<=64);int sum=0;std::set<std::string> seen;
  for(const auto& e:t.entries){const auto* item=catalog.item(e.item);require(item&&item->storage==Storage::stack&&e.weight>=1&&e.weight<=10000&&e.minimum>=1&&e.maximum>=e.minimum&&e.maximum<=item->max_stack&&seen.insert(e.item).second);sum+=e.weight;}
  require(sum<=10000&&tables_.emplace(t.id,t).second&&models_.emplace(t.model,t.id).second);
 }
}
bool LootDrops::supports(const std::string& model) const {return models_.count(model)>0;}
const LootTable& LootDrops::table(const std::string& id) const {return tables_.at(id);}
Result LootDrops::defeat(std::uint64_t victim,const std::string& model,double x,double y){
 auto found=models_.find(model);if(!victim||found==models_.end()||!position(x)||!position(y))return {Code::invalid};
 auto prior=state_.outcomes.find(victim);if(prior!=state_.outcomes.end())return {prior->second.table==found->second?Code::ok:Code::conflict,true};
 if(state_.outcomes.size()>=1024)return {Code::denied};
 auto random=state_.random;const auto& t=tables_.at(found->second);int roll=static_cast<int>(draw(random,10000));LootOutcome outcome;outcome.table=t.id;outcome.x=x;outcome.y=y;
 for(const auto& e:t.entries){if(roll<e.weight){outcome.item=e.item;outcome.quantity=e.minimum+static_cast<int>(draw(random,static_cast<unsigned>(e.maximum-e.minimum+1)));break;}roll-=e.weight;}
 // Even a no-drop outcome owns a receipt. Replay cannot reroll or advance the RNG.
 state_.outcomes.emplace(victim,std::move(outcome));state_.random=random;return {Code::ok};
}
Result LootDrops::claim(Inventory& inventory,std::uint64_t victim,const std::string& player){
 if(inventory.busy_)return {Code::busy};
 if(!inventory.enabled_)return {Code::disabled};
 if(inventory.state_.catalog_signature!=catalog_->signature()||!id(player))return {Code::invalid};
 auto found=state_.outcomes.find(victim);if(found==state_.outcomes.end()||found->second.item.empty())return {Code::invalid};
 if(found->second.collected)return {found->second.collector==player?Code::ok:Code::conflict,true};
 auto next_state=state_;auto& drop=next_state.outcomes.at(victim);drop.collected=true;drop.collector=player;
 Inventory next(inventory.catalog_,inventory.enabled_);next.restore(inventory.state_);
 auto result=next.execute({"loot/"+std::to_string(state_.epoch)+"/"+std::to_string(victim),player,Op::grant,drop.item,"","",drop.quantity});
 if(result.code!=Code::ok)return result;
 if(result.replay)return {Code::conflict};
 using std::swap;swap(inventory.state_,next.state_);state_.outcomes.swap(next_state.outcomes);return {Code::ok};
}
void LootDrops::restore(const LootState& state){
 require(state.epoch==state_.epoch&&state.random&&state.outcomes.size()<=1024);
 for(const auto& pair:state.outcomes){const auto& d=pair.second;auto t=tables_.find(d.table);require(pair.first&&t!=tables_.end()&&position(d.x)&&position(d.y));
  if(d.item.empty()){int total=0;for(const auto& e:t->second.entries)total+=e.weight;require(total<10000&&!d.quantity&&!d.collected&&d.collector.empty());continue;}
  bool found=false;for(const auto& e:t->second.entries)if(e.item==d.item&&d.quantity>=e.minimum&&d.quantity<=e.maximum)found=true;
  require(found&&(d.collected?id(d.collector):d.collector.empty()));
 }
 auto next=state;state_.outcomes.swap(next.outcomes);state_.random=next.random;
}
}
