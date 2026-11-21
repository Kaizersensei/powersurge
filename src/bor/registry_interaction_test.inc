/* Opt-in single-player authored interaction targeting; real BOR bodies and services. Signed: Mech. */
static bool interaction_script_bind(HostActor* actor,const char* definition){
 surgescript_var_t* id=surgescript_var_set_string(surgescript_var_create(),definition);const surgescript_var_t* args[]={id};surgescript_var_t* result=surgescript_var_create();
 surgescript_object_call_function(actor->entity,"setInteraction",args,1,result);bool ok=surgescript_var_get_bool(result);surgescript_var_destroy(id);surgescript_var_destroy(result);return ok;
}
static void registry_interaction_test_tick(void){
 const char* mode=getenv("BSL_JOURNEY_INTERACTIONS");if(!mode)return;
 static int stage,wait,checks,failures;static double floor_y;static uint64_t retired;
 if(++wait<45)return;
 HostActor* p=hero();HostActor* alternate=journey_character("character:michio_alternate");HostActor* arcade=journey_character("character:michio_arcade");
 BslGameplayProbe bag={0};BslConversationProbe talk={0};bsl_host_gameplay_probe(&bag);bsl_host_conversation_probe(&talk);double distance=INFINITY;
#define INTERACT_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("INTERACT %s: %s",ok?"PASS":"FAIL",message);if(!ok){logfile_message("INTERACT observed stage=%d p=%p alt=%p arcade=%p items=%d open=%d",stage,(void*)p,(void*)alternate,(void*)arcade,bag.items,talk.open);stage=99;goto finish;}}while(0)
 if(stage==0){
  INTERACT_CHECK(p&&alternate&&arcade,"registered test cast exists without adding a new actor implementation");checkpoint_park_enemies();floor_y=p->a.y;checkpoint_place(p,160,floor_y);checkpoint_place(alternate,260,floor_y);checkpoint_place(arcade,320,floor_y);stage=!strcmp(mode,"read")?100:1;wait=0;
 }else if(stage==1){
  floor_y=p->a.y;
  INTERACT_CHECK(!*bsl_host_actor_interaction_definition(alternate->lifetime),"no implicit interaction binding is inferred from actor name/model");
  INTERACT_CHECK(interaction_script_bind(alternate,"interaction:training_shop")&&interaction_script_bind(arcade,"interaction:training_reward"),"actor wrappers bind registered interaction recipes through the native component");
  INTERACT_CHECK(!interaction_script_bind(alternate,"interaction:missing")&&!strcmp(bsl_host_actor_interaction_definition(alternate->lifetime),"interaction:training_shop"),"unknown recipe refuses and preserves the existing binding");
  INTERACT_CHECK(bsl_host_actor_interaction_bind(0,"interaction:training_shop")!=0,"missing actor lifetime cannot register an interaction");
  INTERACT_CHECK(interaction_script_bind(p,"interaction:training_shop")&&bsl_host_actor_interaction_nearest(p->lifetime,&distance)==alternate->lifetime&&fabs(distance-10000)<1,"nearest eligible actor excludes the initiating player's own binding");
  INTERACT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0xcafe01,"event:training_reward",40000)==0,"nearer actor wins over a farther event helper through shared dispatch");
  bsl_host_conversation_probe(&talk);BslAnchorRun run={0};bsl_host_anchor_probe(0xcafe01,&run);
  INTERACT_CHECK(talk.open&&talk.speaker==alternate->lifetime&&run.status<0&&!strcmp(talk.definition,"dialogue:training_choice"),"one input dispatch opens the recipe dialogue without also starting the helper");
  bsl_host_conversation_cancel(p->lifetime);checkpoint_place(alternate,400,floor_y);checkpoint_place(arcade,240,floor_y);stage=2;wait=0;
 }else if(stage==2){
  INTERACT_CHECK(bsl_host_actor_interaction_nearest(p->lifetime,&distance)==arcade->lifetime&&fabs(distance-6400)<1,"nearest target follows current world positions rather than registration order");
  checkpoint_place(alternate,260,floor_y);checkpoint_place(arcade,260,floor_y);
  INTERACT_CHECK(bsl_host_actor_interaction_nearest(p->lifetime,&distance)==(alternate->lifetime<arcade->lifetime?alternate->lifetime:arcade->lifetime),"equal actor distances use a stable lifetime tie-break within the scene");checkpoint_place(alternate,400,floor_y);checkpoint_place(arcade,240,floor_y);
  INTERACT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0xcafe02,"event:training_reward",400)==0,"closer event helper retains its existing interaction route");bsl_host_conversation_probe(&talk);
  INTERACT_CHECK(!talk.open,"helper selection does not open an actor dialogue in parallel");stage=3;wait=0;
 }else if(stage==3){
  INTERACT_CHECK(bag.items==4&&bsl_host_progress_variable("variable:training_choices")==1,"selected helper outcome commits exactly once");
  INTERACT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0xcafe03,"event:training_reward",6400)==0,"existing helper wins an exact-distance actor/helper tie");stage=4;wait=0;
 }else if(stage==4){
  INTERACT_CHECK(bag.items==5&&!talk.open,"tie dispatch performs only the helper outcome");
  INTERACT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,63),"actor-only dispatch resolves the bound long-choice recipe and registered outcome");stage=5;wait=0;
 }else if(stage==5){
  INTERACT_CHECK(bag.items==6&&bsl_host_progress_variable("variable:training_choices")==3,"actor-selected reward reaches the initiating stable player's services");
  checkpoint_place(alternate,260,floor_y);checkpoint_place(arcade,1000,floor_y);
  INTERACT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0,"nearby speaker can open again after an earlier actor's outcome");
  INTERACT_CHECK(interaction_script_bind(alternate,"interaction:training_shop"),"reapplying the same binding is idempotent");bsl_host_conversation_probe(&talk);INTERACT_CHECK(talk.open,"unchanged binding does not interrupt the active conversation");
  INTERACT_CHECK(!interaction_script_bind(alternate,"interaction:missing"),"invalid rebind refuses while a conversation is active");bsl_host_conversation_probe(&talk);INTERACT_CHECK(talk.open,"invalid rebind preserves the current conversation");
  INTERACT_CHECK(interaction_script_bind(alternate,"interaction:training_reward"),"valid replacement of a speaker binding is explicit");bsl_host_conversation_probe(&talk);
  INTERACT_CHECK(!talk.open&&!bsl_host_conversation_locked(p->lifetime),"changed binding cancels its window and releases participant policy");stage=6;wait=0;
 }else if(stage==6){
  INTERACT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,63),"a bound speaker may queue a fresh authored outcome");
  INTERACT_CHECK(interaction_script_bind(alternate,"interaction:training_shop"),"replacement binding cancels the speaker's pending outcome ownership");stage=7;wait=0;
 }else if(stage==7){
  INTERACT_CHECK(bag.items==6,"cancelled rebind outcome does not leak another item");
  checkpoint_place(alternate,1000,floor_y);INTERACT_CHECK(!bsl_host_actor_interaction_nearest(p->lifetime,&distance),"actors outside authored ranges are not candidates");
  checkpoint_place(alternate,260,floor_y);alternate->a.vx=20;INTERACT_CHECK(!bsl_host_actor_interaction_nearest(p->lifetime,&distance),"moving actor readiness prevents an unsolicited action interruption");alternate->a.vx=0;
  start(alternate,"attack1",BOR_ACTION);INTERACT_CHECK(!bsl_host_actor_interaction_nearest(p->lifetime,&distance),"committed BOR action is not selected as a conversation candidate");checkpoint_place(alternate,260,floor_y);
  INTERACT_CHECK(bsl_host_interaction_dispatch(p->lifetime,0xcafe04,"missing:event",1)!=0&&bsl_host_interaction_dispatch(0,0,"",INFINITY)!=0,"invalid helper reference and absent player authority refuse");
  INTERACT_CHECK(interaction_script_bind(alternate,""),"empty binding explicitly detaches the interaction");INTERACT_CHECK(!bsl_host_actor_interaction_nearest(p->lifetime,&distance),"detached actor is not inferred from its previous recipe");
  INTERACT_CHECK(interaction_script_bind(alternate,"interaction:training_shop"),"detached living actor can be rebound explicitly");retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);
  INTERACT_CHECK(!bsl_host_actor_interaction_nearest(p->lifetime,&distance),"script-killed actor is excluded before native destruction/unbind");stage=8;wait=0;
 }else if(stage==8){
  INTERACT_CHECK(!*bsl_host_actor_interaction_definition(retired),"native retirement removes the old lifetime binding");retired=p->lifetime;bsl_engine_load_level("levels/teleport-annex.lev");stage=9;wait=0;
 }else if(stage==9){
  INTERACT_CHECK(p&&alternate&&p->lifetime!=retired&&!*bsl_host_actor_interaction_definition(p->lifetime)&&!*bsl_host_actor_interaction_definition(alternate->lifetime)&&bag.items==6,"fresh level actors do not inherit retired lifetime bindings while progress survives");
  floor_y=p->a.y;checkpoint_park_enemies();checkpoint_place(p,160,floor_y);checkpoint_place(alternate,260,floor_y);checkpoint_place(arcade,1000,floor_y);stage=10;wait=0;
 }else if(stage==10){
  INTERACT_CHECK(interaction_script_bind(alternate,"interaction:training_reward")&&bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,63),"fresh destination setup can bind and use the same authored interaction recipe");stage=11;wait=0;
 }else if(stage==11){
  INTERACT_CHECK(bag.items==7,"destination interaction continues using the same player inventory");checkpoint_place(p,120,floor_y);stage=12;wait=0;
 }else if(stage==12){
  INTERACT_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:annex")==0&&bsl_host_progress_save()==0,"interaction effects save through an existing explicit permanent activation");stage=99;
 }else if(stage==100){
  INTERACT_CHECK(bag.items==7&&!*bsl_host_actor_interaction_definition(alternate->lifetime),"fresh Continue restores progress without serializing actor interaction bindings");
  INTERACT_CHECK(interaction_script_bind(alternate,"interaction:training_shop")&&bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,2),"continued level setup can bind and dispatch a recipe shop purchase");stage=101;wait=0;
 }else if(stage==101){
  BslCommerceProbe funds={0};bsl_host_commerce_probe(&funds);INTERACT_CHECK(bag.items==8&&funds.balance==9&&funds.stock==3,"continued recipe purchase commits inventory, currency and stock once");stage=99;
 }
finish:
 if(stage==99){const char* path=getenv("BSL_JOURNEY_INTERACTIONS_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef INTERACT_CHECK
}
