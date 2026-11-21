// Original reward/learning service. No universal stat names, character classes or input bindings. Signed: Atlas.
#include "learning.hpp"
#include <algorithm>
#include <stdexcept>
namespace bsl {
namespace {
bool id(const std::string& s,std::size_t max=128){return !s.empty()&&s.size()<=max&&s.find('\0')==std::string::npos;}
void require(bool value){if(!value)throw std::invalid_argument("invalid learning catalog/reference");}
}
CharacterLearning::CharacterLearning(CharacterStats& stats,LearningCatalog catalog):stats_(stats){
 require(catalog.abilities.size()<=64&&catalog.rewards.size()<=64&&catalog.sources.size()<=64&&!catalog.profiles.empty()&&catalog.profiles.size()<=64);
 for(const auto& a:catalog.abilities){
  require(id(a.id)&&id(a.name)&&((a.capability.empty()&&id(a.animation))||(!a.capability.empty()&&id(a.capability)&&a.animation.empty()))&&id(a.tag)&&!a.conditions.empty()&&a.conditions.size()<=64);std::set<std::string> seen;
  for(const auto& c:a.conditions){const auto& d=stats_.definition(c.stat);require(seen.insert(c.stat).second&&c.minimum>=d.minimum&&c.minimum<=d.maximum);}
  require(abilities_.emplace(a.id,a).second);
 }
 for(const auto& r:catalog.rewards){
  require(id(r.id)&&id(r.name)&&!r.grants.empty()&&r.grants.size()<=64);std::set<std::string> seen;
  for(const auto& g:r.grants){const auto& d=stats_.definition(g.stat);require(seen.insert(g.stat).second&&g.growth==(d.growth.first>0)&&g.amount>=(g.growth?0:-1000000000)&&g.amount<=1000000000);}
  require(rewards_.emplace(r.id,r).second);
 }
 std::set<std::string> models;
 for(const auto& s:catalog.sources){require(id(s.id)&&rewards_.count(s.reward)&&(s.model.empty()||(id(s.model)&&models.insert(s.model).second)));require(sources_.emplace(s.id,s).second);}
 for(const auto& p:catalog.profiles){
  require(id(p.definition)&&p.abilities.size()<=64&&p.rewards.size()<=64);std::set<std::string> seen;
  for(const auto& a:p.abilities)require(abilities_.count(a)&&seen.insert(a).second);
  seen.clear();for(const auto& r:p.rewards)require(rewards_.count(r)&&seen.insert(r).second);
  require(profiles_.emplace(p.definition,p).second);
 }
}
std::vector<std::string> CharacterLearning::learn(const CharacterStats& stats,const std::string& character,std::set<std::string>& earned) const {
 const auto& p=profiles_.at(stats.profile(character).definition);std::vector<std::string> result;
 // Validate every allowed dependency, including locked abilities and rewards.
 const auto values=stats.capture(character);
 for(const auto& r:p.rewards)for(const auto& g:rewards_.at(r).grants)require(values.count(g.stat));
 for(const auto& ref:p.abilities){
  const auto& a=abilities_.at(ref);bool eligible=true;
  for(const auto& c:a.conditions){require(values.count(c.stat));if(stats.view(character,c.stat).units<c.minimum)eligible=false;}
  if(eligible&&earned.insert(ref).second)result.push_back(ref);
 }
 return result;
}
std::vector<std::string> CharacterLearning::refresh(const std::string& character){
 if(!earned_.count(character)&&earned_.size()>=64)throw std::invalid_argument("too many learning characters");
 auto prepared=earned_;auto result=learn(stats_,character,prepared[character]);earned_.swap(prepared);return result;
}
LearningResult CharacterLearning::award(const std::string& character,const std::string& source,const std::string& receipt){
 if(!id(character,256)||!id(source)||!id(receipt,256)||!earned_.count(character)||!sources_.count(source))return {};
 auto previous=receipts_.find(receipt);
 if(previous!=receipts_.end())return {previous->second.character==character&&previous->second.source==source?Code::ok:Code::conflict,true,{}};
 const auto& p=profiles_.at(stats_.profile(character).definition);const auto& reward=sources_.at(source).reward;
 if(std::find(p.rewards.begin(),p.rewards.end(),reward)==p.rewards.end())return {Code::denied,false,{}};
 if(receipts_.size()>=4096)return {Code::busy,false,{}};
 // Prepare earned values, unlocks and receipt together; failure cannot partially grant a reward.
 auto next_stats=stats_;auto code=next_stats.grant(character,rewards_.at(reward).grants);if(code!=Code::ok)return {code,false,{}};
 auto next_earned=earned_;auto unlocked=learn(next_stats,character,next_earned.at(character));
 auto next_receipts=receipts_;next_receipts.emplace(receipt,LearningReceipt{character,source});
 stats_.swap(next_stats);earned_.swap(next_earned);receipts_.swap(next_receipts);return {Code::ok,false,std::move(unlocked)};
}
Code CharacterLearning::restore(const LearningState& state){
 if(state.earned.size()!=earned_.size()||state.receipts.size()>4096)return Code::invalid;
 for(const auto& c:state.earned){
  if(!earned_.count(c.first))return Code::invalid;
  const auto& p=profiles_.at(stats_.profile(c.first).definition);
  for(const auto& a:c.second)if(std::find(p.abilities.begin(),p.abilities.end(),a)==p.abilities.end())return Code::invalid;
  auto checked=c.second;learn(stats_,c.first,checked);if(checked!=c.second)return Code::invalid;
 }
 for(const auto& r:state.receipts){
  if(!id(r.first,256)||!state.earned.count(r.second.character)||!sources_.count(r.second.source))return Code::invalid;
  const auto& p=profiles_.at(stats_.profile(r.second.character).definition);const auto& reward=sources_.at(r.second.source).reward;
  if(std::find(p.rewards.begin(),p.rewards.end(),reward)==p.rewards.end())return Code::invalid;
 }
 auto prepared=state;earned_.swap(prepared.earned);receipts_.swap(prepared.receipts);return Code::ok;
}
bool CharacterLearning::learned(const std::string& character,const std::string& ability) const {auto c=earned_.find(character);return c!=earned_.end()&&c->second.count(ability);}
bool CharacterLearning::permits(const std::string& character,const std::string& capability) const {
 const auto& p=profiles_.at(stats_.profile(character).definition);bool gated=false;
 for(const auto& ref:p.abilities){const auto& a=abilities_.at(ref);if(a.capability==capability){gated=true;if(learned(character,ref))return true;}}
 return !gated;
}
const AbilityDef* CharacterLearning::ability(const std::string& character,const std::string& id) const {return learned(character,id)?&abilities_.at(id):nullptr;}
const AbilityDef* CharacterLearning::slot(const std::string& character,std::size_t index) const {const auto& p=profiles_.at(stats_.profile(character).definition);return index<p.abilities.size()?&abilities_.at(p.abilities[index]):nullptr;}
const RewardSource* CharacterLearning::source(const std::string& id) const {auto s=sources_.find(id);return s==sources_.end()?nullptr:&s->second;}
const RewardSource* CharacterLearning::defeat(const std::string& model) const {for(const auto& s:sources_)if(s.second.model==model)return &s.second;return nullptr;}
}
