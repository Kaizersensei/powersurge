// Named rewards and persistent-in-session ability learning over generic stats. Signed: Atlas.
#pragma once
#include "progression.hpp"
namespace bsl {
struct StatRequirement {std::string stat;std::int64_t minimum=0;};
struct AbilityDef {std::string id,name,animation,tag;std::vector<StatRequirement> conditions;std::string capability{};};
struct RewardDef {std::string id,name;std::vector<StatGrant> grants;};
struct RewardSource {std::string id,reward,model;}; // Empty model: trusted script source; otherwise enemy defeat.
struct LearningProfile {std::string definition;std::vector<std::string> abilities,rewards;};
struct LearningCatalog {std::vector<AbilityDef> abilities;std::vector<RewardDef> rewards;std::vector<RewardSource> sources;std::vector<LearningProfile> profiles;};
struct LearningResult {Code code=Code::invalid;bool replay=false;std::vector<std::string> unlocked;};
struct LearningReceipt {std::string character,source;};
struct LearningState {std::map<std::string,std::set<std::string>> earned;std::map<std::string,LearningReceipt> receipts;std::map<std::string,std::set<std::string>> revoked;};
class CharacterLearning {
public:
 CharacterLearning(CharacterStats&,LearningCatalog);
 std::vector<std::string> refresh(const std::string& character);
 LearningResult award(const std::string& character,const std::string& source,const std::string& receipt);
 const AbilityDef* ability(const std::string& character,const std::string& id) const;
 const AbilityDef* slot(const std::string& character,std::size_t index) const;
 const RewardSource* source(const std::string&) const;
 const RewardSource* defeat(const std::string& model) const;
 bool knows_ability(const std::string& id) const noexcept {return abilities_.find(id)!=abilities_.end();}
 bool learned(const std::string& character,const std::string& id) const;
 // Unassigned capabilities retain the actor template default. Assigned gates need an earned grant. Signed: Mech.
 bool permits(const std::string& character,const std::string& capability) const;
 // Explicit per-character authoring/event operations; definitions stay shared.
 Code grant(const std::string& character,const std::string& ability);
 Code revoke(const std::string& character,const std::string& ability);
 LearningState capture() const {return {earned_,receipts_,revoked_};}
 Code restore(const LearningState&);
 std::size_t receipts() const noexcept {return receipts_.size();}
private:
 CharacterStats& stats_;
 std::map<std::string,AbilityDef> abilities_;
 std::map<std::string,RewardDef> rewards_;
 std::map<std::string,RewardSource> sources_;
 std::map<std::string,LearningProfile> profiles_;
 std::map<std::string,std::set<std::string>> earned_;
 std::map<std::string,LearningReceipt> receipts_;
 std::map<std::string,std::set<std::string>> revoked_;
 std::vector<std::string> learn(const CharacterStats&,const std::string&,std::set<std::string>&,const std::set<std::string>&) const;
};
}
