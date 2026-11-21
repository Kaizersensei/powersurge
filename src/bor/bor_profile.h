/* Runtime-readable, validated Surge physics profiles. Signed: Atlas. */
#include <stddef.h>
#include <errno.h>
#include "../core/nanoparser.h"
#include "../core/asset.h"
typedef struct {
 bool enabled;
#define BOR_PHYSICS(name,value,low,high,integer) double name;
#include "bor_physics.def"
#undef BOR_PHYSICS
} BorPhysics;
typedef struct {const char* name;size_t offset;double low,high;bool integer;} PhysicsField;
static const PhysicsField physics_fields[]={
#define BOR_PHYSICS(name,value,low,high,integer) {#name,offsetof(BorPhysics,name),low,high,integer},
#include "bor_physics.def"
#undef BOR_PHYSICS
};
typedef struct {BorPhysics value;uint64_t seen;bool valid;} PhysicsParse;
static int physics_field(const parsetree_statement_t* statement,void* userdata){
 PhysicsParse* result=userdata;const char* name=nanoparser_get_identifier(statement);
 const parsetree_parameter_t* list=nanoparser_get_parameter_list(statement);
 for(unsigned i=0;i<sizeof(physics_fields)/sizeof(*physics_fields);i++)if(!strcmp(name,physics_fields[i].name)){
  const PhysicsField* field=physics_fields+i;const char* text=nanoparser_get_string(nanoparser_get_nth_parameter(list,1));char* end=NULL;errno=0;
  double value=text?strtod(text,&end):NAN;
  if(nanoparser_get_number_of_parameters(list)!=1||!text||end==text||*end||errno||!isfinite(value)||value<field->low||value>field->high||(field->integer&&floor(value)!=value)||(result->seen&(UINT64_C(1)<<i)))break;
  *(double*)((char*)&result->value+field->offset)=value;result->seen|=UINT64_C(1)<<i;return 0;
 }
 result->valid=false;nanoparser_warn(statement,"Invalid or duplicate BOR physics field: %s",name);return 1;
}
static int load_physics(BorPhysics* profile,const char* model){
 char path[160];snprintf(path,sizeof(path),"physics/%s.physics",model);memset(profile,0,sizeof(*profile));
 if(!asset_exists(path))return 0;
 PhysicsParse result={0};result.valid=true;
 parsetree_program_t* tree=nanoparser_construct_tree(asset_path(path));nanoparser_traverse_program_ex(tree,&result,physics_field);nanoparser_deconstruct_tree(tree);
 if(!result.valid||result.seen!=((UINT64_C(1)<<(sizeof(physics_fields)/sizeof(*physics_fields)))-1)){logfile_message("BOR invalid/incomplete physics profile: %s",path);return -1;}
 result.value.enabled=true;*profile=result.value;logfile_message("BOR physics %s speed=%.3f jump=%.3f dash=%.3f air_brake=%.3f (stock airborne hard cap approximately1440)",path,profile->speed,profile->jump,profile->dash_speed,profile->air_brake);return 1;
}
