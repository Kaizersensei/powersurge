/* Connected durable progress journey, writer and separate fresh reader. Signed: Atlas. */
static void progress_save_test_tick(void){
 const char* mode=getenv("BSL_PROGRESS_TEST");if(!mode)return;
 static int stage,wait,checks,failures;static HostActor *p,*red,*green;static uint64_t old_action;
 if(++wait<60)return;
#define PS_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("PROGRESS SAVE %s: %s",ok?"PASS":"FAIL",m);}while(0)
 BslProgressProbe saved={0};BslGameplayProbe bag={0};BslStatProbe xp={0};BslLearningProbe learned={0};bsl_host_progress_probe(&saved);
 if(stage==0){
  p=hero();for(int i=0;i<64;i++)if(slots[i]&&slots[i]->a.spawn_x<1000){if(!strcmp(slots[i]->a.model->name,"red_grunt"))red=slots[i];if(!strcmp(slots[i]->a.model->name,"Green_Grunt"))green=slots[i];}PS_CHECK(p&&red&&green&&saved.enabled,"save candidate and actor fixtures exist");if(!p||!red||!green){stage=99;return;}
  checkpoint_park_enemies();bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);PS_CHECK(bag.items==3&&xp.total==0,"process begins from authored defaults");
  if(!strcmp(mode,"write")){
   PS_CHECK(bsl_host_progress_save()!=0,"saving without explicit permanent activation is refused");int base=0;PS_CHECK(checkpoint_strike(p,red,true,1,&base),"real combat grants character XP");PS_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0,"noncombat reward joins same progression");
   p->a.freeze=0;p->frozen_tick=false;start(p,"idle",BOR_FREE);p->a.health=40;PS_CHECK(bsl_host_use_item("item:strawberry")==0,"owned bag changes before activation");
   PS_CHECK(bsl_host_anchor_start(0xbeef,"event:training_reward",p->lifetime)==0,"authored event sets selected globals");bsl_host_simulation_tick();PS_CHECK(bsl_host_use_item("item:strawberry")==0,"consume authored event grant through the live item service");checkpoint_place(p,120,p->a.y);stage=1;wait=0;
  }else{PS_CHECK(bsl_host_progress_load()==0,"fresh process validates and queues the saved rest point");stage=3;wait=0;}
 }else if(stage==1){
  PS_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0,"permanent activation captures recovered HP and progress");PS_CHECK(bsl_host_progress_save()==0,"explicit save persists selected activation values");
  p->a.health=30;bsl_host_use_item("item:strawberry");PS_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","post-save")==0,"later XP commits independently of saved activation");PS_CHECK(bsl_host_progress_load()==0,"same-process load queues coherent reconstruction");stage=2;wait=0;
 }else if(stage==2||stage==3){
  p=hero();bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&learned);bsl_host_progress_probe(&saved);
  PS_CHECK(saved.loads==1&&!saved.pending&&bag.health==100&&bag.items==2&&xp.total==5000&&learned.learned,"loaded HP, player bag, XP and unlock agree with saved activation");
  PS_CHECK(fabs(p->a.x-120)<3&&p->a.grounded&&p->a.state==BOR_FREE,"native actor/body reconstruct at authored rest point");
  PS_CHECK(learned.receipts==1&&red->a.health>0,"only persistent objective receipts load; fresh encounter remains alive");
  uint64_t total=xp.total;PS_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0,"loaded objective receipt accepts replay without granting twice");bsl_host_stat_probe("stat:experience",&xp);PS_CHECK(xp.total==total,"replayed objective leaves character XP unchanged");
  PS_CHECK(saved.reward&&saved.choices==1,"selected switch and variable restore without event VM");
  if(stage==3){PS_CHECK(bsl_host_ability_use(p->lifetime,0)==0&&p->a.state==BOR_ACTION,"fresh-process learned ability executes actual BOR action");old_action=p->a.action_id;PS_CHECK(bsl_host_checkpoint_request()==0,"loaded rest point becomes the new rewind baseline");stage=4;wait=0;}else stage=99;
 }else if(stage==4){p=hero();bsl_host_gameplay_probe(&bag);PS_CHECK(bag.items==2&&bag.health==100&&p->a.state==BOR_FREE&&p->a.action_id>old_action,"subsequent rewind cancels learned action and retains loaded progress");
  obstaclemap_t* empty=obstaclemap_create();obstaclemap_build(empty);test_terrain=empty;physicsactor_t* unchanged=p->body;
  PS_CHECK(bsl_host_progress_load()==0,"valid disk load queues before spatial preflight");bsl_host_simulation_tick();test_terrain=NULL;obstaclemap_destroy(empty);bsl_host_progress_probe(&saved);PS_CHECK(saved.loads==1&&saved.failures==1&&p->body==unchanged,"unsupported disk destination refuses without replacing actor/body");
  checkpoint_park_enemies();PS_CHECK(checkpoint_defeat(p,green),"real defeat after refused disk load still enters normal recovery");stage=5;wait=0;
 }else if(stage==5){p=hero();bsl_host_gameplay_probe(&bag);BslCheckpointProbe checkpoint={0};bsl_host_checkpoint_probe(&checkpoint);PS_CHECK(bag.health==100&&bag.items==2&&checkpoint.rewinds==3&&checkpoint.failures==0,"disk-load refusal does not poison automatic checkpoint rewind");stage=99;}
 if(stage==99){const char* file=getenv("BSL_PROGRESS_REPORT");FILE* f=file?fopen(file,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef PS_CHECK
}
