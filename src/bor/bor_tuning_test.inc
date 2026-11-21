/* Author-reported regressions: exact free travel, defense cancels and timing. Signed: Atlas. */
static void control_tuning_checks(double floor){
 HostActor* p=hero();int tick=total_frames;
 const double positions[]={160,1024,4500,8000,-4500,-8000};bool clear_exact=true;
 for(unsigned at=0;at<sizeof(positions)/sizeof(*positions);at++)for(int facing=-1;facing<=1;facing+=2)for(int dy=-1;dy<=1;dy++){
  c2_fixture(floor);test_place(p,positions[at],floor-200);v2d_t from=v2d_new(p->a.x,p->a.y),candidate=v2d_new(from.x+facing*5.5,from.y+dy*500.0/60),resolved=soft_clip(p,from,candidate,false);
  if(resolved.x!=candidate.x||resolved.y!=candidate.y){clear_exact=false;logfile_message("CONTACT_DRIFT at=%.6f direction=%d dy=%d delta=%.9f,%.9f",positions[at],facing,dy,resolved.x-candidate.x,resolved.y-candidate.y);}
 }
 assertion(clear_exact,"unobstructed contact sweeps preserve the exact candidate at near/far/negative coordinates");
 obstaclemap_t* empty=obstaclemap_create();bool jump_keeps_speed=true;
 for(unsigned at=0;at<sizeof(positions)/sizeof(*positions);at++)for(int facing=-1;facing<=1;facing+=2){
  c2_fixture(floor);test_terrain=empty;test_place(p,positions[at],floor-200);motion_tangent(p,facing*330);c2_input(p,facing,IB_FIRE1,1);double takeoff=p->a.vx;move_actor(p);
  logfile_message("JUMP_MOMENTUM x=%.6f direction=%d takeoff=%.6f after=%.6f",positions[at],facing,takeoff,p->a.vx);
  if(fabs(takeoff-facing*330)>.001||fabs(p->a.vx)<320||p->a.vx*facing<=0)jump_keeps_speed=false;
 }
 c2_fixture(floor);test_terrain=empty;test_place(p,4500,floor-200);motion_tangent(p,330);c2_input(p,0,IB_FIRE1,1);double neutral_takeoff=p->a.vx;move_actor(p);assertion(neutral_takeoff==330&&p->a.vx>320&&p->a.vx<=330,"neutral takeoff preserves X; only gradual configured air drag/brake follows");c2_input(p,0,-1,1);double before_second=p->a.vx;c2_input(p,0,IB_FIRE1,1);assertion(p->a.vx==before_second&&p->jumps_used==2,"a second jump preserves current horizontal velocity exactly");
 assertion(jump_keeps_speed,"real jump launch preserves horizontal momentum through the first solver step everywhere");obstaclemap_destroy(empty);test_terrain=NULL;
 c2_fixture(floor);c2_input(p,0,IB_FIRE2,1);uint64_t attack=p->a.action_id;c2_guard(p,-1,IB_FIRE2,1);
 assertion(p->a.state==BOR_GUARD&&p->a.action_id!=attack&&p->a.facing==1,"LT guard safely cancels an attack instead of waiting through it");
 c2_fixture(floor);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames+=14;c2_input(p,1,-1,1);assertion(p->a.state==BOR_DASH,"a fourteen-tick second tap is accepted inside the relaxed fifteen-tick dash window");
 assertion(fabs(bor_animation(p->a.model,"combo1_h")->connect_window-2.0/3)<1e-12,"shipped confirmed-combo connect window increases from half a second to two thirds");
 // An accepted guard consumes outgoing offense and every continuation token.
 c2_fixture(floor);start(p,"attack1",BOR_ACTION);bor_advance(&p->a,.071,NULL,NULL);const BorFrame* outgoing=bor_frame(&p->a);uint64_t old_epoch=p->a.epoch;attack=p->a.action_id;p->a.combo=3;p->a.combo_until=.4;p->a.combo_owner=attack;p->armed_motion=true;c2_guard(p,-1,IB_FIRE2,1);
 assertion(old_epoch&&p->a.state==BOR_GUARD&&!p->a.active&&!p->a.epoch&&p->a.action_id!=attack&&p->a.combo==0&&p->a.combo_until==0&&p->a.combo_owner==0&&!p->armed_motion,"guard cancel invalidates the active attack and consumes old combo/gesture ownership");
 int target_hp=slots[1]->a.health;hit_segment(&p->a,outgoing,false,p);c2_input(p,0,-1,1);assertion(p->a.state==BOR_FREE&&slots[1]->a.health==target_hp,"old attack samples and successful guard release cannot deal or enqueue another hit");c2_input(p,0,IB_FIRE2,1);assertion(!strcmp(p->a.animation->name,"combo1_v")&&p->a.combo_stage==0,"the attack after a defensive cancel starts a fresh unconfirmed chain");
 c2_fixture(floor);BorModel reduced=*p->a.model;BorAnimation clips[128];int count=0;for(int i=0;i<reduced.count;i++)if(strcmp(reduced.animations[i].name,"armedguard"))clips[count++]=reduced.animations[i];reduced.animations=clips;reduced.count=count;p->a.model=&reduced;start(p,"attack1",BOR_ACTION);attack=p->a.action_id;p->a.combo=2;p->a.combo_until=.2;c2_guard(p,-1,IB_FIRE2,1);
 assertion(p->a.action_id==attack&&p->a.state==BOR_ACTION&&p->a.combo==2&&p->a.combo_until==.2,"missing guard animation leaves attack and combo ownership untouched");
 // Nonoffensive boxes alone are insufficient: a stationary guard must not launch/root-move.
 c2_fixture(floor);BorModel moving_guard=*p->a.model;BorAnimation guard_clips[128];int guard_index=-1;for(int i=0;i<moving_guard.count;i++){guard_clips[i]=moving_guard.animations[i];if(!strcmp(guard_clips[i].name,"armedguard"))guard_index=i;}moving_guard.animations=guard_clips;BorFrame guard_frame=guard_clips[guard_index].frames[0];guard_clips[guard_index].frames=&guard_frame;guard_clips[guard_index].count=1;guard_clips[guard_index].loop=0;guard_clips[guard_index].jump_frame=0;guard_clips[guard_index].jump_y=3;p->a.model=&moving_guard;start(p,"attack1",BOR_ACTION);attack=p->a.action_id;p->a.combo=2;p->a.combo_until=.2;double old_x=p->a.x,old_y=p->a.y;
 assertion(!guard_begin(p,1)&&p->a.action_id==attack&&p->a.combo==2&&p->a.combo_until==.2&&p->a.x==old_x&&p->a.y==old_y&&p->a.grounded,"motion-bearing guard jumpframe is rejected before any attack/state mutation");guard_clips[guard_index].jump_frame=-1;guard_frame.move=1;assertion(!guard_begin(p,1)&&p->a.action_id==attack&&p->a.combo_until==.2,"root-moving guard poses cannot bypass the stationary defensive-cancel contract");
 c2_fixture(floor);HostActor* red=slots[1];test_place(red,p->a.x+p->a.model->soft_radius+red->a.model->soft_radius,floor);bool linked=hold_begin(p,red,1);start(p,"grabattack",BOR_ACTION);p->a.freeze=red->a.freeze=0;c2_guard(p,-1,IB_FIRE2,1);
 assertion(linked&&p->a.state==BOR_GUARD&&p->a.partner<0&&red->a.partner<0&&red->a.state==BOR_PAIN,"guard cancellation of a grab attack safely releases both reciprocal participants");
 c2_fixture(floor);start(p,"attack1",BOR_ACTION);attack=p->a.action_id;p->frozen_tick=true;c2_guard(p,-1,IB_FIRE2,1);p->frozen_tick=false;p->a.freeze=.01;c2_guard(p,-1,IB_FIRE2,1);assertion(p->a.action_id==attack&&p->hold_ticks[0]==0,"guard cancel never advances qualification during either hitstop gate");p->a.freeze=0;c2_guard(p,-1,IB_FIRE2,1);assertion(p->a.state==BOR_GUARD,"guard cancel starts once hitstop ends without a fresh action press");
 bool protected_states=true;const BorState protected[]={BOR_PAIN,BOR_FALL,BOR_RISE,BOR_GRABBED,BOR_DEAD,BOR_DASH,BOR_WALL_JUMP,BOR_LEDGE,BOR_CLIMB,BOR_ROLL,BOR_POLE,BOR_LADDER,BOR_BARCLIMB};
 for(unsigned i=0;i<sizeof(protected)/sizeof(*protected);i++){c2_fixture(floor);p->a.state=protected[i];attack=p->a.action_id;if(guard_begin(p,1)||p->a.action_id!=attack)protected_states=false;}
 c2_fixture(floor);p->a.timeline_fault=true;if(guard_begin(p,1))protected_states=false;assertion(protected_states,"defensive cancels cannot escape reactions, traversal, death or timeline faults");
 c2_fixture(floor);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames+=16;c2_input(p,1,-1,1);assertion(p->a.state!=BOR_DASH,"a second tap outside fifteen ticks still expires without a delayed dash");
 // Isolated long floor: rolling is measured without slope, actor or camera-map interference.
 image_t* previous=image_drawing_target();image_t* im=image_create(4096,16);image_set_drawing_target(im);image_clear(color_rgb(255,255,255));image_set_drawing_target(previous);collisionmask_t* mask=collisionmask_create(im,0,0,4096,16,0);obstacle_t* flat=obstacle_create(mask,(point2d_t){-1000,(int)floor-1},OL_DEFAULT,0);obstaclemap_t* map=obstaclemap_create();obstaclemap_add(map,flat);obstaclemap_build(map);
 c2_fixture(floor);test_terrain=map;motion_tangent(p,400);start(p,"roll",BOR_ROLL);p->job=p->physics;p->down_held=true;p->roll_entry_speed=p->a.x;int rolling=0;for(;rolling<120&&p->a.state==BOR_ROLL;rolling++){move_actor(p);advance_host(p);}
 assertion(p->a.grounded&&p->a.state!=BOR_ROLL&&rolling<120,"roll friction terminates a 400px/s flat-floor roll within two seconds");
 obstaclemap_clear(map);obstacle_destroy(flat);obstaclemap_destroy(map);collisionmask_destroy(mask);image_destroy(im);test_terrain=NULL;
 p->physics.enabled=false;total_frames=tick;test_fixture(floor);
}
