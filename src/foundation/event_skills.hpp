#pragma once
#include "events.hpp"
#include "learning.hpp"
namespace bsl {
class EventSkills final:public EventSkillPort {
public:
 EventSkills(CharacterLearning&,std::map<std::string,std::string> owners);
 void rebind(CharacterLearning& l) noexcept {learning_=&l;}
 bool knows_ability(const std::string& id) const noexcept override {return learning_->knows_ability(id);}
 Result invoke(const EventContext&,const EventCommand&) override;
private: CharacterLearning* learning_;const std::map<std::string,std::string> owners_;
};
}
