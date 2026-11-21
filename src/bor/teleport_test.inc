/* Connected same-level, cross-level, recovery and fresh Continue journey. Signed: Atlas. */
static void teleport_test_tick(void){
 const char* mode=getenv("BSL_TELEPORT_TEST");if(!mode)return;static int stage,wait,checks,failures;static uint64_t old_actor;static double old_y;
 if(++wait<60)return;
 HostActor* p=hero();BslTeleportProbe tr={0};BslGameplayProbe bag={0};BslCommerceProbe shop={0};BslCheckpointProbe cp={0};BslStatProbe xp={0};BslQuestProbe quest={0};BslEquipmentProbe eq={0};BslProgressProbe progress={0};
 bsl_host_teleport_probe(&tr);bsl_host_gameplay_probe(&bag);bsl_host_commerce_probe(&shop);bsl_host_checkpoint_probe(&cp);bsl_host_stat_probe("stat:experience",&xp);bsl_host_quest_probe("quest:training_supplies",&quest);bsl_host_equipment_probe("stat:armed",&eq);bsl_host_progress_probe(&progress);
#define TR_CHECK(expr,m) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("TELEPORT %s: %s",ok?"PASS":"FAIL",m);if(!ok)logfile_message("TELEPORT observed stage=%d level=%s actor=%llu hp=%d x=%.1f y=%.1f coins=%d stock=%d bag=%d xp=%llu arrivals=%u failures=%u checkpoint=%s",stage,bsl_engine_level_file(),(unsigned long long)(p?p->lifetime:0),p?p->a.health:0,p?p->a.x:0,p?p->a.y:0,shop.balance,shop.stock,bag.items,(unsigned long long)xp.total,tr.arrivals,tr.failures,cp.chosen);}while(0)
 if(!p){TR_CHECK(false,"controlled actor exists");stage=99;}
 if(stage==0&&p){
  checkpoint_park_enemies();TR_CHECK(tr.enabled&&!tr.busy,"teleporter candidate starts with coherent services");
  if(strcmp(mode,"read")){
   physicsactor_t* standing=physicsactor_create(v2d_new(0,0));int sw,sh,rw,rh;v2d_t sc,rc;physicsactor_bounding_box(standing,&sw,&sh,&sc);physicsactor_roll(p->body);physicsactor_bounding_box(p->body,&rw,&rh,&rc);v2d_t raw=physicsactor_get_position(p->body);
   double standing_top=sc.y-sh*.5-20,rolling_top=rc.y-rh*.5-(raw.y+20);int ceiling=(int)ceil((standing_top+rolling_top)*.5);physicsactor_destroy(standing);
   image_t* prev=image_drawing_target();image_t* image=image_create(128,1);image_set_drawing_target(image);image_clear(color_rgb(255,255,255));image_set_drawing_target(prev);collisionmask_t* mask=collisionmask_create(image,0,0,128,1,0);obstacle_t* roof=obstacle_create(mask,(point2d_t){936,9000+ceiling},OL_DEFAULT,0);obstaclemap_t* map=obstaclemap_create();obstaclemap_add(map,roof);obstaclemap_build(map);test_terrain=map;
   double sx=p->a.x,sy=p->a.y;TR_CHECK(rolling_top>standing_top&&body_clear(p,map,1000,9000),"rolled source fits the synthetic low ceiling");TR_CHECK(!bsl_bor_teleport(p->lifetime,1000,9000,0,0,1)&&p->a.x==sx&&p->a.y==sy,"standing arrival body refuses that ceiling without moving the actor");
   test_terrain=NULL;obstaclemap_destroy(map);obstacle_destroy(roof);collisionmask_destroy(mask);image_destroy(image);physicsactor_restore_state(p->body);
  }

  if(!strcmp(mode,"read")){
   TR_CHECK(progress.loads==1&&!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev"),"fresh Continue routes to the registered annex permanent checkpoint");
   TR_CHECK(shop.balance==9&&shop.stock==3&&bag.items==4&&quest.status==4&&xp.total==5000&&eq.equipped==1,"fresh process retains selected economy, inventory, quest, earned XP and equipment");
   checkpoint_place(p,200,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:return",true)!=0,"return door refuses absent live departure context after disk reconstruction");
   p->a.health=50;TR_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==55,"continued owned item still heals the live actor");stage=99;
  }else{
   if(!strcmp(mode,"fallback")){old_actor=p->lifetime;p->a.health=72;TR_CHECK(bsl_host_anchor_start(0xb51f04,"event:door_unlock",p->lifetime)==0,"prepare fallback journey condition");bsl_host_simulation_tick();stage=20;wait=0;return;}
   old_actor=p->lifetime;TR_CHECK(bsl_host_equipment_set(p->lifetime,"gameplay/sword/player1",true)==0,"equip player-owned instance before travel");
   TR_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_accept",p->lifetime)==0,"accept authored quest");bsl_host_simulation_tick();TR_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",p->lifetime)==0,"buy supplies before travel");bsl_host_simulation_tick();TR_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"award exploration progress before travel");stage=1;wait=0;
  }
 }else if(stage==1){
  TR_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"claim ready quest before travel");bsl_host_simulation_tick();checkpoint_place(p,230,p->a.y);stage=2;wait=0;
 }else if(stage==2){
  TR_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"capture temporary halfway point");checkpoint_place(p,120,p->a.y);stage=3;wait=0;
 }else if(stage==3){
  TR_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0,"capture permanent checkpoint with earned/equipped/economic progress");
  TR_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",p->lifetime)==0,"make unsaved post-checkpoint purchase");bsl_host_simulation_tick();p->a.health=72;checkpoint_place(p,260,p->a.y);old_y=p->a.y;
  TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:local",false)!=0,"contact alone does not activate an action door");TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:local",true)==0,"fresh action requests same-level coordinate travel");
  TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:local",true)!=0&&bsl_host_use_item("item:strawberry")!=0,"in-flight travel rejects duplicate transfers and gameplay mutation");stage=4;wait=0;
 }else if(stage==4){
  TR_CHECK(p->lifetime==old_actor&&tr.arrivals==1&&cp.temporary,"same-level travel preserves actor lifetime, temporary checkpoint and scene");TR_CHECK(fabs(p->a.vx)<.01&&p->a.health==72&&shop.balance==6&&shop.stock==2&&bag.items==5&&xp.total==5000&&quest.status==4&&eq.equipped==1,"default stopped arrival retains live resources and all connected services");
  TR_CHECK(fabs(tr.source_x-260)<.01&&fabs(tr.source_y-old_y)<.01&&fabs(tr.target_x-160)<.01,"source context and arbitrary destination use the actor feet reference");
  checkpoint_place(p,420,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:blocked",true)==0,"blocked destination enters validation lifecycle");stage=5;wait=0;
 }else if(stage==5){
  TR_CHECK(p->lifetime==old_actor&&tr.arrivals==1&&tr.failures==1&&!tr.busy,"obstructed same-level destination refuses without reload or arrival");
  checkpoint_place(p,360,p->a.y);motion_apply(p,BOR_MOTION_SET,3,120,-40);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:momentum",true)==0,"velocity-preserving recipe starts");bsl_host_boot_step();TR_CHECK(fabs(p->a.vx-120)<.01&&fabs(p->a.vy+40)<.01&&p->lifetime==old_actor,"optional velocity survives exact relocation without actor replacement");stage=6;wait=0;
 }else if(stage==6){
  checkpoint_place(p,200,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)!=0&&!tr.busy,"activated locked door cannot bypass its usability condition");
  TR_CHECK(bsl_host_anchor_start(0xb51f04,"event:door_unlock",p->lifetime)==0,"registered event unlocks independent usability");bsl_host_simulation_tick();stage=7;wait=0;
 }else if(stage==7){
  checkpoint_place(p,200,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"unlocked registered destination queues cross-level travel");stage=8;wait=0;
 }else if(stage==8){
  checkpoint_park_enemies();TR_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&p->lifetime!=old_actor&&tr.arrivals==3,"cross-level travel constructs fresh scene actors and completes once");
  TR_CHECK(p->a.health==72&&shop.balance==6&&shop.stock==2&&bag.items==5&&xp.total==5000&&quest.status==4&&eq.equipped==1,"cross-level transfer retains resources, economy, quest, XP and equipped instance");
  TR_CHECK(!cp.temporary&&!strcmp(cp.chosen,"checkpoint:rest"),"unload expires temporary checkpoint but keeps earlier permanent eligible");TR_CHECK(bsl_host_progress_variable("variable:arrivals")==1&&tr.completion_context,"completion event runs once with captured departure/destination context");
  TR_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",old_actor)!=0,"source actor lifetime cannot mutate retained services");checkpoint_place(p,200,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:return",true)==0,"origin-aware return uses the recorded departure");stage=9;wait=0;
 }else if(stage==9){
  checkpoint_park_enemies();TR_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&tr.arrivals==4&&shop.balance==6&&bag.items==5&&p->a.health==72,"return journey preserves current progress without checkpoint rewind");TR_CHECK(bsl_host_progress_variable("variable:arrivals")==2,"linked completion runs once for the return trip");checkpoint_place(p,200,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"repeat outbound trip");stage=10;wait=0;
 }else if(stage==10){
  checkpoint_park_enemies();TR_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&tr.arrivals==5,"repeated travel completes without duplicate invocation");HostActor* enemy=equipment_actor("red_grunt");TR_CHECK(enemy&&checkpoint_defeat(p,enemy),"actual enemy hit defeats the player in the other level");stage=11;wait=0;
 }else if(stage==11){
  checkpoint_park_enemies();TR_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&cp.rewinds>0&&!strcmp(cp.restored,"checkpoint:rest"),"death routes across levels to the eligible permanent checkpoint");TR_CHECK(p->a.health==100&&shop.balance==9&&shop.stock==3&&bag.items==4&&quest.status==4&&xp.total==5000&&eq.equipped==1,"cross-level rewind restores the activation bundle instead of current travel values");TR_CHECK(!bsl_host_progress_flag("switch:door_open")&&bsl_host_progress_variable("variable:arrivals")==0,"checkpoint rollback includes travel-related authored variables");TR_CHECK(bsl_host_anchor_start(0xb51f04,"event:door_unlock",p->lifetime)==0,"unlock again after coherent rewind");bsl_host_simulation_tick();stage=12;wait=0;
 }else if(stage==12){
  checkpoint_place(p,200,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"travel again using services rebound after checkpoint reconstruction");stage=13;wait=0;
 }else if(stage==13){
  checkpoint_park_enemies();checkpoint_place(p,120,p->a.y);stage=14;wait=0;
 }else if(stage==14){
  TR_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:annex")==0&&bsl_host_progress_save()==0,"new annex rest point explicitly publishes selected progress using verified temporary save slot");stage=15;wait=0;
 }else if(stage==15){
  checkpoint_place(p,700,p->a.y);bsl_host_simulation_tick();bsl_host_teleport_probe(&tr);TR_CHECK(tr.busy,"contact-only door is requested by the automatic contact scan");stage=16;wait=0;
 }else if(stage==16){
  TR_CHECK(!tr.busy&&fabs(p->a.x-100)<1&&bsl_host_progress_variable("variable:arrivals")==1,"contact-only local travel completes without triggering unrelated completion events");stage=99;
 }else if(stage==20){
  checkpoint_place(p,200,p->a.y);TR_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"queue destination whose runtime door is missing");stage=21;wait=0;
 }else if(stage==21){
  TR_CHECK(!tr.busy&&!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&tr.failures==1&&tr.arrivals==0&&p->lifetime!=old_actor,"failed cross-level arrival reconstructs the source instead of committing travel");TR_CHECK(p->a.health==72&&shop.balance==12&&shop.stock==4&&bag.items==3&&xp.total==0&&bsl_host_progress_variable("variable:arrivals")==0,"fallback retains live services and does not run completion event");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_TELEPORT_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef TR_CHECK
}
