/* Trusted menu adapters; gameplay transactions remain native-service owned. Signed: President, Atlas. */
#include <surgescript.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include "../foundation/foundation_host.h"
#include "../scripting/scripting.h"
#include "../entities/actor.h"
#include "../core/sprite.h"
#include "../core/image.h"
#include "selection_catalog.h"
static surgescript_var_t* idle(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)a;(void)n;return NULL;}
static surgescript_var_t* begin_new(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)a;(void)n;return surgescript_var_set_bool(surgescript_var_create(),bsl_host_begin_lab_run());}
static surgescript_var_t* character_count(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)a;(void)n;return surgescript_var_set_number(surgescript_var_create(),bsl_selection_character_count());}
static surgescript_var_t* character_at(surgescript_object_t* o,const surgescript_var_t** a,int n){
 (void)o;(void)n;surgescript_var_t* result=surgescript_var_create();BslSelectionCharacter d={0};
 if(surgescript_var_is_number(a[0])){double index=surgescript_var_get_number(a[0]);if(isfinite(index)&&index>=0&&index<=UINT_MAX&&floor(index)==index&&bsl_selection_character_at((unsigned)index,&d))surgescript_var_set_string(result,d.id);}
 return result;
}
static bool definition(const surgescript_var_t* id,BslSelectionCharacter* out){return surgescript_var_is_string(id)&&bsl_selection_character_definition(surgescript_var_fast_get_string(id),out);}
static surgescript_var_t* character_name(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)n;BslSelectionCharacter d={0};surgescript_var_t* result=surgescript_var_create();if(definition(a[0],&d))surgescript_var_set_string(result,d.name);return result;}
static surgescript_var_t* character_model(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)n;BslSelectionCharacter d={0};surgescript_var_t* result=surgescript_var_create();if(definition(a[0],&d))surgescript_var_set_string(result,d.model);return result;}
static surgescript_var_t* character_health(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)n;BslSelectionCharacter d={0};definition(a[0],&d);return surgescript_var_set_number(surgescript_var_create(),d.health);}
static surgescript_var_t* last_error(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)a;(void)n;return surgescript_var_set_string(surgescript_var_create(),bsl_selection_last_error());}
static surgescript_var_t* last_error_code(surgescript_object_t* o,const surgescript_var_t** a,int n){(void)o;(void)a;(void)n;return surgescript_var_set_number(surgescript_var_create(),bsl_selection_last_error_code());}
static surgescript_var_t* begin_selected(surgescript_object_t* o,const surgescript_var_t** a,int n){
 (void)o;(void)n;
 const char* id=surgescript_var_is_string(a[0])?surgescript_var_fast_get_string(a[0]):NULL;
 unsigned participants=0,device=UINT_MAX;
 if(surgescript_var_is_number(a[1])){double v=surgescript_var_get_number(a[1]);if(isfinite(v)&&v>=0&&v<=UINT_MAX&&floor(v)==v)participants=(unsigned)v;}
 if(surgescript_var_is_number(a[2])){double v=surgescript_var_get_number(a[2]);if(isfinite(v)&&v>=0&&v<=UINT_MAX&&floor(v)==v)device=(unsigned)v;}
 return surgescript_var_set_bool(surgescript_var_create(),bsl_selection_begin(id,participants,device));
}
static surgescript_var_t* preview(surgescript_object_t* o,const surgescript_var_t** a,int n){
 (void)n;bool ok=false;BslSelectionCharacter d={0};BslSelectionFrame frame={0};
 if(surgescript_var_is_objecthandle(a[0])&&definition(a[1],&d)&&surgescript_var_is_number(a[2])&&surgescript_var_is_string(a[3])){
  surgescript_objectmanager_t* manager=surgescript_object_manager(o);surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(a[0]);
  if(handle&&surgescript_objectmanager_exists(manager,handle)){
   surgescript_object_t* object=surgescript_objectmanager_get(manager,handle);
   if(!strcmp(surgescript_object_name(object),"Actor")&&bsl_bor_selection_frame(d.model,surgescript_var_fast_get_string(a[3]),surgescript_var_get_number(a[2]),&frame)&&sprite_animation_exists(frame.sprite,frame.animation)){
    actor_t* actor=scripting_actor_ptr(object);
    if(actor){actor_change_animation(actor,sprite_get_animation(frame.sprite,frame.animation));actor->hot_spot=v2d_new((actor->mirror&IF_HFLIP)?image_width(actor_image(actor))-frame.origin_x:frame.origin_x,frame.origin_y);ok=true;}
   }
  }
 }
 return surgescript_var_set_bool(surgescript_var_create(),ok);
}
void scripting_register_presentation(surgescript_vm_t* vm){
 surgescript_vm_bind(vm,"BSL Presentation Actions","state:main",idle,0);
 surgescript_vm_bind(vm,"BSL Presentation Actions","beginNewGame",begin_new,0);
 surgescript_vm_bind(vm,"BSL Presentation Actions","characterCount",character_count,0);
 surgescript_vm_bind(vm,"BSL Presentation Actions","characterAt",character_at,1);
 surgescript_vm_bind(vm,"BSL Presentation Actions","characterName",character_name,1);
 surgescript_vm_bind(vm,"BSL Presentation Actions","characterModel",character_model,1);
 surgescript_vm_bind(vm,"BSL Presentation Actions","characterHealth",character_health,1);
 surgescript_vm_bind(vm,"BSL Presentation Actions","preview",preview,4);
 surgescript_vm_bind(vm,"BSL Presentation Actions","beginSelectedGame",begin_selected,3);
 surgescript_vm_bind(vm,"BSL Presentation Actions","get_lastError",last_error,0);
 surgescript_vm_bind(vm,"BSL Presentation Actions","get_lastErrorCode",last_error_code,0);
}
