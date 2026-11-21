/* Native regressions exercise actual movement, input and lifecycle. Signed: Atlas. */
static void contact_checks(double floor){
 HostActor* p=hero();HostActor* red=slots[1];int tick=total_frames;ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));
 c2_fixture(floor);const double gap=p->a.model->soft_radius+red->a.model->soft_radius;
 test_place(red,p->a.x+gap+8,floor);double other_x=red->a.x;
 for(int i=0;i<15;i++){drive(p,1,p->physics.speed);move_actor(p);}
 assertion(p->a.x<=other_x-gap+.01&&red->a.x==other_x,"soft contact clamps only the mover and never shoves its neighbour");
 c2_fixture(floor);test_place(red,p->a.x+gap,floor);for(int i=0;i<3;i++){drive(p,1,p->physics.speed);move_actor(p);}
 assertion(p->a.state==BOR_GRAB&&red->a.state==BOR_GRABBED&&linked_partner(p)==red,"sustained contact pressure acquires the capability-gated hold without an action button");
 assertion(fabs(red->a.x-p->a.x-gap)<.01,"hold retains the contact alignment rather than snapping through bodies");unlink_actor(p);
 c2_fixture(floor);test_place(red,p->a.x+gap,floor);start(p,"attack1",BOR_ACTION);uint64_t action=p->a.action_id;for(int i=0;i<12;i++){p->input_direction=1;pressure_hold(p);}assertion(p->a.action_id==action&&p->a.partner<0,"automatic hold never interrupts a committed attack");
 // The same capability is available to enemy/NPC templates once they have the required pose.
 c2_fixture(floor);BorModel npc=*red->a.model;BorAnimation clips[128];for(int i=0;i<npc.count;i++)clips[i]=npc.animations[i];clips[npc.count]=*bor_animation(red->a.model,"idle");clips[npc.count].name="grab";npc.count++;npc.animations=clips;npc.hold_mask=BOR_TYPE_PLAYER;const BorModel* original=red->a.model;red->a.model=&npc;test_place(red,p->a.x+gap,floor);
 assertion(hold_begin(red,p,-1)&&p->a.state==BOR_GRABBED,"an enemy with a grab pose and player hold mask uses the shared hold controller");double old_time=world_time;world_time+=.51;enemy_actions(red,1.0/60);assertion(red->a.state==BOR_FREE&&p->a.partner<0&&p->a.state==BOR_PAIN,"an AI holder without follow-up moves releases its victim after the hold timeout");world_time=old_time;red->a.model=original;
 c2_fixture(floor);red->a.model=&npc;npc.hold_mask=BOR_TYPE_ENEMY;test_place(red,p->a.x+gap,floor);assertion(!hold_begin(red,p,-1),"hold faction policy is independent and rejects unsupported victims");red->a.model=original;
 c2_fixture(floor);test_place(red,p->a.x+gap+2,floor);v2d_t from=v2d_new(p->a.x,p->a.y),to=v2d_new(p->a.x+100,p->a.y);v2d_t clipped=soft_clip(p,from,to,false);assertion(clipped.x<=red->a.x-gap+.01&&red->a.x==from.x+gap+2,"high-speed sweeps cannot tunnel through another actor");
 test_place(p,red->a.x-2,floor);from=v2d_new(p->a.x,p->a.y);clipped=soft_clip(p,from,v2d_new(from.x-10,from.y),false);assertion(fabs(clipped.x-from.x+10)<.01,"pre-existing overlaps can separate without an emergency relocation");clipped=soft_clip(p,from,v2d_new(from.x+1,from.y),false);assertion(clipped.x==from.x,"pre-existing overlap cannot deepen toward the actor centre");
 // Descend onto a head, carry briefly, then release; jumping and deletion are safe.
 c2_fixture(floor);test_place(red,200,floor);double head=floor-red->a.model->soft_height;c2_air(p,200,head-1,0,120);move_actor(p);assertion(p->a.grounded&&p->actor_support==red->lifetime&&fabs(p->a.y-head)<.1,"descending feet can use an actor as a temporary one-way platform");
 double x=p->a.x;test_place(red,204,floor);move_actor(p);assertion(p->a.grounded&&fabs(p->a.x-x-4)<.1,"temporary actor platform carries its passenger without shoving other actors");
 for(int i=0;i<10&&p->a.state!=BOR_FREE;i++){advance_host(p);move_actor(p);}
 c2_input(p,0,IB_FIRE1,1);assertion(!p->actor_support&&!p->a.grounded&&p->a.vy==-500,"jump launches once from temporary actor support and releases ownership");
 c2_fixture(floor);test_place(red,200,floor);c2_air(p,200,head-1,0,120);move_actor(p);for(int i=0;i<16;i++)move_actor(p);assertion(!p->actor_support&&!p->a.grounded&&p->ignored_support==red->lifetime,"actor support expires and passes through rather than creating permanent stacks");
 c2_fixture(floor);test_place(red,200,floor);c2_air(p,200,head-1,0,120);move_actor(p);red->suspended=true;move_actor(p);assertion(!p->actor_support,"suspending a support releases its passenger safely");red->suspended=false;
 c2_fixture(floor);test_place(red,200,floor);c2_air(p,200,head-1,0,120);move_actor(p);uint64_t life=red->lifetime;red->lifetime=++actor_lifetime;move_actor(p);assertion(!p->actor_support,"a reused registry slot cannot steal a platform attachment");red->lifetime=life;
 // Contact correction must never replace Surge's safe pose with terrain penetration.
 image_t* previous=image_drawing_target();image_t* im=image_create(256,128);image_set_drawing_target(im);image_clear(color_rgba(0,0,0,0));for(int x=0;x<256;x++)image_line(x,20+x/4,x,127,color_rgb(255,255,255));image_set_drawing_target(previous);
 collisionmask_t* mask=collisionmask_create(im,0,0,256,128,0);obstacle_t* ramp=obstacle_create(mask,(point2d_t){100,(int)floor-128},OL_DEFAULT,0);obstaclemap_t* map=obstaclemap_create();obstaclemap_add(map,ramp);obstaclemap_build(map);
 c2_fixture(floor);test_terrain=map;bool ramp_case=false;
 for(int x=140;x<300&&!ramp_case;x++)for(int y=(int)floor-125;y<(int)floor&&!ramp_case;y++){
  test_place(p,x-8,y-4);test_place(red,x+gap-4,y);v2d_t from=v2d_new(p->a.x,p->a.y),candidate=v2d_new(x,y);
  if(!body_clear(p,map,from.x,from.y)||!body_clear(p,map,candidate.x,candidate.y))continue;
  v2d_t unsafe=soft_clip(p,from,candidate,false);if(body_clear(p,map,unsafe.x,unsafe.y))continue;
  v2d_t safe=contact_resolve(p,from,candidate);ramp_case=body_clear(p,map,safe.x,safe.y)&&fabs(safe.x-candidate.x)<.01&&fabs(safe.y-candidate.y)<.01;
 }
 assertion(ramp_case,"descending-ramp contact preserves the safe terrain result instead of moving back into the ramp");
 // Tiny real constraints must retain their explicit identity; free travel is exact.
 c2_fixture(floor);test_terrain=map;v2d_t tiny_from=v2d_new(80,floor-140),tiny_candidate=v2d_new(80.0003,floor-140);test_place(p,tiny_candidate.x,tiny_candidate.y);test_place(red,81,floor-140);v2d_t tiny=contact_resolve(p,tiny_from,tiny_candidate);
 assertion(tiny.x==tiny_from.x&&fabs(tiny.x-tiny_candidate.x)>0&&fabs(tiny.x-tiny_candidate.x)<.001&&p->contact_x_blocked&&body_clear(p,map,tiny.x,tiny.y),"a true sub-.001 contact constraint stays distinct from untouched free travel and remains terrain-clear");
 obstaclemap_clear(map);obstacle_t* corner=obstacle_create(mask,(point2d_t){100,(int)floor-128},OL_DEFAULT,0);obstaclemap_add(map,corner);obstaclemap_build(map);
 c2_fixture(floor);test_terrain=map;test_place(p,80,floor-140);test_place(red,80+gap,floor-140);v2d_t old=v2d_new(p->a.x,p->a.y),candidate=v2d_new(84,floor-140),safe=contact_resolve(p,old,candidate);assertion(body_clear(p,map,safe.x,safe.y)&&fabs(safe.x-old.x)<.01,"corner contact veto stays in clear space and does not shove the actor toward terrain");
 obstaclemap_clear(map);obstacle_destroy(corner);obstacle_destroy(ramp);obstaclemap_destroy(map);collisionmask_destroy(mask);image_destroy(im);test_terrain=NULL;
 // Guard facing/chord release and held-action reentry.
 c2_fixture(floor);c2_guard(p,-1,IB_FIRE2,1);c2_guard(p,0,IB_FIRE2,1);assertion(p->a.state==BOR_GUARD&&p->a.facing==1,"releasing direction keeps LT guard without turning");c2_input(p,0,IB_FIRE2,1);assertion(p->a.state==BOR_FREE&&p->a.facing==1,"releasing LT exits guard without an attack");c2_guard(p,0,IB_FIRE2,1);assertion(p->a.state==BOR_GUARD,"LT can re-enter guard while the action remains held");
 c2_fixture(floor);c2_input(p,0,IB_FIRE2,1);action=p->a.action_id;c2_guard(p,-1,IB_FIRE2,1);assertion(p->a.action_id!=action&&p->a.state==BOR_GUARD&&p->a.facing==1,"LT held during an attack safely cancels to guard without turning");
 // Direction edges sampled through recovery: two new taps suffice after it.
 c2_fixture(floor);c2_input(p,1,-1,1);start(p,"attack1",BOR_ACTION);c2_input(p,0,-1,1);total_frames+=20;start(p,"idle",BOR_FREE);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames+=5;c2_input(p,1,-1,1);assertion(p->a.state==BOR_DASH,"release edges observed during recovery prevent the old third-tap dash bug");
 c2_fixture(floor);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames+=5;c2_input(p,1,IB_FIRE2,1);assertion(!strcmp(p->a.animation->name,"runattack"),"same-sample second direction tap and sword choose the BOR dash attack");
 // Pressure ownership follows an actor lifetime, not a reusable registry index.
 c2_fixture(floor);test_place(red,p->a.x+gap,floor);p->input_direction=1;pressure_hold(p);pressure_hold(p);life=red->lifetime;red->lifetime=++actor_lifetime;pressure_hold(p);assertion(p->contact_ticks==1&&p->a.partner<0,"replacing a contact target resets its pressure dwell instead of inheriting the old slot timer");red->lifetime=life;
 c2_fixture(floor);p->frozen_tick=true;c2_guard(p,-1,IB_FIRE2,1);assertion(p->hold_ticks[0]==0,"the final frozen hitstop tick cannot qualify a guard chord");p->frozen_tick=false;c2_guard(p,-1,IB_FIRE2,1);assertion(p->a.state==BOR_GUARD,"explicit guard starts immediately after hitstop ends");
 // A one-pixel wall can fit between fixed Surge bodies but must block a wider soft-body hold.
 previous=image_drawing_target();im=image_create(1,128);image_set_drawing_target(im);image_clear(color_rgb(255,255,255));image_set_drawing_target(previous);mask=collisionmask_create(im,0,0,1,128,0);obstacle_t* thinwall=obstacle_create(mask,(point2d_t){116,(int)floor-128},OL_DEFAULT,0);map=obstaclemap_create();obstaclemap_add(map,thinwall);obstaclemap_build(map);
 c2_fixture(floor);test_terrain=map;npc=*red->a.model;npc.soft_radius=24;original=red->a.model;red->a.model=&npc;test_place(p,100,floor-10);test_place(red,132,floor-10);
 assertion(body_clear(p,map,p->a.x,p->a.y)&&body_clear(red,map,red->a.x,red->a.y)&&!hold_begin(p,red,1),"individually clear actors separated by a thin wall cannot acquire a hold through it");red->a.model=original;
 obstaclemap_clear(map);obstacle_destroy(thinwall);obstaclemap_destroy(map);collisionmask_destroy(mask);image_destroy(im);test_terrain=NULL;
 // Native ranged action owns its timeline while Surge retains motion authority.
 c2_fixture(floor);motion_tangent(p,180);input_simulate_button_press(controls,IB_FIRE5);player_actions(p,&keys);input_reset(controls);double before=p->a.x;move_actor(p);assertion(!strcmp(p->a.animation->name,"c2_shoot")&&p->a.x>before&&fabs(p->a.vx-180)<1,"grounded ranged action preserves the existing movement momentum");
 c2_fixture(floor);c2_air(p,100,floor-200,180,0);input_simulate_button_press(controls,IB_FIRE5);player_actions(p,&keys);input_reset(controls);move_actor(p);assertion(!strcmp(p->a.animation->name,"c2_shoot")&&fabs(p->a.vx-180)<1,"airborne ranged action preserves horizontal momentum instead of braking it");
 // Speed-driven four-phase sequence, independent of held direction.
 const double speeds[]={0,30,200,330,380};const int phases[]={0,1,2,2,3};bool sequence=true;
 for(int i=0;i<5;i++){c2_fixture(floor);motion_tangent(p,speeds[i]);locomotion_update(p);if(p->locomotion!=phases[i])sequence=false;}
 assertion(sequence,"actual speed selects idle/walk/run/sprint; sprint begins only above the soft cap");
 c2_fixture(floor);motion_tangent(p,200);c2_input(p,0,-1,1);assertion(p->locomotion==2&&!strcmp(p->a.animation->name,"c2_run"),"coasting keeps the run pose without directional input");motion_tangent(p,0);c2_input(p,1,-1,1);assertion(p->locomotion==0,"pressing a direction at rest does not invent horizontal speed");
 c2_fixture(floor);start(p,"dash",BOR_DASH);locomotion_update(p);assertion(p->locomotion==3,"explicit dash selects sprint even before the next physics step");
 p->physics.enabled=false;total_frames=tick;test_fixture(floor);
}
