/* Connected generic stats -> real damage -> per-character growth -> actor lifecycle journey. Signed: Atlas. */
static bool progression_strike(HostActor* p,HostActor* t,bool armed,int target_health,int* base){
 unsigned serial=t->a.serial;int model=t->a.model_index;
 bor_init(&t->a,bor_models+model,model);t->a.serial=serial;t->a.grounded=true;t->a.health=target_health;
 t->placed=true;t->suspended=false;t->a.x=p->a.x+500;t->a.y=p->a.y;render_frame(t);
 p->a.freeze=0;p->a.invulnerable=0;p->a.partner=-1;p->a.grounded=true;
 if(!start(p,"idle",BOR_FREE))return false;
 p->action_channel=armed?0:1;
 if(!start(p,armed?"attack1":"unarmed",BOR_ACTION))return false;
 // Run the actual authored animation until its first active attack window.
 for(int i=0;i<180&&!p->a.active&&!p->a.finished;++i)bor_advance_animation(&p->a,1.0/60,hit_segment,p);
 if(!p->a.active)return false;
 *base=bor_frame(&p->a)->damage;
 double x,y,w,h,tx,ty,tw,th;bor_rect(&p->a,true,&x,&y,&w,&h);bor_rect(&t->a,false,&tx,&ty,&tw,&th);
 t->a.x+=x+w*.5-tx-tw*.5;t->a.y+=y+h*.5-ty-th*.5;render_frame(t);
 hit_segment(&p->a,bor_frame(&p->a),false,p);return true;
}
static void progression_test_tick(void){
 if(!getenv("BSL_PROGRESSION_TEST")||!bsl_gameplay_enabled())return;
 static int stage,wait,checks,failures,raw_first,damage_first,raw_second,damage_second;
 static uint64_t original,retained;static HostActor* p;static HostActor* t;static HostActor* alternate;static HostActor* arcade;static int alternate_damage,arcade_damage;
 if(++wait<45)return;
#define PROGRESS_CHECK(x,m) do{bool passed=(x);++checks;if(!passed)++failures;logfile_message("PROGRESSION %s: %s",passed?"PASS":"FAIL",m);}while(0)
 BslStatProbe q={0};bsl_host_stat_probe("stat:armed",&q);
 if(stage==0){
  p=hero();for(int i=0;i<64;++i)if(slots[i]&&slots[i]!=p){if(bor_is_player(&slots[i]->a)){if(slots[i]->a.x>420)arcade=slots[i];else alternate=slots[i];}else if(!slots[i]->a.model->training_dummy&&!t)t=slots[i];}
  PROGRESS_CHECK(p&&t&&alternate&&arcade,"live player, alternate and enemy exist in original course copy");
  if(!p||!t||!alternate||!arcade){stage=99;return;}
  original=p->lifetime;PROGRESS_CHECK(q.stage==1&&q.total==0,"new run begins at authored proficiency base");
  p->a.health=70;PROGRESS_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==75,"real inventory effect still reaches stat-enabled live actor");
  PROGRESS_CHECK(bsl_host_stat_test_seed("stat:armed",9999)==0,"explicit fixture seeds just below the next level");
  PROGRESS_CHECK(progression_strike(p,t,true,10000,&raw_first),"real BOR armed animation reaches its attack window");
  damage_first=10000-t->a.health;bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(damage_first==raw_first+1&&q.stage==2&&q.last_bonus==1&&q.total==10010,"swing crosses level but current hit keeps entry-level bonus");
  int health=t->a.health;t->a.invulnerable=0;hit_segment(&p->a,bor_frame(&p->a),false,p);bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(health==t->a.health&&q.total==10010,"repeated overlap neither damages nor awards twice");
  PROGRESS_CHECK(progression_strike(p,t,true,10000,&raw_second),"second real armed action begins");
  damage_second=10000-t->a.health;bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(damage_second==raw_second+2&&q.total==10021,"next action uses new proficiency level for actual damage");
  int base=0;PROGRESS_CHECK(progression_strike(p,t,true,1,&base),"real enemy defeat resolves through combat");bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(t->a.health==0&&q.last_damage==1&&q.total==10132,"effective damage is capped by remaining HP; hit and defeat awards add once");
  PROGRESS_CHECK(progression_strike(p,t,false,10000,&base),"real unarmed action reaches attack window");bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(t->a.health==10000-base&&q.total==10132&&q.last_bonus==0,"unarmed hit neither receives nor trains armed bonus");
  retained=q.total;t->a.x=p->a.x+500;t->a.y=p->a.y;commit_pose(t);rebase(t);
  p->a.freeze=0;p->a.invulnerable=0;start(p,"idle",BOR_FREE);p->a.vx=p->a.vy=0;rebase(p);motion_apply(p,BOR_MOTION_STOP,3,0,0);
  stage=1;wait=0;
 }else if(stage==1){
  PROGRESS_CHECK(bsl_host_select_character("character:michio_alternate")==0,"safe character switch succeeds after combat recovery");bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(!q.present,"alternate has no armed stat at all");
  bsl_host_stat_probe("stat:drive",&q);PROGRESS_CHECK(q.present&&q.units==2&&q.stage==1&&q.total==0,"same service exposes alternate's unrelated authored stat");
  PROGRESS_CHECK(bsl_host_stat_test_seed("stat:drive",4998)==0,"alternate seeds its own different growth curve");
  int base=0;PROGRESS_CHECK(progression_strike(alternate,t,true,10000,&base),"same attack executes with alternate stat mapping");
  alternate_damage=10000-t->a.health;bsl_host_stat_probe("stat:drive",&q);
  PROGRESS_CHECK(alternate_damage==base+2&&q.stage==2&&q.units==5&&q.total==5020,"alternate uses fixed-cost growth and retains entry damage");
  PROGRESS_CHECK(progression_strike(alternate,t,true,10000,&base),"alternate next action executes");bsl_host_stat_probe("stat:drive",&q);
  PROGRESS_CHECK(10000-t->a.health==base+5&&q.total==5042,"alternate's next action uses changed numeric stat");
  bsl_host_stat_probe("stat:focus",&q);PROGRESS_CHECK(q.units==9&&q.stage==0,"ordinary scalar stat responds to data-mapped hit events without XP");
  t->a.x=alternate->a.x+500;commit_pose(t);rebase(t);alternate->a.freeze=0;start(alternate,"idle",BOR_FREE);alternate->a.vx=alternate->a.vy=0;rebase(alternate);motion_apply(alternate,BOR_MOTION_STOP,3,0,0);
  stage=3;wait=0;
 }else if(stage==3){
  PROGRESS_CHECK(bsl_host_select_character("character:michio_arcade")==0,"no-progression character can take control");
  int base=0;PROGRESS_CHECK(progression_strike(arcade,t,true,10000,&base),"arcade character executes same real attack without growth rules");arcade_damage=10000-t->a.health;
  bsl_host_stat_probe("stat:focus",&q);PROGRESS_CHECK(arcade_damage==base&&q.units==5&&q.stage==0,"no-progression profile leaves damage and scalar base unchanged");
  t->a.x=arcade->a.x+500;commit_pose(t);rebase(t);arcade->a.freeze=0;start(arcade,"idle",BOR_FREE);arcade->a.vx=arcade->a.vy=0;rebase(arcade);motion_apply(arcade,BOR_MOTION_STOP,3,0,0);
  stage=4;wait=0;
 }else if(stage==4){
  PROGRESS_CHECK(bsl_host_select_character("character:michio")==0,"return to original character");bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(q.stage==2&&q.total==retained,"switching preserves the initiating character's progress");
  bsl_engine_restart();stage=2;wait=0;
 }else if(stage==2){
  p=hero();bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(p&&p->lifetime!=original&&q.total==retained&&q.stage==2,"real level destruction/recreation preserves character proficiency");
  PROGRESS_CHECK(bsl_host_action_begin(original,999999,"melee.primary")==0,"destroyed actor cannot submit a practice award");bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(q.total==retained,"stale submission leaves progress intact");
  BslGameplayProbe owner={0};bsl_host_gameplay_probe(&owner);PROGRESS_CHECK(owner.items==2&&owner.health==75,"one player bag and character HP remain coherent through combat, three switches and reload");
  PROGRESS_CHECK(bsl_host_select_character("character:michio_alternate")==0,"alternate replacement can take control after reload");bsl_host_stat_probe("stat:drive",&q);
  PROGRESS_CHECK(q.total==5042&&q.units==5,"alternate's different earned stat survives actual actor replacement");bsl_host_stat_probe("stat:focus",&q);PROGRESS_CHECK(q.units==9&&q.stage==0,"scalar earned value also survives actor replacement");
  reset_all();bsl_host_stat_probe("stat:armed",&q);
  PROGRESS_CHECK(q.stage==1&&q.total==0,"explicit fresh-run reset clears earned proficiency");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_PROGRESSION_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"first_base\":%d,\"first_damage\":%d,\"next_base\":%d,\"next_damage\":%d,\"retained_units\":%llu,\"alternate_first_damage\":%d,\"arcade_damage\":%d}\n",failures?"false":"true",checks,failures,raw_first,damage_first,raw_second,damage_second,(unsigned long long)retained,alternate_damage,arcade_damage);fclose(f);}engine_quit();}
#undef PROGRESS_CHECK
}
