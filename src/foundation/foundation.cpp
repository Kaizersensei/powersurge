// Original host-neutral data/services implementation. Signed: Atlas.
#include "foundation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <locale>
#include <stdexcept>
#include <utility>

namespace bsl {
namespace {
bool text(const std::string& s){return !s.empty() && s.size()<=256 && s.find('\0')==std::string::npos;}
bool identity(const std::string& id,const std::string& prefix){
 if(id.compare(0,prefix.size(),prefix)!=0 || id.size()==prefix.size() || !text(id))return false;
 return std::all_of(id.begin()+static_cast<std::ptrdiff_t>(prefix.size()),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';});
}
void require(bool condition,const char* message){if(!condition)throw std::invalid_argument(message);}
template<class T> const T* lookup(const std::map<std::string,T>& map,const std::string& id) noexcept {
 auto i=map.find(id);return i==map.end()?nullptr:&i->second;
}
bool request_shape(const Request& r){
 if(!text(r.transaction)||!text(r.owner)||r.quantity<=0||r.context.size()>1024||r.context.find('\0')!=std::string::npos)return false;
 if(r.definition.find('\0')!=std::string::npos||r.instance.find('\0')!=std::string::npos||r.slot.find('\0')!=std::string::npos)return false;
 if(r.definition.size()>256||r.instance.size()>256||r.slot.size()>256)return false;
 switch(r.op){
 case Op::grant:case Op::remove:return !r.definition.empty() && r.slot.empty();
 case Op::equip:return !r.definition.empty()&&!r.instance.empty()&&!r.slot.empty()&&r.quantity==1;
 case Op::assign:return !r.definition.empty()&&!r.slot.empty()&&r.quantity==1;
 case Op::use:return !r.definition.empty()&&r.instance.empty()&&r.slot.empty()&&r.quantity==1;
 case Op::unequip:return r.definition.empty()&&r.instance.empty()&&!r.slot.empty()&&r.quantity==1;
 }
 return false;
}
bool owned(const Bag& b,const ItemDef& item,const std::string& instance){
 if(item.storage==Storage::stack){auto i=b.stacks.find(item.id);return instance.empty()&&i!=b.stacks.end()&&i->second>0;}
 auto i=b.unique.find(instance);return !instance.empty()&&i!=b.unique.end()&&i->second==item.id;
}
void clear_assignments(Bag& b,const Selection& selected){
 for(auto i=b.assigned.begin();i!=b.assigned.end();)if(i->second==selected)i=b.assigned.erase(i);else ++i;
}
struct BusyGuard { bool& flag; explicit BusyGuard(bool& value):flag(value){flag=true;}~BusyGuard(){flag=false;} };
void swap_state(InventoryState& a,InventoryState& b) noexcept {
 a.catalog_signature.swap(b.catalog_signature);a.bags.swap(b.bags);a.receipts.swap(b.receipts);a.issued_instances.swap(b.issued_instances);
}
}
Catalog::Catalog(std::vector<ItemDef> items,std::vector<EffectDef> effects,std::vector<DialogueDef> dialogues,std::vector<FieldDef> fields,std::vector<SwitchDef> switches,std::vector<VariableDef> variables,std::vector<EventDef> events){
 for(auto& e:effects){require(identity(e.id,"effect:")&&e.amount>0,"invalid effect");auto id=e.id;require(effects_.emplace(id,std::move(e)).second,"duplicate effect");}
 for(auto& i:items){
  require(identity(i.id,"item:")&&!i.name.empty()&&i.max_stack>0&&i.max_stack<=1000000,"invalid item");
  require(text(i.category)&&i.description.size()<=65536&&i.description.find('\0')==std::string::npos,"invalid item presentation");
  require(i.storage==Storage::stack||i.storage==Storage::unique,"invalid storage");
  if(i.storage==Storage::unique)require(i.max_stack==1&&text(i.slot)&&i.effect.empty(),"unique equipment requires one instance, a slot and no consumable effect");
  else require(i.slot.empty(),"stack equipment not supported in this version");
  require(i.effect.empty()||effect(i.effect),"missing or wrong-type effect");
  auto id=i.id;require(items_.emplace(id,std::move(i)).second,"duplicate item");
 }
 for(auto& d:dialogues){require(identity(d.id,"dialogue:")&&!d.text.empty()&&d.choices.size()>=2&&d.choices.size()<=64,"invalid dialogue");for(const auto& choice:d.choices)require(!choice.empty(),"empty choice");auto id=d.id;require(dialogues_.emplace(id,std::move(d)).second,"duplicate dialogue");}
 for(auto& f:fields){require(identity(f.id,"field:")&&std::isfinite(f.strength)&&f.strength>=0&&f.strength<=1000000&&(f.direction==1||f.direction==-1),"invalid wind field");auto id=f.id;require(fields_.emplace(id,std::move(f)).second,"duplicate field");}
 for(auto& f:switches){require(identity(f.id,"switch:")&&(f.scope==Scope::session||f.scope==Scope::player),"invalid switch");auto id=f.id;require(switches_.emplace(id,std::move(f)).second,"duplicate switch");}
 for(auto& v:variables){require(identity(v.id,"variable:")&&(v.scope==Scope::session||v.scope==Scope::player)&&v.minimum<=v.initial&&v.initial<=v.maximum,"invalid variable range/scope");auto id=v.id;require(variables_.emplace(id,std::move(v)).second,"duplicate variable");}
 for(auto& e:events){
  require(identity(e.id,"event:")&&!e.commands.empty()&&e.commands.size()<=256,"invalid event");
  for(std::size_t index=0;index<e.commands.size();++index){
   const auto& c=e.commands[index];
   require(c.skip==0||c.op==EventOp::if_switch,"unexpected command skip");
   require(c.receipt.empty()||c.op==EventOp::grant_reward,"unexpected reward claim key");
   require((c.op==EventOp::spawn_character||c.op==EventOp::spawn_actor)||(c.x==0&&c.y==0),"unexpected spawn coordinates");
   require(c.offer.empty()||c.op==EventOp::buy_shop_item||c.op==EventOp::sell_shop_item,"unexpected shop offer");
   switch(c.op){
   case EventOp::grant_skill:case EventOp::revoke_skill:require(identity(c.target,"ability:")&&c.value==0,"invalid skill command");break;
   case EventOp::spawn_actor:require(identity(c.target,"actor:")&&c.value==0&&c.x>=-10000000&&c.x<=10000000&&c.y>=-10000000&&c.y<=10000000,"invalid actor spawn");break;
   case EventOp::spawn_character:require(identity(c.target,"character:")&&c.value==0&&c.x>=-10000000&&c.x<=10000000&&c.y>=-10000000&&c.y<=10000000,"invalid character spawn");break;
   case EventOp::set_switch:require(flag(c.target)&&(c.value==0||c.value==1),"invalid switch command");break;
   case EventOp::set_variable:{auto* v=variable(c.target);require(v&&c.value>=v->minimum&&c.value<=v->maximum,"invalid variable assignment");break;}
   case EventOp::add_variable:require(variable(c.target),"invalid variable reference");break;
   case EventOp::grant_item:case EventOp::remove_item:{auto* i=item(c.target);require(i&&i->storage==Storage::stack&&c.value>0&&c.value<=1000000,"invalid event inventory command");break;}
   case EventOp::buy_shop_item:case EventOp::sell_shop_item:require(identity(c.target,"shop:")&&identity(c.offer,"offer:")&&c.value>=1&&c.value<=1000000,"invalid shop command");break;
   case EventOp::accept_quest:case EventOp::claim_quest:require(identity(c.target,"quest:")&&c.value==0,"invalid quest command");break;
   case EventOp::grant_reward:require(identity(c.target,"source:")&&text(c.receipt)&&c.receipt.size()<=96&&c.value==0&&std::all_of(c.receipt.begin(),c.receipt.end(),[](unsigned char ch){return (ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-'||ch=='.'||ch=='/';}),"invalid event reward command");break;
   case EventOp::advance_calendar:require(c.target.empty()&&c.value>=1&&c.value<=31536000,"invalid calendar skip");break;
   case EventOp::wait_ticks:require(c.target.empty()&&c.value>=1&&c.value<=216000,"invalid event wait");break;
   case EventOp::if_switch:require(flag(c.target)&&(c.value==0||c.value==1)&&c.skip>=1&&index+1+static_cast<std::size_t>(c.skip)<=e.commands.size(),"invalid event branch");break;
   default:require(false,"unknown event command");
   }
  }
  auto id=e.id;require(events_.emplace(id,std::move(e)).second,"duplicate event");
 }
 for(const auto& entry:dialogues_){const auto& d=entry.second;require(d.outcomes.empty()||d.outcomes.size()==d.choices.size(),"dialogue outcome count mismatch");require(d.choice_help.empty()||d.choice_help.size()==d.choices.size(),"dialogue choice-help count mismatch");for(const auto& outcome:d.outcomes)require(outcome.empty()||event(outcome),"missing dialogue outcome event");}
 std::ostringstream signature;signature.imbue(std::locale::classic());signature<<std::hexfloat;
 auto add=[&](const std::string& value){signature<<value.size()<<':'<<value<<';';};
 for(const auto& entry:items_){const auto& i=entry.second;add(i.id);add(i.name);signature<<static_cast<int>(i.storage)<<';'<<i.max_stack<<';';add(i.slot);add(i.effect);add(i.category);add(i.description);}
 for(const auto& entry:effects_){add(entry.first);signature<<entry.second.amount<<';';}
 for(const auto& entry:dialogues_){const auto& d=entry.second;add(d.id);add(d.text);signature<<d.choices.size()<<';';for(const auto& choice:d.choices)add(choice);signature<<d.outcomes.size()<<';';for(const auto& outcome:d.outcomes)add(outcome);signature<<d.choice_help.size()<<';';for(const auto& help:d.choice_help)add(help);}
 for(const auto& entry:fields_){add(entry.first);signature<<entry.second.strength<<';'<<entry.second.direction<<';';}
 for(const auto& entry:switches_){const auto& f=entry.second;add(f.id);signature<<static_cast<int>(f.scope)<<';'<<f.initial<<';';}
 for(const auto& entry:variables_){const auto& v=entry.second;add(v.id);signature<<static_cast<int>(v.scope)<<';'<<v.initial<<';'<<v.minimum<<';'<<v.maximum<<';';}
 for(const auto& entry:events_){add(entry.first);signature<<entry.second.commands.size()<<';';for(const auto& c:entry.second.commands){signature<<static_cast<int>(c.op)<<';';add(c.target);signature<<c.value<<';'<<c.skip<<';';if(c.op==EventOp::spawn_character||c.op==EventOp::spawn_actor)signature<<c.x<<';'<<c.y<<';';if(c.op==EventOp::grant_reward)add(c.receipt);if(c.op==EventOp::buy_shop_item||c.op==EventOp::sell_shop_item)add(c.offer);}}
 signature_=signature.str();

}
const ItemDef* Catalog::item(const std::string& s) const noexcept{return lookup(items_,s);}
const EffectDef* Catalog::effect(const std::string& s) const noexcept{return lookup(effects_,s);}
const DialogueDef* Catalog::dialogue(const std::string& s) const noexcept{return lookup(dialogues_,s);}
const FieldDef* Catalog::field(const std::string& s) const noexcept{return lookup(fields_,s);}
const SwitchDef* Catalog::flag(const std::string& s) const noexcept{return lookup(switches_,s);}
const VariableDef* Catalog::variable(const std::string& s) const noexcept{return lookup(variables_,s);}
const EventDef* Catalog::event(const std::string& s) const noexcept{return lookup(events_,s);}
Inventory::Inventory(const Catalog& catalog,bool enabled):catalog_(catalog),enabled_(enabled){state_.catalog_signature=catalog_.signature();}
const Bag* Inventory::bag(const std::string& owner) const noexcept{return lookup(state_.bags,owner);}
std::vector<InventoryEntry> Inventory::entries(const std::string& owner) const {
 std::vector<InventoryEntry> result;auto* b=bag(owner);if(!b)return result;
 auto append=[&](const std::string& definition,const std::string& instance,int quantity){
  const auto* item=catalog_.item(definition);if(!item)return;
  Selection selected{definition,instance};bool equipped=false,assigned=false;
  for(const auto& slot:b->equipped)equipped=equipped||(!instance.empty()&&slot.second==instance);
  for(const auto& slot:b->assigned)assigned=assigned||slot.second==selected;
  result.push_back({selected,item->name,item->category,item->description,item->slot,quantity,equipped,assigned,!item->effect.empty()});
 };
 for(const auto& entry:b->stacks)append(entry.first,"",entry.second);
 for(const auto& entry:b->unique)append(entry.second,entry.first,1);
 std::sort(result.begin(),result.end(),[](const InventoryEntry& a,const InventoryEntry& b){return std::tie(a.selection.definition,a.selection.instance)<std::tie(b.selection.definition,b.selection.instance);});
 return result;
}
Result Inventory::resize(const std::string& owner,int slots){
 if(busy_)return {Code::busy};
 if(!enabled_)return {Code::disabled};
 if(!text(owner)||slots<1||slots>128)return {Code::invalid};
 auto* current=bag(owner);if(current&&current->stacks.size()+current->unique.size()>static_cast<unsigned>(slots))return {Code::denied};
 auto next=state_;next.bags[owner].slot_limit=slots;validate(next);swap_state(state_,next);return {Code::ok};
}
Result Inventory::execute(const Request& request,EffectPort* port){
 if(busy_)return {Code::busy};
 if(!enabled_)return {Code::disabled};
 if(!request_shape(request))return {Code::invalid};
 const auto key=std::make_pair(request.owner,request.transaction);
 auto previous=state_.receipts.find(key);
 if(previous!=state_.receipts.end())return previous->second.request==request?Result{Code::ok,true}:Result{Code::conflict};
 BusyGuard guard(busy_);
 const ItemDef* item=request.op==Op::unequip?nullptr:catalog_.item(request.definition);
 if(request.op!=Op::unequip&&!item)return {Code::invalid};
 InventoryState next=state_;Bag& b=next.bags[request.owner];const EffectDef* effect=nullptr;
 switch(request.op){
 case Op::grant:
  if(item->storage==Storage::stack){
   if(!request.instance.empty())return {Code::invalid};
   auto found=b.stacks.find(item->id);int count=found==b.stacks.end()?0:found->second;
   if(static_cast<std::int64_t>(count)+request.quantity>item->max_stack)return {Code::denied};
   b.stacks[item->id]=count+request.quantity;
  }else{
   if(request.quantity!=1||!text(request.instance))return {Code::invalid};
   if(!next.issued_instances.insert(request.instance).second)return {Code::denied};
   b.unique.emplace(request.instance,item->id);
  }
  break;
 case Op::remove:
  if(!owned(b,*item,request.instance))return {Code::denied};
  if(item->storage==Storage::stack){
   auto& count=b.stacks.at(item->id);if(request.quantity>count)return {Code::denied};count-=request.quantity;
   if(count==0){b.stacks.erase(item->id);clear_assignments(b,{item->id,""});}
  }else{
   if(request.quantity!=1)return {Code::invalid};
   for(const auto& equipped:b.equipped)if(equipped.second==request.instance)return {Code::denied};
   b.unique.erase(request.instance);clear_assignments(b,{item->id,request.instance});
  }
  break;
 case Op::equip:
  if(item->storage!=Storage::unique||!owned(b,*item,request.instance)||request.slot!=item->slot)return {Code::denied};
  b.equipped[request.slot]=request.instance;break;
 case Op::unequip:
  if(!b.equipped.erase(request.slot))return {Code::denied};
  break;
 case Op::assign:
  if(!owned(b,*item,request.instance))return {Code::denied};
  b.assigned[request.slot]={item->id,request.instance};break;
 case Op::use:
  if(item->storage!=Storage::stack||!owned(b,*item,""))return {Code::denied};
  effect=catalog_.effect(item->effect);
  if(!port||!effect||!port->can_apply(request.owner,*effect))return {Code::denied};
  if(--b.stacks.at(item->id)==0){b.stacks.erase(item->id);clear_assignments(b,{item->id,""});}
  break;
 }
 if(b.slot_limit&&b.stacks.size()+b.unique.size()>static_cast<unsigned>(b.slot_limit))return {Code::denied};
 next.receipts.emplace(key,Receipt{request});validate(next);
 // All allocation/validation occurs before the external non-throwing mutation.
 // Port must not publish callbacks; execute rejects reentrant writes.
 if(effect)port->apply(request.owner,*effect);
 swap_state(state_,next);return {Code::ok};
}
InventoryState Inventory::capture() const {if(busy_)throw std::logic_error("capture during inventory commit");return state_;}
void Inventory::validate(const InventoryState& state) const {
 require(state.catalog_signature==catalog_.signature(),"inventory content snapshot mismatch");
 require(enabled_||(state.bags.empty()&&state.receipts.empty()&&state.issued_instances.empty()),"inventory disabled");std::set<std::string> unique;
 for(const auto& id:state.issued_instances)require(text(id),"invalid issued identity");
 for(const auto& owner:state.bags){
  require(text(owner.first),"invalid owner");const auto& b=owner.second;
  require(b.slot_limit>=0&&b.slot_limit<=128&&(!b.slot_limit||b.stacks.size()+b.unique.size()<=static_cast<unsigned>(b.slot_limit)),"invalid inventory capacity");
  for(const auto& stack:b.stacks){const auto* def=catalog_.item(stack.first);require(def&&def->storage==Storage::stack&&stack.second>0&&stack.second<=def->max_stack,"invalid stack");}
  for(const auto& u:b.unique){const auto* def=catalog_.item(u.second);require(def&&def->storage==Storage::unique&&text(u.first)&&unique.insert(u.first).second&&state.issued_instances.count(u.first),"invalid or duplicate unique instance");}
  for(const auto& equipped:b.equipped){auto u=b.unique.find(equipped.second);require(u!=b.unique.end(),"equipment not owned");const auto* def=catalog_.item(u->second);require(def&&equipped.first==def->slot,"equipment slot mismatch");}
  for(const auto& assigned:b.assigned){const auto* def=catalog_.item(assigned.second.definition);require(text(assigned.first)&&def&&owned(b,*def,assigned.second.instance),"assignment not owned");}
 }
 for(const auto& receipt:state.receipts){
  const auto& r=receipt.second.request;require(request_shape(r)&&receipt.first==std::make_pair(r.owner,r.transaction),"invalid transaction receipt");
  require(state.bags.count(r.owner)>0,"receipt owner missing");
  const auto* def=r.op==Op::unequip?nullptr:catalog_.item(r.definition);
  require(r.op==Op::unequip||def,"receipt has missing definition");
  if(def){
   if(def->storage==Storage::stack)require(r.instance.empty()&&r.quantity<=def->max_stack,"invalid stack receipt");
   else require(text(r.instance)&&r.quantity==1&&state.issued_instances.count(r.instance),"invalid instance receipt");
   if(r.op==Op::equip)require(def->storage==Storage::unique&&r.slot==def->slot,"invalid equip receipt");
   if(r.op==Op::use)require(def->storage==Storage::stack&&!def->effect.empty(),"invalid use receipt");
  }
 }
}
void Inventory::restore(const InventoryState& state){if(busy_)throw std::logic_error("restore during inventory commit");validate(state);auto next=state;swap_state(state_,next);}
Lease Policies::menu(const std::string& owner,bool pause){require(text(owner),"invalid menu owner");require(next_!=std::numeric_limits<Lease>::max(),"lease identity exhausted");auto id=next_++;entries_.emplace(id,Entry{owner,{owner},pause,false,1});return id;}
Lease Policies::dialogue(const std::string& owner,const std::vector<std::string>& participants,double factor){
 require(text(owner)&&!participants.empty()&&std::isfinite(factor)&&factor>=0&&factor<=1,"invalid dialogue lease");for(const auto& p:participants)require(text(p),"invalid participant");
 require(next_!=std::numeric_limits<Lease>::max(),"lease identity exhausted");auto id=next_++;entries_.emplace(id,Entry{owner,participants,false,true,factor});return id;
}
bool Policies::close(Lease id) noexcept{return entries_.erase(id)>0;}
void Policies::interrupt(const std::string& participant){for(auto i=entries_.begin();i!=entries_.end();)if(i->second.is_dialogue&&std::find(i->second.participants.begin(),i->second.participants.end(),participant)!=i->second.participants.end())i=entries_.erase(i);else ++i;}
bool Policies::paused() const noexcept{for(const auto& e:entries_)if(e.second.pause)return true;return false;}
bool Policies::blocked(const std::string& p) const noexcept{if(paused())return true;for(const auto& e:entries_)if(std::find(e.second.participants.begin(),e.second.participants.end(),p)!=e.second.participants.end())return true;return false;}
double Policies::calendar_factor() const noexcept{if(paused())return 0;double factor=1;for(const auto& e:entries_)if(e.second.is_dialogue)factor=std::min(factor,e.second.factor);return factor;}
TimeSlice Policies::sample(double elapsed) const{require(std::isfinite(elapsed)&&elapsed>=0,"invalid elapsed time");return {paused()?0:elapsed,elapsed*calendar_factor(),elapsed};}
Conversation::Conversation(const Catalog& catalog,Policies& policies):catalog_(catalog),policies_(policies){}
Conversation::~Conversation(){cancel();}
void Conversation::open(const std::string& definition,const std::string& owner,const std::vector<std::string>& participants,double factor){
 if(state_==ConversationState::open&&state()!=ConversationState::open)finish(ConversationState::interrupted);
 require(state_!=ConversationState::open,"conversation already open");
 const auto* next=catalog_.dialogue(definition);require(next,"unknown dialogue");
 auto next_participants=participants;
 auto lease=policies_.dialogue(owner,participants,factor);
 participants_=std::move(next_participants);definition_=next;lease_=lease;choice_=-1;state_=ConversationState::open;
}
void Conversation::finish(ConversationState state) noexcept{if(state_==ConversationState::open){bool owned=policies_.active(lease_);policies_.close(lease_);state_=owned?state:ConversationState::interrupted;lease_=0;}}
bool Conversation::choose(int index) noexcept{
 if(state()!=ConversationState::open){finish(ConversationState::interrupted);return false;}
 if(index<0||static_cast<std::size_t>(index)>=definition_->choices.size())return false;
 choice_=index;finish(ConversationState::chosen);return true;
}
bool Conversation::hit(const std::string& participant) noexcept{
 if(state()!=ConversationState::open||std::find(participants_.begin(),participants_.end(),participant)==participants_.end())return false;
 finish(ConversationState::interrupted);return true;
}
void Conversation::cancel() noexcept{finish(ConversationState::cancelled);}
WindHelper::WindHelper(std::string instance,FieldDef definition):instance_(std::move(instance)),definition_(std::move(definition)){
 require(text(instance_)&&std::isfinite(definition_.strength)&&definition_.strength>=0&&definition_.strength<=1000000&&(definition_.direction==1||definition_.direction==-1),"invalid wind helper");
}
std::vector<LoopChange> WindHelper::enter(const std::string& p){require(text(p),"invalid wind participant");bool empty=occupants_.empty();if(occupants_.insert(p).second&&empty)return {LoopChange::start};return {};}
std::vector<LoopChange> WindHelper::leave(const std::string& p){if(occupants_.erase(p)&&occupants_.empty())return {LoopChange::stop};return {};}
std::vector<LoopChange> WindHelper::clear(){if(occupants_.empty())return {};occupants_.clear();return {LoopChange::stop};}
std::vector<FieldRequest> WindHelper::requests() const{std::vector<FieldRequest> result;for(const auto& p:occupants_)result.push_back({instance_,p,definition_.strength*definition_.direction});return result;}
}
