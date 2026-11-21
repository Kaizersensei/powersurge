/* One connected generic-spawn/ownership/reward/lifecycle journey. Signed: Mech. */
static bool registry_init_again(HostActor* h,const char* definition){
 surgescript_object_t* controller=surgescript_objectmanager_get(surgescript_object_manager(h->entity),surgescript_object_child(h->entity,"BORController"));
 surgescript_var_t* value=surgescript_var_set_string(surgescript_var_create(),definition);const surgescript_var_t* args[]={value};
 surgescript_var_t* result=controller_init_character(controller,args,1);bool ok=surgescript_var_get_bool(result);surgescript_var_destroy(result);surgescript_var_destroy(value);return ok;
}
static void registry_character_test_tick(void){
 if(!getenv("BSL_REGISTRY_CHARACTER_TEST"))return;
 static int stage,wait,checks,failures;static uint64_t original;static HostActor *primary,*alternate;BslGameplayProbe bag={0};BslStatProbe xp={0};BslLearningProbe ability={0};BslCharacterDefinition d={0};
 if(++wait<60)return;
#define REG_CHECK(x,m) do{bool passed=(x);++checks;if(!passed)++failures;logfile_message("REGISTRY %s: %s",passed?"PASS":"FAIL",m);}while(0)
 if(stage==0){
  primary=hero();bsl_host_gameplay_probe(&bag);
  for(int i=0;i<64;i++)if(slots[i]&&bsl_host_character_definition("character:michio_alternate",&d)==0&&slots[i]->lifetime==d.actor)alternate=slots[i];
  REG_CHECK(primary&&alternate,"registry factory created both live character bodies");
  if(!primary||!alternate){stage=99;return;}
  original=primary->lifetime;
  REG_CHECK(!strcmp(surgescript_object_name(primary->entity),"BSL Registry Character")&&!strcmp(surgescript_object_name(alternate->entity),"BSL Registry Character"),"same generic entity class replaces character-specific wrappers");
  REG_CHECK(bsl_host_character_at(0,&d)&&!strcmp(d.model,"michio_sword")&&!strcmp(d.name,"Michio - Registry journey"),"registry metadata resolves authored identity, name and model");
  REG_CHECK(!bsl_host_character_at(64,&d)&&!d.actor&&!d.id[0],"out-of-range query clears output and returns no character");
  REG_CHECK(bsl_host_character_definition("character:missing",&d)==2&&!d.id[0],"unknown registry ID refuses without inventing a character");
  REG_CHECK(controlled(primary)&&!controlled(alternate)&&bag.items==3&&bag.health==83,"authored starting health and one stable player bag are used");
  primary->a.health=70;
  REG_CHECK(registry_init_again(primary,"character:michio")&&primary->a.health==70,"same-definition initialization is idempotent and preserves live HP");
  REG_CHECK(!registry_init_again(primary,"character:michio_alternate")&&controlled(primary)&&primary->a.health==70,"changing definition on a live controller refuses atomically");
  REG_CHECK(!registry_init_again(primary,"character:missing")&&controlled(primary),"invalid reinitialization cannot steal input authority");
  REG_CHECK(bsl_host_bind_character(primary->lifetime,"character:michio_alternate")!=0,"one actor cannot claim a second character identity");
  REG_CHECK(bsl_host_use_item("item:strawberry")==0&&primary->a.health==75,"inventory consumption heals the actual generic actor");
  bsl_host_gameplay_probe(&bag);REG_CHECK(bag.items==2,"one item transaction updates the stable player bag");
  REG_CHECK(bsl_host_select_character("character:michio_alternate")==0&&controlled(alternate)&&!controlled(primary),"safe character switching transfers exactly one input authority");
  REG_CHECK(alternate->a.health==90,"alternate starts from its own authored health");
  alternate->a.health=41;
  REG_CHECK(bsl_host_select_character("character:michio")==0&&primary->a.health==75,"switch back retains independent health and shared player ownership");
  bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&ability);
  REG_CHECK(xp.units==1&&xp.total==0&&!ability.learned,"generic actors use the existing progression and locked-ability services");
  REG_CHECK(bsl_host_reward(primary->lifetime,"source:lab_exploration","journey/first")==0,"registered event reward accepts the generic actor owner");
  REG_CHECK(bsl_host_reward(primary->lifetime,"source:lab_exploration","journey/first")==0,"duplicate reward returns its committed receipt");
  bsl_host_stat_probe("stat:experience",&xp);REG_CHECK(xp.total==2500,"replayed event reward does not duplicate XP");
  REG_CHECK(!bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump"),"assigned double-jump capability is locked before its XP threshold");
  REG_CHECK(player_jump(primary,0,false,false),"first jump remains available while double jump is locked");
  {double vy=primary->a.vy;int used=primary->jumps_used;REG_CHECK(!player_jump(primary,0,true,false)&&primary->a.vy==vy&&primary->jumps_used==used,"locked second jump changes neither velocity nor jump allowance");}
  bsl_engine_restart();stage=1;wait=0;
 }else if(stage==1){
  primary=hero();bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);
  REG_CHECK(primary&&primary->lifetime!=original&&!bsl_host_controlled(original),"actual level reload retires old actor authority and spawns new generic bodies");
  if(!primary){stage=99;return;}
  REG_CHECK(bag.health==75&&bag.items==2&&xp.total==2500,"health, inventory and progression survive actor reconstruction without reseeding");
  REG_CHECK(bsl_host_character_definition("character:michio_alternate",&d)==0&&d.actor&&d.actor!=original,"alternate has a fresh registered world binding");
  alternate=NULL;for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==d.actor)alternate=slots[i];
  if(!alternate){REG_CHECK(false,"replacement alternate is available for capability ownership check");stage=99;return;}
  REG_CHECK(bsl_host_select_character("character:michio_alternate")==0,"reconstructed alternate is safely controllable");
  bsl_host_gameplay_probe(&bag);REG_CHECK(bag.health==41&&bag.items==2,"alternate health and player bag survive real scene teardown");
  REG_CHECK(bsl_host_select_character("character:michio")==0,"return to the progressed character after reconstruction");
  REG_CHECK(bsl_host_reward(hero()->lifetime,"source:lab_exploration","journey/second")==0,"second registered objective reaches the authored growth threshold");
  bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&ability);
  REG_CHECK(xp.total==5000&&xp.units==2&&ability.learned,"XP, level and learned ability remain tied to the selected character");
  REG_CHECK(bsl_host_capability_allowed(hero()->lifetime,"traversal.double_jump"),"earned double-jump capability is routed to traversal");
  REG_CHECK(!bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"one character's earned traversal does not unlock another character");
  REG_CHECK(bsl_host_ability_use(hero()->lifetime,0)!=0,"passive capability cannot be invoked as an attack action");
  REG_CHECK(player_jump(hero(),0,false,false),"first jump remains available after unlock");
  {double vx=hero()->a.vx;REG_CHECK(player_jump(hero(),0,true,false)&&hero()->jumps_used==2&&!strcmp(hero()->a.animation->name,"doublejump")&&hero()->a.vx==vx,"learned second jump executes the real traversal and preserves horizontal momentum");}

  stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_REGISTRY_CHARACTER_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef REG_CHECK
}
