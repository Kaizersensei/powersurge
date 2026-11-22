/* Shared script-object adapter for the private physics lab. Signed: Mech. */
extern bool bsl_lab_object(const char*,const char*,float*,float*,float*);
extern void bsl_lab_remove(const char*);
typedef struct {char id[129];} BslLabProp;
static surgescript_var_t* bsl_lab_prop_update(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 (void)n;if(level_editmode()||!surgescript_var_is_string(p[0]))return NULL;
 surgescript_objectmanager_t* manager=surgescript_object_manager(obj);
 surgescript_object_t* entity=surgescript_objectmanager_get(manager,surgescript_object_parent(obj));if(!entity)return NULL;
 const char* identity=level_get_entity_id(entity);if(!identity||!*identity||strlen(identity)>128)return NULL;
 BslLabProp* slot=surgescript_object_userdata(obj);
 if(!slot){slot=calloc(1,sizeof(*slot));if(!slot)return NULL;strcpy(slot->id,identity);surgescript_object_set_userdata(obj,slot);}
 if(strcmp(slot->id,identity))return NULL;
 v2d_t at=scripting_util_world_position(entity);float x=at.x,y=at.y,angle=0;
 if(bsl_lab_object(slot->id,surgescript_var_fast_get_string(p[0]),&x,&y,&angle)){
  scripting_util_set_world_position(entity,v2d_new(x,y));
  surgescript_object_t* actor=surgescript_objectmanager_get(manager,surgescript_object_child(entity,"Actor"));
  if(actor){actor_t* visual=scripting_actor_ptr(actor);if(visual)visual->angle=-angle;}
 }
 return NULL;
}
static surgescript_var_t* bsl_lab_prop_destroy(surgescript_object_t* obj,const surgescript_var_t** p,int n){
 (void)p;(void)n;BslLabProp* slot=surgescript_object_userdata(obj);if(slot){bsl_lab_remove(slot->id);free(slot);surgescript_object_set_userdata(obj,NULL);}return NULL;
}
