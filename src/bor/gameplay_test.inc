/* Opt-in connected ownership fixture in real Surge actors/physics/HP. Signed: Atlas. */
static void gameplay_test_tick(void){
 if(!getenv("BSL_GAMEPLAY_TEST")||!bsl_gameplay_enabled())return;
 static int stage,checks,failures,wait;static uint64_t first_lifetime;static HostActor* first;static HostActor* second;
 if(++wait<45)return;
#define GAME_CHECK(x,m) do{bool passed=(x);checks++;if(!passed)failures++;logfile_message("GAMEPLAY %s: %s",passed?"PASS":"FAIL",m);}while(0)
 BslGameplayProbe p={0};bsl_host_gameplay_probe(&p);
 if(stage==0){
  for(int i=0;i<64;i++)if(slots[i]){if(controlled(slots[i]))first=slots[i];else if(bor_is_player(&slots[i]->a))second=slots[i];}
  GAME_CHECK(first&&second,"two file-bound characters share a single-player session");
  if(!first||!second){stage=99;return;}
  first_lifetime=first->lifetime;
  GAME_CHECK(bsl_host_anchor_start(0xabc0,"event:gameplay_wait",second->lifetime)==3,"inactive character cannot submit player interaction commands");
  GAME_CHECK(bsl_host_anchor_start(0xabcd,"event:gameplay_wait",first->lifetime)==0,"owned event starts on selected character");
  bsl_host_simulation_tick();BslAnchorRun run={0};bsl_host_anchor_probe(0xabcd,&run);
  GAME_CHECK(run.status==2,"selected-character event is waiting");
  bsl_host_hit(second->lifetime);bsl_host_anchor_probe(0xabcd,&run);
  GAME_CHECK(run.status==2,"inactive character hit cannot interrupt selected player's event");
  second->a.health=0;
  bsl_host_unbind(second->lifetime);bsl_host_anchor_probe(0xabcd,&run);
  GAME_CHECK(run.status==2,"inactive character detachment cannot cancel selected player's event");
  bsl_host_bind(second->lifetime,&second->a);bsl_host_bind_character(second->lifetime,"character:michio_alternate");
  GAME_CHECK(second->a.health==0&&second->a.state==BOR_DEAD,"zero-HP replacement cannot re-enter live free state");
  {unsigned serial=second->a.serial;int model=second->a.model_index;bool grounded=second->a.grounded;bor_init(&second->a,bor_models+model,model);second->a.serial=serial;second->a.grounded=grounded;rebase(second);}
  bsl_host_hit(first->lifetime);bsl_host_anchor_probe(0xabcd,&run);
  GAME_CHECK(run.status==4,"selected character hit still cancels its event continuation");

  GAME_CHECK(!strcmp(p.player,"player/1")&&!strcmp(p.character,"player/1/character:michio")&&p.items==3&&p.health==100,"authored initial state resolves to stable player and character IDs");
  GAME_CHECK(controlled(first)&&!controlled(second),"BOR player type alone does not give a second actor input authority");
  first->a.health=70;
  GAME_CHECK(bsl_host_use_item("item:strawberry")==0&&first->a.health==75,"owner item heals the actual currently bound BOR actor");
  bsl_host_gameplay_probe(&p);GAME_CHECK(p.items==2,"one consumption changes the player bag");
  GAME_CHECK(bsl_host_bind_character(first->lifetime,"character:michio")==0&&first->a.health==75,"repeated binding does not reset live health");
  GAME_CHECK(bsl_host_select_character("character:missing")==2&&controlled(first),"unknown selection does not disturb authority");
  first->a.state=BOR_ACTION;
  GAME_CHECK(bsl_host_select_character("character:michio_alternate")==3&&controlled(first),"committed action rejects switching");
  first->a.state=BOR_FREE;
  second->suspended=true;
  GAME_CHECK(bsl_host_select_character("character:michio_alternate")==3&&controlled(first),"suspended targets cannot become active against stale terrain coverage");
  second->suspended=false;second->placed=false;
  GAME_CHECK(bsl_host_select_character("character:michio_alternate")==3&&controlled(first),"unplaced target cannot take control");
  second->placed=true;
  GAME_CHECK(bsl_host_select_character("character:michio_alternate")==0&&controlled(second)&&!controlled(first),"standing safe-point selection transfers one input authority");
  second->a.health=40;
  GAME_CHECK(bsl_host_use_item("item:strawberry")==0&&second->a.health==45&&first->a.health==75,"effects follow selected character without overwriting the previous character");
  bsl_host_gameplay_probe(&p);GAME_CHECK(p.items==1,"switching retains one player-owned inventory");
  GAME_CHECK(bsl_host_select_character("character:michio")==0&&first->a.health==75,"switch back retains independent live character HP");
  stage=1;wait=0;
 }else if(stage==1){
  /* Restart really unloads/destroys and reconstructs actors; no synthetic rebind. */
  bsl_engine_restart();stage=2;wait=0;
 }else if(stage==2){
  first=hero();second=NULL;for(int i=0;i<64;i++)if(slots[i]&&slots[i]!=first&&bor_is_player(&slots[i]->a))second=slots[i];
  GAME_CHECK(first&&first->lifetime!=first_lifetime&&p.actor==first->lifetime,"level restart replaces the actual actor lifetime");
  GAME_CHECK(p.health==75&&p.items==1&&!strcmp(p.character,"player/1/character:michio"),"level reload preserves selected character health and player bag without reseeding");
  GAME_CHECK(second&&second->a.health==45,"detached alternate character state restores to its replacement actor");
  GAME_CHECK(!bsl_host_controlled(first_lifetime),"stale actor lifetime cannot retain control after replacement");
  GAME_CHECK(bsl_host_select_character("character:michio_alternate")==0,"replacement alternate can be selected safely");
  bsl_host_gameplay_probe(&p);GAME_CHECK(p.health==45&&p.items==1,"health effect and inventory routing survive reload plus switching");
  stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_GAMEPLAY_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef GAME_CHECK
}
