// Dialogue authoring operations keep labels, help and outcome links together. Signed: President.
#pragma once
#include "documents.hpp"
#include <algorithm>
#include <stdexcept>
namespace ps::authoring {
struct Choice {std::string label,information,outcome;};
inline void dialogue_text(const Json& j,const char* key){if(!j.contains(key)||!j[key].is_string())throw std::runtime_error(std::string(key)+": text required");auto& s=j[key].get_ref<const std::string&>();if(s.size()>65536||s.find('\0')!=s.npos)throw std::runtime_error(std::string(key)+": invalid text");}
inline std::vector<Choice> dialogue_rows(const Json& j){
 dialogue_text(j,"id");dialogue_text(j,"name");dialogue_text(j,"text");
 if(!j.contains("choices")||!j["choices"].is_array()||j["choices"].size()<2||j["choices"].size()>64)throw std::runtime_error("This runtime supports 2–64 choices");
 for(auto* key:{"outcomes","choice_help"})if(j.contains(key)&&(!j[key].is_array()||(!j[key].empty()&&j[key].size()!=j["choices"].size())))throw std::runtime_error(std::string(key)+": choice count mismatch");
 std::vector<Choice> rows;for(size_t i=0;i<j["choices"].size();++i){Choice row;auto value=[&](const char* key){if(!j.contains(key)||j[key].empty())return std::string{};auto& v=j[key][i];if(!v.is_string())throw std::runtime_error(std::string(key)+": text required");auto s=v.get<std::string>();if(s.size()>65536||s.find('\0')!=s.npos)throw std::runtime_error(std::string(key)+": invalid text");return s;};row.label=value("choices");row.information=value("choice_help");row.outcome=value("outcomes");rows.push_back(std::move(row));}return rows;
}
inline void dialogue_rows(Json& j,const std::vector<Choice>& rows){
 if(rows.size()<2||rows.size()>64)throw std::runtime_error("This runtime supports 2–64 choices");Json next=j;next["choices"]=Json::array();next["choice_help"]=Json::array();next["outcomes"]=Json::array();for(auto& row:rows){next["choices"].push_back(row.label);next["choice_help"].push_back(row.information);next["outcomes"].push_back(row.outcome);}dialogue_rows(next);j.swap(next);
}
inline void dialogue_move(Json& j,size_t from,size_t to){auto rows=dialogue_rows(j);if(from>=rows.size()||to>=rows.size())throw std::runtime_error("Choice index out of range");auto row=rows[from];rows.erase(rows.begin()+from);rows.insert(rows.begin()+to,std::move(row));dialogue_rows(j,rows);}
inline void dialogue_insert(Json& j,size_t at,const Choice& row){auto rows=dialogue_rows(j);if(at>rows.size()||rows.size()==64)throw std::runtime_error("Cannot add another choice");rows.insert(rows.begin()+at,row);dialogue_rows(j,rows);}
inline void dialogue_erase(Json& j,size_t at){auto rows=dialogue_rows(j);if(at>=rows.size()||rows.size()==2)throw std::runtime_error("At least two choices are required");rows.erase(rows.begin()+at);dialogue_rows(j,rows);}
}
