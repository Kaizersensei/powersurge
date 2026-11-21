// Original gameplay lifecycle service. No physics or duplicated combat loop. Signed: Atlas.
#include "gameplay.hpp"
#include <algorithm>
#include <stdexcept>
namespace bsl {
namespace {bool identity(const std::string& s){return !s.empty()&&s.size()<=128&&s.find('\0')==std::string::npos;}}
PlayerRoster::PlayerRoster(const GameplayProfile& p):profile_(p),selected_(p.initial){
 if(!identity(p.mode)||!identity(p.save)||!identity(p.player)||!identity(p.initial)||p.characters.empty()||p.characters.size()>64)throw std::invalid_argument("invalid gameplay profile");
 for(const auto& d:p.characters){
  if(!identity(d.id)||!identity(d.name)||!identity(d.model)||p.player.size()+1+d.id.size()>256||d.health<1||!characters_.emplace(d.id,CharacterState{p.player+"/"+d.id,d.id,d.health,0}).second)throw std::invalid_argument("invalid/duplicate character");
 }
 if(!character(selected_))throw std::invalid_argument("unknown initial character");
}
const CharacterDef* PlayerRoster::definition(const std::string& id) const noexcept {for(const auto& d:profile_.characters)if(d.id==id)return &d;return nullptr;}
const CharacterState* PlayerRoster::character(const std::string& id) const noexcept {auto i=characters_.find(id);return i==characters_.end()?nullptr:&i->second;}
const CharacterState* PlayerRoster::bound(std::uint64_t actor) const noexcept {if(actor)for(const auto& e:characters_)if(e.second.actor==actor)return &e.second;return nullptr;}
const CharacterState& PlayerRoster::selected() const {return characters_.at(selected_);}
bool PlayerRoster::controls(std::uint64_t actor) const noexcept {auto i=characters_.find(selected_);return actor&&i!=characters_.end()&&i->second.actor==actor;}
Code PlayerRoster::attach(const std::string& id,std::uint64_t actor,const std::string& model,int maximum){
 const auto* d=definition(id);if(!actor||!d||d->model!=model||d->health>maximum)return Code::invalid;
 auto& c=characters_.at(id);if(c.actor==actor)return Code::ok;
 if(c.actor||bound(actor))return Code::conflict;
 c.detached_health=std::max(0,std::min(c.detached_health,maximum));c.actor=actor;started_=true;return Code::ok;
}
void PlayerRoster::detach(std::uint64_t actor,int health) noexcept {for(auto& e:characters_)if(actor&&e.second.actor==actor){e.second.detached_health=std::max(0,health);e.second.actor=0;return;}}
Code PlayerRoster::choose_initial(const std::string& id){
 if(!character(id))return Code::invalid;
 if(started_)return Code::denied;
 auto prepared=id;selected_.swap(prepared);return Code::ok;
}
Code PlayerRoster::select(const std::string& id){auto i=characters_.find(id);if(i==characters_.end())return Code::invalid;if(!i->second.actor)return Code::denied;auto prepared=id;selected_.swap(prepared);return Code::ok;}
Code PlayerRoster::restore_progress(const std::string& selected,const std::map<std::string,int>& health){
 if(!character(selected)||health.size()!=characters_.size())return Code::invalid;
 for(const auto& c:characters_){auto h=health.find(c.first);if(h==health.end()||h->second<0||h->second>1000000)return Code::invalid;}
 if(health.at(selected)<=0)return Code::denied;
 auto prepared=characters_;auto next=selected;for(auto& c:prepared)c.second.detached_health=health.at(c.first);
 characters_.swap(prepared);selected_.swap(next);started_=true;return Code::ok;
}

}
