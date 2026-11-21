/* Deliberate pickup, capacity retry, checkpoint rollback, save and fresh Continue. Signed: Atlas. */
static void pickup_input(HostActor* p,bool crouch,bool trigger,bool sword,bool ranged){
 input_reset(controls);if(crouch)input_simulate_button_down(controls,IB_DOWN);if(trigger)input_simulate_button_down(controls,IB_FIRE7);if(sword)input_simulate_button_down(controls,IB_FIRE2);if(ranged)input_simulate_button_down(controls,IB_FIRE5);ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof keys);player_actions(p,&keys);input_reset(controls);
}
static bool pickup_held_ranged(HostActor* p){
 input_t* original=controls;controls=input_create_computer();input_simulate_button_down(controls,IB_FIRE5);input_update();
 input_simulate_button_down(controls,IB_FIRE5);input_simulate_button_down(controls,IB_DOWN);input_simulate_button_down(controls,IB_FIRE7);bool held=input_button_down(controls,IB_FIRE5)&&!input_button_pressed(controls,IB_FIRE5);
 ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof keys);player_actions(p,&keys);input_destroy(controls);controls=original;return held;
}
static void pickup_test_tick(void){
 const char* mode=getenv("BSL_PICKUP_TEST");if(!mode)return;
 static int stage,wait,checks,failures;static HostActor* p;
 if(++wait<60)return;
#define PICK_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("PICKUP %s: %s",ok?"PASS":"FAIL",m);}while(0)
 BslGameplayProbe bag={0};BslProgressProbe progress={0};bsl_host_gameplay_probe(&bag);p=hero();
 if(stage==0){
  PICK_CHECK(p!=NULL,"live pickup candidate actor exists");if(!p){stage=99;return;}
  checkpoint_park_enemies();
  if(!strcmp(mode,"read")){
   bsl_host_progress_probe(&progress);PICK_CHECK(progress.loads==1&&bag.items==5&&!bsl_host_pickup_available(0xb51e01)&&bsl_host_progress_flag("switch:berries_claimed"),"fresh-process Continue restores bag and hides claimed pickup from saved selected progress");
   p->a.health=50;PICK_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==55,"continued player consumes the collected item through actual BOR HP");bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==4,"consumption removes one item from the same player bag");stage=99;
  }else{
   PICK_CHECK(bag.items==3&&bsl_host_pickup_available(0xb51e01),"authored unclaimed pickup begins available without automatic collection");
   PICK_CHECK(bsl_host_pickup_try(p->lifetime)==-1,"walking/standing near a marker does not collect without crouch context");
   pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==3,"out-of-range trigger cannot collect a pickup");pickup_input(p,true,false,false,false);
   checkpoint_place(p,230,p->a.y);stage=1;wait=0;
  }
 }else if(stage==1){
  p->a.health=75;PICK_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"temporary checkpoint captures pre-pickup bag and claim flag");
  pickup_input(p,true,false,false,false);bool held_ranged=pickup_held_ranged(p);bsl_host_gameplay_probe(&bag);PICK_CHECK(held_ranged&&bag.items==3&&bsl_host_pickup_available(0xb51e01)&&p->a.state==BOR_FREE,"sustained ranged channel with available capacity excludes pickup without starting another attack");pickup_input(p,true,false,false,false);
  PICK_CHECK(bsl_host_anchor_start(0xcafe,"event:pickup_fill",p->lifetime)==0,"capacity setup uses a real authored inventory event");bsl_host_simulation_tick();bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==19,"fixture fills the real bag below its stack cap");
  pickup_input(p,true,false,false,false);pickup_input(p,true,true,true,false);bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==19&&bsl_host_pickup_available(0xb51e01)&&p->a.state==BOR_GUARD,"armed guard chord takes priority over pickup");pickup_input(p,true,false,false,false);
  pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==19&&bsl_host_pickup_available(0xb51e01)&&!bsl_host_progress_flag("switch:berries_claimed"),"capacity refusal retains bag and world claim availability");
  PICK_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==80,"existing consumable frees capacity and heals actual actor");pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==18&&bsl_host_pickup_available(0xb51e01),"holding trigger after space becomes available does not collect automatically");
  pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==20&&!bsl_host_pickup_available(0xb51e01)&&bsl_host_progress_flag("switch:berries_claimed"),"fresh crouch-trigger atomically grants quantity and commits claim flag");
  pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==20,"held trigger cannot duplicate the collected stack");pickup_input(p,false,false,false,false);
  PICK_CHECK(bsl_host_checkpoint_request()==0,"rewind queues after the committed pickup");stage=2;wait=0;
 }else if(stage==2){
  bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==3&&bag.health==75&&bsl_host_pickup_available(0xb51e01)&&!bsl_host_progress_flag("switch:berries_claimed"),"checkpoint restores pre-claim bag, HP, receipt and world availability together");
  checkpoint_park_enemies();pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);bsl_host_gameplay_probe(&bag);PICK_CHECK(bag.items==5&&!bsl_host_pickup_available(0xb51e01),"rolled-back pickup can be earned again once");pickup_input(p,false,false,false,false);checkpoint_place(p,120,p->a.y);stage=3;wait=0;
 }else if(stage==3){
  PICK_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&bsl_host_progress_save()==0,"explicit permanent activation saves bag and selected claim flag");p->a.health=40;bsl_host_use_item("item:strawberry");PICK_CHECK(bsl_host_progress_load()==0,"load queues fresh level reconstruction after later consumption");stage=4;wait=0;
 }else if(stage==4){
  bsl_host_gameplay_probe(&bag);bsl_host_progress_probe(&progress);PICK_CHECK(progress.loads==1&&bag.items==5&&bag.health==100&&!bsl_host_pickup_available(0xb51e01),"same-process fresh load restores saved custody without resurrecting the claimed pickup");stage=99;
 }
 if(stage==99){const char* file=getenv("BSL_PICKUP_REPORT");FILE* f=file?fopen(file,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef PICK_CHECK
}
