/* Native common-event reward -> combat unlock -> rewind -> fresh Continue. Signed: Atlas. */
static void event_reward_test_tick(void){
 const char* mode=getenv("BSL_EVENT_REWARD_TEST");if(!mode)return;
 static int stage,wait,checks,failures;static uint64_t original;
 if(++wait<60)return;
#define ER_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("EVENT_REWARD %s: %s",ok?"PASS":"FAIL",m);}while(0)
 HostActor* p=hero();HostActor* enemy=equipment_actor("red_grunt");HostActor* alternate=equipment_alternate();BslStatProbe xp={0};BslLearningProbe learned={0};BslProgressProbe progress={0};BslAnchorRun run={0};
 bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&learned);bsl_host_progress_probe(&progress);
 if(stage==0){
  ER_CHECK(p&&enemy&&alternate,"live isolated native actors and event fixtures exist");if(!p||!enemy||!alternate){stage=99;return;}original=p->lifetime;checkpoint_park_enemies();
  if(!strcmp(mode,"read")){
   ER_CHECK(progress.loads==1&&xp.total==5000&&learned.learned&&progress.reward&&progress.choices==1,"fresh Continue reconstructs XP, learned action and selected event outcome together");
   ER_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_retry",p->lifetime)==0,"fresh level accepts a new run with the same authored objective claim");stage=8;wait=0;
  }else{
   ER_CHECK(xp.total==0&&!learned.learned&&!progress.reward,"fresh run starts with unclaimed objective and locked ability");
   ER_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",alternate->lifetime)!=0,"inactive actor cannot queue a player-owned reward event");
   ER_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"placed event queues its declared wait and registered reward command");
   bsl_host_simulation_tick();bsl_host_anchor_probe(0xb51f01,&run);ER_CHECK(run.status==2&&run.next==1,"reward has not run before its authored wait");bsl_host_hit(p->lifetime);stage=1;wait=0;
  }
 }else if(stage==1){
  bsl_host_anchor_probe(0xb51f01,&run);ER_CHECK(run.status==4&&xp.total==0&&!progress.reward,"hit cancels waiting event without granting later XP or outcomes");
  checkpoint_place(p,230,p->a.y);stage=2;wait=0;
 }else if(stage==2){
  p->a.health=75;ER_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"temporary activation captures the pre-objective progress");
  ER_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"cancelled event can explicitly restart under a new run identity");stage=3;wait=0;
 }else if(stage==3){
  bsl_host_anchor_probe(0xb51f01,&run);ER_CHECK(run.status==3&&xp.total==2500&&progress.reward&&progress.choices==1,"authored event commits XP and its following selected progress commands");
  ER_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_retry",p->lifetime)==0,"a separate event can retry the same stable reward claim");stage=4;wait=0;
 }else if(stage==4){
  bsl_host_anchor_probe(0xb51f01,&run);ER_CHECK(run.status==3&&xp.total==2500&&learned.receipts==1,"retry does not duplicate XP or learning receipts");ER_CHECK(bsl_host_checkpoint_request()==0,"queue coherent rewind after the objective reward");stage=5;wait=0;
 }else if(stage==5){
  ER_CHECK(xp.total==0&&!learned.learned&&learned.receipts==0&&!progress.reward&&progress.choices==0,"checkpoint rewinds XP, reward claim and selected event outcome together");checkpoint_park_enemies();
  ER_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"rewound objective is legitimately available again");stage=6;wait=0;
 }else if(stage==6){
  ER_CHECK(xp.total==2500&&progress.reward&&progress.choices==1,"reclaimed objective commits exactly once after rewind");
  int base=0;ER_CHECK(checkpoint_strike(p,enemy,true,1,&base)&&enemy->a.health==0,"real BOR attack defeats the registered enemy after event reward");bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&learned);
  ER_CHECK(xp.total==5000&&xp.units==2&&learned.learned&&learned.receipts==2,"combat and event awards share growth and jointly unlock the real action");
  p->a.freeze=0;p->frozen_tick=false;p->a.grounded=true;start(p,"idle",BOR_FREE);ER_CHECK(bsl_host_ability_use(p->lifetime,0)==0&&!strcmp(p->a.animation->name,"attack5"),"event-supported unlock executes its registered BOR timeline");checkpoint_park_enemies();checkpoint_place(p,120,p->a.y);stage=7;wait=0;
 }else if(stage==7){
  ER_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&bsl_host_progress_save()==0,"permanent rest explicitly saves progression and stable reward claims via verified temporary-slot publication");
  ER_CHECK(bsl_host_progress_load()==0,"load reconstructs a fresh level rather than serializing the event VM");stage=8;wait=0;
 }else if(stage==8){
  ER_CHECK(xp.total==5000&&learned.learned&&learned.receipts==1&&progress.reward&&progress.choices==1,"fresh reconstruction retains earned progress and durable objective claim while discarding transient defeat receipts");
  if(strcmp(mode,"read")){ER_CHECK(p->lifetime!=original&&bsl_host_anchor_start(0xb51f01,"event:objective_retry",original)!=0,"old actor lifetime loses event reward authority");ER_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_retry",p->lifetime)==0,"fresh actor can retry the saved claim");stage=9;wait=0;}
  else {bsl_host_anchor_probe(0xb51f01,&run);ER_CHECK(run.status==3,"fresh-process retry completes using the restored stable reward claim");stage=99;}
 }else if(stage==9){bsl_host_anchor_probe(0xb51f01,&run);ER_CHECK(run.status==3&&xp.total==5000&&learned.receipts==1,"saved claim refuses duplicate award after fresh actor reconstruction");stage=99;}
 if(stage==99){const char* path=getenv("BSL_EVENT_REWARD_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef ER_CHECK
}
