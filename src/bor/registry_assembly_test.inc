/* Runs from host preupdate so a paused failed setup can still be inspected/retried. Signed: Mech. */
static surgescript_object_t* assembly_test_module(void){
 surgescript_object_t* cast=placement_entity("BSL Registry Journey Cast");if(!cast)return NULL;
 surgescript_objecthandle_t h=surgescript_object_child(cast,"BSLPlayerAssembly");return h?surgescript_objectmanager_get(surgescript_object_manager(cast),h):NULL;
}
static void assembly_test_required(surgescript_object_t* module,bool required){
 surgescript_var_t* value=surgescript_var_set_bool(surgescript_var_create(),required);const surgescript_var_t* args[]={value};surgescript_object_call_function(module,"set_required",args,1,NULL);surgescript_var_destroy(value);
}
static bool assembly_test_retry(surgescript_object_t* module){
 surgescript_var_t* value=surgescript_var_create();surgescript_object_call_function(module,"retry",NULL,0,value);bool result=surgescript_var_get_bool(value);surgescript_var_destroy(value);return result;
}
static bool assembly_capture_pending,assembly_capture_done;
void bsl_bor_player_assembly_test_render(void){
 if(!assembly_capture_pending)return;
 const char* path=getenv("BSL_ASSEMBLY_CAPTURE");
 if(path){ALLEGRO_STATE files;al_store_state(&files,ALLEGRO_STATE_NEW_FILE_INTERFACE);al_set_standard_file_interface();image_save(video_get_backbuffer(),path);al_restore_state(&files);}
 assembly_capture_pending=false;assembly_capture_done=true;
}
void bsl_bor_player_assembly_test_tick(void){
 if(!getenv("BSL_REGISTRY_ASSEMBLY_TEST"))return;
 static int stage,wait,frames,checks,failures;static uint64_t other_lifetime,player_lifetime;static double calendar_at_fault,simulation_at_fault,ui_at_fault;
 BslPlayerAssemblyProbe setup={0};BslCalendarProbe clock={0};BslGameplayProbe bag={0};bsl_host_player_assembly_probe(&setup);bsl_host_calendar_probe(&clock);bsl_host_gameplay_probe(&bag);
 surgescript_object_t* module=assembly_test_module();HostActor* p=hero();HostActor* alternate=journey_character("character:michio_alternate");
#define ASSEMBLY_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("ASSEMBLY %s: %s",ok?"PASS":"FAIL",message);}while(0)
 if(++frames>1200){ASSEMBLY_CHECK(false,"assembly scenario completes within its bounded frame budget");stage=99;}
 if(stage==0&&module&&setup.required&&alternate){
  ASSEMBLY_CHECK(!p&&!setup.ready,"blocked selected character does not receive a partial world binding");
  other_lifetime=alternate->lifetime;alternate->a.health=37;checkpoint_park_enemies();
  ASSEMBLY_CHECK(!assembly_test_retry(module),"retry refuses before a failed initial setup");assembly_test_required(module,false);stage=1;wait=0;
 }else if(stage==1&&++wait>=130){
  ASSEMBLY_CHECK(!setup.required&&!setup.failed&&!setup.ready&&!p&&!clock.paused,"a deliberate playerless level can opt out without a false failure or pause");
  assembly_test_required(module,true);stage=2;wait=0;
 }else if(stage==2&&setup.failed){
  ASSEMBLY_CHECK(setup.required&&!setup.ready&&setup.waiting==120&&strstr(setup.message,"character:michio")&&strstr(setup.message,"retry"),"bounded initial-assembly failure reports the missing selected definition and recovery action");
  ASSEMBLY_CHECK(clock.paused&&bsl_host_captures_commands(),"failed setup owns a pause/input policy without requiring a live actor");
  ASSEMBLY_CHECK(alternate&&alternate->lifetime==other_lifetime&&alternate->a.health==37&&bag.items==3,"partial world setup preserves existing actors and player inventory");
  calendar_at_fault=clock.elapsed;simulation_at_fault=clock.simulation;ui_at_fault=clock.ui;stage=3;wait=0;
 }else if(stage==3&&++wait>=20){
  ASSEMBLY_CHECK(clock.paused&&clock.elapsed==calendar_at_fault&&clock.simulation==simulation_at_fault&&clock.ui>ui_at_fault,"world/calendar remain paused while the existing UI clock advances");
  assembly_capture_pending=true;stage=7;
 }else if(stage==7&&assembly_capture_done){
  ASSEMBLY_CHECK(assembly_test_retry(module),"explicit retry reaches the bootstrap through the native script adapter");stage=4;wait=0;
 }else if(stage==4&&setup.failed&&setup.retries==1){
  ASSEMBLY_CHECK(!p&&!setup.retry_pending&&alternate&&alternate->lifetime==other_lifetime&&alternate->a.health==37,"an uncleared retry fails again without recreating/resetting existing actors");
  surgescript_object_t* gate=level_get_entity_by_id("b51d01");ASSEMBLY_CHECK(gate!=NULL,"live obstruction remains the same editable level object");
  if(!gate){stage=99;return;}scripting_util_set_world_position(gate,v2d_new(1000,9850));level_set_obstaclemap_dirty();
  ASSEMBLY_CHECK(assembly_test_retry(module),"clearing a live obstruction permits another explicit same-scene retry");stage=5;wait=0;
 }else if(stage==5&&setup.ready&&p&&++wait>=60){
  ASSEMBLY_CHECK(!setup.failed&&!setup.retry_pending&&setup.retries==2&&!clock.paused,"successful assembly releases every startup pause and pending retry");
  ASSEMBLY_CHECK(alternate&&alternate->lifetime==other_lifetime&&alternate->a.health==37&&p->a.health==83&&bag.items==3,"recovery retains existing actor identity/HP, player inventory and authored new-player health");
  surgescript_object_t* gate=level_get_entity_by_id("b51d01");v2d_t at=gate?scripting_util_world_position(gate):v2d_new(0,0);
  ASSEMBLY_CHECK(gate&&at.x==1000&&at.y==9850,"unsaved live level edit survives retry without a reload");
  BslCharacterDefinition d={0};ASSEMBLY_CHECK(bsl_host_character_definition("character:michio",&d)==0&&d.actor==p->lifetime&&d.selected&&!bsl_host_controlled(other_lifetime),"exactly the selected new body receives input authority");
  ASSEMBLY_CHECK(!assembly_test_retry(module),"retry refuses once startup has completed");
  player_lifetime=p->lifetime;surgescript_object_call_function(p->entity,"destroy",NULL,0,NULL);stage=6;wait=0;
 }else if(stage==6&&++wait>=130){
  ASSEMBLY_CHECK(setup.ready&&!setup.failed&&!p&&!bsl_host_controlled(player_lifetime)&&!clock.paused,"later intentional actor removal does not become a startup failure or force an automatic respawn");stage=99;
 }
 if(stage==99){const char* report=getenv("BSL_REGISTRY_ASSEMBLY_REPORT");FILE* f=report?fopen(report,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}bsl_engine_quit();}
#undef ASSEMBLY_CHECK
}
