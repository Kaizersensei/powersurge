/* Selected-only scene assembly with dormant character state and late materialization. Signed: Mech. */
static void registry_dormant_test_tick(void){
 const char* mode=getenv("BSL_JOURNEY_DORMANT");if(!mode)return;
 static int stage,wait,checks,failures;static uint64_t retired,local_actor;
 if(++wait<60)return;
 HostActor* p=hero();HostActor* primary=journey_character("character:michio");HostActor* alternate=journey_character("character:michio_alternate");
 BslGameplayProbe bag={0};BslCheckpointProbe cp={0};BslProgressProbe save={0};BslStatProbe xp={0};BslTeleportProbe travel={0};
 bsl_host_gameplay_probe(&bag);bsl_host_checkpoint_probe(&cp);bsl_host_progress_probe(&save);bsl_host_stat_probe("stat:experience",&xp);bsl_host_teleport_probe(&travel);
 surgescript_object_t* registry=placement_registry();
#define DORM_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("DORMANT %s: %s",ok?"PASS":"FAIL",message);if(!ok){logfile_message("DORMANT observed mode=%s stage=%d level=%s hp=%d items=%d xp=%llu busy=%d arrivals=%u",mode,stage,bsl_engine_level_file(),p?p->a.health:0,bag.items,(unsigned long long)xp.total,bsl_host_boot_busy(),travel.arrivals);stage=99;goto finish;}}while(0)
 DORM_CHECK(p&&registry,"selected body and canonical registry are available without requiring the entire roster");
 checkpoint_park_enemies();
 if(stage==0){
  surgescript_var_t* selected=surgescript_var_create();surgescript_object_call_function(registry,"selectedDefinition",NULL,0,selected);
  bool selection=surgescript_var_is_string(selected)&&!strcmp(surgescript_var_fast_get_string(selected),!strcmp(mode,"read")?"character:michio_alternate":"character:michio");surgescript_var_destroy(selected);
  DORM_CHECK(selection,"read-only selected definition reflects current startup or saved selection");
  if(!strcmp(mode,"read")){
   DORM_CHECK(alternate==p&&!primary&&!bsl_host_boot_busy()&&save.loads==1&&!save.pending,"fresh Continue completes with only the saved selected alternate instantiated");
   DORM_CHECK(p->a.health==100&&bag.items==2&&xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"selected HP, bag, XP and learned traversal restore from the permanent activation");
   DORM_CHECK(!strcmp(cp.chosen,"checkpoint:rest")&&!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev"),"Continue reconstructs the registered rest point without inactive roster bodies");
   DORM_CHECK(factory_body(registry,"create","character:michio",380,9850)!=0,"a saved dormant primary can materialize later through the ordinary factory");stage=20;wait=0;
  }else{
   DORM_CHECK(primary==p&&!alternate&&p->a.health==83&&bag.items==3,"new selected-only scene leaves the alternate dormant with authored player defaults");
   DORM_CHECK(!factory_select(registry,"character:michio_alternate")&&controlled(p),"selecting an absent character refuses without changing current input authority");
   DORM_CHECK(factory_body(registry,"create","character:michio_alternate",380,9850)!=0,"trusted caller can explicitly materialize an alternate before earning its state");stage=1;wait=0;
  }
 }else if(stage==1){
  DORM_CHECK(alternate&&factory_select(registry,"character:michio_alternate"),"newly materialized alternate can be safely selected");
  alternate->a.health=37;
  DORM_CHECK(bsl_host_reward(alternate->lifetime,"source:lab_exploration","dormant/one")==0&&bsl_host_reward(alternate->lifetime,"source:lab_exploration","dormant/two")==0,"alternate earns its own progression through registered reward sources");
  DORM_CHECK(bsl_host_use_item("item:strawberry")==0&&alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"alternate owns learned double jump and HP while consuming the player's shared bag");
  DORM_CHECK(factory_select(registry,"character:michio"),"control returns to the primary without transferring character progression");
  retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);checkpoint_place(primary,120,primary->a.y);stage=2;wait=0;
 }else if(stage==2){
  DORM_CHECK(!alternate&&!bsl_host_controlled(retired)&&bag.items==2&&xp.total==0,"removed alternate leaves no body/control but keeps shared inventory and independent primary XP");
  DORM_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&p->a.health==100,"permanent activation captures the dormant roster with the live primary");
  p->a.health=61;
  DORM_CHECK(bsl_host_anchor_start(0xdc0001,"event:door_unlock",p->lifetime)==0,"registered event enables travel without materializing the dormant alternate");stage=3;wait=0;
 }else if(stage==3){
  checkpoint_place(p,260,p->a.y);local_actor=p->lifetime;
  DORM_CHECK(bsl_host_teleport_request(p->lifetime,"door:local",true)==0,"local travel queues with a dormant roster entry");stage=4;wait=0;
 }else if(stage==4){
  DORM_CHECK(p->lifetime==local_actor&&!alternate&&!travel.busy&&travel.arrivals==1&&p->a.health==61&&bag.items==2,"local travel completes without reload or waiting for absent characters");
  checkpoint_place(p,200,p->a.y);
  DORM_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"cross-level travel queues with only the selected character present");stage=5;wait=0;
 }else if(stage==5){
  DORM_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&!alternate&&!travel.busy&&travel.arrivals==2,"destination completes with a fresh selected body and no forced alternate spawn");
  DORM_CHECK(p->lifetime!=local_actor&&!bsl_host_controlled(local_actor)&&p->a.health==61&&bag.items==2&&xp.total==0,"travel retires old authority and preserves live resources independently of dormant state");
  HostActor* enemy=equipment_actor("Green_Grunt");DORM_CHECK(enemy&&checkpoint_defeat(p,enemy),"real enemy damage defeats the selected actor in the destination");stage=6;wait=0;
 }else if(stage==6){
  DORM_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&!strcmp(cp.restored,"checkpoint:rest")&&cp.rewinds==1&&!alternate&&p->a.health==100&&bag.items==2,"cross-level defeat reconstructs the permanent activation without requiring absent characters");
  DORM_CHECK(factory_body(registry,"create","character:michio_alternate",380,9850)!=0,"dormant alternate can materialize after travel and checkpoint reconstruction");stage=7;wait=0;
 }else if(stage==7){
  DORM_CHECK(alternate&&alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump")&&!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump"),"late body restores its own HP and unlock without copying the primary state");
  DORM_CHECK(factory_select(registry,"character:michio_alternate"),"late alternate can become the controlled character");
  primary->a.health=58;surgescript_object_call_function(primary->entity,"destroy",NULL,0,NULL);checkpoint_place(alternate,120,alternate->a.y);stage=8;wait=0;
 }else if(stage==8){
  DORM_CHECK(!primary&&alternate==p&&xp.total==5000,"selected alternate retains earned XP while primary is dormant");
  DORM_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&p->a.health==100,"activation captures selected alternate and dormant primary together");
  DORM_CHECK(bsl_host_progress_save()==0,"existing temporary-slot writer saves the activation without serializing missing actor bodies");
  p->a.health=11;DORM_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==16,"post-save live consumption remains outside the saved activation");stage=99;
 }else if(stage==20){
  DORM_CHECK(primary&&primary->a.health==58&&!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump")&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"late primary restores saved dormant HP and retains independent locked traversal");
  DORM_CHECK(factory_select(registry,"character:michio"),"freshly restored dormant primary can be safely selected");
  bsl_host_stat_probe("stat:experience",&xp);DORM_CHECK(xp.total==0,"saved dormant primary has its own XP rather than the alternate's progression");
  DORM_CHECK(bsl_host_use_item("item:strawberry")==0&&primary->a.health==63,"restored player bag targets the newly selected live actor");
  DORM_CHECK(bsl_host_checkpoint_request()==0,"loaded activation can rewind later selection and inventory changes");stage=21;wait=0;
 }else if(stage==21){
  DORM_CHECK(alternate==p&&p->a.health==100&&primary&&primary->a.health==58&&bag.items==2&&!strcmp(cp.restored,"checkpoint:rest"),"loaded checkpoint restores selected alternate, primary HP and player bag coherently");
  DORM_CHECK(xp.total==5000&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump")&&!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump"),"rewind preserves independent character XP and learned capabilities after late materialization");stage=99;
 }
finish:
 if(stage==99){const char* path=getenv("BSL_JOURNEY_DORMANT_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef DORM_CHECK
}
