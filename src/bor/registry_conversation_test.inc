/* Actor-owned dialogue uses the real UI policy, native Events and BOR actor lifetimes. Signed: Mech. */
static bool conversation_script_open(HostActor* speaker,HostActor* initiator,const char* definition,double range,double factor){
 surgescript_var_t* actor=surgescript_var_set_objecthandle(surgescript_var_create(),surgescript_object_handle(initiator->entity));
 surgescript_var_t* id=surgescript_var_set_string(surgescript_var_create(),definition);surgescript_var_t* radius=surgescript_var_set_number(surgescript_var_create(),range);surgescript_var_t* time=surgescript_var_set_number(surgescript_var_create(),factor);const surgescript_var_t* args[]={actor,id,radius,time};surgescript_var_t* result=surgescript_var_create();
 surgescript_object_call_function(speaker->entity,"beginConversation",args,4,result);bool accepted=surgescript_var_get_bool(result);surgescript_var_destroy(actor);surgescript_var_destroy(id);surgescript_var_destroy(radius);surgescript_var_destroy(time);surgescript_var_destroy(result);return accepted;
}
static bool conversation_script_choose(HostActor* initiator,int choice){
 surgescript_var_t* index=surgescript_var_set_number(surgescript_var_create(),choice);const surgescript_var_t* args[]={index};surgescript_var_t* result=surgescript_var_create();surgescript_object_call_function(initiator->entity,"chooseConversation",args,1,result);bool accepted=surgescript_var_get_bool(result);surgescript_var_destroy(index);surgescript_var_destroy(result);return accepted;
}
static bool talk_capture_pending,talk_capture_done;
void bsl_bor_actor_conversation_test_render(void){
 if(!talk_capture_pending)return;
 BslUiProbe ui={0};bsl_ui_probe(&ui);if(ui.dialogue<1)return;
 const char* path=getenv("BSL_TALK_CAPTURE");if(path){ALLEGRO_STATE files;al_store_state(&files,ALLEGRO_STATE_NEW_FILE_INTERFACE);al_set_standard_file_interface();image_save(video_get_backbuffer(),path);al_restore_state(&files);}
 talk_capture_pending=false;talk_capture_done=true;
}
static void registry_conversation_test_tick(void){
 const char* mode=getenv("BSL_JOURNEY_CONVERSATION");if(!mode)return;
 static int stage,wait,checks,failures;static double calendar_before,simulation_before,floor_y;static uint64_t retired;static int before_items,initial_balance,initial_stock;
 if(++wait<45)return;
 HostActor* p=hero();HostActor* alternate=journey_character("character:michio_alternate");
 BslGameplayProbe bag={0};BslCalendarProbe clock={0};BslConversationProbe talk={0};bsl_host_gameplay_probe(&bag);bsl_host_calendar_probe(&clock);bsl_host_conversation_probe(&talk);
 surgescript_object_t* registry=placement_registry();
#define TALK_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("TALK %s: %s",ok?"PASS":"FAIL",message);if(!ok){logfile_message("TALK observed stage=%d open=%d state=%d speaker=%llu initiator=%llu run=%d hp=%d items=%d factor=%.3f",stage,talk.open,talk.state,(unsigned long long)talk.speaker,(unsigned long long)talk.initiator,talk.run_status,p?p->a.health:0,bag.items,clock.factor);stage=99;goto finish;}}while(0)
 if(stage==0){
  TALK_CHECK(p&&alternate&&registry,"real registered player and separate actor speaker are available");BslCommerceProbe funds={0};bsl_host_commerce_probe(&funds);initial_balance=funds.balance;initial_stock=funds.stock;checkpoint_park_enemies();floor_y=p->a.y;checkpoint_place(p,200,floor_y);checkpoint_place(alternate,380,floor_y);
  if(!strcmp(mode,"read")){TALK_CHECK(bag.items==5&&bsl_host_progress_variable("variable:training_choices")==1,"fresh Continue retains the prior actor conversation reward, purchase and receipts");stage=101;}else stage=1;wait=0;
 }else if(stage==1){
  floor_y=p->a.y;
  TALK_CHECK(!talk.open&&clock.factor==1,"initial dialogue has no stale participant or calendar policy");
  TALK_CHECK(bsl_host_conversation_open(p->lifetime,alternate->lifetime,"missing:dialogue",256,.1)!=0&&bsl_host_conversation_open(p->lifetime,alternate->lifetime,"dialogue:training_choice",NAN,.1)!=0&&bsl_host_conversation_open(p->lifetime,alternate->lifetime,"dialogue:training_choice",256,2)!=0,"unknown definition/nonfinite range/invalid clock factor refuse without opening");
  TALK_CHECK(bsl_host_conversation_open(p->lifetime,alternate->lifetime,"dialogue:training_choice",32,.1)!=0&&bsl_host_conversation_open(alternate->lifetime,p->lifetime,"dialogue:training_choice",256,.1)!=0,"distance and initiating-player authority are enforced");
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:training_choice",256,.1),"trusted actor script opens a registered conversation through the shared native service");calendar_before=clock.elapsed;simulation_before=clock.simulation;stage=2;wait=0;
 }else if(stage==2){
  TALK_CHECK(talk.open&&talk.actor_owned&&talk.initiator==p->lifetime&&talk.speaker==alternate->lifetime&&!strcmp(talk.player,"player/1")&&!strcmp(talk.character,"player/1/character:michio"),"conversation captures initiating player/character independently of speaker identity");
  TALK_CHECK(!clock.paused&&clock.factor==.1&&clock.simulation>simulation_before&&clock.elapsed>calendar_before&&fabs((clock.elapsed-calendar_before)-.1*(clock.simulation-simulation_before))<.05,"world simulation continues while only the calendar uses its authored conversation rate");
  TALK_CHECK(bsl_host_conversation_locked(p->lifetime)&&bsl_host_conversation_locked(alternate->lifetime)&&bsl_host_input_locked(p->lifetime)&&bsl_host_input_locked(alternate->lifetime),"explicit participant action/input gates are owned by the conversation");
  TALK_CHECK(!factory_select(registry,"character:michio_alternate")&&!conversation_script_choose(alternate,0)&&bsl_host_conversation_choose(p->lifetime,99)!=0,"speaker cannot choose as player, invalid choice stays open and safe selection refuses while talking");
  BslOwnedUiProbe ui={0};bsl_ui_actor_dialogue_probe(&ui);v2d_t screen=video_get_screen_size();
  TALK_CHECK(ui.owned&&!ui.compact&&ui.w<(int)screen.x&&ui.h<(int)screen.y&&ui.x>=0&&ui.y>=0&&ui.x+ui.w<=screen.x&&ui.y+ui.h<=screen.y,"roomier single-player actor layout stays localized and inside the screen");
  bsl_ui_actor_dialogue_context(true,alternate->a.x,alternate->a.y-alternate->a.model->soft_height,true,0,0,960,540);bsl_ui_actor_dialogue_probe(&ui);
  TALK_CHECK(ui.owned&&ui.compact&&ui.x>=0&&ui.y>=0&&ui.x+ui.w<=960&&ui.y+ui.h<=540,"compact player-local layout fits its supplied viewport bounds");
  talk_capture_pending=true;stage=201;wait=0;
 }else if(stage==201&&talk_capture_done){
  TALK_CHECK(bsl_host_conversation_cancel(alternate->lifetime)==0,"speaker may explicitly cancel its own conversation");stage=3;wait=0;
 }else if(stage==3){
  TALK_CHECK(!talk.open&&clock.factor==1&&!bsl_host_conversation_locked(p->lifetime)&&!bsl_host_conversation_locked(alternate->lifetime)&&bag.items==3,"cancellation releases participant/time policy without dispatching an outcome");
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1)&&conversation_script_choose(p,63),"selected choice enters the same native outcome runner used by the existing UI");
  bsl_host_conversation_probe(&talk);TALK_CHECK(!talk.open&&talk.run_status==0&&!strcmp(talk.character,"player/1/character:michio")&&!strncmp(talk.owner,"actor/",6),"queued outcome retains captured player/character and actor-lifetime owner");stage=4;wait=0;
 }else if(stage==4){
  TALK_CHECK(talk.run_status==3&&bag.items==4&&bsl_host_progress_flag("switch:training_reward")&&bsl_host_progress_variable("variable:training_choices")==1,"authored item/switch/variable outcome commits to the stable player's services");
  bsl_host_hit(alternate->lifetime);bsl_host_conversation_probe(&talk);TALK_CHECK(talk.run_status==3&&bag.items==4,"later speaker interruption does not undo committed effects");
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:training_choice",256,.1)&&conversation_script_choose(p,2),"actor-owned shop choice queues the existing authored purchase event");stage=41;wait=0;
 }else if(stage==41){
  BslCommerceProbe funds={0};bsl_host_commerce_probe(&funds);
  TALK_CHECK(talk.run_status==3&&bag.items==5&&funds.balance==initial_balance-3&&funds.stock==initial_stock-1,"actor shop dispatch commits player inventory, currency and stock together");
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1)&&conversation_script_choose(p,63),"another explicit interaction creates a distinct queued outcome");bsl_host_hit(alternate->lifetime);stage=5;wait=0;
 }else if(stage==5){
  TALK_CHECK(talk.run_status==4&&bag.items==5&&bsl_host_progress_variable("variable:training_choices")==1,"speaker hit cancels queued outcome before any new effects execute");
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1)&&conversation_script_choose(p,63),"outcome ownership is established before removing its speaker");
  retired=alternate->lifetime;surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);stage=6;wait=0;
 }else if(stage==6){
  TALK_CHECK(!alternate&&!talk.open&&talk.run_status==4&&bag.items==5&&clock.factor==1&&!bsl_host_conversation_locked(retired),"speaker removal before dispatch leaves no reward, body or conversation lock");
  TALK_CHECK(factory_body(registry,"create","character:michio_alternate",380,9850)!=0,"speaker can be explicitly recreated through the ordinary registry");stage=7;wait=0;
 }else if(stage==7){
  TALK_CHECK(alternate&&alternate->lifetime!=retired,"replacement speaker has a fresh lifetime");checkpoint_place(alternate,380,floor_y);stage=8;wait=0;
 }else if(stage==8){
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1),"conversation binds to the replacement rather than a stale actor handle");
  HostActor* enemy=equipment_actor("Green_Grunt");int base=0;TALK_CHECK(enemy&&checkpoint_strike(enemy,alternate,true,80,&base)&&alternate->a.health<80&&alternate->a.health>0,"a real third-party BOR hit applies damage to the inactive speaker");stage=9;wait=0;
 }else if(stage==9){
  TALK_CHECK(!talk.open&&clock.factor==1&&bag.items==5,"real speaker damage interrupts dialogue without awarding its unchosen outcome");checkpoint_park_enemies();checkpoint_place(p,200,floor_y);checkpoint_place(alternate,380,floor_y);stage=10;wait=0;
 }else if(stage==10){
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1),"participants can speak again after reaction cleanup");
  HostActor* enemy=equipment_actor("Green_Grunt");int base=0;TALK_CHECK(enemy&&checkpoint_strike(enemy,p,true,80,&base)&&p->a.health<80&&p->a.health>0,"a real BOR hit applies to the initiating player while the world runs");stage=11;wait=0;
 }else if(stage==11){
  TALK_CHECK(!talk.open&&clock.factor==1&&!bsl_host_conversation_locked(p->lifetime),"initiator hit releases dialogue ownership and time/input modifiers");checkpoint_park_enemies();checkpoint_place(p,200,floor_y);checkpoint_place(alternate,380,floor_y);stage=12;wait=0;
 }else if(stage==12){
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1),"range-monitor exercise starts from a valid live conversation");checkpoint_place(alternate,1000,floor_y);stage=13;wait=0;
 }else if(stage==13){
  TALK_CHECK(!talk.open&&clock.factor==1&&bag.items==5,"separating participants closes the window and policy without granting an outcome");checkpoint_place(alternate,380,floor_y);stage=14;wait=0;
 }else if(stage==14){
  HostActor* red=equipment_actor("red_grunt");TALK_CHECK(red!=NULL,"non-roster BOR actor is available as an explicit scripted speaker");checkpoint_place(red,330,floor_y);red->a.ai_wait=10000;stage=15;wait=0;
 }else if(stage==15){
  HostActor* red=equipment_actor("red_grunt");TALK_CHECK(red&&bsl_host_conversation_open(p->lifetime,red->lifetime,"dialogue:training_choice",256,.1)==0,"ordinary non-roster actor can participate without becoming a player character");red->a.ai_wait=0;stage=16;wait=0;
 }else if(stage==16){
  HostActor* red=equipment_actor("red_grunt");TALK_CHECK(talk.open&&red&&red->a.state==BOR_FREE&&bsl_host_conversation_locked(red->lifetime)&&!bsl_host_conversation_locked(alternate->lifetime),"only explicit participants lose action agency; speaker AI cannot start an attack mid-conversation");
  TALK_CHECK(bsl_host_conversation_cancel(p->lifetime)==0,"initiator can end a non-roster actor conversation");red->a.ai_wait=10000;checkpoint_park_enemies();stage=17;wait=0;
 }else if(stage==17){
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1)&&conversation_script_choose(p,63),"queued outcome captures context before a later safe character switch");
  TALK_CHECK(factory_select(registry,"character:michio_alternate"),"normal safe switching resumes after the choice closes the window");stage=18;wait=0;
 }else if(stage==18){
  TALK_CHECK(p==alternate&&talk.run_status==4&&bag.items==5&&!strcmp(talk.character,"player/1/character:michio"),"switch cancellation prevents redirecting the old character's pending outcome");
  TALK_CHECK(factory_select(registry,"character:michio"),"player may return to the original character without transferring inventory");stage=19;wait=0;
 }else if(stage==19){
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1),"scene-change cleanup starts with an owned open conversation");before_items=bag.items;retired=p->lifetime;bsl_engine_load_level("levels/teleport-annex.lev");stage=20;wait=0;
 }else if(stage==20){
  TALK_CHECK(p&&p->lifetime!=retired&&!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&!talk.open&&!talk.actor_owned&&clock.factor==1&&bag.items==before_items,"scene teardown retires participant context and policies while player progress survives");checkpoint_place(p,120,floor_y);stage=21;wait=0;
 }else if(stage==21){
  TALK_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:annex")==0&&bsl_host_progress_save()==0,"actor conversation effects join an explicit permanent activation and compact save");stage=99;
 }else if(stage==101){
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:training_choice",256,.1)&&bsl_host_conversation_cancel(p->lifetime)==0,"fresh process matches the prior interaction counter before testing receipt independence");
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:choice_list",256,.1)&&conversation_script_choose(p,63),"fresh process can start and choose another actor conversation");stage=102;wait=0;
 }else if(stage==102){
  TALK_CHECK(talk.run_status==3&&bag.items==6&&bsl_host_progress_variable("variable:training_choices")==2,"new process conversation identity cannot replay an old saved reward receipt at the same counter");
  TALK_CHECK(conversation_script_open(alternate,p,"dialogue:training_choice",256,.1)&&conversation_script_choose(p,2),"continued actor-owned shop can initiate another purchase at the prior interaction counter");stage=103;wait=0;
 }else if(stage==103){
  BslCommerceProbe funds={0};bsl_host_commerce_probe(&funds);
  TALK_CHECK(talk.run_status==3&&bag.items==7&&funds.balance==initial_balance-3&&funds.stock==initial_stock-1,"fresh shop purchase commits once instead of replaying the saved transaction");stage=99;
 }
finish:
 if(stage==99){const char* path=getenv("BSL_JOURNEY_CONVERSATION_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef TALK_CHECK
}
