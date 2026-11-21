// Authored equipment-to-stat contributions, derived from actual player-owned slot bindings. Signed: Atlas.
#pragma once
#include "progression.hpp"
namespace bsl {
struct EquipmentRule {std::string item,character;StatModifiers modifiers;};
class EquipmentStats {
public:
 EquipmentStats(const Catalog&,const StatCatalog&,std::vector<EquipmentRule>);
 StatModifiers modifiers(const Inventory&,const std::string& owner,const std::string& character_definition) const;
private:
 std::string signature_;std::map<std::string,std::string> item_slots_;std::set<std::string> profiles_;
 std::map<std::pair<std::string,std::string>,StatModifiers> rules_;
};
}
