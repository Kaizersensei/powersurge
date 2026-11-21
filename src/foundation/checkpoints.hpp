// Bounded checkpoint progress and prepared service restoration. No scene/VM snapshot. Signed: Atlas.
#pragma once
#include "events.hpp"
#include "learning.hpp"
#include "loot.hpp"
#include <optional>
namespace bsl {
enum class CheckpointKind {entry,temporary,permanent};
struct CheckpointDef {std::string id,level;std::uint64_t placement=0;CheckpointKind kind=CheckpointKind::temporary;int priority=10;double range=64;};
struct RewindProgress {
 std::string mode,save,player,selected;
 std::map<std::string,int> health; // Character definition -> selected resource value.
 std::map<std::string,StatValues> stats; // Stable character instance -> earned values.
 LearningState learning;
 InventoryState inventory;
 EventState events; // Registered values only; no runs/VM continuations.
 double calendar=0;
};
struct CheckpointRecord {std::string id,level;CheckpointKind kind=CheckpointKind::entry;int priority=0,facing=1;double x=0,y=0;std::uint64_t sequence=0;RewindProgress progress;std::optional<LootState> loot;}; // Level-local runtime checkpoint journal, never durable progress.
class CheckpointBook {
public:
 void enter(const std::string& level);
 void leave() noexcept;
 void record(CheckpointRecord);
 std::vector<const CheckpointRecord*> candidates(bool cross_level=false) const;
 bool entry() const noexcept {return entry_.has_value();}
 bool temporary() const noexcept {return temporary_.has_value();}
 const std::string& level() const noexcept {return level_;}
private:
 std::string level_;std::uint64_t sequence_=0;
 std::optional<CheckpointRecord> entry_,temporary_,permanent_;
};
// All validation/allocation occurs at construction. Commit swaps already prepared states only.
class RewindServices {
public:
 RewindServices(Inventory&,Events&,const InventoryState&,const EventState&);
 void commit() noexcept;
private:
 bool committed_=false;Inventory& inventory_;Events& events_;InventoryState inventory_state_;EventState event_state_;
};
}
