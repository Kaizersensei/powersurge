/* Actual trusted SurgeScript factory, replacement and shared-service ownership. Signed: Mech. */
static surgescript_var_t* factory_call(surgescript_object_t* registry,const char* method,const char* definition,double x,double y){
 surgescript_var_t* id=surgescript_var_set_string(surgescript_var_create(),definition);
 surgescript_var_t* px=surgescript_var_set_number(surgescript_var_create(),x);surgescript_var_t* py=surgescript_var_set_number(surgescript_var_create(),y);
 const surgescript_var_t* args[]={id,px,py};surgescript_var_t* result=surgescript_var_create();
 surgescript_object_call_function(registry,method,args,!strcmp(method,"create")?3:1,result);
 surgescript_var_destroy(id);surgescript_var_destroy(px);surgescript_var_destroy(py);return result;
}
static bool factory_select(surgescript_object_t* registry,const char* definition){
 surgescript_var_t* v=factory_call(registry,"select",definition,0,0);bool ok=surgescript_var_get_bool(v);surgescript_var_destroy(v);return ok;
}
static surgescript_objecthandle_t factory_body(surgescript_object_t* registry,const char* method,const char* definition,double x,double y){
 surgescript_var_t* v=factory_call(registry,method,definition,x,y);surgescript_objecthandle_t result=surgescript_var_get_objecthandle(v);surgescript_var_destroy(v);return result;
}
static int factory_live_count(void){int count=0;for(int i=0;i<64;i++)if(slots[i])++count;return count;}
static void registry_factory_test_tick(void){
 if(!getenv("BSL_REGISTRY_FACTORY_TEST"))return;
 static int stage,wait,checks,failures;static uint64_t retired;
 if(++wait<60)return;
 HostActor* p=hero();HostActor* primary=journey_character("character:michio");HostActor* alternate=journey_character("character:michio_alternate");
 BslGameplayProbe bag={0};BslStatProbe xp={0};bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);
 surgescript_objectmanager_t* manager=surgescript_vm_objectmanager(surgescript_vm());
 // Use the setup's script-retained service; a C-only handle does not root an ordinary script object.
 surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");
 surgescript_var_t* name=surgescript_var_set_string(surgescript_var_create(),"BSL Registry Journey Cast");const surgescript_var_t* args[]={name};surgescript_var_t* found=surgescript_var_create();
 surgescript_object_call_function(level,"findEntity",args,1,found);surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(found);surgescript_var_destroy(name);surgescript_var_destroy(found);
 surgescript_object_t* cast=handle&&surgescript_objectmanager_exists(manager,handle)?surgescript_objectmanager_get(manager,handle):NULL;
 surgescript_objecthandle_t service=cast?surgescript_object_child(cast,"BSL Character Registry"):0;
 surgescript_object_t* registry=service&&surgescript_objectmanager_exists(manager,service)?surgescript_objectmanager_get(manager,service):NULL;
