// Explicit recipient and player-scoped, authored one-time claim keys. Signed: Atlas.
#include "event_rewards.hpp"
#include <stdexcept>
namespace bsl {
EventRewards::EventRewards(CharacterLearning& learning,std::map<std::string,std::string> owners):learning_(&learning),owners_(std::move(owners)){
 if(owners_.empty()||owners_.size()>64)throw std::invalid_argument("invalid event reward owners");
 for(const auto& owner:owners_)if(owner.first.empty()||owner.second.empty()||owner.first.size()>256||owner.second.size()>256||owner.first.find('\0')!=std::string::npos||owner.second.find('\0')!=std::string::npos)throw std::invalid_argument("invalid event reward owner identity");
}
Result EventRewards::grant(const EventContext& context,const EventCommand& command){
 const auto owner=owners_.find(context.character);const auto* source=learning_->source(command.target);
 if(command.op!=EventOp::grant_reward||owner==owners_.end()||owner->second!=context.player||!source||!source->model.empty())return {Code::denied};
 const auto receipt="event/"+std::to_string(context.player.size())+":"+context.player+"/"+command.receipt;
 if(receipt.size()>256)return {Code::invalid};
 const auto result=learning_->award(context.character,command.target,receipt);return {result.code,result.replay};
}
}
