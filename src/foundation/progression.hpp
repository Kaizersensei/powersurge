// Character-owned generic numeric stats, optional growth and action receipts. Signed: Atlas.
#pragma once
#include "foundation.hpp"
namespace bsl {
struct StatGrowth {std::uint64_t first=0,increment=0,scale=1000;std::int64_t gain=0;};
struct StatDef {std::string id,name;std::int64_t initial=0,minimum=0,maximum=0;std::uint64_t scale=1;StatGrowth growth;};
struct StatTerm {std::string stat;std::int64_t numerator=1;std::uint64_t denominator=1;};
struct StatFormula {std::string id;std::int64_t constant=0,minimum=0,maximum=2147483647;std::vector<StatTerm> terms;};
struct StatGrant {std::string stat;bool growth=false;std::int64_t amount=0;};
struct StatAward {std::string stat;bool growth=false;std::int64_t action=0,hit=0,defeat=0;};
struct StatActionRule {std::string tag;std::vector<StatAward> awards;std::map<std::string,std::string> outputs;};
struct CharacterStatProfile {std::string definition;std::vector<std::string> stats;std::vector<StatActionRule> actions;};
struct StatCatalog {std::vector<StatDef> stats;std::vector<StatFormula> formulas;std::vector<CharacterStatProfile> profiles;};
struct StatValue {std::int64_t units=0;std::uint64_t xp=0;};
using StatModifiers=std::map<std::string,std::int64_t>; // Transient contributions in authored integer units; never earned progress.
using StatValues=std::map<std::string,StatValue>;
struct StatView {std::int64_t units=0;std::uint64_t scale=1,xp=0,into=0,next=0,stage=0,xp_scale=1000;};
struct StatActionSnapshot {
 bool accepted=false;std::map<std::string,std::int64_t> outputs;
 std::int64_t output(const std::string& id) const {auto i=outputs.find(id);return i==outputs.end()?0:i->second;}
};
class CharacterStats {
public:
 explicit CharacterStats(StatCatalog);
 void character(const std::string& instance,const std::string& definition);
 const CharacterStatProfile& profile(const std::string&) const;
 const StatDef& definition(const std::string& id) const {return definitions_.at(id);}
 StatView view(const std::string& character,const std::string& stat) const;
 std::int64_t effective_units(const std::string& character,const std::string& stat,const StatModifiers& = {}) const;
 StatValues capture(const std::string&) const;
 // Explicit reconstruction while simulation is stopped; invalidates that character's receipts.
 Code restore(const std::string&,const StatValues&);
 StatActionSnapshot begin(const std::string& character,std::uint64_t actor,std::uint64_t action,const std::string& tag,const StatModifiers& = {});
 bool hit(std::uint64_t actor,std::uint64_t action,std::uint64_t target,std::uint64_t episode,int actual,bool defeated);
 Code grant(const std::string& character,const std::vector<StatGrant>&);
 std::string action_character(std::uint64_t actor,std::uint64_t action) const;
 void swap(CharacterStats&) noexcept;
 void retire(std::uint64_t actor) noexcept;
 void clear_actions() noexcept {actions_.clear();defeated_.clear();}
private:
 struct Character {std::string definition;StatValues values;};
 struct Action {std::uint64_t id;std::string character,tag;std::vector<StatAward> awards;StatActionSnapshot snapshot;std::set<std::pair<std::uint64_t,std::uint64_t>> hits;};
 std::map<std::string,StatDef> definitions_;
 std::map<std::string,StatFormula> formulas_;
 std::map<std::string,std::uint64_t> formula_denominators_;
 std::map<std::string,CharacterStatProfile> profiles_;
 std::map<std::string,Character> characters_;
 std::map<std::uint64_t,Action> actions_;
 std::set<std::uint64_t> defeated_;
 void award(StatValues&,const StatAward&,int event,bool defeat=false) const;
 std::int64_t evaluate(const std::string& character,const StatFormula&,const StatModifiers&) const;
 void validate_modifiers(const std::string& character,const StatModifiers&) const;
 std::int64_t modified_units(const std::string& character,const std::string& stat,const StatModifiers&) const;
 static constexpr std::uint64_t maximum_xp=1000000000000000ULL;
 static std::uint64_t cost(const StatGrowth&,std::uint64_t steps) noexcept;
};
}
