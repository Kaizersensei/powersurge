/* Real defeat -> rolled drop -> deliberate pickup -> rewind -> durable selected progress. Signed: Atlas. */
static bool loot_strike(HostActor* p,HostActor* target){
 // Grounded combat fixture: wait for an authored attack frame that really overlaps the grounded target.
 // The older progression helper centers boxes by lifting its target; unsuitable for testing floor pickups.
 unsigned serial=target->a.serial;int model=target->a.model_index;double sx=target->a.spawn_x,sy=target->a.spawn_y;
 bor_init(&target->a,bor_models+model,model);target->a.serial=serial;target->a.spawn_x=sx;target->a.spawn_y=sy;target->a.initialized=true;target->a.grounded=true;target->a.health=1;target->a.ai_wait=10000;target->placed=true;target->suspended=false;target->a.x=p->a.x+p->a.facing*34;target->a.y=p->a.y;commit_pose(target);rebase(target);render_frame(target);
 p->a.freeze=p->a.invulnerable=0;p->frozen_tick=false;p->a.partner=-1;p->a.grounded=true;p->action_channel=0;
 if(!start(p,"idle",BOR_FREE)||!start(p,"attack1",BOR_ACTION))return false;
 for(int i=0;i<180&&target->a.health>0&&!p->a.finished;++i)bor_advance_animation(&p->a,1.0/60,hit_segment,p);
 return target->a.health==0;
}
static bool loot_approach(HostActor* p,const BslLootView* drop,double floor_y){
 // The authored marker overlaps this test area. Stand on supported floor where enemy loot is actually nearer.
 double ax=0,ay=0;if(!bsl_bor_pickup_site(0xb51e01,&ax,&ay))return false;
 for(int offset=0;offset<=64;++offset)for(int direction=-1;direction<=1;direction+=2){double x=drop->x+direction*offset,distance=hypot(x-drop->x,floor_y-drop->y);
  if(distance<=drop->range&&distance+.25<hypot(x-ax,floor_y-ay)){checkpoint_place(p,x,floor_y);return true;}
 }
 logfile_message("LOOT cannot approach drop %.2f,%.2f range=%.2f from supported y=%.2f authored=%.2f,%.2f",drop->x,drop->y,drop->range,floor_y,ax,ay);return false;
}
static void loot_test_tick(void){
 const char* mode=getenv("BSL_LOOT_TEST");if(!mode)return;
 static int stage,wait,checks,failures;static HostActor *p,*red,*green,*arcade;static double floor_y;static uint32_t first_random;static uint64_t first_victim;
 if(++wait<60)return;
#define LOOT_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("LOOT %s: %s",ok?"PASS":"FAIL",m);}while(0)
 BslGameplayProbe bag={0};BslLootProbe loot={0};BslLootView drop={0};BslStatProbe xp={0};BslProgressProbe progress={0};p=hero();bsl_host_gameplay_probe(&bag);bsl_host_loot_probe(&loot);
 if(stage==0){
  for(int i=0;i<64;i++)if(slots[i]){HostActor* h=slots[i];if(!strcmp(h->a.model->name,"red_grunt")&&h->a.spawn_x<1000)red=h;if(!strcmp(h->a.model->name,"Green_Grunt")&&h->a.spawn_x<1000)green=h;}
  LOOT_CHECK(p&&red&&green&&loot.enabled,"live player, two enemies and loot service exist");if(!p||!red||!green){stage=99;return;}
  checkpoint_park_enemies();floor_y=p->a.y;
  if(!strcmp(mode,"read")){
   bsl_host_progress_probe(&progress);bsl_host_stat_probe("stat:experience",&xp);LOOT_CHECK(progress.loads==1&&bag.items==5&&xp.total==2500&&loot.outcomes==0,"fresh Continue retains collected bag and character XP, reconstructs fresh world without transient drops");
   p->a.health=50;LOOT_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==55,"continued character consumes collected loot through real BOR health");
   bsl_host_gameplay_probe(&bag);LOOT_CHECK(bag.items==4,"continued use consumes from the same player inventory");
   LOOT_CHECK(loot_strike(p,red)&&red->a.health==0,"fresh reconstructed enemy remains a real combat encounter");bsl_host_loot_probe(&loot);bsl_host_stat_probe("stat:experience",&xp);LOOT_CHECK(loot.pending==1&&loot.outcomes==1&&xp.total==5000,"fresh encounter has a new defeat identity and earns one new XP/drop result");stage=99;
  }else{LOOT_CHECK(bag.items==3&&loot.outcomes==0,"new run has its original bag and no fabricated drops");checkpoint_place(p,230,floor_y);stage=1;wait=0;}
 }else if(stage==1){
  p->a.health=75;LOOT_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"capture before combat holds bag, XP and empty loot journal");
  LOOT_CHECK(loot_strike(p,red)&&red->a.health==0,"actual accepted BOR hit defeats a real registered enemy");bsl_host_loot_probe(&loot);bsl_host_stat_probe("stat:experience",&xp);
  LOOT_CHECK(loot.outcomes==1&&loot.pending==1&&xp.total==2500&&bsl_host_loot_view(0,&drop)&&drop.quantity==2,"native defeat independently grants character XP and rolls the guaranteed two-item recipe");
  first_random=loot.random;first_victim=red->lifetime;bsl_host_combat_result(p->lifetime,p->a.action_id,red->lifetime,p->a.epoch,1,true,1);bsl_host_loot_probe(&loot);bsl_host_stat_probe("stat:experience",&xp);
  LOOT_CHECK(loot.outcomes==1&&loot.random==first_random&&xp.total==2500,"duplicate outcome cannot grant XP again, reroll or advance loot randomness");
  if(!bsl_host_loot_view(0,&drop)){stage=99;return;}LOOT_CHECK(loot_approach(p,&drop,floor_y),"supported approach selects enemy drop as nearest candidate");stage=2;wait=0;
 }else if(stage==2){
  bsl_host_gameplay_probe(&bag);LOOT_CHECK(bag.items==3&&loot.pending==1,"standing on an enemy drop does not automatically collect it");
  LOOT_CHECK(bsl_host_anchor_start(0xd001,"event:pickup_fill",p->lifetime)==0,"real inventory event sets up near-capacity bag");bsl_host_simulation_tick();bsl_host_gameplay_probe(&bag);LOOT_CHECK(bag.items==19,"near-capacity inventory is shared with authored and enemy pickups");
  pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);bsl_host_loot_probe(&loot);LOOT_CHECK(bag.items==19&&loot.pending==1&&loot.collected==0,"full bag refuses the complete drop without losing custody");
  LOOT_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==80,"consumption heals actual actor and creates sufficient stack space");pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);bsl_host_loot_probe(&loot);LOOT_CHECK(bag.items==18&&loot.pending==1,"held trigger does not silently retry after capacity becomes available");
  pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);bsl_host_loot_probe(&loot);LOOT_CHECK(bag.items==20&&loot.pending==0&&loot.collected==1,"fresh deliberate gesture atomically moves drop into the player's bag");
  pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);LOOT_CHECK(bag.items==20,"holding the collected drop gesture cannot duplicate items");pickup_input(p,false,false,false,false);
  LOOT_CHECK(bsl_host_checkpoint_request()==0,"queue rewind after enemy reward, consumption and pickup");stage=3;wait=0;
 }else if(stage==3){
  bsl_host_stat_probe("stat:experience",&xp);LOOT_CHECK(bag.items==3&&bag.health==75&&xp.total==0&&loot.outcomes==0&&red->a.health>0,"checkpoint rolls back HP, inventory, XP, defeat receipt and drop custody together");
  checkpoint_park_enemies();LOOT_CHECK(loot_strike(p,red),"rewound enemy can be defeated again");bsl_host_loot_probe(&loot);LOOT_CHECK(loot.pending==1&&loot.random==first_random&&bsl_host_loot_view(0,&drop)&&drop.victim==first_victim&&drop.quantity==2,"rewind restores RNG and permits the same rolled-back encounter result once");
  checkpoint_place(p,230,floor_y);stage=4;wait=0;
 }else if(stage==4){
  LOOT_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"capture with a live pending drop retains its transient custody");
  pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);pickup_input(p,false,false,false,false);bsl_host_gameplay_probe(&bag);bsl_host_loot_probe(&loot);
  LOOT_CHECK(bag.items==5&&bsl_host_progress_flag("switch:berries_claimed")&&loot.pending==1,"nearer authored pickup wins shared selection while a farther enemy drop remains pending");
  LOOT_CHECK(bsl_host_checkpoint_request()==0,"queue rollback of mixed authored/enemy pickup selection");stage=41;wait=0;
 }else if(stage==41){
  LOOT_CHECK(bag.items==3&&!bsl_host_progress_flag("switch:berries_claimed")&&loot.pending==1,"mixed selector rollback restores authored claim and pending enemy custody together");
  if(!bsl_host_loot_view(0,&drop)){LOOT_CHECK(false,"pending drop still exists before collection");stage=99;return;}
  LOOT_CHECK(loot_approach(p,&drop,floor_y),"supported floor approach reaches the pending enemy drop");p->a.grounded=true;pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);pickup_input(p,false,false,false,false);bsl_host_gameplay_probe(&bag);bsl_host_loot_probe(&loot);LOOT_CHECK(bag.items==5&&loot.pending==0&&loot.collected==1&&!bsl_host_progress_flag("switch:berries_claimed"),"nearer enemy drop collects while farther authored pickup remains unclaimed");
  p->a.health=50;bsl_host_use_item("item:strawberry");LOOT_CHECK(bsl_host_checkpoint_request()==0,"queue rewind after consuming a captured pending drop");stage=5;wait=0;
 }else if(stage==5){
  LOOT_CHECK(bag.items==3&&bag.health==75&&loot.pending==1&&loot.collected==0&&red->a.health==0,"same-level rewind restores the pending drop and pre-collection bag, retains prior enemy defeat");
  if(!bsl_host_loot_view(0,&drop)){stage=99;return;}LOOT_CHECK(loot_approach(p,&drop,floor_y),"supported floor approach reaches the pending enemy drop");p->a.grounded=true;pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);pickup_input(p,false,false,false,false);bsl_host_gameplay_probe(&bag);bsl_host_loot_probe(&loot);LOOT_CHECK(bag.items==5&&loot.pending==0&&loot.collected==1&&!bsl_host_progress_flag("switch:berries_claimed"),"restored pending enemy drop is collectible exactly once without claiming authored berries");checkpoint_place(p,120,floor_y);stage=6;wait=0;
 }else if(stage==6){
  LOOT_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&bsl_host_progress_save()==0,"permanent activation explicitly saves collected inventory and selected progress");
  loot_strike(p,green);bsl_host_loot_probe(&loot);LOOT_CHECK(loot.outcomes==2,"later enemy creates another remembered weighted outcome, including possible no-drop");
  p->a.health=40;bsl_host_use_item("item:strawberry");LOOT_CHECK(bsl_host_progress_load()==0,"fresh load queues after unsaved consumption and a later loot roll");stage=7;wait=0;
 }else if(stage==7){
  bsl_host_progress_probe(&progress);bsl_host_stat_probe("stat:experience",&xp);LOOT_CHECK(progress.loads==1&&bag.items==5&&bag.health==100&&xp.total==2500&&loot.outcomes==0,"same-process load keeps saved inventory and XP, discards unsaved rolls and reconstructs fresh enemies");
  // Fresh level reconstruction destroys all old HostActor pointers; resolve this level's lifetimes again.
  red=green=arcade=NULL;for(int i=0;i<64;i++)if(slots[i]){HostActor* h=slots[i];if(!strcmp(h->a.model->name,"red_grunt")&&h->a.spawn_x<1000)red=h;if(!strcmp(h->a.model->name,"Green_Grunt")&&h->a.spawn_x<1000)green=h;if(h->a.model==p->a.model&&h->a.spawn_x>420)arcade=h;}
  checkpoint_park_enemies();
  LOOT_CHECK(arcade&&red&&green,"fresh course retains no-progression character and new enemy lifetimes");if(!arcade||!red||!green){stage=99;return;}checkpoint_place(arcade,200,p->a.y);stage=8;wait=0;
 }else if(stage==8){
  LOOT_CHECK(bsl_host_select_character("character:michio_arcade")==0,"select no-reward profile through ordinary safe character-switch rules");p=hero();bsl_host_stat_probe("stat:experience",&xp);LOOT_CHECK(!xp.present,"arcade character has no experience stat or reward acceptance");
  LOOT_CHECK(loot_strike(p,red)&&red->a.health==0,"no-reward character defeats an actual eligible enemy");bsl_host_loot_probe(&loot);LOOT_CHECK(loot.outcomes==1&&loot.pending==1,"loot rolls even when the character declines the XP reward");checkpoint_place(p,120,floor_y);stage=9;wait=0;
 }else if(stage==9){
  LOOT_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0,"capture pending loot from a no-reward character defeat");p->a.health=50;bsl_host_use_item("item:strawberry");LOOT_CHECK(bsl_host_checkpoint_request()==0,"queue no-reward checkpoint rewind after later consumption");stage=10;wait=0;
 }else if(stage==10){
  bsl_host_stat_probe("stat:experience",&xp);LOOT_CHECK(red->a.health==0&&loot.pending==1&&loot.outcomes==1&&!xp.present&&bag.items==5&&bag.health==100,"rewind retains XP-independent native defeat and its pending drop without duplicating a living enemy");stage=99;
 }
 if(stage==99){const char* file=getenv("BSL_LOOT_REPORT");FILE* f=file?fopen(file,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef LOOT_CHECK
}
