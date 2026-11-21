/* Actual player quest -> pickup/event facts -> reward/unlock -> rewind/Continue. Signed: Atlas. */
static void quest_test_tick(void){
 const char* mode=getenv("BSL_QUEST_TEST");if(!mode)return;static int stage,wait,checks,failures;static uint64_t original;
 if(++wait<60)return;
 HostActor* p=hero();HostActor* alternate=equipment_alternate();BslQuestProbe quest={0};BslStatProbe xp={0};BslLearningProbe learned={0};BslGameplayProbe bag={0};BslProgressProbe progress={0};
 bsl_host_quest_probe("quest:training_supplies",&quest);bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&learned);bsl_host_gameplay_probe(&bag);bsl_host_progress_probe(&progress);
#define QUEST_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("QUEST %s: %s",ok?"PASS":"FAIL",m);if(!ok)logfile_message("QUEST observed status=%d met=%d/%d XP=%llu receipts=%u items=%d",quest.status,quest.met,quest.total,(unsigned long long)xp.total,learned.receipts,bag.items);}while(0)
 if(stage==0){
  QUEST_CHECK(p&&alternate&&quest.enabled,"live player and compiled quest catalog exist");if(!p||!alternate||!quest.enabled){stage=99;return;}original=p->lifetime;checkpoint_park_enemies();
  if(!strcmp(mode,"read")){
   QUEST_CHECK(progress.loads==1&&quest.status==4&&xp.total==5000&&learned.learned&&learned.receipts==2&&bag.items==5,"fresh Continue restores completed quest, both durable claims, earned ability and collected bag");
   QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"freshly reconstructed actor can retry a completed quest event");bsl_host_simulation_tick();bsl_host_stat_probe("stat:experience",&xp);QUEST_CHECK(xp.total==5000,"fresh-process claim retry does not award again");
   p->a.health=50;QUEST_CHECK(bsl_host_use_item("item:strawberry")==0&&bsl_host_use_item("item:strawberry")==0&&p->a.health==60,"collected quest supplies remain usable through actual owner-bound HP");bsl_host_quest_probe("quest:training_supplies",&quest);QUEST_CHECK(quest.status==4&&quest.met==1,"completed quest stays complete when consumption drops supplies below the objective threshold");stage=99;
  }else{
   QUEST_CHECK(quest.status==1&&bag.items==3&&xp.total==0,"new quest is available with insufficient starting supplies");
   QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_accept",alternate->lifetime)!=0,"inactive actor cannot accept for the controlled player");
   QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"premature claim is evaluated through the authored event command");bsl_host_simulation_tick();bsl_host_quest_probe("quest:training_supplies",&quest);QUEST_CHECK(quest.status==1,"denied claim does not accept or complete the quest");
   QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_accept",p->lifetime)==0,"authored common event accepts the quest");bsl_host_simulation_tick();bsl_host_quest_probe("quest:training_supplies",&quest);QUEST_CHECK(quest.status==2,"accepted quest becomes active against the same player's live facts");checkpoint_place(p,230,p->a.y);stage=1;wait=0;
  }
 }else if(stage==1){
  p->a.health=75;QUEST_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"temporary checkpoint captures accepted but unfinished quest and original bag");
  pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);pickup_input(p,true,false,false,false);bsl_host_gameplay_probe(&bag);QUEST_CHECK(bag.items==5,"deliberate real pickup supplies the inventory objective");
  QUEST_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"existing exploration event provides the second registered objective");stage=2;wait=0;
 }else if(stage==2){
  QUEST_CHECK(quest.status==3&&quest.met==2&&xp.total==2500,"inventory and completed exploration facts jointly make the quest ready");
  QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"ready quest claim queues for its captured character");bsl_host_simulation_tick();bsl_host_quest_probe("quest:training_supplies",&quest);bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&learned);
  QUEST_CHECK(quest.status==4&&xp.total==5000&&learned.learned&&learned.receipts==2,"quest reward and completion commit together and unlock the registered action");
  QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"completed quest accepts a stable retry");bsl_host_simulation_tick();bsl_host_stat_probe("stat:experience",&xp);QUEST_CHECK(xp.total==5000,"completed retry has no duplicate reward");
  QUEST_CHECK(bsl_host_checkpoint_request()==0,"queue checkpoint rewind after quest completion");stage=3;wait=0;
 }else if(stage==3){
  QUEST_CHECK(quest.status==2&&quest.met==0&&xp.total==0&&!learned.learned&&learned.receipts==0&&bag.items==3,"rewind restores accepted quest, unfinished objectives, bag and reward claims coherently");checkpoint_park_enemies();
  pickup_input(p,true,false,false,false);pickup_input(p,true,true,false,false);pickup_input(p,true,false,false,false);
  QUEST_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"rewound objective and pickup are available again");stage=4;wait=0;
 }else if(stage==4){
  QUEST_CHECK(quest.status==3&&xp.total==2500&&bag.items==5,"replayed facts make the rewound quest ready again");QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"reclaim uses the same quest identity after coherent rewind");bsl_host_simulation_tick();bsl_host_quest_probe("quest:training_supplies",&quest);bsl_host_stat_probe("stat:experience",&xp);QUEST_CHECK(quest.status==4&&xp.total==5000,"reclaimed reward reaches the replacement learning service");
  p->a.freeze=0;p->frozen_tick=false;p->a.grounded=true;start(p,"idle",BOR_FREE);QUEST_CHECK(bsl_host_ability_use(p->lifetime,0)==0&&!strcmp(p->a.animation->name,"attack5"),"quest-supported unlock executes the real BOR animation");checkpoint_place(p,120,p->a.y);stage=5;wait=0;
 }else if(stage==5){
  QUEST_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&bsl_host_progress_save()==0,"permanent activation explicitly saves quest state and claims through verified temporary-slot publication");QUEST_CHECK(bsl_host_progress_load()==0,"load requests fresh level reconstruction");stage=6;wait=0;
 }else if(stage==6){
  QUEST_CHECK(progress.loads==1&&quest.status==4&&xp.total==5000&&learned.receipts==2&&bag.items==5,"fresh reconstruction joins quest progress to the saved inventory and reward claims");QUEST_CHECK(p->lifetime!=original&&bsl_host_anchor_start(0xb51f02,"event:quest_claim",original)!=0,"stale actor lifetime loses quest command authority");
  QUEST_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"fresh actor can retry the saved quest claim");stage=7;wait=0;
 }else if(stage==7){QUEST_CHECK(quest.status==4&&xp.total==5000&&learned.receipts==2,"fresh actor retry leaves the completed quest and reward totals unchanged");stage=99;}
 if(stage==99){const char* path=getenv("BSL_QUEST_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef QUEST_CHECK
}
