// Host-neutral data services. Original implementation. Signed: Atlas.
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace bsl {
enum class Storage { stack, unique };
struct EffectDef { std::string id; int amount; };
struct ItemDef { std::string id, name; Storage storage; int max_stack; std::string slot, effect; std::string category="misc", description=""; };
struct DialogueDef { std::string id, text; std::vector<std::string> choices; std::vector<std::string> outcomes={}; std::vector<std::string> choice_help={}; };
struct FieldDef { std::string id; double strength; int direction; };
enum class Scope { session, player };
struct SwitchDef { std::string id; Scope scope; bool initial; };
struct VariableDef { std::string id; Scope scope; int initial, minimum, maximum; };
enum class EventOp { set_switch, set_variable, add_variable, grant_item, remove_item, wait_ticks, if_switch, grant_reward, accept_quest, claim_quest, buy_shop_item, sell_shop_item, advance_calendar, spawn_character, spawn_actor };
struct EventCommand { EventOp op; std::string target; int value=0, skip=0; std::string receipt={},offer={}; int x=0,y=0; };
struct EventDef { std::string id; std::vector<EventCommand> commands; };
class Catalog {
public:
 Catalog(std::vector<ItemDef>, std::vector<EffectDef>, std::vector<DialogueDef>, std::vector<FieldDef>, std::vector<SwitchDef> = {}, std::vector<VariableDef> = {}, std::vector<EventDef> = {});
 Catalog(const Catalog&)=default;
 Catalog(Catalog&&)=default;
 Catalog& operator=(const Catalog&)=delete;
 Catalog& operator=(Catalog&&)=delete;
 const std::map<std::string,ItemDef>& items() const noexcept { return items_; }
 const ItemDef* item(const std::string&) const noexcept;
 const EffectDef* effect(const std::string&) const noexcept;
 const DialogueDef* dialogue(const std::string&) const noexcept;
 const FieldDef* field(const std::string&) const noexcept;
 const SwitchDef* flag(const std::string&) const noexcept;
 const VariableDef* variable(const std::string&) const noexcept;
 const EventDef* event(const std::string&) const noexcept;
 const std::map<std::string,EventDef>& events() const noexcept { return events_; }
 const std::string& signature() const noexcept { return signature_; }
private:
 std::string signature_;
 std::map<std::string, ItemDef> items_;
 std::map<std::string, EffectDef> effects_;
 std::map<std::string, DialogueDef> dialogues_;
 std::map<std::string, FieldDef> fields_;
 std::map<std::string, SwitchDef> switches_;
 std::map<std::string, VariableDef> variables_;
 std::map<std::string, EventDef> events_;
};
struct Selection {
 std::string definition, instance;
 bool operator==(const Selection& r) const { return definition==r.definition && instance==r.instance; }
};
// Value view: callers retain no pointers into a mutable bag.
struct InventoryEntry {
 Selection selection;
 std::string name,category,description,slot;
 int quantity;
 bool equipped,assigned,usable;
};
struct Bag {
 int slot_limit=0; // Zero retains unbounded legacy bags; new workspaces configure a finite limit.
 std::map<std::string,int> stacks;
 std::map<std::string,std::string> unique;
 std::map<std::string,std::string> equipped;
 std::map<std::string,Selection> assigned;
 bool operator==(const Bag& r) const { return std::tie(slot_limit,stacks,unique,equipped,assigned)==std::tie(r.slot_limit,r.stacks,r.unique,r.equipped,r.assigned); }
};
enum class Op { grant, remove, equip, unequip, assign, use };
enum class Code { ok, disabled, invalid, denied, conflict, busy };
struct Request {
 std::string transaction, owner;
 Op op;
 std::string definition, instance, slot;
 int quantity=1;
 std::string context={}; // Bounded transaction semantics; legacy requests leave empty.
 bool operator==(const Request& r) const {
  return std::tie(transaction,owner,op,definition,instance,slot,quantity,context)==std::tie(r.transaction,r.owner,r.op,r.definition,r.instance,r.slot,r.quantity,r.context);
 }
};
struct Result { Code code; bool replay=false; };
struct Receipt { Request request; };
struct InventoryState {
 std::string catalog_signature;
 std::map<std::string,Bag> bags;
 std::map<std::pair<std::string,std::string>,Receipt> receipts;
 std::set<std::string> issued_instances;
};
class EffectPort {
public:
 virtual ~EffectPort()=default;
 // Pure validation followed by synchronous, infallible mutation with no notifications.
 // Host keeps target identities/lifetimes stable across this boundary.
 virtual bool can_apply(const std::string& owner,const EffectDef&) const noexcept=0;
 virtual void apply(const std::string& owner,const EffectDef&) noexcept=0;
};
class SessionDataStore;
class Inventory {
public:
 explicit Inventory(const Catalog&,bool enabled=true);
 Inventory(const Inventory&)=delete;
 Inventory& operator=(const Inventory&)=delete;
 Result execute(const Request&,EffectPort* = nullptr);
 const Bag* bag(const std::string&) const noexcept;
 Result resize(const std::string& owner,int slots);
 const std::string& catalog_signature() const noexcept {return state_.catalog_signature;}
 std::vector<InventoryEntry> entries(const std::string& owner) const;
 InventoryState capture() const;
 // Inventory-only capture: host must restore world/effects in the same session transaction.
 void restore(const InventoryState&);
private:
 const Catalog catalog_;
 bool enabled_, busy_=false;
 InventoryState state_;
 void validate(const InventoryState&) const;
 friend class SessionDataStore;
 friend class RewindServices;
 friend class PickupClaims;
 friend class LootDrops;
 friend class Commerce;
};
using Lease=std::uint64_t;
struct TimeSlice { double simulation,calendar,ui; };
class Policies {
public:
 Lease menu(const std::string& owner,bool pause=true);
 Lease dialogue(const std::string& owner,const std::vector<std::string>& participants,double factor=.1);
 bool close(Lease) noexcept;
 bool active(Lease id) const noexcept { return entries_.count(id)>0; }
 void interrupt(const std::string& participant);
 bool paused() const noexcept;
 bool blocked(const std::string& participant) const noexcept;
 double calendar_factor() const noexcept;
 TimeSlice sample(double elapsed) const;
private:
 struct Entry { std::string owner; std::vector<std::string> participants; bool pause,is_dialogue; double factor; };
 std::map<Lease,Entry> entries_;
 Lease next_=1;
};
enum class ConversationState { idle, open, chosen, interrupted, cancelled };
class Conversation {
public:
 Conversation(const Catalog&,Policies&);
 ~Conversation();
 Conversation(const Conversation&)=delete;
 Conversation& operator=(const Conversation&)=delete;
 void open(const std::string& definition,const std::string& owner,const std::vector<std::string>& participants,double factor=.1);
 bool choose(int index) noexcept;
 bool hit(const std::string& participant) noexcept;
 void cancel() noexcept;
 ConversationState state() const noexcept { return state_==ConversationState::open&&!policies_.active(lease_)?ConversationState::interrupted:state_; }
 int choice() const noexcept { return choice_; }
 const DialogueDef* definition() const noexcept { return definition_; }
private:
 const Catalog catalog_;
 Policies& policies_;
 const DialogueDef* definition_=nullptr;
 std::vector<std::string> participants_;
 Lease lease_=0;
 ConversationState state_=ConversationState::idle;
 int choice_=-1;
 void finish(ConversationState) noexcept;
};
enum class LoopChange { start, stop };
struct FieldRequest { std::string helper, participant; double acceleration_x; };
class WindHelper {
public:
 WindHelper(std::string instance,FieldDef definition);
 std::vector<LoopChange> enter(const std::string& participant);
 std::vector<LoopChange> leave(const std::string& participant);
 std::vector<LoopChange> clear();
 std::vector<FieldRequest> requests() const;
private:
 std::string instance_;
 FieldDef definition_;
 std::set<std::string> occupants_;
};
}
