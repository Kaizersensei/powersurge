// Shared wind-definition contract; WindHelper retains runtime authority. Signed: President.
#pragma once
#include "scoped_state.hpp"
#include <cmath>
namespace ps::authoring {
inline bsl::FieldDef field_definition(const Json& j,bool validate_bounds=true){
 for(auto* key:{"id","name","kind"}){if(!j.contains(key)||!j[key].is_string())throw std::runtime_error(std::string(key)+": text required");auto& text=j[key].get_ref<const std::string&>();if(text.size()>65536||text.find('\0')!=text.npos)throw std::runtime_error(std::string(key)+": invalid text");}
 if(j["kind"]!="field")throw std::runtime_error("Expected a field definition");
 if(!j.contains("strength")||!j["strength"].is_number())throw std::runtime_error("Field strength: finite number required");
 bsl::FieldDef d{j["id"].get<std::string>(),j["strength"].get<double>(),scoped_integer(j,"direction")};
 if(!std::isfinite(d.strength))throw std::runtime_error("Field strength: finite number required");
 if(validate_bounds&&(d.strength<0||d.strength>1000000||(d.direction!=1&&d.direction!=-1)))throw std::runtime_error("Wind strength must be0–1000000 px/s^2 and direction must be Left(-1) or Right(1)");
 return d;
}
}
