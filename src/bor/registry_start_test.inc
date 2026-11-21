/* Startup selection, fresh slot isolation and saved selection restoration. Signed: Mech. */
static void registry_start_test_tick(void){
 const char* mode=getenv("BSL_REGISTRY_START_TEST");if(!mode)return;
 static int stage,wait,checks,failures;if(++wait<60)return;
 HostActor* p=hero();HostActor* primary=journey_character("character:michio");HostActor* alternate=journey_character("character:michio_alternate");
 BslGameplayProbe bag={0};BslStatProbe xp={0};BslProgressProbe saved={0};BslCharacterDefinition d={0};
 bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);bsl_host_progress_probe(&saved);
#define START_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("START %s: %s",ok?"PASS":"FAIL",message);}while(0)
 if(!p||!primary||!alternate){START_CHECK(false,"registered character bodies are available");stage=99;}
 if(stage==0){
  checkpoint_park_enemies();START_CHECK(bag.items==(!strcmp(mode,"read")?2:3),"initial player inventory comes from the new run or the selected saved slot");
  START_CHECK(bsl_host_choose_initial_character("character:michio")!=0,"startup choice is closed once character bodies are attached");
  if(!strcmp(mode,"default")){
   START_CHECK(p==primary&&bag.health==83&&xp.total==0,"fresh second slot starts at authored primary health and XP");
   START_CHECK(alternate->a.health==90&&!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump")&&!bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"another slot's character health and learned abilities do not leak into a new run");
   START_CHECK(!bsl_host_progress_flag("switch:training_reward")&&bsl_host_progress_variable("variable:training_choices")==0&&saved.loads==0,"fresh slot has no imported event outcomes or implicit Continue");stage=99;
  }else if(!strcmp(mode,"read")){
   START_CHECK(saved.loads==1&&!saved.pending&&p==alternate&&bag.health==100&&xp.total==5000,"Continue restores its saved character selection instead of a conflicting launch preference");
   START_CHECK(bsl_host_character_definition("character:michio_alternate",&d)==0&&d.selected&&d.actor==p->lifetime,"registry discovery reflects the restored input owner");
   START_CHECK(primary->a.health==83&&!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump")&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"saved alternate progress stays independent from the primary character");
   START_CHECK(player_jump(p,0,false,false)&&player_jump(p,0,true,false)&&p->jumps_used==2,"restored alternate can execute its earned second jump");stage=99;
  }else{
   START_CHECK(p==alternate&&bag.health==90&&xp.total==0&&!controlled(primary),"new-run choice binds only the alternate with its own authored starting values");
   START_CHECK(bsl_host_character_definition("character:michio_alternate",&d)==0&&d.selected&&d.actor==p->lifetime,"metadata selection agrees with the actual initial actor authority");
   START_CHECK(primary->a.health==83&&!bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"initial selection does not grant progression or reseed the other character");
   if(!strcmp(mode,"choose")){
    surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_var_t* name=surgescript_var_set_string(surgescript_var_create(),"BSL Registry Initial Choice");const surgescript_var_t* args[]={name};surgescript_var_t* found=surgescript_var_create();
    surgescript_object_call_function(level,"findEntity",args,1,found);surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(found);surgescript_objectmanager_t* manager=surgescript_object_manager(level);bool accepted=false;
    if(handle&&surgescript_objectmanager_exists(manager,handle)){surgescript_var_t* value=surgescript_var_create();surgescript_object_call_function(surgescript_objectmanager_get(manager,handle),"get_accepted",NULL,0,value);accepted=surgescript_var_get_bool(value);surgescript_var_destroy(value);}
    surgescript_var_destroy(name);surgescript_var_destroy(found);START_CHECK(accepted,"trusted SurgeScript startup selection succeeds before the first body is attached");stage=99;
   }else{
    p->a.health=37;START_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==42,"selected initial character receives the item effect through the existing player inventory");
    START_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","start/one")==0&&bsl_host_reward(p->lifetime,"source:lab_exploration","start/two")==0&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"selected initial character earns its own capability through registered rewards");
    checkpoint_place(p,120,p->a.y);stage=1;wait=0;
   }
  }
 }else if(stage==1){
  START_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&p->a.health==100,"initially selected alternate captures the permanent activation normally");
  START_CHECK(bsl_host_progress_save()==0,"explicit save writes the selected character and its earned state to the chosen isolated slot");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_REGISTRY_START_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef START_CHECK
}
