// Original generic stat service. No character names, skill lists or physics authority. Signed: Atlas.
#include "progression.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
namespace bsl {
namespace {
bool id(const std::string& s,std::size_t limit=128){return !s.empty()&&s.size()<=limit&&s.find('\0')==std::string::npos;}
void require(bool valid){if(!valid)throw std::invalid_argument("invalid stat catalog/reference");}
constexpr std::int64_t bound=1000000000;
}
CharacterStats::CharacterStats(StatCatalog catalog){
 require(!catalog.stats.empty()&&catalog.stats.size()<=64&&catalog.formulas.size()<=64&&!catalog.profiles.empty()&&catalog.profiles.size()<=64);
 for(const auto& d:catalog.stats){
  const auto& g=d.growth;
  require(id(d.id)&&id(d.name)&&d.minimum>=-bound&&d.maximum<=bound&&d.minimum<=d.initial&&d.initial<=d.maximum&&d.scale>=1&&d.scale<=1000000);
  require(g.first<=1000000000&&g.increment<=1000000000&&g.scale>=1&&g.scale<=1000000&&g.gain>=0&&g.gain<=bound);
  require(g.first>0||(g.increment==0&&g.gain==0));require(definitions_.emplace(d.id,d).second);
 }
 for(const auto& f:catalog.formulas){
  require(id(f.id)&&f.constant>=-bound&&f.constant<=bound&&f.minimum>=-bound&&f.maximum<=2147483647&&f.minimum<=f.maximum&&f.terms.size()<=64);
  std::uint64_t common=1;
  for(const auto& t:f.terms){
   require(definitions_.count(t.stat)&&t.numerator>=-1000000&&t.numerator<=1000000&&t.denominator>=1&&t.denominator<=1000000);
   auto denominator=definitions_.at(t.stat).scale*t.denominator;auto factor=denominator/std::gcd(common,denominator);
   require(common<=1000000000000ULL/factor);common*=factor;
  }
  formula_denominators_.emplace(f.id,common);
  require(formulas_.emplace(f.id,f).second);
 }
 for(const auto& p:catalog.profiles){
  require(id(p.definition)&&p.stats.size()<=64&&p.actions.size()<=64);std::set<std::string> stats,tags;
  for(const auto& s:p.stats)require(definitions_.count(s)&&stats.insert(s).second);
  for(const auto& a:p.actions){
   require(id(a.tag)&&tags.insert(a.tag).second&&a.awards.size()<=64&&a.outputs.size()<=64);std::set<std::string> awarded;
   for(const auto& w:a.awards){
    require(stats.count(w.stat)&&awarded.insert(w.stat).second);const auto& d=definitions_.at(w.stat);
    require(w.growth==(d.growth.first>0));
    for(auto amount:{w.action,w.hit,w.defeat})require(amount>=(w.growth?0:-bound)&&amount<=bound);
   }
   for(const auto& output:a.outputs){require(id(output.first)&&formulas_.count(output.second));for(const auto& term:formulas_.at(output.second).terms)require(stats.count(term.stat));}
  }
  require(profiles_.emplace(p.definition,p).second);
 }
}
void CharacterStats::character(const std::string& instance,const std::string& definition){
 require(id(instance,256)&&profiles_.count(definition));auto found=characters_.find(instance);
 if(found!=characters_.end()){require(found->second.definition==definition);return;}
 require(characters_.size()<64);Character prepared{definition,{}};
 for(const auto& stat:profiles_.at(definition).stats)prepared.values.emplace(stat,StatValue{definitions_.at(stat).initial,0});
 characters_.emplace(instance,std::move(prepared));
}
const CharacterStatProfile& CharacterStats::profile(const std::string& character) const {return profiles_.at(characters_.at(character).definition);}
std::uint64_t CharacterStats::cost(const StatGrowth& g,std::uint64_t n) noexcept {
 // Clamp before multiplication; even tiny fixed costs must remain safe at the XP cap.
 if(n&&g.first>maximum_xp/n)return maximum_xp+1;
 auto total=n*g.first;if(!g.increment||n<2)return total;
 auto a=n,b=n-1;if(a%2==0)a/=2;else b/=2;
 if(b&&a>maximum_xp/b)return maximum_xp+1;
 auto triangle=a*b;if(triangle&&g.increment>(maximum_xp-total)/triangle)return maximum_xp+1;
 return total+triangle*g.increment;
}
StatView CharacterStats::view(const std::string& character,const std::string& stat) const {
 const auto& d=definitions_.at(stat);const auto& v=characters_.at(character).values.at(stat);const auto& g=d.growth;
 StatView result{v.units,d.scale,v.xp,0,0,0,g.scale};if(!g.first)return result;
 std::uint64_t lo=0,hi=maximum_xp/g.first;
 while(lo<hi){auto mid=lo+(hi-lo+1)/2;if(cost(g,mid)<=v.xp)lo=mid;else hi=mid-1;}
 const auto room=static_cast<std::uint64_t>(d.maximum-d.initial);
 result.units=g.gain&&lo>room/static_cast<std::uint64_t>(g.gain)?d.maximum:d.initial+static_cast<std::int64_t>(lo)*g.gain;
 result.stage=lo+1;result.into=v.xp-cost(g,lo);
 result.next=g.increment&&lo>(maximum_xp-g.first)/g.increment?maximum_xp+1:g.first+lo*g.increment;
 return result;
}
void CharacterStats::validate_modifiers(const std::string& character,const StatModifiers& modifiers) const {
 require(modifiers.size()<=64);const auto& values=characters_.at(character).values;
 for(const auto& m:modifiers)require(values.count(m.first)&&m.second>=-64000000000LL&&m.second<=64000000000LL);
}
std::int64_t CharacterStats::modified_units(const std::string& character,const std::string& stat,const StatModifiers& modifiers) const {
 const auto& d=definitions_.at(stat);auto m=modifiers.find(stat);return std::clamp(view(character,stat).units+(m==modifiers.end()?0:m->second),d.minimum,d.maximum);
}
std::int64_t CharacterStats::effective_units(const std::string& character,const std::string& stat,const StatModifiers& modifiers) const {validate_modifiers(character,modifiers);return modified_units(character,stat,modifiers);}
StatValues CharacterStats::capture(const std::string& character) const {return characters_.at(character).values;}
Code CharacterStats::restore(const std::string& character,const StatValues& values){
 auto found=characters_.find(character);if(found==characters_.end())return Code::invalid;
 if(values.size()!=found->second.values.size())return Code::invalid;
 for(const auto& e:values){
  if(!found->second.values.count(e.first))return Code::invalid;
  const auto& d=definitions_.at(e.first);
  if(e.second.xp>maximum_xp||e.second.units<d.minimum||e.second.units>d.maximum||(d.growth.first?e.second.units!=d.initial:e.second.xp!=0))return Code::denied;
 }
 auto prepared=values;found->second.values.swap(prepared);
 for(auto i=actions_.begin();i!=actions_.end();)if(i->second.character==character)i=actions_.erase(i);else ++i;
 return Code::ok;
}
std::int64_t CharacterStats::evaluate(const std::string& character,const StatFormula& f,const StatModifiers& modifiers) const {
 const auto common=static_cast<std::int64_t>(formula_denominators_.at(f.id));auto whole=f.constant;std::int64_t fraction=0;
 for(const auto& t:f.terms){
  auto v=view(character,t.stat);auto product=modified_units(character,t.stat,modifiers)*t.numerator;auto denominator=static_cast<std::int64_t>(v.scale*t.denominator);
  whole+=product/denominator;fraction+=(product%denominator)*(common/denominator);
 }
 // Exact rational accumulation, then one truncation toward zero for the integer output.
 whole+=fraction/common;auto remainder=fraction%common;
 if(whole>0&&remainder<0)--whole;else if(whole<0&&remainder>0)++whole;
 return std::clamp(whole,f.minimum,f.maximum);
}
void CharacterStats::award(StatValues& values,const StatAward& w,int event,bool defeat) const {
 auto amount=event==0?w.action:w.hit+(defeat?w.defeat:0);auto& v=values.at(w.stat);const auto& d=definitions_.at(w.stat);
 if(w.growth)v.xp+=std::min<std::uint64_t>(static_cast<std::uint64_t>(amount),maximum_xp-v.xp);
 else v.units=std::clamp(v.units+amount,d.minimum,d.maximum);
}
StatActionSnapshot CharacterStats::begin(const std::string& character,std::uint64_t actor,std::uint64_t action,const std::string& tag,const StatModifiers& modifiers){
 if(!actor||!action||!characters_.count(character)||tag.size()>128||tag.find('\0')!=std::string::npos)return {};
 auto previous=actions_.find(actor);
 if(previous!=actions_.end()){
  if(previous->second.id==action)return previous->second.character==character&&previous->second.tag==tag?previous->second.snapshot:StatActionSnapshot{};
  if(previous->second.id>action)return {};
 }else if(actions_.size()>=64)return {};
 validate_modifiers(character,modifiers);
 Action next{action,character,tag,{},StatActionSnapshot{true,{}},{}};const auto& p=profile(character);
 for(const auto& a:p.actions)if(a.tag==tag){next.awards=a.awards;for(const auto& o:a.outputs)next.snapshot.outputs.emplace(o.first,evaluate(character,formulas_.at(o.second),modifiers));break;}
 auto values=capture(character);for(const auto& w:next.awards)award(values,w,0);
 // Prepare both allocation-bearing results before committing the action or earned values.
 auto result=next.snapshot;
 if(previous==actions_.end())actions_.emplace(actor,std::move(next));else previous->second=std::move(next);
 characters_.at(character).values.swap(values);return result;
}
bool CharacterStats::hit(std::uint64_t actor,std::uint64_t action,std::uint64_t target,std::uint64_t episode,int actual,bool defeated){
 auto i=actions_.find(actor);if(i==actions_.end()||i->second.id!=action||!target||!episode||actual<=0)return false;
 auto& a=i->second;auto key=std::make_pair(target,episode);if(a.hits.count(key)||a.hits.size()>=4096)return false;
 bool first=defeated&&!defeated_.count(target);if(first&&defeated_.size()>=64)return false;
 auto values=capture(a.character);for(const auto& w:a.awards)award(values,w,1,first);
 auto hits=a.hits;hits.insert(key);auto deaths=defeated_;if(first)deaths.insert(target);
 characters_.at(a.character).values.swap(values);a.hits.swap(hits);defeated_.swap(deaths);return true;
}
Code CharacterStats::grant(const std::string& character,const std::vector<StatGrant>& grants){
 auto found=characters_.find(character);if(found==characters_.end()||grants.empty()||grants.size()>64)return Code::invalid;
 auto values=found->second.values;std::set<std::string> seen;
 for(const auto& g:grants){
  if(!values.count(g.stat)||!seen.insert(g.stat).second)return Code::invalid;
  const auto& d=definition(g.stat);
  if(g.growth!=(d.growth.first>0)||g.amount<(g.growth?0:-bound)||g.amount>bound)return Code::invalid;
  award(values,{g.stat,g.growth,g.amount,0,0},0);
 }
 found->second.values.swap(values);return Code::ok;
}
std::string CharacterStats::action_character(std::uint64_t actor,std::uint64_t action) const {
 auto i=actions_.find(actor);return i!=actions_.end()&&i->second.id==action?i->second.character:std::string{};
}
void CharacterStats::swap(CharacterStats& other) noexcept {
 definitions_.swap(other.definitions_);formulas_.swap(other.formulas_);formula_denominators_.swap(other.formula_denominators_);
 profiles_.swap(other.profiles_);characters_.swap(other.characters_);actions_.swap(other.actions_);defeated_.swap(other.defeated_);
}
void CharacterStats::retire(std::uint64_t actor) noexcept {actions_.erase(actor);defeated_.erase(actor);}
}
