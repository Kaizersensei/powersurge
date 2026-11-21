/* File-authored non-roster instantiation through the retained script factory. Signed: Mech. */
static HostActor* actor_event_body(const char* definition){for(int i=0;i<64;i++)if(slots[i]&&!strcmp(bsl_host_actor_template_for(slots[i]->lifetime),definition))return slots[i];return NULL;}
static void registry_actor_event_test_tick(void){
 const char* mode=getenv("BSL_JOURNEY_ACTOR_EVENTS");if(!mode)return;
 HostActor* p=hero();if(!p||bsl_host_boot_busy())return;
 static int stage,wait,checks,failures,base_count,health;static double floor_y;static uint64_t retired;
 if(++wait<45)return;
 HostActor* npc=actor_event_body("actor:merchant");BslAnchorRun run={0};BslGameplayProbe bag={0};BslCommerceProbe funds={0};bsl_host_gameplay_probe(&bag);bsl_host_commerce_probe(&funds);
#define OBJECT_EVENT_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("OBJECT_EVENT %s: %s",ok?"PASS":"FAIL",message);if(!ok){logfile_message("OBJECT_EVENT stage=%d count=%d npc=%p hp=%d items=%d funds=%d",stage,factory_live_count(),(void*)npc,p->a.health,bag.items,funds.balance);stage=99;goto finish;}}while(0)
 if(stage==0){
  checkpoint_park_enemies();floor_y=p->a.y;health=p->a.health;base_count=factory_live_count();
  OBJECT_EVENT_CHECK(actor_factory_service()&&!npc,"loaded setup retains the non-roster provider without implicitly populating actors");
  OBJECT_EVENT_CHECK(bsl_bor_event_spawn_actor("actor:missing",260,9850)==2&&bsl_bor_event_spawn_actor("actor:merchant",NAN,9850)==2&&factory_live_count()==base_count,"native event port refuses unknown templates and invalid coordinates before creating a body");
  if(!strcmp(mode,"read")){stage=100;wait=45;return;}
  HostActor* alternate=journey_character("character:michio_alternate");OBJECT_EVENT_CHECK(alternate&&bsl_host_anchor_start(0x170001,"event:registry_actor_wait",alternate->lifetime)!=0,"inactive roster body cannot initiate a player-owned spawn event");
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170001,"event:registry_actor_wait",p->lifetime)==0,"authored wait and actor command start through the normal event entry point");bsl_host_simulation_tick();bsl_host_anchor_probe(0x170001,&run);
  OBJECT_EVENT_CHECK(run.status==2&&run.next==1&&!actor_event_body("actor:merchant"),"authored wait defers the actual factory invocation");bsl_host_hit(p->lifetime);stage=1;wait=0;
 }else if(stage==1){
  bsl_host_anchor_probe(0x170001,&run);OBJECT_EVENT_CHECK(run.status==4&&!npc&&factory_live_count()==base_count&&bsl_host_progress_variable("variable:training_choices")==0,"hit cancellation prevents pending creation and subsequent authored effects");
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170002,"event:registry_actor_now",p->lifetime)==0,"a separate explicit run may create an actor before waiting");bsl_host_simulation_tick();npc=actor_event_body("actor:merchant");bsl_host_anchor_probe(0x170002,&run);
  OBJECT_EVENT_CHECK(npc&&run.status==2&&run.next==2&&factory_live_count()==base_count+1,"spawn executes once through the real registered factory before the following wait");
  OBJECT_EVENT_CHECK(npc->agency==1&&!bsl_host_controlled(npc->lifetime)&&controlled(p)&&!strcmp(bsl_host_actor_interaction_definition(npc->lifetime),"interaction:training_shop"),"event-created actor receives its template agency and interaction without becoming a player character");
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170002,"event:registry_actor_now",p->lifetime)!=0&&factory_live_count()==base_count+1,"an active placement run refuses duplicate activation without another spawn");bsl_host_anchor_cancel(0x170002);stage=2;wait=0;
 }else if(stage==2){
  bsl_host_anchor_probe(0x170002,&run);OBJECT_EVENT_CHECK(run.status==4&&npc&&factory_live_count()==base_count+1&&bsl_host_progress_variable("variable:training_choices")==0,"cancelling after creation preserves the committed actor but cancels remaining commands");actor_factory_destroy(npc);checkpoint_place(p,260,floor_y);stage=3;wait=0;
 }else if(stage==3){
  OBJECT_EVENT_CHECK(!npc&&factory_live_count()==base_count,"retiring the created body releases its lifetime binding");
  checkpoint_place(p,260,9850); /* Match the authored point exactly; floor height differs along this course. */
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170003,"event:registry_actor_block",p->lifetime)==0,"blocked-coordinate fixture executes through normal authored commands");bsl_host_simulation_tick();bsl_host_anchor_probe(0x170003,&run);
  OBJECT_EVENT_CHECK(run.status==5&&run.next==1&&run.error==3&&!actor_event_body("actor:merchant")&&factory_live_count()==base_count,"occupied player position fails exact spawn preflight without leaking a native body");
  OBJECT_EVENT_CHECK(bsl_host_progress_variable("variable:training_choices")==3&&p->a.health==health&&bag.items==3&&controlled(p),"failed creation keeps earlier effects and player state while skipping later commands");checkpoint_place(p,160,floor_y);stage=4;wait=0;
 }else if(stage==4){
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170004,"event:registry_actor_wait",p->lifetime)==0,"an explicit new run can retry at the now-clear authored position");stage=5;wait=0;
 }else if(stage==5){
  bsl_host_anchor_probe(0x170004,&run);OBJECT_EVENT_CHECK(run.status==3&&npc&&factory_live_count()==base_count+1&&bsl_host_progress_variable("variable:training_choices")==9,"successful retry creates one actor and continues the authored event");
  stage=51;wait=0; /* The authored wait ends soon after creation; allow normal falling/settling before dialogue. */
 }else if(stage==51){
  OBJECT_EVENT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,2),"event-created merchant drives its registered shop outcome");stage=6;wait=0;
 }else if(stage==6){
  OBJECT_EVENT_CHECK(bag.items==4&&funds.balance==9&&funds.stock==3,"event-created merchant transaction commits existing player inventory and economy services");retired=npc->lifetime;actor_factory_destroy(npc);stage=7;wait=0;
 }else if(stage==7){
  OBJECT_EVENT_CHECK(!npc&&bsl_host_anchor_start(0x170005,"event:registry_actor_wait",p->lifetime)==0,"pending source-level spawn is queued before scene departure");bsl_host_simulation_tick();bsl_host_anchor_probe(0x170005,&run);OBJECT_EVENT_CHECK(run.status==2,"departure fixture leaves a real waiting event");bsl_engine_load_level("levels/teleport-annex.lev");stage=8;wait=0;
 }else if(stage==8){
  checkpoint_park_enemies();floor_y=p->a.y;OBJECT_EVENT_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&!npc&&!*bsl_host_actor_template_for(retired),"scene unload retires created actors and does not leak the cancelled source-level spawn into the destination");
  OBJECT_EVENT_CHECK(bag.items==4&&funds.balance==9&&bsl_host_progress_variable("variable:training_choices")==9,"travel retains intended player progress and committed event variables");
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170006,"event:registry_actor_object",p->lifetime)==0,"world-object templates use the same actor command");bsl_host_simulation_tick();HostActor* object=actor_event_body("actor:training_object");
  OBJECT_EVENT_CHECK(object&&object->a.model->training_dummy&&object->agency==1&&controlled(p)&&!bsl_host_controlled(object->lifetime),"helper model initializes outside the roster with existing BOR behavior");actor_factory_destroy(object);
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170007,"event:registry_actor_wait",p->lifetime)==0,"destination may explicitly reconstruct an actor through its own retained provider");stage=9;wait=0;
 }else if(stage==9){
  bsl_host_anchor_probe(0x170007,&run);OBJECT_EVENT_CHECK(run.status==3&&npc&&npc->lifetime!=retired&&npc->a.health==100,"destination command creates a fresh instance rather than restoring hidden NPC health");npc->a.health=55;checkpoint_place(p,120,floor_y);stage=10;wait=0;
 }else if(stage==10){
  OBJECT_EVENT_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:annex")==0&&bsl_host_progress_save()==0,"normal compact save records player progress without transient actor bodies");stage=99;
 }else if(stage==100){
  OBJECT_EVENT_CHECK(bag.items==4&&funds.balance==9&&!npc&&bsl_host_progress_variable("variable:training_choices")==9,"fresh Continue restores committed progress without replaying completed or cancelled spawn commands");checkpoint_place(p,160,floor_y);
  OBJECT_EVENT_CHECK(bsl_host_anchor_start(0x170100,"event:registry_actor_wait",p->lifetime)==0,"continued session may explicitly request a new actor instance");stage=101;wait=0;
 }else if(stage==101){
  bsl_host_anchor_probe(0x170100,&run);OBJECT_EVENT_CHECK(run.status==3&&npc&&npc->a.health==100&&!bsl_host_controlled(npc->lifetime),"continued authored spawn uses template defaults and retains player input ownership");
  stage=105;wait=0;
 }else if(stage==105){
  OBJECT_EVENT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,2),"continued event-created actor dispatches a fresh economy transaction");stage=102;wait=0;
 }else if(stage==102){
  OBJECT_EVENT_CHECK(bag.items==5&&funds.balance==6&&funds.stock==2,"continued transaction is not mistaken for an earlier event reward receipt");
  surgescript_object_t* registry=actor_factory_service();OBJECT_EVENT_CHECK(registry!=NULL,"provider failure fixture targets only the retained non-roster registry");surgescript_object_call_function(registry,"destroy",NULL,0,NULL);actor_factory_destroy(npc);stage=103;wait=0;
 }else if(stage==103){
  OBJECT_EVENT_CHECK(!actor_factory_service()&&!npc&&bsl_host_anchor_start(0x170101,"event:registry_actor_wait",p->lifetime)==0,"event can report an unavailable canonical actor provider without replacing the setup");stage=104;wait=0;
 }else if(stage==104){
  bsl_host_anchor_probe(0x170101,&run);OBJECT_EVENT_CHECK(run.status==5&&run.next==1&&run.error==1&&!npc&&controlled(p),"missing canonical actor registry fails without falling back to an unrelated factory");
  OBJECT_EVENT_CHECK(bag.items==5&&funds.balance==6,"provider refusal preserves committed inventory and economy state");stage=99;
 }
finish:
 if(stage==99){const char* path=getenv("BSL_JOURNEY_ACTOR_EVENTS_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef OBJECT_EVENT_CHECK
}