#define FACTORY_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("FACTORY %s: %s",ok?"PASS":"FAIL",message);}while(0)
 if(!primary){FACTORY_CHECK(false,"primary body survives the replacement fixture");stage=99;}
 if(!registry){FACTORY_CHECK(false,"script-retained setup registry remains available");stage=99;}
 if(stage==0){
  checkpoint_park_enemies();FACTORY_CHECK(p&&alternate,"factory-composed roster is available in the loaded level");
  if(!p||!alternate){stage=99;return;}
  FACTORY_CHECK(factory_body(registry,"find","character:michio",0,0)==surgescript_object_handle(primary->entity),"registry lookup resolves the actual live entity by definition");
  int count=factory_live_count();FACTORY_CHECK(!factory_body(registry,"create","character:missing",380,9850)&&factory_live_count()==count,"unknown definition cannot create a controller");
  FACTORY_CHECK(!factory_body(registry,"create","character:michio_alternate",380,9850)&&factory_live_count()==count,"duplicate create refuses without replacing the existing body");
  FACTORY_CHECK(!factory_select(registry,"character:missing")&&controlled(primary),"unknown selection retains the active input authority");
  start(primary,"attack1",BOR_ACTION);FACTORY_CHECK(!factory_select(registry,"character:michio_alternate")&&controlled(primary),"scripted selection respects BOR action commitment");
  checkpoint_place(primary,160,primary->a.y);
  FACTORY_CHECK(factory_select(registry,"character:michio_alternate")&&controlled(alternate)&&!controlled(primary),"script adapter safely transfers the existing input owner");
  alternate->a.health=37;
  FACTORY_CHECK(bsl_host_reward(alternate->lifetime,"source:lab_exploration","factory/one")==0&&bsl_host_reward(alternate->lifetime,"source:lab_exploration","factory/two")==0,"alternate earns progression through the shared reward service");
  FACTORY_CHECK(bsl_host_use_item("item:strawberry")==0&&alternate->a.health==42,"controlled alternate consumes from the stable player's bag");
  FACTORY_CHECK(factory_select(registry,"character:michio")&&primary->a.health==83,"switching back does not copy character health");
  FACTORY_CHECK(bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump")&&!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump"),"learned capability belongs to the rewarded character before removal");
  retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);stage=1;wait=0;
 }else if(stage==1){
  FACTORY_CHECK(!alternate&&!bsl_host_controlled(retired)&&!factory_body(registry,"find","character:michio_alternate",0,0),"destroyed body is detached and cannot be found through a stale cached handle");
  int count=factory_live_count();
  FACTORY_CHECK(!factory_body(registry,"create","character:michio_alternate",NAN,9850)&&!factory_body(registry,"create","character:michio_alternate",10000001,9850)&&factory_live_count()==count,"nonfinite and out-of-bounds spawn positions refuse without a live controller");
  FACTORY_CHECK(!factory_select(registry,"character:michio_alternate")&&controlled(primary),"a detached character cannot steal input before its replacement exists");
  FACTORY_CHECK(factory_body(registry,"create","character:michio_alternate",380,9850)!=0,"trusted caller recreates a detached character at an authored position");stage=2;wait=0;
 }else if(stage==2){
  FACTORY_CHECK(alternate&&alternate->lifetime!=retired&&controlled(primary)&&!controlled(alternate),"replacement receives a new actor lifetime without taking primary input");
  if(!alternate){stage=99;return;}
  FACTORY_CHECK(alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"replacement retains character health and learned traversal instead of reseeding defaults");
  FACTORY_CHECK(factory_select(registry,"character:michio_alternate"),"newly grounded replacement is eligible for safe selection");
  bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);
  FACTORY_CHECK(bag.items==2&&bag.health==42&&xp.total==5000,"selected replacement projects its XP and the unchanged player inventory");
  retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);stage=3;wait=0;
 }else if(stage==3){
  FACTORY_CHECK(!alternate&&!hero()&&!bsl_host_controlled(retired),"selected-body removal releases input and lifetime-owned services");
  FACTORY_CHECK(factory_body(registry,"create","character:michio_alternate",380,9850)!=0,"selected character can receive a new body through the same registry factory");stage=4;wait=0;
 }else if(stage==4){
  FACTORY_CHECK(alternate&&p==alternate&&alternate->lifetime!=retired&&controlled(alternate)&&!controlled(primary),"selected replacement reattaches exactly one live input owner");
  if(!alternate){stage=99;return;}
  FACTORY_CHECK(bag.items==2&&bag.health==42&&xp.total==5000&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"active-body replacement preserves character progress and player inventory together");
  FACTORY_CHECK(bsl_host_use_item("item:strawberry")==0&&alternate->a.health==47,"effects bind to the replacement rather than the retired body");
  FACTORY_CHECK(factory_select(registry,"character:michio")&&controlled(primary)&&primary->a.health==83,"caller returns safely to the unchanged primary character");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_REGISTRY_FACTORY_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef FACTORY_CHECK
}
