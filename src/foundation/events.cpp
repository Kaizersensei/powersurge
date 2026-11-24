// Original implementation inspired by event-authoring requirements. Signed: Atlas.
#include "events.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace bsl {
namespace {
bool terminal(EventStatus s){return s==EventStatus::completed||s==EventStatus::cancelled||s==EventStatus::failed;}
bool identity(const std::string& s){return !s.empty()&&s.size()<=256&&s.find('\0')==std::string::npos;}
bool run_key(const std::string& s){return !s.empty()&&s.size()<=128&&std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-';});}
}
Events::Events(const Catalog& catalog,Inventory& inventory):catalog_(catalog),inventory_(inventory){
 if(inventory_.capture().catalog_signature!=catalog_.signature())throw std::invalid_argument("event/inventory catalog mismatch");
}
void Events::bind_rewards(EventRewardPort& port){
 if(rewards_||!runs_.empty())throw std::invalid_argument("reward adapter must bind once before event execution");
 rewards_=&port;
}
void Events::bind_skills(EventSkillPort& port){
 if(skills_||!runs_.empty())throw std::invalid_argument("skill adapter must bind once before event execution");
 for(const auto& entry:catalog_.events())for(std::size_t i=0;i<entry.second.commands.size();++i){const auto& command=entry.second.commands[i];if((command.op==EventOp::grant_skill||command.op==EventOp::revoke_skill)&&!port.knows_ability(command.target))throw std::invalid_argument("Event "+entry.first+" command "+std::to_string(i+1)+" references unknown ability "+command.target);}
 skills_=&port;
}
void Events::bind_quests(EventQuestPort& port){
 if(quests_||!runs_.empty())throw std::invalid_argument("quest adapter must bind once before event execution");
 quests_=&port;
}
void Events::bind_trades(EventTradePort& port){
 if(trades_||!runs_.empty())throw std::invalid_argument("trade adapter must bind once before event execution");
 trades_=&port;
}
void Events::bind_clock(EventClockPort& port){if(clock_||!runs_.empty())throw std::invalid_argument("clock adapter must bind once before event execution");clock_=&port;}
void Events::bind_actors(EventActorPort& port){if(actors_||!runs_.empty())throw std::invalid_argument("actor adapter must bind once before event execution");actors_=&port;}
std::string Events::key(Scope scope,const std::string& player) const {
 if(scope==Scope::session)return "";
 if(!identity(player))throw std::invalid_argument("player-scoped state requires explicit player subject");
 return player;
}
int Events::value(const std::string& id,Scope scope,int initial,const std::string& player) const {
 auto found=values_.find({id,key(scope,player)});return found==values_.end()?initial:found->second;
}
bool Events::flag(const std::string& id,const std::string& player) const {
 auto* f=catalog_.flag(id);if(!f)throw std::invalid_argument("unknown switch");return value(id,f->scope,f->initial,player)!=0;
}
int Events::variable(const std::string& id,const std::string& player) const {
 auto* v=catalog_.variable(id);if(!v)throw std::invalid_argument("unknown variable");return value(id,v->scope,v->initial,player);
}
Result Events::start(const std::string& id,const std::string& definition,const EventContext& context){
 if(context.travel&&!valid_travel_context(*context.travel))return {Code::invalid};
 if(!run_key(id)||!catalog_.event(definition)||!identity(context.owner)||!identity(context.player))return {Code::invalid};
 auto found=runs_.find(id);
 if(found!=runs_.end()){
  const auto& r=found->second;
  const bool same=r.definition==definition&&r.context.owner==context.owner&&r.context.player==context.player&&r.context.character==context.character&&r.context.travel==context.travel;
  return {same?Code::ok:Code::conflict,same};
 }
 const auto* event=catalog_.event(definition);
 const bool skill=std::any_of(event->commands.begin(),event->commands.end(),[](const EventCommand& c){return c.op==EventOp::grant_skill||c.op==EventOp::revoke_skill;});
 const bool reward=std::any_of(event->commands.begin(),event->commands.end(),[](const EventCommand& c){return c.op==EventOp::grant_reward;});
 const bool quest=std::any_of(event->commands.begin(),event->commands.end(),[](const EventCommand& c){return c.op==EventOp::accept_quest||c.op==EventOp::claim_quest;});
 const bool actor=std::any_of(event->commands.begin(),event->commands.end(),[](const EventCommand& c){return c.op==EventOp::spawn_character||c.op==EventOp::spawn_actor;});
 if((!context.character.empty()&&!identity(context.character))||((reward||quest||actor||skill)&&!identity(context.character)))return {Code::invalid};
 const bool trade=std::any_of(event->commands.begin(),event->commands.end(),[](const EventCommand& c){return c.op==EventOp::buy_shop_item||c.op==EventOp::sell_shop_item;});
 const bool clock=std::any_of(event->commands.begin(),event->commands.end(),[](const EventCommand& c){return c.op==EventOp::advance_calendar;});
 if((skill&&!skills_)||(actor&&!actors_)||(clock&&!clock_)||(reward&&!rewards_)||(quest&&!quests_)||(trade&&!trades_))return {Code::disabled};
 if(runs_.size()>=4096)return {Code::denied};
 EventRun r;r.id=id;r.definition=definition;r.context=context;runs_.emplace(id,std::move(r));return {Code::ok};
}
const EventRun* Events::run(const std::string& id) const noexcept {auto found=runs_.find(id);return found==runs_.end()?nullptr:&found->second;}
bool Events::cancel(const std::string& id) noexcept {
 auto found=runs_.find(id);if(found==runs_.end()||terminal(found->second.status))return false;
 found->second.status=EventStatus::cancelled;found->second.wait_ticks=0;return true;
}
void Events::cancel_participant(const std::string& id) noexcept {
 for(auto& entry:runs_){auto& r=entry.second;if((r.context.owner==id||r.context.player==id)&&!terminal(r.status)){r.status=EventStatus::cancelled;r.wait_ticks=0;}}
}
Code Events::execute(EventRun& r,const EventCommand& c){
 switch(c.op){
 case EventOp::spawn_character:case EventOp::spawn_actor:{if(!actors_)return Code::disabled;const auto result=actors_->spawn(r.context,c);if(result.code!=Code::ok)return result.code;break;}
 case EventOp::advance_calendar:{if(!clock_)return Code::disabled;auto result=clock_->skip(r.context,c.value);if(result.code!=Code::ok)return result.code;break;}
 case EventOp::set_switch:{auto* f=catalog_.flag(c.target);values_[{c.target,key(f->scope,r.context.player)}]=c.value;break;}
 case EventOp::set_variable:case EventOp::add_variable:{
  auto* v=catalog_.variable(c.target);std::int64_t next=c.value;
  if(c.op==EventOp::add_variable)next+=variable(c.target,r.context.player);
  if(next<v->minimum||next>v->maximum)return Code::denied;
  values_[{c.target,key(v->scope,r.context.player)}]=static_cast<int>(next);break;
 }
 case EventOp::grant_item:case EventOp::remove_item:{
  const auto op=c.op==EventOp::grant_item?Op::grant:Op::remove;
  const auto result=inventory_.execute({"event/"+r.id+"/"+std::to_string(r.next),r.context.player,op,c.target,"","",c.value});
  if(result.code!=Code::ok)return result.code;
  break;
 }
 case EventOp::grant_skill:case EventOp::revoke_skill:{if(!skills_)return Code::disabled;const auto result=skills_->invoke(r.context,c);if(result.code!=Code::ok)return result.code;break;}
 case EventOp::grant_reward:{
  if(!rewards_)return Code::disabled;
  const auto result=rewards_->grant(r.context,c);if(result.code!=Code::ok)return result.code;
  break;
 }
 case EventOp::accept_quest:case EventOp::claim_quest:{
  if(!quests_)return Code::disabled;
  const auto result=quests_->invoke(r.context,c);if(result.code!=Code::ok)return result.code;
  break;
 }
 case EventOp::buy_shop_item:case EventOp::sell_shop_item:{
  if(!trades_)return Code::disabled;
  const auto result=trades_->invoke(r.context,c,"event/"+r.id+"/"+std::to_string(r.next));if(result.code!=Code::ok)return result.code;
  break;
 }
 case EventOp::wait_ticks:r.wait_ticks=c.value;r.status=EventStatus::waiting;break;
 case EventOp::if_switch:if(flag(c.target,r.context.player)!=(c.value!=0))r.next+=static_cast<std::size_t>(c.skip);break;
 }
 ++r.next;return Code::ok;
}
void Events::tick(unsigned command_budget){
 if(command_budget<1||command_budget>64)throw std::invalid_argument("event command budget must be1..64");
 for(auto& entry:runs_){
  auto& r=entry.second;if(terminal(r.status))continue;
  if(r.wait_ticks>0&&--r.wait_ticks>0)continue;
  const auto* definition=catalog_.event(r.definition);
  r.status=EventStatus::running;
  while(command_budget&&r.next<definition->commands.size()){
   --command_budget;
   auto result=execute(r,definition->commands[r.next]);
   if(result!=Code::ok){r.error=result;r.status=EventStatus::failed;break;}
   if(r.status==EventStatus::waiting)break;
  }
  if(r.status!=EventStatus::failed&&r.status!=EventStatus::waiting&&r.next==definition->commands.size())r.status=EventStatus::completed;
 }
}
EventState Events::capture() const {return {catalog_.signature(),values_,runs_};}
void Events::restore(const EventState& state){
 auto require=[](bool ok){if(!ok)throw std::invalid_argument("invalid event snapshot");};
 require(state.catalog_signature==catalog_.signature()&&state.runs.size()<=4096&&state.values.size()<=16384);
 for(const auto& entry:state.values){
  const auto* flag=catalog_.flag(entry.first.first);const auto* variable=catalog_.variable(entry.first.first);
  require(flag||variable);Scope scope=flag?flag->scope:variable->scope;
  require(scope==Scope::session?entry.first.second.empty():identity(entry.first.second));
  require(flag?(entry.second==0||entry.second==1):(entry.second>=variable->minimum&&entry.second<=variable->maximum));
 }
 for(const auto& entry:state.runs){
  const auto& r=entry.second;const auto* def=catalog_.event(r.definition);
  require(run_key(r.id)&&entry.first==r.id&&def&&identity(r.context.owner)&&identity(r.context.player));
  require(r.context.character.empty()||identity(r.context.character));
  require(!r.context.travel||valid_travel_context(*r.context.travel));
  if(std::any_of(def->commands.begin(),def->commands.end(),[](const EventCommand& c){return c.op==EventOp::grant_skill||c.op==EventOp::revoke_skill||c.op==EventOp::grant_reward||c.op==EventOp::accept_quest||c.op==EventOp::claim_quest||c.op==EventOp::spawn_character||c.op==EventOp::spawn_actor;}))require(identity(r.context.character));
  require(static_cast<int>(r.status)>=0&&r.status<=EventStatus::failed&&static_cast<int>(r.error)>=0&&r.error<=Code::busy&&r.next<=def->commands.size());
  require(r.status==EventStatus::failed?r.error!=Code::ok:r.error==Code::ok);
  if(r.status==EventStatus::waiting)require(r.next>0&&def->commands[r.next-1].op==EventOp::wait_ticks&&r.wait_ticks>0&&r.wait_ticks<=def->commands[r.next-1].value);
  else require(r.wait_ticks==0);
  if(r.status==EventStatus::queued)require(r.next==0);
  if(r.status==EventStatus::completed)require(r.next==def->commands.size());
  if(r.status==EventStatus::failed||r.status==EventStatus::running)require(r.next<def->commands.size());
 }
 auto values=state.values;auto runs=state.runs;
 // Loading interrupts unfinished sequences; completed effects/journals remain committed.
 for(auto& entry:runs)if(!terminal(entry.second.status)){entry.second.status=EventStatus::cancelled;entry.second.wait_ticks=0;}
 values_.swap(values);runs_.swap(runs);
}

}
