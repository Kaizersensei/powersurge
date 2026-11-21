/* Authored event -> existing factory -> actual replacement actor and retained services. Signed: Mech. */
// Remove only the canonical shared provider to verify an explicit unavailable result. Signed: Mech.
static bool registry_remove_shared_provider(void){
 surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_objectmanager_t* manager=surgescript_object_manager(level);
 surgescript_var_t* name=surgescript_var_set_string(surgescript_var_create(),"BSL Runtime Setup");const surgescript_var_t* args[]={name};surgescript_var_t* found=surgescript_var_create();
 surgescript_object_call_function(level,"findEntity",args,1,found);surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(found);surgescript_var_destroy(name);surgescript_var_destroy(found);
 if(!handle||!surgescript_objectmanager_exists(manager,handle))return false;
 surgescript_objecthandle_t service=surgescript_object_child(surgescript_objectmanager_get(manager,handle),"BSL Character Registry");
 if(!service||!surgescript_objectmanager_exists(manager,service))return false;
 surgescript_object_call_function(surgescript_objectmanager_get(manager,service),"destroy",NULL,0,NULL);return true;
}
static void registry_event_test_tick(void){
 if(!getenv("BSL_REGISTRY_EVENT_TEST"))return;
 static int stage,wait,checks,failures;static uint64_t retired;
 if(++wait<60)return;
 HostActor* primary=journey_character("character:michio");HostActor* alternate=journey_character("character:michio_alternate");BslAnchorRun run={0};BslGameplayProbe bag={0};BslStatProbe xp={0};
#define SPAWN_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("SPAWN_EVENT %s: %s",ok?"PASS":"FAIL",message);}while(0)
 if(!primary){SPAWN_CHECK(false,"primary actor is available");stage=99;}
 if(stage==0){
  checkpoint_park_enemies();SPAWN_CHECK(alternate&&bsl_host_select_character("character:michio_alternate")==0,"alternate can earn its own state before removal");if(!alternate){stage=99;return;}
  alternate->a.health=37;
  SPAWN_CHECK(bsl_host_reward(alternate->lifetime,"source:lab_exploration","spawn/one")==0&&bsl_host_reward(alternate->lifetime,"source:lab_exploration","spawn/two")==0&&bsl_host_use_item("item:strawberry")==0,"existing reward and inventory services prepare the detached character state");
  SPAWN_CHECK(bsl_host_select_character("character:michio")==0,"primary resumes control before the inactive body is removed");
  retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);stage=1;wait=0;
 }else if(stage==1){
  SPAWN_CHECK(!alternate&&!bsl_host_controlled(retired),"inactive body is fully detached before its event-driven replacement");
  SPAWN_CHECK(bsl_host_anchor_start(0xca0050,"event:registry_spawn",primary->lifetime)==0,"placed owner starts the authored wait then spawn command");
  bsl_host_simulation_tick();bsl_host_anchor_probe(0xca0050,&run);SPAWN_CHECK(run.status==2&&run.next==1,"spawn has not occurred during the authored wait");bsl_host_hit(primary->lifetime);stage=2;wait=0;
 }else if(stage==2){
  bsl_host_anchor_probe(0xca0050,&run);SPAWN_CHECK(run.status==4&&!alternate&&bsl_host_progress_variable("variable:training_choices")==0,"hit cancellation prevents both pending spawn and subsequent event effects");
  SPAWN_CHECK(bsl_host_anchor_start(0xca0051,"event:registry_spawn",primary->lifetime)==0,"an explicit new event run can restart the interrupted sequence");stage=3;wait=0;
 }else if(stage==3){
  bsl_host_anchor_probe(0xca0051,&run);SPAWN_CHECK(run.status==3&&alternate&&alternate->lifetime!=retired,"authored spawn completes through the actual registry factory");
  if(!alternate){stage=99;return;}
  SPAWN_CHECK(controlled(primary)&&!controlled(alternate)&&alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"spawn preserves earned character values without taking control");
  bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);SPAWN_CHECK(bag.items==2&&xp.total==0&&!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump"),"primary progression and player inventory remain distinct from the spawned character");
  SPAWN_CHECK(bsl_host_progress_variable("variable:training_choices")==7,"commands after successful spawn execute normally");
  SPAWN_CHECK(bsl_host_anchor_start(0xca0052,"event:registry_spawn",alternate->lifetime)!=0,"inactive actor cannot start a player-owned spawn sequence");
  SPAWN_CHECK(bsl_host_anchor_start(0xca0052,"event:registry_spawn",primary->lifetime)==0,"separate owner can request a second explicit attempt");retired=alternate->lifetime;stage=4;wait=0;
 }else if(stage==4){
  bsl_host_anchor_probe(0xca0052,&run);SPAWN_CHECK(run.status==5&&run.next==1&&alternate&&alternate->lifetime==retired,"duplicate body request fails at the spawn command and retains the original body");
  SPAWN_CHECK(bsl_host_progress_variable("variable:training_choices")==7&&controlled(primary),"failed spawn does not redirect input or overwrite committed progress");
  retired=primary->lifetime;bsl_engine_load_level("levels/registry-runtime.lev");stage=5;wait=0;
 }else if(stage==5){
  SPAWN_CHECK(!strcmp(bsl_engine_level_file(),"levels/registry-runtime.lev")&&primary->lifetime!=retired&&!bsl_host_controlled(retired),"shared setup creates fresh bindings after actual level teardown");checkpoint_park_enemies();
  SPAWN_CHECK(alternate&&alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"character health and learning survive entry through the shared setup");
  bsl_host_gameplay_probe(&bag);SPAWN_CHECK(controlled(primary)&&bag.items==2,"shared setup retains stable player inventory and exactly the selected input owner");
  if(!alternate){stage=99;return;}retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);stage=6;wait=0;
 }else if(stage==6){
  SPAWN_CHECK(!alternate&&bsl_host_anchor_start(0xca0060,"event:registry_spawn",primary->lifetime)==0,"shared runtime accepts the same authored character-spawn sequence");stage=7;wait=0;
 }else if(stage==7){
  bsl_host_anchor_probe(0xca0060,&run);SPAWN_CHECK(run.status==3&&alternate&&alternate->lifetime!=retired,"event factory resolves the shared setup without a legacy BOR Setup entity");
  if(!alternate){stage=99;return;}
  SPAWN_CHECK(alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump")&&controlled(primary),"shared-provider creation retains character values without stealing input");
  surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);SPAWN_CHECK(registry_remove_shared_provider(),"missing-provider fixture removes the actual shared service");stage=8;wait=0;
 }else if(stage==8){
  SPAWN_CHECK(!alternate&&bsl_host_anchor_start(0xca0061,"event:registry_spawn",primary->lifetime)==0,"event can report an unavailable provider through normal command failure");stage=9;wait=0;
 }else if(stage==9){
  bsl_host_anchor_probe(0xca0061,&run);SPAWN_CHECK(run.status==5&&run.next==1&&!alternate&&controlled(primary),"missing canonical provider fails safely without borrowing another object's registry");
  bsl_host_gameplay_probe(&bag);SPAWN_CHECK(bag.items==2&&bsl_host_progress_variable("variable:training_choices")==7,"unavailable factory does not alter player inventory or committed event state");
  retired=primary->lifetime;bsl_engine_load_level("levels/checkpoint-services.lev");stage=10;wait=0;
 }else if(stage==10){
  SPAWN_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&primary->lifetime!=retired&&!bsl_host_controlled(retired),"return to legacy setup retires the shared-level actor authority");checkpoint_park_enemies();
  if(!alternate){SPAWN_CHECK(false,"legacy reentry supplies the inactive body");stage=99;return;}
  retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);stage=11;wait=0;
 }else if(stage==11){
  SPAWN_CHECK(!alternate&&bsl_host_anchor_start(0xca0062,"event:registry_spawn",primary->lifetime)==0,"legacy reentry can request a fresh factory operation");stage=12;wait=0;
 }else if(stage==12){
  bsl_host_anchor_probe(0xca0062,&run);SPAWN_CHECK(run.status==3&&alternate&&alternate->lifetime!=retired,"legacy provider is rediscovered after the previous shared provider was destroyed");
  bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);SPAWN_CHECK(alternate&&alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump")&&bag.items==2&&xp.total==0&&controlled(primary),"round trip preserves per-character progress and the stable player's bag");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_REGISTRY_EVENT_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef SPAWN_CHECK
}
