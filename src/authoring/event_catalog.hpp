// Read-only references to the existing registered service definitions. Signed: President.
#pragma once
#include "event_sequence.hpp"
#include "../foundation/gameplay_records.hpp"
#include "../foundation/learning_records.hpp"
#include "../foundation/quest_records.hpp"
#include "../foundation/commerce_records.hpp"
#include "../foundation/actor_template_records.hpp"
namespace ps::authoring {
using EventReference=std::pair<std::string,std::string>;
inline std::vector<EventReference> event_references(const std::string& kind,const std::string& shop=""){
 std::vector<EventReference> refs;if(kind=="ability"){for(auto& a:compiled_learning_catalog().abilities)refs.emplace_back(a.id,a.name);}else if(kind=="source"){for(auto& s:compiled_learning_catalog().sources)if(s.model.empty())refs.emplace_back(s.id,s.reward);}else if(kind=="character"){for(auto& c:compiled_gameplay_profile().characters)refs.emplace_back(c.id,c.name);}else if(kind=="actor"){for(auto& a:compiled_actor_templates())refs.emplace_back(a.id,a.name);}else if(kind=="quest"){for(auto& q:compiled_quests())refs.emplace_back(q.id,q.name);}else if(kind=="shop"){for(auto& s:compiled_commerce().shops)refs.emplace_back(s.id,s.name);}else if(kind=="offer"){for(auto& s:compiled_commerce().shops)if(s.id==shop)for(auto& o:s.offers)refs.emplace_back(o.id,o.item);}return refs;
}
inline void validate_event_reference(const std::string& kind,const std::string& id,const std::string& shop=""){auto refs=event_references(kind,shop);if(std::none_of(refs.begin(),refs.end(),[&](auto& r){return r.first==id;}))throw std::runtime_error("Missing registered "+kind+": "+id);}
inline void event_external_references(const Json& command){auto* spec=command_spec(command.at("op").get<std::string>());auto kind=std::string(spec->reference);if(kind=="ability"||kind=="source"||kind=="character"||kind=="actor"||kind=="quest"||kind=="shop"){auto target=command.value("target",std::string{});validate_event_reference(kind,target);if(kind=="shop")validate_event_reference("offer",command.value("offer",std::string{}),target);}}
}
