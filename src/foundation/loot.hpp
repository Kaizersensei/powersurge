// Transient enemy drop custody; authored tables, seeded rolls, prepared claims. Signed: Atlas.
#pragma once
#include "foundation.hpp"
namespace bsl {
struct LootEntry {std::string item;int weight=0,minimum=1,maximum=1;};
struct LootTable {std::string id,model;double range=36;std::vector<LootEntry> entries;};
struct LootOutcome {
 std::string table,item,collector;int quantity=0;double x=0,y=0;bool collected=false;
};
struct LootState {std::uint64_t epoch=0;std::uint32_t random=1;std::map<std::uint64_t,LootOutcome> outcomes;};
class LootDrops {
public:
 LootDrops(const Catalog&,std::vector<LootTable>,std::uint64_t epoch,std::uint32_t seed);
 bool supports(const std::string& model) const;
 Result defeat(std::uint64_t victim,const std::string& model,double x,double y);
 Result claim(Inventory&,std::uint64_t victim,const std::string& player);
 const LootState& state() const noexcept {return state_;}
 const LootTable& table(const std::string&) const;
 LootState capture() const {return state_;}
 void restore(const LootState&); // Validated same-level checkpoint bundle, never disk/world serialization.
private:
 const Catalog* catalog_;std::map<std::string,LootTable> tables_;std::map<std::string,std::string> models_;LootState state_;
};
}
