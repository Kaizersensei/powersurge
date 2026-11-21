// One-time player-owned quests over registered progress and existing character rewards. Signed: Atlas.
#pragma once
#include "events.hpp"
#include "learning.hpp"
namespace bsl {
enum class ObjectiveKind {item_count,flag,variable};
enum class QuestStatus {locked,available,active,ready,completed,invalid};
struct QuestObjective {std::string label,target;ObjectiveKind kind;int minimum=1;};
struct QuestDef {std::string id,name,description,accepted,completed,source;std::vector<std::string> prerequisites;std::vector<QuestObjective> objectives;};
struct ObjectiveView {std::string label;int current,required;bool met;};
struct QuestView {QuestStatus status;std::vector<ObjectiveView> objectives;};
class Quests final:public EventQuestPort {
public:
 Quests(Inventory&,Events&,CharacterLearning&,std::vector<QuestDef>,std::map<std::string,std::string> character_owners);
 void rebind(CharacterLearning& learning) noexcept {learning_=&learning;}
 const std::vector<QuestDef>& definitions() const noexcept {return definitions_;}
 QuestView view(const std::string& quest,const std::string& player) const;
 // Validate selected flags and durable reward claims before committing any restored bundle.
 void validate_progress(const EventState&,const LearningState&) const;
 Result invoke(const EventContext&,const EventCommand&) override;
private:
 Inventory& inventory_;Events& events_;CharacterLearning* learning_;
 const std::vector<QuestDef> definitions_;
 const std::map<std::string,std::string> owners_;
 const QuestDef* definition(const std::string&) const noexcept;
};
}
