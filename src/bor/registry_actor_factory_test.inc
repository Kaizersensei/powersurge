/* Non-roster templates over real BOR bodies and gameplay services. Signed: Mech. */
static surgescript_object_t* actor_factory_service(void){
 surgescript_object_t* characters=placement_registry();if(!characters)return NULL;
 surgescript_objectmanager_t* manager=surgescript_object_manager(characters);surgescript_object_t* setup=surgescript_objectmanager_get(manager,surgescript_object_parent(characters));surgescript_objecthandle_t service=surgescript_object_child(setup,"BSL Actor Registry");return service&&surgescript_objectmanager_exists(manager,service)?surgescript_objectmanager_get(manager,service):NULL;
}
static HostActor* actor_factory_host(surgescript_objecthandle_t entity){for(int i=0;i<64;i++)if(slots[i]&&surgescript_object_handle(slots[i]->entity)==entity)return slots[i];return NULL;}
static void actor_factory_destroy(HostActor* actor){if(actor)surgescript_object_call_function(actor->entity,"destroy",NULL,0,NULL);}
static void registry_actor_factory_test_tick(void){
 const char* mode=getenv("BSL_JOURNEY_ACTORS");if(!mode)return;
 HostActor* p=hero();if(!p||bsl_host_boot_busy())return;
 static int stage,wait,checks,failures,initial_health;static double floor_y,quiet_x;static surgescript_objecthandle_t first,second,quiet,active;static uint64_t retired;
 if(++wait<45)return;
 surgescript_object_t* registry=actor_factory_service();HostActor* npc=actor_factory_host(first);HostActor* passive=actor_factory_host(quiet);HostActor* ai=actor_factory_host(active);BslGameplayProbe bag={0};BslCommerceProbe funds={0};bsl_host_gameplay_probe(&bag);bsl_host_commerce_probe(&funds);
#define ACTOR_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("ACTOR %s: %s",ok?"PASS":"FAIL",message);if(!ok){logfile_message("ACTOR stage=%d npc=%p passive=%p ai=%p hp=%d items=%d funds=%d",stage,(void*)npc,(void*)passive,(void*)ai,p->a.health,bag.items,funds.balance);stage=99;goto finish;}}while(0)
 if(stage==0){
  ACTOR_CHECK(registry!=NULL,"canonical setup retains the trusted non-roster actor factory");checkpoint_park_enemies();floor_y=p->a.y;initial_health=p->a.health;
  BslActorTemplate def={0};ACTOR_CHECK(bsl_host_actor_template_definition("actor:merchant",&def)&&def.passive&&!strcmp(def.model,"michio_sword")&&!bsl_host_actor_template_definition("actor:missing",&def),"template IDs resolve compiled BOR models and refuse unknown records");
  int count=factory_live_count();ACTOR_CHECK(!factory_body(registry,"create","actor:missing",260,9850)&&!factory_body(registry,"create","actor:merchant",NAN,9850)&&factory_live_count()==count,"unknown definition and nonfinite positions leave no native actor behind");
  ACTOR_CHECK(!factory_body(registry,"create","actor:merchant",p->a.x,p->a.y)&&factory_live_count()==count,"occupied player position refuses through existing exact spawn preflight");
  first=factory_body(registry,"create","actor:merchant",260,9850);npc=actor_factory_host(first);
  ACTOR_CHECK(npc&&npc->agency==1&&!strcmp(bsl_host_actor_template_for(npc->lifetime),"actor:merchant")&&!bsl_host_controlled(npc->lifetime),"registered template body initializes as an explicitly passive non-player actor");
  ACTOR_CHECK(!strcmp(bsl_host_actor_interaction_definition(npc->lifetime),"interaction:training_shop")&&npc->a.health==npc->a.model->health&&p->a.health==initial_health,"default interaction and independent BOR health do not alter player state");
  BslCharacterDefinition character={0};int roster_count=0;while(bsl_host_character_at((unsigned)roster_count,&character))++roster_count;
  ACTOR_CHECK(roster_count==3&&bsl_host_bind_character(npc->lifetime,"character:michio")!=0&&bsl_host_actor_template_bind(p->lifetime,"actor:merchant")!=0,"non-roster actor ownership cannot silently overwrite or become a player character");
  second=factory_body(registry,"create","actor:merchant",50,9850);HostActor* other=actor_factory_host(second);
  ACTOR_CHECK(other&&other!=npc&&other->lifetime!=npc->lifetime,"several actors share one immutable template with independent live state");
  npc->a.health=57;ACTOR_CHECK(other->a.health==other->a.model->health&&p->a.health==initial_health,"changing one instance health does not mutate another instance or the player");npc->a.health=npc->a.model->health;
  surgescript_var_t* id=surgescript_var_set_string(surgescript_var_create(),"actor:merchant");const surgescript_var_t* args[]={id};surgescript_var_t* result=surgescript_var_create();surgescript_object_call_function(npc->entity,"initialize",args,1,result);bool same=surgescript_var_get_bool(result);surgescript_var_set_string(id,"actor:quiet_ninja");surgescript_object_call_function(npc->entity,"initialize",args,1,result);bool changed=surgescript_var_get_bool(result);surgescript_var_destroy(id);surgescript_var_destroy(result);
  ACTOR_CHECK(same&&!changed&&!strcmp(bsl_host_actor_template_for(npc->lifetime),"actor:merchant"),"same-template initialization is idempotent and in-place model replacement refuses");
  retired=npc->lifetime;actor_factory_destroy(npc);actor_factory_destroy(other);first=second=0;stage=!strcmp(mode,"read")?100:1;wait=0;
  if(!strcmp(mode,"legacy")){ACTOR_CHECK(!*bsl_host_actor_template_for(p->lifetime),"legacy setup retains the same separate ownership boundary");stage=99;}
 }else if(stage==1){
  ACTOR_CHECK(!*bsl_host_actor_template_for(retired),"retirement removes actor-template and interaction ownership");
  surgescript_objecthandle_t object=factory_body(registry,"create","actor:training_object",260,9850);HostActor* helper=actor_factory_host(object);
  ACTOR_CHECK(helper&&helper->a.model->training_dummy&&helper->agency==1&&!bsl_host_controlled(helper->lifetime),"helper-style BOR models use the same factory without joining the roster");actor_factory_destroy(helper);stage=13;wait=0;
 }else if(stage==13){
  first=factory_body(registry,"create","actor:merchant",260,9850);npc=actor_factory_host(first);ACTOR_CHECK(npc&&bsl_host_actor_interaction_bind(npc->lifetime,"")==0,"reset fixture starts with a live template actor and explicit runtime detachment");retired=npc->lifetime;reset_all();stage=14;wait=0;
 }else if(stage==14){
  ACTOR_CHECK(npc&&npc->lifetime==retired&&npc->agency==1&&!strcmp(bsl_host_actor_template_for(retired),"actor:merchant")&&bsl_host_bind_character(retired,"character:michio")!=0,"new-game reset preserves body-owned template identity and ownership boundaries");
  ACTOR_CHECK(!strcmp(bsl_host_actor_interaction_definition(retired),"interaction:training_shop")&&bag.items==3&&p->a.health==initial_health,"fresh session restores the actor's default interaction without taking player progress ownership");actor_factory_destroy(npc);first=0;checkpoint_park_enemies();stage=2;wait=0;
 }else if(stage==2){
  quiet=factory_body(registry,"create","actor:quiet_ninja",260,9850);passive=actor_factory_host(quiet);ACTOR_CHECK(passive&&passive->agency==1&&!bor_is_player(&passive->a),"enemy-shaped models can explicitly use passive agency");quiet_x=passive->a.x;stage=3;wait=0;
 }else if(stage==3){
  ACTOR_CHECK(passive&&fabs(passive->a.x-quiet_x)<.1&&passive->a.state==BOR_FREE&&p->a.health==initial_health,"passive agency suppresses enemy decisions while source physics continues");checkpoint_place(p,120,floor_y);stage=4;wait=0;
 }else if(stage==4){
  ACTOR_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0,"normal permanent activation works with a passive template actor present");
  int base=0;ACTOR_CHECK(progression_strike(p,passive,false,passive->a.model->health,&base)&&passive->a.health<passive->a.model->health,"passive agency retains real BOR hurtboxes, damage and reactions");
  ACTOR_CHECK(bsl_host_checkpoint_request()==0,"existing rewind accepts a world containing a template actor");stage=5;wait=0;
 }else if(stage==5){
  ACTOR_CHECK(passive&&passive->agency==1&&passive->a.health==passive->a.model->health&&!strcmp(bsl_host_actor_template_for(passive->lifetime),"actor:quiet_ninja"),"rewind restores model health and retains agency/template association");actor_factory_destroy(passive);quiet=0;stage=6;wait=0;
 }else if(stage==6){
  active=factory_body(registry,"create","actor:active_ninja",260,9850);ai=actor_factory_host(active);ACTOR_CHECK(ai&&ai->agency==2,"model-AI agency is explicit rather than inferred from NPC naming");quiet_x=ai->a.x;stage=7;wait=0;
 }else if(stage==7){
  ACTOR_CHECK(ai&&(fabs(ai->a.x-quiet_x)>.1||ai->a.state!=BOR_FREE||ai->ai_phase!=0||ai->think_time>0),"model-AI actors execute the existing BOR enemy decision path");actor_factory_destroy(ai);active=0;checkpoint_place(p,160,floor_y);stage=8;wait=0;
 }else if(stage==8){
  first=factory_body(registry,"create","actor:merchant",260,9850);npc=actor_factory_host(first);ACTOR_CHECK(npc&&bsl_host_actor_interaction_bind(npc->lifetime,"")==0&&placed_script_reset(npc)&&!strcmp(bsl_host_actor_interaction_definition(npc->lifetime),"interaction:training_shop"),"dynamic interaction reset uses the template default when no placement override exists");stage=9;wait=0;
 }else if(stage==9){
  ACTOR_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,2),"created merchant dispatches its authored shop choice to player services");stage=10;wait=0;
 }else if(stage==10){
  ACTOR_CHECK(bag.items==4&&funds.balance==9&&funds.stock==3,"template merchant purchase commits player bag, funds and stock together");retired=npc->lifetime;bsl_engine_load_level("levels/teleport-annex.lev");stage=11;wait=0;
 }else if(stage==11){
  ACTOR_CHECK(!*bsl_host_actor_template_for(retired)&&bag.items==4&&funds.balance==9,"scene travel retires dynamic actors while preserving player progress");checkpoint_park_enemies();floor_y=p->a.y;first=factory_body(registry,"create","actor:merchant",260,9850);npc=actor_factory_host(first);
  ACTOR_CHECK(npc&&npc->lifetime!=retired&&npc->a.health==npc->a.model->health,"destination explicitly creates a fresh body from the same registry template");npc->a.health=55;checkpoint_place(p,120,floor_y);stage=12;wait=0;
 }else if(stage==12){
  ACTOR_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:annex")==0&&bsl_host_progress_save()==0,"compact save records player progress without dynamic actor health/lifetimes");stage=99;
 }else if(stage==100){
  ACTOR_CHECK(bag.items==4&&funds.balance==9&&funds.stock==3,"fresh Continue retains intended player economy/progress state");first=factory_body(registry,"create","actor:merchant",260,9850);npc=actor_factory_host(first);
  ACTOR_CHECK(npc&&npc->a.health==100&&!bsl_host_controlled(npc->lifetime),"continued caller creates a fresh body rather than loading prior transient NPC health");checkpoint_park_enemies();floor_y=p->a.y;checkpoint_place(p,160,floor_y);stage=101;wait=0;
 }else if(stage==101){
  ACTOR_CHECK(bsl_host_interaction_dispatch(p->lifetime,0,"",INFINITY)==0&&conversation_script_choose(p,2),"freshly created continued actor can dispatch another shop transaction");stage=102;wait=0;
 }else if(stage==102){
  ACTOR_CHECK(bag.items==5&&funds.balance==6&&funds.stock==2,"continued actor transaction commits once without replaying the earlier purchase");stage=99;
 }
finish:
 if(stage==99){const char* path=getenv("BSL_JOURNEY_ACTORS_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef ACTOR_CHECK
}
