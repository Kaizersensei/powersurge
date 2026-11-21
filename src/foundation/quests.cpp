// No separate mutable quest journal: selected switches plus atomic learning claims are authoritative. Signed: Atlas.
#include "quests.hpp"
#include <algorithm>
#include <stdexcept>
namespace bsl {
namespace {bool key(const std::string& s){return !s.empty()&&s.size()<=256&&s.find('\0')==std::string::npos;}void need(bool ok){if(!ok)throw std::invalid_argument("invalid quest catalog/service bundle");}}
Quests::Quests(Inventory& inventory,Events& events,CharacterLearning& learning,std::vector<QuestDef> definitions,std::map<std::string,std::string> owners):inventory_(inventory),events_(events),learning_(&learning),definitions_(std::move(definitions)),owners_(std::move(owners)){
 need(&events_.inventory_==&inventory_&&events_.catalog_signature()==inventory_.capture().catalog_signature&&definitions_.size()<=64&&!owners_.empty()&&owners_.size()<=64);
 for(const auto& o:owners_)need(key(o.first)&&key(o.second));
 std::set<std::string> ids,state;
 for(const auto& q:definitions_){
  need(key(q.id)&&key(q.name)&&q.description.size()<=4096&&ids.insert(q.id).second&&!q.objectives.empty()&&q.objectives.size()<=16&&q.prerequisites.size()<=16);
  for(const auto& ref:{q.accepted,q.completed}){const auto* flag=events_.catalog_.flag(ref);need(flag&&flag->scope==Scope::player&&!flag->initial&&state.insert(ref).second);}
  const auto* source=learning_->source(q.source);need(source&&source->model.empty());
  std::set<std::string> prerequisites;
  for(const auto& ref:q.prerequisites)need(events_.catalog_.flag(ref)&&ref!=q.accepted&&ref!=q.completed&&prerequisites.insert(ref).second);
  for(const auto& o:q.objectives){
   need(key(o.label));
   if(o.kind==ObjectiveKind::item_count){const auto* item=events_.catalog_.item(o.target);need(item&&item->storage==Storage::stack&&o.minimum>=1&&o.minimum<=item->max_stack);}
   else if(o.kind==ObjectiveKind::flag)need(events_.catalog_.flag(o.target)&&o.target!=q.accepted&&o.target!=q.completed&&o.minimum==1);
   else if(o.kind==ObjectiveKind::variable){const auto* v=events_.catalog_.variable(o.target);need(v&&o.minimum>=v->minimum&&o.minimum<=v->maximum);}
   else need(false);
  }
 }
}
const QuestDef* Quests::definition(const std::string& id) const noexcept {for(const auto& q:definitions_)if(q.id==id)return &q;return nullptr;}
QuestView Quests::view(const std::string& id,const std::string& player) const {
 const auto* q=definition(id);need(q&&std::any_of(owners_.begin(),owners_.end(),[&](const auto& owner){return owner.second==player;}));
 bool ready=true;QuestView result{QuestStatus::active,{}};
 for(const auto& o:q->objectives){int current=0;
  if(o.kind==ObjectiveKind::flag)current=events_.flag(o.target,player)?1:0;
  else if(o.kind==ObjectiveKind::variable)current=events_.variable(o.target,player);
  else if(const auto* bag=inventory_.bag(player)){const auto found=bag->stacks.find(o.target);if(found!=bag->stacks.end())current=found->second;}
  const bool met=current>=o.minimum;ready=ready&&met;result.objectives.push_back({o.label,current,o.minimum,met});
 }
 const bool accepted=events_.flag(q->accepted,player),completed=events_.flag(q->completed,player);
 if(completed)result.status=accepted?QuestStatus::completed:QuestStatus::invalid;
 else if(accepted)result.status=ready?QuestStatus::ready:QuestStatus::active;
 else result.status=std::all_of(q->prerequisites.begin(),q->prerequisites.end(),[&](const auto& ref){return events_.flag(ref,player);})?QuestStatus::available:QuestStatus::locked;
 return result;
}
void Quests::validate_progress(const EventState& state,const LearningState& learning) const {
 need(state.catalog_signature==events_.catalog_signature());
 std::set<std::string> players;for(const auto& owner:owners_)players.insert(owner.second);
 for(const auto& player:players)for(const auto& q:definitions_){
  const auto accepted=state.values.find({q.accepted,player}),completed=state.values.find({q.completed,player});
  const bool active=accepted!=state.values.end()&&accepted->second!=0,done=completed!=state.values.end()&&completed->second!=0;
  const auto receipt=learning.receipts.find("quest/"+std::to_string(player.size())+":"+player+"/"+q.id);
  need(done==(receipt!=learning.receipts.end())&&(!done||active));
  if(done){const auto owner=owners_.find(receipt->second.character);need(receipt->second.source==q.source&&owner!=owners_.end()&&owner->second==player);}
 }
}
Result Quests::invoke(const EventContext& context,const EventCommand& command){
 const auto* q=definition(command.target);auto owner=owners_.find(context.character);
 if(!q||owner==owners_.end()||owner->second!=context.player||(command.op!=EventOp::accept_quest&&command.op!=EventOp::claim_quest))return {Code::denied};
 const auto current=view(q->id,context.player).status;
 if(current==QuestStatus::invalid)return {Code::conflict};
 if(command.op==EventOp::accept_quest){
  if(current!=QuestStatus::available)return {current==QuestStatus::locked?Code::denied:Code::ok,current!=QuestStatus::locked};
  auto values=events_.values_;values[{q->accepted,context.player}]=1;events_.values_.swap(values);return {Code::ok};
 }
 const auto receipt="quest/"+std::to_string(context.player.size())+":"+context.player+"/"+q->id;
 if(receipt.size()>256)return {Code::invalid};
 const auto learning=learning_->capture();const auto previous=learning.receipts.find(receipt);
 if(current==QuestStatus::completed)return {previous!=learning.receipts.end()&&previous->second.source==q->source&&previous->second.character==context.character?Code::ok:Code::conflict,true};
 if(previous!=learning.receipts.end())return {Code::conflict};
 if(current!=QuestStatus::ready)return {Code::denied};
 auto values=events_.values_;values[{q->completed,context.player}]=1;
 // All flag allocation/validation precedes the existing atomic XP/unlock/receipt commit.
 const auto result=learning_->award(context.character,q->source,receipt);
 if(result.code!=Code::ok)return {result.code,result.replay};
 events_.values_.swap(values);return {Code::ok,false};
}
}
