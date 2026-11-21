/* One actual single-player defeat/rewind journey plus recovery boundary cases. Signed: Atlas. */
static void checkpoint_park_enemies(void){for(int i=0;i<64;i++)if(slots[i]&&!bor_is_player(&slots[i]->a)&&!slots[i]->a.model->training_dummy){HostActor* h=slots[i];h->a.x=h->a.spawn_x+1500;h->a.y=h->a.spawn_y;h->a.ai_wait=10000;commit_pose(h);rebase(h);motion_apply(h,BOR_MOTION_STOP,3,0,0);}}
static void checkpoint_place(HostActor* p,double x,double y){p->a.freeze=0;p->frozen_tick=false;start(p,"idle",BOR_FREE);p->a.x=x;p->a.y=y;commit_pose(p);rebase(p);motion_apply(p,BOR_MOTION_STOP,3,0,0);}
static bool checkpoint_strike(HostActor* p,HostActor* target,bool armed,int hp,int* base){double x=target->a.spawn_x,y=target->a.spawn_y;bool ok=progression_strike(p,target,armed,hp,base);target->a.spawn_x=x;target->a.spawn_y=y;target->a.initialized=true;return ok;}
static bool checkpoint_defeat(HostActor* p,HostActor* enemy){
 unsigned serial=enemy->a.serial;int model=enemy->a.model_index;double sx=enemy->a.spawn_x,sy=enemy->a.spawn_y;
 bor_init(&enemy->a,bor_models+model,model);enemy->a.serial=serial;enemy->a.spawn_x=sx;enemy->a.spawn_y=sy;enemy->a.initialized=true;enemy->placed=true;enemy->suspended=false;enemy->a.x=p->a.x+500;enemy->a.y=p->a.y;
 p->a.health=1;p->a.freeze=p->a.invulnerable=0;enemy->action_channel=0;
 if(!start(enemy,"attack1",BOR_ACTION))return false;
 for(int i=0;i<180&&!enemy->a.active&&!enemy->a.finished;i++)bor_advance_animation(&enemy->a,1.0/60,hit_segment,enemy);
 if(!enemy->a.active)return false;
 double x,y,w,h,tx,ty,tw,th;bor_rect(&enemy->a,true,&x,&y,&w,&h);bor_rect(&p->a,false,&tx,&ty,&tw,&th);enemy->a.x+=tx+tw*.5-x-w*.5;enemy->a.y+=ty+th*.5-y-h*.5;
 hit_segment(&enemy->a,bor_frame(&enemy->a),false,enemy);return p->a.health==0&&p->a.state==BOR_DEAD;
}
static void checkpoint_test_tick(void){
 if(!getenv("BSL_CHECKPOINT_TEST")||!bsl_gameplay_enabled())return;
 static int stage,wait,checks,failures;static HostActor *p,*red,*green,*alternate;static double tx,ty;static uint64_t old_action;static unsigned expected;
 if(++wait<60)return;
#define CP_CHECK(x,m) do{bool passed=(x);++checks;if(!passed)++failures;logfile_message("CHECKPOINT %s: %s",passed?"PASS":"FAIL",m);}while(0)
 BslCheckpointProbe c={0};BslGameplayProbe bag={0};BslStatProbe xp={0};BslLearningProbe ability={0};bsl_host_checkpoint_probe(&c);
 if(stage==0){
  p=hero();for(int i=0;i<64;i++)if(slots[i]){HostActor* h=slots[i];if(bor_is_player(&h->a)&&h!=p&&h->a.x<420)alternate=h;if(!strcmp(h->a.model->name,"red_grunt")&&h->a.spawn_x<1000)red=h;if(!strcmp(h->a.model->name,"Green_Grunt")&&h->a.spawn_x<1000)green=h;}
  CP_CHECK(p&&red&&green&&alternate&&c.enabled,"live checkpoint candidate and actor fixtures exist");if(!p||!red||!green||!alternate){stage=99;return;}
  CP_CHECK(!strcmp(c.chosen,"checkpoint:level_entry")&&!c.temporary,"settled level initializes an explicit entry fallback");
  CP_CHECK(bsl_host_checkpoint_activate(alternate->lifetime,"checkpoint:halfway")!=0,"inactive actor cannot capture player checkpoint");
  checkpoint_park_enemies();checkpoint_place(p,230,p->a.y);stage=1;wait=0;
 }else if(stage==1){
  p->a.health=70;int capture=bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway");if(capture)logfile_message("CHECKPOINT activation code=%d pos=%.2f,%.2f v=%.2f,%.2f state=%d grounded=%d",capture,p->a.x,p->a.y,p->a.vx,p->a.vy,p->a.state,p->a.grounded);CP_CHECK(capture==0,"nearby standing player activates authored temporary checkpoint");tx=p->a.x;ty=p->a.y;
  int base=0;CP_CHECK(checkpoint_strike(p,red,true,1,&base),"post-checkpoint combat defeats actual enemy");
  CP_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0&&bsl_host_use_item("item:strawberry")==0,"post-checkpoint objective XP and consumable commit");bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&ability);bsl_host_gameplay_probe(&bag);
  CP_CHECK(xp.total==5000&&ability.learned&&bag.items==2,"progress after capture includes unlock and reduced bag");
  p->a.freeze=0;p->frozen_tick=false;p->a.health=50;start(p,"idle",BOR_FREE);start(p,"attack1",BOR_ACTION);old_action=p->a.action_id;
  CP_CHECK(bsl_host_anchor_start(0xabc1,"event:gameplay_wait",p->lifetime)==0,"owned waiting event starts alongside committed action");bsl_host_simulation_tick();BslAnchorRun run={0};bsl_host_anchor_probe(0xabc1,&run);CP_CHECK(run.status==2,"real event runner is waiting before defeat");
  physicsactor_lock_horizontally_for(p->body,10);CP_CHECK(checkpoint_defeat(p,green),"real enemy attack defeats player during action/event wait");bsl_host_anchor_probe(0xabc1,&run);CP_CHECK(run.status==4,"death hit cancels owned event continuation");stage=2;wait=0;
 }else if(stage==2){
  bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&ability);bsl_host_checkpoint_probe(&c);
  CP_CHECK(c.rewinds==1&&!strcmp(c.restored,"checkpoint:halfway")&&c.failures==0,"automatic defeat resolves temporary checkpoint once");
  if(c.rewinds!=1){stage=99;wait=60;return;}
  CP_CHECK(bag.health==70&&bag.items==3&&xp.total==0&&!ability.learned&&ability.receipts==0,"HP, bag, growth, learned ability and reward receipts rewind together");
  CP_CHECK(fabs(p->a.x-tx)<2&&fabs(p->a.y-ty)<6&&p->a.state==BOR_FREE&&p->a.partner<0&&p->a.action_id>old_action,"native actor returns safely with fresh FSM and monotonic action identity");
  CP_CHECK(physicsactor_hlock_timer(p->body)==0&&physicsactor_get_movmode(p->body)==MM_FLOOR,"fresh body discards horizontal lock and non-floor movement state");
  BslAnchorRun run={0};bsl_host_anchor_probe(0xabc1,&run);CP_CHECK(run.status==-1&&red->a.health>0,"event VM run is absent and post-checkpoint defeated enemy reconstructs alive");
  checkpoint_park_enemies();int base=0;CP_CHECK(checkpoint_strike(p,red,true,1,&base)&&bsl_host_reward(p->lifetime,"source:lab_exploration","lab/exploration")==0,"rolled-back enemy and objective rewards can be earned again");
  p->a.freeze=0;p->frozen_tick=false;start(p,"idle",BOR_FREE);p->a.health=40;bsl_host_use_item("item:strawberry");
  // A script receipt resembling an enemy key must never masquerade as that enemy's defeat.
  char receipt[64];snprintf(receipt,sizeof receipt,"defeat/%llu",(unsigned long long)green->lifetime);CP_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration",receipt)==0,"trusted scripted reward accepts its own receipt identity");
  checkpoint_park_enemies();checkpoint_place(p,120,p->a.y);stage=3;wait=0;
 }else if(stage==3){
  p->a.health=40;CP_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&p->a.health==100,"permanent activation recovers HP before capturing rewind state");bsl_host_checkpoint_probe(&c);
  CP_CHECK(!strcmp(c.chosen,"checkpoint:rest")&&c.save_offer,"authored permanent priority wins and emits separate save offer");
  p->a.health=25;bsl_host_use_item("item:strawberry");CP_CHECK(checkpoint_defeat(p,green),"second real defeat follows further item consumption");stage=4;wait=0;
 }else if(stage==4){
  bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);bsl_host_learning_probe(&ability);bsl_host_checkpoint_probe(&c);
  CP_CHECK(c.rewinds==2&&!strcmp(c.restored,"checkpoint:rest")&&bag.health==100&&bag.items==2&&xp.total==7500&&ability.learned,"permanent rewind restores activation-era recovery, bag and earned progression");
  CP_CHECK(red->a.health==0&&green->a.health>0,"typed native defeat receipt keeps old enemy defeated; script-name collision cannot kill another enemy");
  checkpoint_park_enemies();checkpoint_place(alternate,200,p->a.y);stage=40;wait=0;
 }else if(stage==40){
  int selected=bsl_host_select_character("character:michio_alternate");
  if(selected)logfile_message("CHECKPOINT switch code=%d primary ready=%d state=%d ground=%d velocity=%.2f,%.2f; alternate ready=%d state=%d ground=%d velocity=%.2f,%.2f at=%.2f,%.2f",selected,bsl_bor_switch_ready(p->lifetime),p->a.state,p->a.grounded,p->a.vx,p->a.vy,bsl_bor_switch_ready(alternate->lifetime),alternate->a.state,alternate->a.grounded,alternate->a.vx,alternate->a.vy,alternate->a.x,alternate->a.y);
  CP_CHECK(selected==0,"alternate can take control after recovery");
  CP_CHECK(bsl_host_checkpoint_request()==0,"manual rewind queues at a controlled simulation boundary");stage=5;wait=0;
 }else if(stage==5){
  p=hero();bsl_host_gameplay_probe(&bag);bsl_host_checkpoint_probe(&c);
  CP_CHECK(c.rewinds==3&&!strcmp(bag.character,"player/1/character:michio")&&bag.items==2&&bag.health==100,"rewind restores selected character and player bag without redirecting ownership");
  bsl_engine_restart();stage=6;wait=0;
 }else if(stage==6){
  p=hero();bsl_host_checkpoint_probe(&c);CP_CHECK(!c.temporary&&!strcmp(c.chosen,"checkpoint:rest"),"actual level unload expires temporary memory but retains matching permanent activation");
  // Travel farther than a brick ROI margin but below the camera's auto-teleport threshold.
  checkpoint_place(p,120+video_get_screen_size().x*1.4,p->a.y);checkpoint_rebase_camera(p);CP_CHECK(bsl_host_checkpoint_request()==0,"off-camera permanent rewind requested");stage=7;wait=0;
 }else if(stage==7){
  p=hero();bsl_host_checkpoint_probe(&c);CP_CHECK(c.rewinds==4&&c.failures==0&&fabs(p->a.x-120)<3&&p->a.grounded,"off-camera rewind synchronizes camera/terrain before subsequent native physics");
  BslActorRewind bad_plan[64];unsigned count=0;int player_index=-1,other_index=-1;
  for(int i=0;i<64;i++)if(slots[i]){bad_plan[count]=(BslActorRewind){slots[i]->lifetime,slots[i]->a.health,slots[i]->a.x,slots[i]->a.y};if(slots[i]==p)player_index=(int)count;else if(slots[i]->a.health>0&&slots[i]->a.model->soft_solid)other_index=(int)count;count++;}
  if(player_index>=0&&other_index>=0){bad_plan[other_index].x=bad_plan[player_index].x;bad_plan[other_index].y=bad_plan[player_index].y;physicsactor_t* original=p->body;BslRewindPlan* rejected=bsl_bor_rewind_prepare(bad_plan,count,p->lifetime);CP_CHECK(!rejected&&p->body==original,"overlapping final living destinations are refused before body/service commit");bsl_bor_rewind_discard(rejected);}
  checkpoint_park_enemies();checkpoint_place(p,230,p->a.y);p->a.grounded=true;CP_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"lower-priority halfway point is available as a distinct fallback");
  // Controlled terrain edit blocks the preferred station; every other actor still has supported ground.
  image_t* previous=image_drawing_target();image_t* im=image_create(1024,128);image_set_drawing_target(im);image_clear(color_rgb(255,255,255));image_set_drawing_target(previous);
  collisionmask_t* floor_mask=collisionmask_create(im,0,0,1024,8,0);collisionmask_t* wall_mask=collisionmask_create(im,0,0,256,128,0);
  int bw,bh;v2d_t body_center;physicsactor_bounding_box(p->body,&bw,&bh,&body_center);int floor=(int)ceil(body_center.y+bh*.5)+1;obstacle_t* ground[8];obstacle_t* wall=obstacle_create(wall_mask,(point2d_t){0,floor-128},OL_DEFAULT,0);obstaclemap_t* map=obstaclemap_create();for(int i=0;i<8;i++){ground[i]=obstacle_create(floor_mask,(point2d_t){i*1024,floor},OL_DEFAULT,0);obstaclemap_add(map,ground[i]);}obstaclemap_add(map,wall);obstaclemap_build(map);test_terrain=map;
  bsl_host_checkpoint_request();bsl_host_simulation_tick();bsl_host_checkpoint_probe(&c);CP_CHECK(c.rewinds==5&&!strcmp(c.restored,"checkpoint:halfway")&&p->a.x>256,"blocked preferred station falls back to a clear lower-priority checkpoint");
  obstaclemap_clear(map);obstaclemap_build(map);physicsactor_t* unchanged=p->body;bsl_host_gameplay_probe(&bag);int before=bag.items;
  bsl_host_checkpoint_request();bsl_host_simulation_tick();bsl_host_checkpoint_probe(&c);bsl_host_gameplay_probe(&bag);CP_CHECK(c.rewinds==5&&c.failures==1&&p->body==unchanged&&bag.items==before,"no supported candidate refuses before changing services or actor body");
  test_terrain=NULL;obstaclemap_destroy(map);obstacle_destroy(wall);for(int i=0;i<8;i++)obstacle_destroy(ground[i]);collisionmask_destroy(wall_mask);collisionmask_destroy(floor_mask);image_destroy(im);
  expected=c.rewinds;reset_all();bsl_host_checkpoint_probe(&c);bsl_host_gameplay_probe(&bag);bsl_host_stat_probe("stat:experience",&xp);CP_CHECK(c.rewinds==0&&!c.temporary&&xp.total==0&&bag.items==3,"fresh disposable run clears checkpoint memory and earned progress");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_CHECKPOINT_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"successful_rewinds\":%u,\"temporary_x\":%.2f,\"temporary_y\":%.2f}\n",failures?"false":"true",checks,failures,expected,tx,ty);fclose(f);}engine_quit();}
#undef CP_CHECK
}
