// Common-event adapter over the existing atomic reward/learning service. Signed: Atlas.
#pragma once
#include "events.hpp"
#include "learning.hpp"
namespace bsl {
class EventRewards final:public EventRewardPort {
public:
 EventRewards(CharacterLearning&,std::map<std::string,std::string> character_owners);
 // Stopped-simulation commit: follows replacement of the validated learning/stat bundle.
 void rebind(CharacterLearning& learning) noexcept {learning_=&learning;}
 Result grant(const EventContext&,const EventCommand&) override;
private:
 CharacterLearning* learning_;
 const std::map<std::string,std::string> owners_;
};
}
