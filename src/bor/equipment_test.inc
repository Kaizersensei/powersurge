/* Actual owner equipment -> entry snapshot -> native damage -> checkpoint/fresh Continue. Signed: Atlas. */
static HostActor* equipment_actor(const char* model){for(int i=0;i<64;i++)if(slots[i]&&!strcmp(slots[i]->a.model->name,model)&&!bor_is_player(&slots[i]->a))return slots[i];return NULL;}
static HostActor* equipment_alternate(void){for(int i=0;i<64;i++)if(slots[i]&&bor_is_player(&slots[i]->a)&&slots[i]->a.spawn_x>300&&slots[i]->a.spawn_x<420)return slots[i];return NULL;}
static bool equipment_strike(HostActor* p,HostActor* target,bool unequip_during,int* base,int* damage){
 unsigned serial=target->a.serial;int model=target->a.model_index;double sx=target->a.spawn_x,sy=target->a.spawn_y;
 bor_init(&target->a,bor_models+model,model);target->a.serial=serial;target->a.spawn_x=sx;target->a.spawn_y=sy;target->a.initialized=true;target->a.grounded=true;target->a.health=10000;target->a.x=p->a.x+500;target->a.y=p->a.y;target->a.ai_wait=10000;target->placed=true;target->suspended=false;render_frame(target);
 p->a.freeze=p->a.invulnerable=0;p->frozen_tick=false;p->a.partner=-1;p->a.grounded=true;p->action_channel=0;
 if(!start(p,"idle",BOR_FREE)||!start(p,"attack1",BOR_ACTION))return false;
 for(int i=0;i<180&&!p->a.active&&!p->a.finished;++i)bor_advance_animation(&p->a,1.0/60,hit_segment,p);
 if(!p->a.active)return false;
 *base=bor_frame(&p->a)->damage;
 if(unequip_during&&bsl_host_equipment_set(p->lifetime,"gameplay/sword/player1",false)!=0)return false;
 double x,y,w,h,tx,ty,tw,th;bor_rect(&p->a,true,&x,&y,&w,&h);bor_rect(&target->a,false,&tx,&ty,&tw,&th);target->a.x+=x+w*.5-tx-tw*.5;target->a.y+=y+h*.5-ty-th*.5;render_frame(target);hit_segment(&p->a,bor_frame(&p->a),false,p);*damage=10000-target->a.health;
 target->a.x=target->a.spawn_x+1500;target->a.y=target->a.spawn_y;commit_pose(target);rebase(target);return true;
}
static void equipment_test_tick(void){
 const char* mode=getenv("BSL_EQUIPMENT_TEST");if(!mode)return;static int stage,wait,checks,failures;static uint64_t old_actor;static HostActor* p;int base=0,damage=0;
 if(++wait<60)return;
#define EQ_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("EQUIPMENT %s: %s",ok?"PASS":"FAIL",m);}while(0)
 p=hero();HostActor* enemy=equipment_actor("red_grunt");HostActor* alternate=equipment_alternate();BslEquipmentProbe eq={0};BslGameplayProbe bag={0};BslStatProbe stat={0};BslProgressProbe progress={0};bsl_host_gameplay_probe(&bag);bsl_host_equipment_probe("stat:armed",&eq);
 if(stage==0){
  EQ_CHECK(p&&enemy&&alternate&&eq.enabled,"live equipment candidate, owned sword and character fixtures exist");if(!p||!enemy||!alternate){stage=99;return;}checkpoint_park_enemies();old_actor=p->lifetime;
  if(!strcmp(mode,"read")){
   bsl_host_progress_probe(&progress);EQ_CHECK(progress.loads==1&&eq.equipped==1&&eq.base==1&&eq.effective==3&&bag.items==3,"fresh Continue derives equipped stats from the saved owned instance, without storing effective values");
   EQ_CHECK(equipment_strike(p,enemy,false,&base,&damage)&&damage==base+3,"freshly continued actor uses reconstructed equipment in actual damage");p->a.health=50;EQ_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==55,"equipment does not change owner-bound item healing");stage=99;
  }else{
   EQ_CHECK(eq.base==1&&eq.effective==1&&eq.equipped==0,"bag owns the starting sword but unbound equipment has no contribution");
   EQ_CHECK(bsl_host_equipment_set(alternate->lifetime,"gameplay/sword/player1",true)!=0&&bsl_host_equipment_set(p->lifetime,"missing",true)!=0,"inactive actor and unowned instance cannot equip the player's weapon");
   EQ_CHECK(equipment_strike(p,enemy,false,&base,&damage)&&damage==base+1,"unbound weapon leaves existing BOR base plus character formula damage");bsl_host_stat_probe("stat:armed",&stat);uint64_t earned=stat.total;
   EQ_CHECK(bsl_host_equipment_set(p->lifetime,"gameplay/sword/player1",true)==0,"trusted menu adapter equips the exact player-owned sword instance");bsl_host_equipment_probe("stat:armed",&eq);bsl_host_stat_probe("stat:armed",&stat);EQ_CHECK(eq.equipped==1&&eq.base==1&&eq.effective==3&&stat.total==earned,"equipment changes effective value while earned proficiency/XP remain unchanged");
   EQ_CHECK(equipment_strike(p,enemy,true,&base,&damage)&&damage==base+3,"unequipping during an authored attack preserves its entry equipment snapshot");bsl_host_equipment_probe("stat:armed",&eq);EQ_CHECK(eq.effective==1&&eq.equipped==0,"unequip removes the live modifier immediately for subsequent actions");
   EQ_CHECK(equipment_strike(p,enemy,false,&base,&damage)&&damage==base+1,"next actual attack uses the unequipped value");
   EQ_CHECK(bsl_host_equipment_set(p->lifetime,"gameplay/sword/player1",true)==0,"re-equip before checkpoint capture");checkpoint_place(p,230,p->a.y);stage=1;wait=0;
  }
 }else if(stage==1){
  p->a.health=75;EQ_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"checkpoint captures owned equipment alongside earned progress and inventory");
  EQ_CHECK(bsl_host_equipment_set(p->lifetime,"gameplay/sword/player1",false)==0&&bsl_host_use_item("item:strawberry")==0&&p->a.health==80,"later equipment removal and item use commit independently");EQ_CHECK(bsl_host_checkpoint_request()==0,"queue coherent equipment/inventory checkpoint rewind");stage=2;wait=0;
 }else if(stage==2){
  EQ_CHECK(eq.equipped==1&&eq.base==1&&eq.effective==3&&bag.items==3&&bag.health==75,"rewind restores the equipped instance and derives its contribution with the original bag and HP");checkpoint_park_enemies();checkpoint_place(p,120,p->a.y);checkpoint_place(alternate,200,p->a.y);stage=3;wait=0;
 }else if(stage==3){
  EQ_CHECK(bsl_host_select_character("character:michio_alternate")==0,"safe control switch retains the same player-owned equipment");p=hero();bsl_host_equipment_probe("stat:drive",&eq);EQ_CHECK(eq.present&&eq.base==2&&eq.effective==6&&eq.equipped==1,"same sword maps to unrelated Drive stat through the selected character profile");bsl_host_equipment_probe("stat:armed",&eq);EQ_CHECK(!eq.present,"alternate is not forced to acquire an Armed stat");
  EQ_CHECK(equipment_strike(p,enemy,false,&base,&damage)&&damage==base+6,"alternate character consumes its own equipment-modified formula in actual damage");checkpoint_place(p,200,p->a.y);stage=4;wait=0;
 }else if(stage==4){
  EQ_CHECK(bsl_host_select_character("character:michio")==0,"switch back without transferring or reseeding equipment");p=hero();EQ_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&bsl_host_progress_save()==0,"permanent activation explicitly saves owner instance/slot and base earned progress");
  bsl_host_equipment_set(p->lifetime,"gameplay/sword/player1",false);p->a.health=50;bsl_host_use_item("item:strawberry");EQ_CHECK(bsl_host_progress_load()==0,"queue fresh reconstruction after unsaved equipment removal and consumption");stage=5;wait=0;
 }else if(stage==5){
  bsl_host_progress_probe(&progress);EQ_CHECK(progress.loads==1&&eq.equipped==1&&eq.base==1&&eq.effective==3&&bag.items==3&&bag.health==100,"fresh load restores slot ownership and derives effective stats without double-applying bonuses");EQ_CHECK(bsl_host_equipment_set(old_actor,"gameplay/sword/player1",false)!=0,"destroyed actor lifetime cannot change reconstructed equipment");
  EQ_CHECK(equipment_strike(p,enemy,false,&base,&damage)&&damage==base+3,"reconstructed actor's real attack uses the saved equipment contribution");stage=99;
 }
 if(stage==99){const char* file=getenv("BSL_EQUIPMENT_REPORT");FILE* f=file?fopen(file,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef EQ_CHECK
}
