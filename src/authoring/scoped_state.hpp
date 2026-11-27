// Definition-only scalar authoring; runtime values remain owned by Events. Signed: President.
#pragma once
#include "documents.hpp"
#include <limits>
#include <stdexcept>
namespace ps::authoring {
struct ScopedDefinition {bool is_switch=false,flag=false;int initial=0,minimum=0,maximum=0;std::string scope;};
inline int scoped_integer(const Json& j,const char* key){
 if(!j.contains(key)||!j[key].is_number_integer())throw std::runtime_error(std::string(key)+": signed32-bit integer required");
 if(j[key].is_number_unsigned()){auto v=j[key].get<uint64_t>();if(v>2147483647ULL)throw std::runtime_error(std::string(key)+": integer out of range");return int(v);}
 auto v=j[key].get<int64_t>();if(v<INT32_MIN||v>INT32_MAX)throw std::runtime_error(std::string(key)+": integer out of range");return int(v);
}
inline ScopedDefinition scoped_definition(const Json& j,bool validate_bounds=true){
 for(auto* key:{"kind","name","scope"}){if(!j.contains(key)||!j[key].is_string())throw std::runtime_error(std::string(key)+": text required");auto& s=j[key].get_ref<const std::string&>();if(s.size()>65536||s.find('\0')!=s.npos)throw std::runtime_error(std::string(key)+": invalid text");}
 ScopedDefinition d;auto kind=j["kind"].get<std::string>();if(kind!="switch"&&kind!="variable")throw std::runtime_error("Expected a switch or variable definition");d.is_switch=kind=="switch";d.scope=j["scope"].get<std::string>();if(d.scope!="session"&&d.scope!="player")throw std::runtime_error("Unsupported scope");
 if(d.is_switch){if(!j.contains("initial")||!j["initial"].is_boolean())throw std::runtime_error("Switch initial value must be boolean");d.flag=j["initial"].get<bool>();}
 else{d.initial=scoped_integer(j,"initial");d.minimum=scoped_integer(j,"minimum");d.maximum=scoped_integer(j,"maximum");if(validate_bounds&&(d.minimum>d.maximum||d.initial<d.minimum||d.initial>d.maximum))throw std::runtime_error("Initial value must be within minimum/maximum bounds");}
 return d;
}
inline void scoped_edit(const Json& before,const Json& after){
 auto old=scoped_definition(before),next=scoped_definition(after);if(old.is_switch!=next.is_switch||old.scope!=next.scope)throw std::runtime_error("Changing state scope requires an explicit save migration; draft preserved");
 if(!next.is_switch&&(next.minimum>old.minimum||next.maximum<old.maximum))throw std::runtime_error("Narrowing variable bounds requires an explicit save migration; draft preserved");
}
}
