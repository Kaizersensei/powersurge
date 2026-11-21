// Original single-player checkpoint policy and progress commit seam. Signed: Atlas.
#include "checkpoints.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace bsl {
namespace {bool id(const std::string& x){return !x.empty()&&x.size()<=256&&x.find('\0')==std::string::npos;}}
void CheckpointBook::enter(const std::string& level){if(!id(level))throw std::invalid_argument("invalid checkpoint level");if(level_!=level){auto next=level;leave();level_.swap(next);}}
void CheckpointBook::leave() noexcept {entry_.reset();temporary_.reset();level_.clear();}
void CheckpointBook::record(CheckpointRecord r){
 if(!id(r.id)||r.level!=level_||level_.empty()||!std::isfinite(r.x)||!std::isfinite(r.y)||std::abs(r.x)>1e7||std::abs(r.y)>1e7||r.priority<0||r.priority>1000||(r.facing!=1&&r.facing!=-1)||r.kind<CheckpointKind::entry||r.kind>CheckpointKind::permanent||sequence_==std::numeric_limits<std::uint64_t>::max())throw std::invalid_argument("invalid checkpoint record");
 if(r.kind==CheckpointKind::entry&&r.priority!=0)throw std::invalid_argument("entry checkpoint must be fallback");
 r.sequence=sequence_+1;
 // Build the replacement optional before changing the sequence or an existing record.
 std::optional<CheckpointRecord> next{std::move(r)};
 auto& target=next->kind==CheckpointKind::entry?entry_:next->kind==CheckpointKind::temporary?temporary_:permanent_;
 target.swap(next);++sequence_;
}
std::vector<const CheckpointRecord*> CheckpointBook::candidates(bool cross_level) const {
 std::vector<const CheckpointRecord*> out;
 for(const auto* candidate:{&entry_,&temporary_,&permanent_})if(candidate->has_value()&&(candidate->value().level==level_||(cross_level&&candidate->value().kind==CheckpointKind::permanent)))out.push_back(&candidate->value());
 std::sort(out.begin(),out.end(),[](const auto* a,const auto* b){return a->priority!=b->priority?a->priority>b->priority:a->sequence>b->sequence;});return out;
}
RewindServices::RewindServices(Inventory& inventory,Events& events,const InventoryState& bag,const EventState& globals):inventory_(inventory),events_(events){
 if(inventory.busy_||inventory.state_.catalog_signature!=events.catalog_.signature()|| &events.inventory_!=&inventory||!globals.runs.empty())throw std::invalid_argument("invalid rewind service boundary");
 Inventory next(inventory.catalog_,inventory.enabled_);next.restore(bag);Events event_next(events.catalog_,next);event_next.restore(globals);
 inventory_state_=std::move(next.state_);event_state_={events.catalog_.signature(),std::move(event_next.values_),{}};
}
void RewindServices::commit() noexcept {
 if(committed_)return;
 committed_=true;
 auto& a=inventory_.state_;a.catalog_signature.swap(inventory_state_.catalog_signature);a.bags.swap(inventory_state_.bags);a.receipts.swap(inventory_state_.receipts);a.issued_instances.swap(inventory_state_.issued_instances);
 events_.values_.swap(event_state_.values);events_.runs_.swap(event_state_.runs);
}
}
