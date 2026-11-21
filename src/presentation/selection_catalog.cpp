// Pure compiled-profile catalog; live state is touched only by explicit solo commit. Signed: Atlas.
#include "selection_catalog.h"
#include "gameplay_records.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
namespace {
char error[256]="";
int error_code=static_cast<int>(bsl::Code::ok);
const bsl::GameplayProfile& catalog(){static const auto profile=compiled_gameplay_profile();return profile;}
void fail(const char* message,int code=static_cast<int>(bsl::Code::invalid)){error_code=code;std::snprintf(error,sizeof error,"%s",message);}
void view(const bsl::CharacterDef& d,BslSelectionCharacter& out){
 std::snprintf(out.id,sizeof out.id,"%s",d.id.c_str());std::snprintf(out.name,sizeof out.name,"%s",d.name.c_str());
 std::snprintf(out.model,sizeof out.model,"%s",d.model.c_str());out.health=d.health;
}
}
extern "C" unsigned bsl_selection_character_count(void){try{return static_cast<unsigned>(catalog().characters.size());}catch(const std::exception&){fail("Character catalog unavailable.");return 0;}}
extern "C" bool bsl_selection_character_at(unsigned index,BslSelectionCharacter* out){
 if(!out)return false;
 *out={};
 try{const auto& entries=catalog().characters;if(index>=entries.size())return false;view(entries[index],*out);return true;}catch(const std::exception&){fail("Character catalog unavailable.");return false;}
}
extern "C" bool bsl_selection_character_definition(const char* id,BslSelectionCharacter* out){
 if(!out)return false;
 *out={};if(!id)return false;
 try{for(const auto& d:catalog().characters)if(d.id==id){view(d,*out);return true;}return false;}catch(const std::exception&){fail("Character catalog unavailable.");return false;}
}
extern "C" const char* bsl_selection_last_error(void){return error;}
extern "C" int bsl_selection_last_error_code(void){return error_code;}
extern "C" bool bsl_selection_begin(const char* id,unsigned participants,unsigned device){
 error[0]=0;error_code=static_cast<int>(bsl::Code::ok);
 // A lobby draft is not a supported multiplayer gameplay session. Refuse before reset.
 if(participants!=1){fail("This build can launch one player; multiplayer gameplay is not wired yet.",static_cast<int>(bsl::Code::denied));return false;}
 if(device>1){fail("Solo gameplay currently accepts keyboard or gamepad 1 only.",static_cast<int>(bsl::Code::denied));return false;}
 BslSelectionCharacter selected{};
 if(!bsl_selection_character_definition(id,&selected)){fail("Choose a registered character before starting.");return false;}
 const int code=bsl_host_begin_selected_run(selected.id);
 if(code!=static_cast<int>(bsl::Code::ok)){fail("Cannot start this character while gameplay, travel or editor work is active.",code);return false;}
 return true;
}
