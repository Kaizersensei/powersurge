// Original bounded stat contribution adapter. No earned-state or inventory mutations. Signed: Atlas.
#include "equipment.hpp"
#include <algorithm>
#include <stdexcept>
namespace bsl {
namespace {void require(bool ok){if(!ok)throw std::invalid_argument("invalid equipment stat catalog/binding");}}
EquipmentStats::EquipmentStats(const Catalog& catalog,const StatCatalog& stats,std::vector<EquipmentRule> rules):signature_(catalog.signature()){
 CharacterStats checked(stats);require(rules.size()<=256);
 for(const auto& item:catalog.items())if(item.second.storage==Storage::unique&&!item.second.slot.empty())item_slots_.emplace(item.first,item.second.slot);
 std::set<std::string> slots;for(const auto& item:item_slots_)slots.insert(item.second);require(slots.size()<=64);
 std::map<std::string,std::set<std::string>> available;for(const auto& p:stats.profiles){profiles_.insert(p.definition);available.emplace(p.definition,std::set<std::string>(p.stats.begin(),p.stats.end()));}
 for(const auto& r:rules){require(item_slots_.count(r.item)&&available.count(r.character)&&!r.modifiers.empty()&&r.modifiers.size()<=64);
  for(const auto& m:r.modifiers)require(available.at(r.character).count(m.first)&&m.second>=-1000000000&&m.second<=1000000000);
  require(rules_.emplace(std::make_pair(r.item,r.character),r.modifiers).second);
 }
}
StatModifiers EquipmentStats::modifiers(const Inventory& inventory,const std::string& owner,const std::string& character) const {
 require(inventory.catalog_signature()==signature_&&profiles_.count(character));StatModifiers result;const auto* bag=inventory.bag(owner);if(!bag)return result;require(bag->equipped.size()<=64);
 for(const auto& slot:bag->equipped){auto item=bag->unique.find(slot.second);require(item!=bag->unique.end());auto expected=item_slots_.find(item->second);require(expected!=item_slots_.end()&&expected->second==slot.first);
  auto rule=rules_.find({item->second,character});if(rule==rules_.end())continue;
  for(const auto& m:rule->second)result[m.first]+=m.second;
 }
 return result;
}
}
