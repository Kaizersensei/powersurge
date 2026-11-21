// Stable single-player/character ownership, independent of actor lifetime. Signed: Atlas.
#pragma once
#include "foundation.hpp"
namespace bsl {
struct CharacterDef { std::string id,name,model; int health; };
struct CharacterState { std::string id,definition; int detached_health; std::uint64_t actor=0; };
struct StartingItem { std::string definition,instance,slot; int quantity; };
struct GameplayProfile { std::string mode,save,player,initial; std::vector<CharacterDef> characters; std::vector<StartingItem> items; };
class PlayerRoster {
public:
 explicit PlayerRoster(const GameplayProfile&);
 const GameplayProfile& profile() const noexcept {return profile_;}
 const CharacterDef* definition(const std::string&) const noexcept;
 const CharacterState* character(const std::string&) const noexcept;
 const CharacterState* bound(std::uint64_t) const noexcept;
 const CharacterState& selected() const;
 bool controls(std::uint64_t) const noexcept;
 Code attach(const std::string& definition,std::uint64_t actor,const std::string& model,int maximum_health);
 void detach(std::uint64_t actor,int health) noexcept;
 // Startup-only choice closes on the first attachment or restored progress. Signed: Mech.
 Code choose_initial(const std::string& definition);
 Code select(const std::string& definition);
 Code restore_progress(const std::string& selected,const std::map<std::string,int>& health);
private:
 GameplayProfile profile_;
 std::map<std::string,CharacterState> characters_;
 std::string selected_;
 bool started_=false;
};
}
