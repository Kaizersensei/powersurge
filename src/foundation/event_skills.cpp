#include "event_skills.hpp"
#include <stdexcept>
namespace bsl {
EventSkills::EventSkills(CharacterLearning& l,std::map<std::string,std::string> owners):learning_(&l),owners_(std::move(owners)){
 if(owners_.empty()||owners_.size()>64)throw std::invalid_argument("invalid skill owners");
 for(const auto& p:owners_)if(p.first.empty()||p.second.empty()||p.first.size()>256||p.second.size()>256||p.first.find('\0')!=std::string::npos||p.second.find('\0')!=std::string::npos)throw std::invalid_argument("invalid skill owner");
}
Result EventSkills::invoke(const EventContext& context,const EventCommand& command){
 auto owner=owners_.find(context.character);if(owner==owners_.end()||owner->second!=context.player)return {Code::denied};
 if(command.op==EventOp::grant_skill)return {learning_->grant(context.character,command.target)};
 if(command.op==EventOp::revoke_skill)return {learning_->revoke(context.character,command.target)};
 return {Code::invalid};
}
}
