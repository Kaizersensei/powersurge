/* Fresh destination routing and constructor-time selected progress. Signed: Atlas. */
static bool boot_gate_open(void){
 surgescript_object_t* gate=level_get_entity_by_id("0000000000b51d01");if(!gate)return false;
 surgescript_var_t* value=surgescript_var_create();surgescript_object_call_function(gate,"get_opened",NULL,0,value);bool opened=surgescript_var_get_bool(value);surgescript_object_t* brick=surgescript_objectmanager_get(surgescript_object_manager(gate),surgescript_object_child(gate,"Brick"));
 if(brick){surgescript_object_call_function(brick,"get_enabled",NULL,0,value);opened=opened&&!surgescript_var_get_bool(value);}else opened=false;surgescript_var_destroy(value);return opened;
}
static void progress_boot_test_tick(void){
 const char* mode=getenv("BSL_PROGRESS_BOOT_TEST");if(!mode)return;
 static int stage,wait,checks,failures;static uint64_t source_actor;
 if(++wait<60)return;
#define BOOT_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("PROGRESS BOOT %s: %s",ok?"PASS":"FAIL",m);}while(0)
 BslBootProbe boot={0};BslGameplayProbe bag={0};BslProgressProbe progress={0};BslStatProbe xp={0};BslLearningProbe learned={0};bsl_host_boot_probe(&boot);bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);bsl_host_progress_probe(&progress);HostActor* p=hero();
 if(stage==0){
  BOOT_CHECK(p&&boot.enabled&&!boot.busy,"boot candidate exposes coherent live actor and no unfinished transition");if(!p){stage=99;return;}
  if(!strcmp(mode,"continue")){BOOT_CHECK(boot.arrivals==1&&!strcmp(boot.level,"levels/checkpoint-services.lev"),"fresh-process Continue routes from the entry scene to the saved destination");stage=1;}
  else{
   BOOT_CHECK(!strcmp(boot.level,"levels/resume-entry.lev")&&xp.total==0&&bag.items==3&&!boot.observed_reward&&boot.observed_actors==0,"entry constructors see authored defaults before actor binding");
   source_actor=p->lifetime;checkpoint_park_enemies();p->a.health=75;BOOT_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","prior-progress")==0,"source session carries distinct unsaved character progress");
   int requested=bsl_host_progress_load();
   if(!strcmp(mode,"missing")){BOOT_CHECK(requested!=0,"missing destination is refused before source unload");bsl_host_boot_probe(&boot);bsl_host_gameplay_probe(&bag);BOOT_CHECK(!boot.busy&&!strcmp(boot.level,"levels/resume-entry.lev")&&bag.actor==source_actor&&bag.health==75&&bag.items==3,"admission failure preserves source actor and player progress");stage=99;}
   else{BOOT_CHECK(requested==0,"validated load queues a fresh scene transition");stage=1;wait=0;return;}
  }
 }
 if(stage==1){
  bsl_host_boot_probe(&boot);bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&learned);p=hero();
  bool recovery=!strcmp(mode,"rollback")||!strcmp(mode,"no_bridge")||!strcmp(mode,"missing_actor");
  if(recovery){
   BOOT_CHECK(boot.failures==1&&!boot.busy&&!strcmp(boot.level,"levels/resume-entry.lev"),"failed destination returns to source level without a stalled boot gate");
   BOOT_CHECK(bag.health==75&&bag.items==3&&xp.total==2500&&!learned.learned&&bag.actor!=source_actor,"fallback rebinds fresh source actors to the prior progress rather than the saved target values");
   BOOT_CHECK(!boot.observed_reward&&boot.observed_items==3&&boot.observed_xp==2500&&boot.observed_actors==0,"source constructors see restored prior values before any actor binds");
  }else{
   BOOT_CHECK(boot.arrivals==1&&boot.failures==0&&!boot.busy&&!strcmp(boot.level,"levels/checkpoint-services.lev"),"saved destination completes exactly one fresh boot");
   BOOT_CHECK(bag.health==100&&bag.items==2&&xp.total==5000&&learned.learned&&p&&p->a.grounded&&fabs(p->a.x-120)<3,"destination actor, bag, XP, unlock and rest position agree with the save");
   BOOT_CHECK(boot.observations==2&&boot.observed_reward&&boot.observed_items==2&&boot.observed_xp==5000&&boot.observed_actors==0,"restored services precede the first level object and every actor constructor");
   BOOT_CHECK(boot.mutations_refused,"constructor-time reward and event mutation are refused during staged boot");
   BOOT_CHECK(boot_gate_open()&&bsl_host_progress_variable("variable:training_choices")==1,"saved switch reconstructs real gate collision/visual state and selected variable");
   BOOT_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0,"saved objective receipt accepts idempotent replay after scene boot");bsl_host_stat_probe("stat:experience",&xp);BOOT_CHECK(xp.total==5000,"scene reconstruction does not grant the objective twice");
   BOOT_CHECK(bsl_host_ability_use(p->lifetime,0)==0&&p->a.state==BOR_ACTION,"continued character can execute the earned BOR action");
  }
  stage=99;
 }
 if(stage==99){const char* file=getenv("BSL_PROGRESS_BOOT_REPORT");FILE* f=file?fopen(file,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef BOOT_CHECK
}
