// Existing BOR health is the effect target; no duplicate stat store. Signed: Atlas.
#include "bor_health_effects.hpp"
#include <algorithm>
#include <stdexcept>
namespace bsl {
void BorHealthEffects::bind(const std::string& owner,BorActor& actor){
 if(owner.empty()||owner.size()>256||owner.find('\0')!=std::string::npos)throw std::invalid_argument("invalid effect owner");
 for(const auto& binding:actors_)if(binding.second==&actor&&binding.first!=owner)throw std::invalid_argument("actor already bound to another owner");
 auto existing=actors_.find(owner);if(existing!=actors_.end()&&existing->second!=&actor)throw std::invalid_argument("unbind old actor before replacement");
 actors_[owner]=&actor;
}
void BorHealthEffects::unbind(const std::string& owner) noexcept{actors_.erase(owner);}
bool BorHealthEffects::can_apply(const std::string& owner,const EffectDef& e) const noexcept{
 auto found=actors_.find(owner);if(found==actors_.end())return false;const auto& a=*found->second;
 return a.model&&a.state!=BOR_DEAD&&e.amount>0&&a.health>0&&a.health<a.model->health;
}
void BorHealthEffects::apply(const std::string& owner,const EffectDef& e) noexcept{
 auto& a=*actors_.find(owner)->second;
 a.health=static_cast<int>(std::min<std::int64_t>(a.model->health,static_cast<std::int64_t>(a.health)+e.amount));
}
}
