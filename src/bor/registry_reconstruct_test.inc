/* Absent checkpoint-selected body: rollback, final-position overlap and cross-level reconstruction. Signed: Mech. */
static void registry_reconstruct_test_tick(void){
 if(!getenv("BSL_JOURNEY_RECONSTRUCT"))return;
 static int stage,wait,checks,failures,live_count;static uint64_t retired,current;static unsigned failed_at;static double old_spawn_x,old_spawn_y;
 if(++wait<60)return;
 HostActor* p=hero();HostActor* primary=journey_character("character:michio");HostActor* alternate=journey_character("character:michio_alternate");
 BslGameplayProbe bag={0};BslCheckpointProbe cp={0};BslProgressProbe save={0};BslStatProbe xp={0};BslTeleportProbe travel={0};
 bsl_host_gameplay_probe(&bag);bsl_host_checkpoint_probe(&cp);bsl_host_progress_probe(&save);bsl_host_stat_probe("stat:experience",&xp);bsl_host_teleport_probe(&travel);
 surgescript_object_t* registry=placement_registry();
#define RECON_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("RECONSTRUCT %s: %s",ok?"PASS":"FAIL",message);if(!ok){logfile_message("RECONSTRUCT observed stage=%d level=%s hp=%d items=%d xp=%llu actors=%d rewinds=%u failures=%u",stage,bsl_engine_level_file(),p?p->a.health:0,bag.items,(unsigned long long)xp.total,factory_live_count(),cp.rewinds,cp.failures);stage=99;goto finish;}}while(0)
 if(stage==0){
  RECON_CHECK(p==primary&&!alternate&&registry,"selected-only fixture starts with one roster body and a canonical factory");checkpoint_park_enemies();
  RECON_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","reconstruct/one")==0&&bsl_host_reward(p->lifetime,"source:lab_exploration","reconstruct/two")==0&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"primary earns the capability to be captured before its body is removed");
  checkpoint_place(p,120,p->a.y);stage=1;wait=0;
 }else if(stage==1){
  RECON_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&bsl_host_progress_save()==0,"permanent activation and existing temporary-slot writer capture selected primary progress");
  RECON_CHECK(factory_body(registry,"create","character:michio_alternate",380,9850)!=0,"alternate materializes through the ordinary registry factory");stage=2;wait=0;
 }else if(stage==2){
  RECON_CHECK(alternate&&factory_select(registry,"character:michio_alternate"),"alternate takes control before the checkpoint-selected body is deliberately removed");
  alternate->a.health=37;RECON_CHECK(bsl_host_use_item("item:strawberry")==0&&alternate->a.health==42,"post-activation item use changes alternate HP and player inventory");
  retired=primary->lifetime;surgescript_object_call_function(primary->entity,"destroy",NULL,0,NULL);checkpoint_place(alternate,120,alternate->a.y);stage=3;wait=0;
 }else if(stage==3){
  RECON_CHECK(!primary&&p==alternate&&!bsl_host_controlled(retired)&&bag.items==2&&xp.total==0,"removed checkpoint-selected character leaves current control and independent live progress intact");
  RECON_CHECK(!factory_body(registry,"create","character:michio",p->a.x,p->a.y),"ordinary creation still refuses the space currently occupied by the alternate");
  old_spawn_x=p->a.spawn_x;old_spawn_y=p->a.spawn_y;p->a.spawn_x=10000001;p->a.spawn_y=old_spawn_y;
  current=p->lifetime;live_count=factory_live_count();failed_at=cp.failures;
  RECON_CHECK(bsl_host_checkpoint_request()==0,"rewind queues while another actor's final reset location is deliberately out of bounds");stage=4;wait=0;
 }else if(stage==4){
  RECON_CHECK(cp.failures==failed_at+1&&cp.rewinds==0&&!cp.requested,"late spatial preflight refuses every candidate rather than partially committing");
  RECON_CHECK(!primary&&p==alternate&&p->lifetime==current&&controlled(p)&&p->a.health==42&&bag.items==2&&xp.total==0,"failed preparation preserves current selection, HP, inventory and XP");
  BslCharacterDefinition d={0};RECON_CHECK(bsl_host_character_definition("character:michio",&d)==0&&!d.actor&&!factory_body(registry,"find","character:michio",0,0)&&factory_live_count()==live_count,"failed candidate preparation removes temporary native bodies and leaves no roster/factory binding");
  p->a.spawn_x=old_spawn_x;p->a.spawn_y=old_spawn_y;
  RECON_CHECK(bsl_host_checkpoint_request()==0,"explicit retry is accepted after correcting the final reset position");stage=5;wait=0;
 }else if(stage==5){
  RECON_CHECK(primary==p&&primary->lifetime!=retired&&!bsl_host_controlled(retired)&&cp.rewinds==1&&!strcmp(cp.restored,"checkpoint:rest"),"retry recreates and selects the absent recorded primary with a fresh lifetime");
  RECON_CHECK(p->a.health==100&&bag.items==3&&xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"checkpoint publication restores character HP, bag, XP and learned capability together");
  RECON_CHECK(alternate&&alternate->a.health==90&&!controlled(alternate)&&factory_live_count()==live_count+1&&fabs(p->a.x-120)<3&&fabs(alternate->a.x-old_spawn_x)<3,"final-position preflight moves the alternate away before publishing the restored primary at its return point");
  RECON_CHECK(p->a.state==BOR_FREE&&p->a.partner<0&&p->jumps_used==0&&save.saves==1&&save.loads==0,"reconstruction clears transient actor state without loading or writing another save");
  RECON_CHECK(factory_select(registry,"character:michio_alternate"),"restored roster still supports ordinary safe switching");
  retired=primary->lifetime;surgescript_object_call_function(primary->entity,"destroy",NULL,0,NULL);
  alternate->a.health=70;RECON_CHECK(bsl_host_use_item("item:strawberry")==0&&alternate->a.health==75,"a second post-activation item use remains live before cross-level defeat");
  RECON_CHECK(bsl_host_anchor_start(0xec0001,"event:door_unlock",alternate->lifetime)==0,"registered event opens the cross-level route");stage=6;wait=0;
 }else if(stage==6){
  checkpoint_place(p,200,p->a.y);current=p->lifetime;
  RECON_CHECK(!primary&&bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"live alternate travels while the checkpoint-selected primary remains absent");stage=7;wait=0;
 }else if(stage==7){
  checkpoint_park_enemies();RECON_CHECK(alternate==p&&!primary&&!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&p->lifetime!=current&&p->a.health==75&&bag.items==2,"annex creates only the live selected alternate and retains current progress");
  HostActor* enemy=equipment_actor("Green_Grunt");RECON_CHECK(enemy&&checkpoint_defeat(p,enemy),"actual enemy damage defeats the alternate in the destination");stage=8;wait=0;
 }else if(stage==8){
  checkpoint_park_enemies();RECON_CHECK(primary==p&&!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&!strcmp(cp.restored,"checkpoint:rest")&&cp.rewinds==2,"cross-level defeat return materializes the different checkpoint-selected primary transactionally");
  RECON_CHECK(p->a.health==100&&bag.items==3&&xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump")&&alternate&&alternate->a.health==90&&!controlled(alternate),"cross-level rewind restores independent character state and player inventory coherently");
  RECON_CHECK(alternate->a.state==BOR_FREE&&alternate->a.partner<0&&alternate->a.freeze<=0,"cross-level rewind clears the inactive alternate's committed/linked state");
  logfile_message("RECONSTRUCT inactive readiness x=%.2f y=%.2f vx=%.2f vy=%.2f grounded=%d suspended=%d view=%d",alternate->a.x,alternate->a.y,alternate->a.vx,alternate->a.vy,alternate->a.grounded,alternate->suspended,bsl_bor_switch_ready(alternate->lifetime));
  // Non-selected reset positions may be airborne. Prepare a supported site for the separate provider-failure case.
  checkpoint_place(alternate,380,p->a.y);stage=81;wait=0;
 }else if(stage==81){
  logfile_message("RECONSTRUCT placed readiness x=%.2f y=%.2f vx=%.2f vy=%.2f grounded=%d suspended=%d view=%d",alternate->a.x,alternate->a.y,alternate->a.vx,alternate->a.vy,alternate->a.grounded,alternate->suspended,bsl_bor_switch_ready(alternate->lifetime));
  RECON_CHECK(factory_select(registry,"character:michio_alternate"),"alternate remains selectable at a supported stable position after reconstruction");
  retired=primary->lifetime;surgescript_object_call_function(primary->entity,"destroy",NULL,0,NULL);alternate->a.health=63;
  RECON_CHECK(bsl_host_use_item("item:strawberry")==0&&alternate->a.health==68,"live state diverges again before testing an unavailable factory");
  surgescript_object_call_function(registry,"destroy",NULL,0,NULL);stage=9;wait=0;
 }else if(stage==9){
  RECON_CHECK(!primary&&!registry&&p==alternate,"canonical factory is explicitly absent without removing the active actor");
  current=p->lifetime;live_count=factory_live_count();failed_at=cp.failures;
  RECON_CHECK(bsl_host_checkpoint_request()==0,"rewind request reaches explicit missing-factory handling");stage=10;wait=0;
 }else if(stage==10){
  RECON_CHECK(cp.failures==failed_at+1&&cp.rewinds==2&&!primary&&p==alternate&&p->lifetime==current&&controlled(p),"missing factory refuses reconstruction without another body or input-owner change");
  RECON_CHECK(p->a.health==68&&bag.items==2&&xp.total==0&&factory_live_count()==live_count&&save.saves==1&&save.loads==0,"missing-factory failure preserves live progress and actor count without save rewrites");stage=99;
 }
finish:
 if(stage==99){const char* path=getenv("BSL_JOURNEY_RECONSTRUCT_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef RECON_CHECK
}
