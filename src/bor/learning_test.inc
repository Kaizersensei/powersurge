/* Real combat -> named XP reward -> learned BOR action -> actor replacement. Signed: Atlas. */
static void learning_test_tick(void){
 if(!getenv("BSL_LEARNING_TEST")||!bsl_gameplay_enabled())return;
 static int stage,wait,checks,failures,ability_base,ability_damage;static uint64_t original,retained;
 static HostActor *p,*t,*other,*alternate;BslStatProbe q={0};BslLearningProbe learned={0};
 if(++wait<45)return;
#define LEARN_CHECK(x,m) do{bool passed=(x);++checks;if(!passed)++failures;logfile_message("LEARNING %s: %s",passed?"PASS":"FAIL",m);}while(0)
 if(stage==0){
  p=hero();for(int i=0;i<64;i++)if(slots[i]&&slots[i]!=p){HostActor* h=slots[i];if(bor_is_player(&h->a)&&h->a.x<420)alternate=h;else if(!strcmp(h->a.model->name,"red_grunt")&&h->a.x<1000)t=h;else if(!strcmp(h->a.model->name,"Green_Grunt")&&h->a.x<1000)other=h;}
  LEARN_CHECK(p&&t&&other&&alternate,"course contains live character bindings and additional enemy fixtures");if(!p||!t||!other||!alternate){stage=99;return;}
  original=p->lifetime;bsl_host_stat_probe("stat:experience",&q);bsl_host_learning_probe(&learned);
  LEARN_CHECK(q.units==1&&q.total==0&&!learned.learned&&learned.receipts==0,"new run starts with base character level and locked additional ability");
  LEARN_CHECK(bsl_host_ability_use(p->lifetime,0)==3&&p->a.state==BOR_FREE,"locked ability cannot start an action");
  LEARN_CHECK(bsl_host_reward(alternate->lifetime,"source:lab_exploration","invalid/inactive")!=0,"inactive actor cannot claim a player-controlled script reward");
  LEARN_CHECK(bsl_host_reward(p->lifetime,"source:enemy_red_grunt","invalid/source")!=0,"trusted script hook cannot fabricate a native enemy-defeat source");
  int base=0;LEARN_CHECK(progression_strike(p,t,true,1,&base),"existing armed attack still works while additional ability is locked");
  bsl_host_stat_probe("stat:experience",&q);bsl_host_learning_probe(&learned);
  LEARN_CHECK(t->a.health==0&&q.total==2500&&q.units==1&&learned.receipts==1,"actual accepted enemy defeat credits character XP once");
  bsl_host_combat_result(p->lifetime,p->a.action_id,t->lifetime,p->a.epoch,1,true,1);bsl_host_stat_probe("stat:experience",&q);
  LEARN_CHECK(q.total==2500,"duplicate combat outcome does not grant another enemy reward");
  LEARN_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0,"separate registered objective grants noncombat XP");
  bsl_host_stat_probe("stat:experience",&q);bsl_host_learning_probe(&learned);
  LEARN_CHECK(q.total==5000&&q.units==2&&learned.learned&&learned.receipts==2,"objective and defeat rewards jointly cross authored unlock threshold");
  LEARN_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0,"repeating objective returns its existing receipt");bsl_host_stat_probe("stat:experience",&q);
  LEARN_CHECK(q.total==5000,"repeated objective does not duplicate XP");
  LEARN_CHECK(bsl_host_ability_use(p->lifetime,0)!=0,"newly learned ability cannot interrupt committed attack or hitstop");
  p->a.freeze=0;p->frozen_tick=false;p->a.grounded=true;start(p,"idle",BOR_FREE);
  unsigned serial=other->a.serial;int model=other->a.model_index;bor_init(&other->a,bor_models+model,model);other->a.serial=serial;other->a.grounded=true;other->a.health=10000;other->placed=true;other->suspended=false;other->a.x=p->a.x+500;other->a.y=p->a.y;
  bsl_host_stat_probe("stat:armed",&q);retained=q.total;
  LEARN_CHECK(bsl_host_ability_use(p->lifetime,0)==0&&!strcmp(p->a.animation->name,"attack5")&&p->a.state==BOR_ACTION,"earned ability starts its actual registered BOR animation through normal FSM");
  LEARN_CHECK(bsl_host_ability_use(p->lifetime,0)!=0,"ability cannot restart its own committed timeline");
  for(int i=0;i<180&&!p->a.active&&!p->a.finished;i++)bor_advance_animation(&p->a,1.0/60,hit_segment,p);
  LEARN_CHECK(p->a.active,"learned ability uses the real animation attack windows");
  ability_base=bor_frame(&p->a)->damage;double x,y,w,h,tx,ty,tw,th;bor_rect(&p->a,true,&x,&y,&w,&h);bor_rect(&other->a,false,&tx,&ty,&tw,&th);other->a.x+=x+w*.5-tx-tw*.5;other->a.y+=y+h*.5-ty-th*.5;render_frame(other);hit_segment(&p->a,bor_frame(&p->a),false,p);ability_damage=10000-other->a.health;
  bsl_host_stat_probe("stat:armed",&q);LEARN_CHECK(ability_damage==ability_base+1&&q.total==retained,"registered ability tag uses authored damage formula without inheriting primary practice awards");
  other->a.x=p->a.x+500;other->a.y=p->a.y;commit_pose(other);rebase(other);
  p->a.freeze=0;p->frozen_tick=false;p->a.invulnerable=0;start(p,"idle",BOR_FREE);p->a.vx=p->a.vy=0;rebase(p);motion_apply(p,BOR_MOTION_STOP,3,0,0);
  p->a.health=70;LEARN_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==75,"inventory use remains tied to stable player and current character HP");
  stage=1;wait=0;
 }else if(stage==1){
  LEARN_CHECK(bsl_host_select_character("character:michio_alternate")==0,"safe switch to alternate character succeeds");bsl_host_stat_probe("stat:experience",&q);bsl_host_learning_probe(&learned);
  LEARN_CHECK(q.total==0&&!learned.learned,"character XP and ability unlock do not leak to another profile");
  LEARN_CHECK(bsl_host_reward(alternate->lifetime,"source:lab_exploration","lab/exploration")==4,"same objective receipt cannot redirect its credit after character switch");
  LEARN_CHECK(bsl_host_select_character("character:michio")==0,"return to original character before level recreation");
  bsl_engine_restart();stage=2;wait=0;
 }else if(stage==2){
  p=hero();bsl_host_stat_probe("stat:experience",&q);bsl_host_learning_probe(&learned);
  LEARN_CHECK(p&&p->lifetime!=original&&q.total==5000&&q.units==2&&learned.learned&&learned.receipts==2,"character progression, learned action and reward receipts survive real level recreation");
  LEARN_CHECK(bsl_host_ability_use(original,0)!=0&&bsl_host_reward(original,"source:lab_exploration","invalid/stale")!=0,"destroyed actor lifetime loses both action and reward authority");
  LEARN_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0,"objective receipt survives actor lifetime replacement");
  BslGameplayProbe bag={0};bsl_host_gameplay_probe(&bag);LEARN_CHECK(bag.items==2&&bag.health==75,"player inventory and character HP remain coherent with retained unlocks");
  p->a.freeze=0;p->frozen_tick=false;start(p,"idle",BOR_FREE);LEARN_CHECK(bsl_host_ability_use(p->lifetime,0)==0,"replacement actor can execute retained learned action");
  reset_all();bsl_host_stat_probe("stat:experience",&q);bsl_host_learning_probe(&learned);
  LEARN_CHECK(q.total==0&&q.units==1&&!learned.learned&&learned.receipts==0,"explicit fresh-run reset clears earned stats, abilities and reward receipts together");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_LEARNING_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"ability_base\":%d,\"ability_damage\":%d,\"character_xp_units\":5000,\"practice_retained\":%llu}\n",failures?"false":"true",checks,failures,ability_base,ability_damage,(unsigned long long)retained);fclose(f);}engine_quit();}
#undef LEARN_CHECK
}
