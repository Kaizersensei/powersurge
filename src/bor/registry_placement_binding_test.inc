/* Stable authored placement ownership over real level/actor lifecycles. Signed: Mech. */
static HostActor* placed_fixture_actor(void){
 surgescript_object_t* entity=level_get_entity_by_id("6d65636800001501");if(!entity)return NULL;
 uint64_t id=bsl_bor_entity_actor(surgescript_object_handle(entity));for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==id)return slots[i];return NULL;
}
static bool placed_script_reset(HostActor* actor){surgescript_var_t* result=surgescript_var_create();surgescript_object_call_function(actor->entity,"resetInteractionBinding",NULL,0,result);bool ok=surgescript_var_get_bool(result);surgescript_var_destroy(result);return ok;}
static void registry_placement_binding_test_tick(void){
 const char* mode=getenv("BSL_JOURNEY_PLACED");if(!mode)return;
 static int stage,wait,checks,failures;static unsigned attempts;static uint64_t retired;static double floor_y;
 HostActor* p=hero();if(!p||bsl_host_boot_busy())return;
 if(++wait<45)return;
 HostActor* npc=placed_fixture_actor();BslActorPlacementProbe binding={0};BslGameplayProbe bag={0};BslConversationProbe talk={0};BslCommerceProbe funds={0};bsl_host_actor_placement_probe(0,&binding);bsl_host_gameplay_probe(&bag);bsl_host_conversation_probe(&talk);bsl_host_commerce_probe(&funds);
#define PLACED_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("PLACED %s: %s",ok?"PASS":"FAIL",message);if(!ok){logfile_message("PLACED observed stage=%d status=%d actor=%llu npc=%p items=%d funds=%d attempts=%u",stage,binding.status,(unsigned long long)binding.actor,(void*)npc,bag.items,funds.balance,binding.attempts);logfile_message("PLACED dialogue open=%d state=%d player=(%.1f,%.1f) speaker=(%.1f,%.1f)",talk.open,talk.state,p?p->a.x:0,p?p->a.y:0,npc?npc->a.x:0,npc?npc->a.y:0);stage=99;goto finish;}}while(0)
 if(stage==0){
  PLACED_CHECK(p&&bsl_host_actor_placement_count()==1&&!strcmp(binding.placement,"6d65636800001501"),"current registered level exposes one stable authored placement binding");
  if(!strcmp(mode,"missing")||!strcmp(mode,"wrong")){
   PLACED_CHECK(!binding.actor&&binding.status==(!strcmp(mode,"missing")?BSL_ACTOR_PLACEMENT_MISSING:BSL_ACTOR_PLACEMENT_WRONG_OBJECT),"missing or wrong-type placement is diagnosed without guessing another actor");
   double distance;PLACED_CHECK(!bsl_host_actor_interaction_nearest(p->lifetime,&distance)&&bag.items==3,"failed placement binding creates no recipient or progress effects");stage=99;goto finish;
  }
  if(!strcmp(mode,"late")){
   PLACED_CHECK(!binding.actor&&binding.status==BSL_ACTOR_PLACEMENT_NO_CONTROLLER&&!npc,"matching placement without initialized BOR body remains explicitly pending");
   surgescript_object_t* entity=level_get_entity_by_id("6d65636800001501");surgescript_object_call_function(entity,"initialize",NULL,0,NULL);stage=100;wait=0;goto finish;
  }
  PLACED_CHECK(npc&&binding.status==BSL_ACTOR_PLACEMENT_BOUND&&binding.actor==npc->lifetime&&!strcmp(bsl_host_actor_interaction_definition(npc->lifetime),"interaction:training_shop"),"persistent placed actor receives its authored recipe without a polling script");
  PLACED_CHECK(!bsl_host_controlled(npc->lifetime)&&bsl_host_actor_placement_reset(p->lifetime)!=0,"non-roster speaker does not become a player and unbound actors cannot reset a placement");
  attempts=binding.attempts;retired=npc->lifetime;checkpoint_park_enemies();floor_y=p->a.y;checkpoint_place(p,160,floor_y);checkpoint_place(npc,260,floor_y);stage=!strcmp(mode,"read")?200:1;wait=0;
 }else if(stage==1){
  PLACED_CHECK(binding.attempts==attempts,"resolved placement stops probing instead of reapplying the authored profile each tick");
  PLACED_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0,"placed speaker participates in the shared single-player interaction dispatcher");bsl_host_conversation_probe(&talk);
  PLACED_CHECK(talk.open&&talk.speaker==npc->lifetime&&!strcmp(talk.player,"player/1")&&!strcmp(talk.character,"player/1/character:michio"),"placed non-roster conversation captures the initiating player and character");
  BslOwnedUiProbe legacy={0},full={0},small={0};bsl_ui_actor_dialogue_context(false,NAN,NAN,false,0,0,0,0);bsl_ui_actor_dialogue_probe(&legacy);bsl_ui_actor_dialogue_context(true,npc->a.x,npc->a.y-npc->a.model->soft_height,false,0,0,0,0);bsl_ui_actor_dialogue_probe(&full);
  PLACED_CHECK(full.body_pixels<legacy.body_pixels&&full.footer_pixels<legacy.footer_pixels,"owned windows reduce text against the authored full-window font reference at the same viewport");v2d_t screen=video_get_screen_size();bsl_ui_actor_dialogue_context(true,npc->a.x,npc->a.y-npc->a.model->soft_height,false,0,0,(int)(screen.x*.5),(int)(screen.y*.5));bsl_ui_actor_dialogue_probe(&small);
  PLACED_CHECK(small.text_scale<full.text_scale&&small.body_pixels<full.body_pixels&&small.title_pixels<full.title_pixels&&small.footer_pixels<full.footer_pixels,"reduced viewport scales body, title and choice-footer fonts with its windows");bsl_ui_actor_dialogue_context(true,npc->a.x,npc->a.y-npc->a.model->soft_height,true,0,0,0,0);bsl_ui_actor_dialogue_probe(&small);
  PLACED_CHECK(small.text_scale<full.text_scale&&small.body_pixels<full.body_pixels,"compact authored window geometry proportionally reduces text at the same viewport");bsl_ui_actor_dialogue_context(true,npc->a.x,npc->a.y-npc->a.model->soft_height,false,0,0,0,0);
  talk_capture_done=false;talk_capture_pending=true;stage=11;wait=0;
 }else if(stage==11&&talk_capture_done){
  PLACED_CHECK(conversation_script_choose(p,2),"placed actor's authored choice queues the existing shop purchase");stage=2;wait=0;
 }else if(stage==2){
  PLACED_CHECK(bag.items==4&&funds.balance==9&&funds.stock==3,"placed interaction commits player inventory, funds and stock together");
  PLACED_CHECK(bsl_host_actor_interaction_bind(npc->lifetime,"interaction:training_reward")==0,"runtime profile override is accepted independently of authored placement defaults");stage=3;wait=0;
 }else if(stage==3){
  PLACED_CHECK(!strcmp(bsl_host_actor_interaction_definition(npc->lifetime),"interaction:training_reward")&&binding.attempts==attempts&&!strcmp(binding.interaction,"interaction:training_shop"),"runtime override survives later updates while authored default remains available");
  PLACED_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,63),"overridden interaction can queue a registered outcome");
  PLACED_CHECK(placed_script_reset(npc),"explicit actor reset restores its authored placement interaction through the native component");stage=4;wait=0;
 }else if(stage==4){
  PLACED_CHECK(bag.items==4&&talk.run_status==4&&!strcmp(bsl_host_actor_interaction_definition(npc->lifetime),"interaction:training_shop"),"reset cancels pending override outcome without leaking rewards or undoing the earlier purchase");
  PLACED_CHECK(bsl_host_actor_interaction_bind(npc->lifetime,"")==0,"runtime detachment is a valid explicit override");stage=5;wait=0;
 }else if(stage==5){
  PLACED_CHECK(!*bsl_host_actor_interaction_definition(npc->lifetime)&&binding.actor==npc->lifetime&&binding.attempts==attempts,"empty override stays detached until reset; placement resolver does not silently restore it");
  PLACED_CHECK(placed_script_reset(npc),"detached placed actor can explicitly restore the authored interaction");retired=npc->lifetime;surgescript_object_call_function(npc->entity,"destroy",NULL,0,NULL);stage=6;wait=0;
 }else if(stage==6){
  PLACED_CHECK(!npc&&!binding.actor&&binding.status==BSL_ACTOR_PLACEMENT_MISSING&&!*bsl_host_actor_interaction_definition(retired),"removed placement loses its live binding and reports the absent owner");
  bsl_engine_load_level("levels/teleport-annex.lev");stage=7;wait=0;
 }else if(stage==7){
  PLACED_CHECK(npc&&npc->lifetime!=retired&&binding.status==BSL_ACTOR_PLACEMENT_BOUND&&!strcmp(binding.id,"actor-binding:annex_merchant")&&bag.items==4,"destination's same numeric placement ID resolves its own fresh actor/default while progress survives");
  checkpoint_park_enemies();floor_y=p->a.y;checkpoint_place(p,160,floor_y);checkpoint_place(npc,260,floor_y);stage=8;wait=0;
 }else if(stage==8){
  PLACED_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&bsl_host_conversation_cancel(p->lifetime)==0,"fresh destination placement is immediately usable without a demo binding consumer");checkpoint_place(p,120,floor_y);stage=9;wait=0;
 }else if(stage==9){
  PLACED_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:annex")==0&&bsl_host_progress_save()==0,"placed interaction effects join the normal explicit permanent save");stage=99;
 }else if(stage==100){
  PLACED_CHECK(npc&&binding.actor==npc->lifetime&&binding.status==BSL_ACTOR_PLACEMENT_BOUND&&!strcmp(bsl_host_actor_interaction_definition(npc->lifetime),"interaction:training_shop"),"delayed BOR construction resolves the original stable placement automatically");attempts=binding.attempts;stage=101;wait=0;
 }else if(stage==101){
  PLACED_CHECK(binding.attempts==attempts,"delayed binding also stops retry work after success");stage=99;
 }else if(stage==200){
  PLACED_CHECK(bag.items==4&&funds.balance==9&&funds.stock==3&&!strcmp(binding.id,"actor-binding:annex_merchant"),"fresh Continue restores progress and reconstructs the authored destination binding independently");
  PLACED_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,2),"continued placed actor can perform another ordinary shop interaction");stage=201;wait=0;
 }else if(stage==201){
  PLACED_CHECK(bag.items==5&&funds.balance==6&&funds.stock==2,"continued placed purchase commits once without replaying the older receipt");stage=99;
 }
finish:
 if(stage==99){const char* path=getenv("BSL_JOURNEY_PLACED_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef PLACED_CHECK
}
