/* Real UI scene helpers (enabled only by BOR_POC_LEDGE_UI). Signed: Atlas. */
static bool ledge_ui_started;
static surgescript_object_t* moving_fixture(void){
 surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_var_t* name=surgescript_var_set_string(surgescript_var_create(),"BOR Moving Ledge");const surgescript_var_t* args[]={name};surgescript_var_t* result=surgescript_var_create();surgescript_object_call_function(level,"findEntity",args,1,result);
 surgescript_objectmanager_t* manager=surgescript_object_manager(level);surgescript_objecthandle_t handle=surgescript_var_get_objecthandle(result);surgescript_object_t* found=handle&&surgescript_objectmanager_exists(manager,handle)?surgescript_objectmanager_get(manager,handle):NULL;surgescript_var_destroy(name);surgescript_var_destroy(result);return found;
}
static bool prepare_ledge_ui(const ALLEGRO_KEYBOARD_STATE* keys){
 if(!getenv("BOR_POC_LEDGE_UI")||ledge_ui_started)return false;
 HostActor* p=hero();surgescript_object_t* fixture=moving_fixture();if(!p||!fixture)return true;
 surgescript_object_t* brick=surgescript_objectmanager_get(surgescript_object_manager(fixture),surgescript_object_child(fixture,"Brick"));
 const obstacle_t* support=obstaclemap_find_by_id(terrain_map(),scripting_brick_identity(brick),OL_DEFAULT);if(!support)return true;
 point2d_t at=obstacle_get_position(support);p->a.x=at.x-20;p->a.y=at.y+40;commit_pose(p);rebase(p);physicsactor_detach_from_ground(p->body);p->a.grounded=false;p->was_ground=false;motion_apply(p,BOR_MOTION_SET,3,0,10);if(strcmp(p->a.animation->name,"jump"))start(p,"jump",BOR_ACTION);
 if(down(keys,ALLEGRO_KEY_D)||input_button_down(controls,IB_RIGHT)){ledge_ui_started=true;return false;}
 return true;
}
/* Real native profile/input/terrain regression fixtures. Signed: Atlas. */
static void c2_fixture(double floor){
 test_terrain=NULL;hero()->physics.enabled=true;test_fixture(floor);
 for(int i=1;i<5;i++){slots[i]->a.x=1800+i*100;commit_pose(slots[i]);rebase(slots[i]);}
}
static void c2_input(HostActor* p,int direction,int button,int ticks){
 ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));
 for(int i=0;i<ticks;i++){input_reset(controls);if(direction)input_simulate_button_down(controls,direction>0?IB_RIGHT:IB_LEFT);if(button>=0)input_simulate_button_down(controls,button);player_actions(p,&keys);}
 input_reset(controls);
}
/* Explicit modifier helper; ordinary direction/attack tests stay unmodified. */
static void c2_guard(HostActor* p,int direction,int button,int ticks){
 ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));
 for(int i=0;i<ticks;i++){input_reset(controls);input_simulate_button_down(controls,IB_FIRE7);if(direction)input_simulate_button_down(controls,direction>0?IB_RIGHT:IB_LEFT);if(button>=0)input_simulate_button_down(controls,button);player_actions(p,&keys);}
 input_reset(controls);
}
static void c2_air(HostActor* p,double x,double y,double vx,double vy){
 test_place(p,x,y);physicsactor_detach_from_ground(p->body);p->a.grounded=false;p->was_ground=false;
 motion_apply(p,BOR_MOTION_SET,3,vx,vy);start(p,"jump",BOR_ACTION);p->jumps_used=1;
}
static void c2_movement_checks(double floor){
 HostActor* p=hero();BorPhysics profile;int tick=total_frames;ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));
 assertion(load_physics(&profile,"michio_sword")==1&&profile.speed==330&&profile.acceleration==320&&profile.gravity==1000&&profile.jump==500&&profile.dash_jump_strength==400,"runtime loads the validated editable BOR/C2 profile");
 BorPhysics variant,invalid;
 assertion(load_physics(&variant,"__test_variant")==1&&variant.speed==420,"a disk-edited physics profile is interpreted by the same executable without recompilation");
 assertion(load_physics(&invalid,"__test_invalid")==-1&&!invalid.enabled,"native loading rejects a nonfinite profile atomically");
 c2_fixture(floor);assertion(physicsactor_get_acc(p->body)==320&&physicsactor_get_grv(p->body)==1000&&physicsactor_get_airdrag(p->body)==1,"selected C2 profile configures the actual Surge physics authority");
 c2_input(p,0,IB_FIRE2,1);assertion(!strcmp(p->a.animation->name,"combo1_v")&&p->a.state==BOR_ACTION,"neutral armed press immediately starts the first vertical BOR stage");uint64_t held_action=p->a.action_id;c2_input(p,0,IB_FIRE2,20);assertion(p->a.action_id==held_action&&p->a.state==BOR_ACTION,"held action alone neither repeats attacks nor enters guard");
 c2_fixture(floor);c2_guard(p,-1,IB_FIRE2,1);assertion(p->a.state==BOR_GUARD&&p->guard_kind==1&&p->a.facing==1,"LT plus armed action guards immediately without turning");player_actions(p,&keys);assertion(p->a.state==BOR_FREE&&!strcmp(p->a.animation->name,"idle"),"guard release never emits an attack");
 c2_fixture(floor);c2_input(p,0,IB_FIRE6,1);player_actions(p,&keys);assertion(!strcmp(p->a.animation->name,"unarmed"),"neutral unarmed press uses punches instead of the sword chain");
 c2_fixture(floor);start(p,"attack1",BOR_ACTION);uint64_t action=p->a.action_id;c2_input(p,-1,-1,1);assertion(p->a.state==BOR_ACTION&&p->a.action_id==action,"direction alone does not defensively cancel an attack");c2_guard(p,-1,IB_FIRE2,1);assertion(p->a.state==BOR_GUARD&&p->a.action_id!=action,"explicit LT guard safely replaces the attack on its first eligible tick");
 c2_fixture(floor);input_simulate_button_down(controls,IB_FIRE7);input_simulate_button_down(controls,IB_LEFT);input_simulate_button_down(controls,IB_FIRE2);input_simulate_button_down(controls,IB_FIRE6);for(int i=0;i<9;i++)player_actions(p,&keys);input_reset(controls);assertion(p->guard_kind==1,"simultaneous LT action holds select armed guard");
 c2_fixture(floor);c2_guard(p,-1,IB_FIRE6,1);assertion(p->guard_kind==2,"LT plus unarmed enters its independent guard");
 HostActor* red=slots[1];BorFrame impact=*bor_frame(&red->a);impact.ox=impact.oy=0;impact.ax=-10;impact.ay=-30;impact.aw=impact.ah=24;impact.no_block=0;
 BorAnimation strike=*red->a.animation;strike.frames=&impact;strike.count=1;red->a.animation=&strike;red->a.frame=0;red->a.x=p->a.x+2;red->a.y=p->a.y;red->a.active=true;red->a.epoch=98765;red->a.facing=-1;
 assertion(guard_hit(red,p,&impact)&&p->a.health==100&&red->a.freeze>=.12,"initial unarmed window parries a compatible frontal hit without damage");
 red->a.epoch++;p->guard_age=6;assertion(!guard_hit(red,p,&impact),"unarmed defense after six ticks permits the hit");p->guard_kind=1;assertion(guard_hit(red,p,&impact),"armed guard blocks the same compatible hit after the parry window");red->a.epoch++;impact.no_block=1;assertion(!guard_hit(red,p,&impact),"source no-block attacks bypass both guard styles");
 c2_fixture(floor);red->a.x=p->a.x+12;red->a.y=p->a.y;red->a.grounded=true;c2_input(p,1,-1,1);assertion(p->a.partner<0,"approaching a target no longer grabs automatically");c2_input(p,1,IB_FIRE6,1);c2_input(p,1,-1,1);assertion(linked_partner(p)==red,"toward plus unarmed tap acquires the contextual grab");
 c2_fixture(floor);c2_input(p,1,IB_FIRE1,1);assertion(!p->a.grounded&&p->a.vy==-500&&p->jumps_used==1,"first C2 jump launches through the existing Surge body");player_actions(p,&keys);assertion(p->a.vy==0,"releasing a controlled jump cuts upward motion at the configured ratio");
 c2_input(p,1,IB_FIRE1,1);assertion(!strcmp(p->a.animation->name,"doublejump")&&p->a.vy==-500&&p->jumps_used==2,"second jump uses its authored clip and independent jump allowance");player_actions(p,&keys);c2_input(p,0,IB_FIRE1,1);assertion(p->jumps_used==2,"a third airborne jump is refused");
 p->a.y=floor-1;physicsactor_set_position(p->body,v2d_new(p->a.x,p->a.y-20));physicsactor_set_ysp(p->body,100);move_actor(p);for(int i=0;i<20;i++){advance_host(p);player_actions(p,&keys);move_actor(p);}assertion(p->a.grounded&&p->jumps_used==0&&p->a.state==BOR_FREE,"double-jump landing restores ordinary control rather than retaining a looping action");
 c2_fixture(floor);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames+=8;c2_input(p,1,-1,1);assertion(p->a.state==BOR_DASH&&p->a.vx==400,"directional double tap inside nine ticks starts a 400px/s dash");
 for(int i=0;i<14;i++)move_actor(p);
 assertion(p->a.state==BOR_DASH&&p->job_tick==14,"dash remains owned through the fourteenth physics step");move_actor(p);assertion(p->a.state==BOR_FREE&&p->job_tick==15,"dash releases at exactly fifteen physics steps");
 c2_fixture(floor);c2_air(p,100,floor-200,200,0);double x=p->a.x;for(int i=0;i<10;i++)move_actor(p);assertion(p->a.vx>160&&p->a.vx<180&&p->a.x>x+25,"ordinary neutral air motion decays gradually without erasing takeoff momentum");
 p->physics.air_brake=0;p->physics.air_drag=1;double retained=p->a.vx;move_actor(p);assertion(fabs(p->a.vx-retained)<1e-6,"profile can explicitly restore neutral momentum preservation");
 c2_fixture(floor);c2_air(p,100,floor-10000,0,0);for(int i=0;i<90;i++){drive(p,1,p->physics.speed);move_actor(p);}assertion(p->a.vx>210&&p->a.vx<=241,"sustained air steering uses the reduced independent air speed");
 c2_fixture(floor);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames+=8;c2_input(p,1,-1,1);c2_input(p,1,IB_FIRE1,1);assertion(p->a.vy==-400&&!p->a.grounded,"dash jump uses BOR running-jump strength rather than the C2 multiplier");
 c2_fixture(floor);c2_air(p,100,floor-3000,0,1400);for(int i=0;i<12;i++)move_actor(p);assertion(physicsactor_get_topyspeed(p->body)==1920&&p->a.vy>960&&p->a.vy<1500,"requested fall limit reaches the body while the stock airborne hard cap remains visible");
 c2_fixture(floor);c2_air(p,100,floor-200,0,0);p->jumps_used=0;p->physics.jump_count=1;c2_input(p,0,IB_FIRE1,1);assertion(p->jumps_used==1&&p->a.vy==0,"walking off with one allowed jump cannot create an extra airborne jump");p->physics.jump_count=2;
 c2_fixture(floor);BorAnimation cancel_clip=*bor_animation(p->a.model,"attack1");BorCancel cancel={0,2,0,BOR_INPUT_ATTACK,"freespecial3"};cancel_clip.cancels=&cancel;cancel_clip.cancel_count=1;start(p,"attack1",BOR_ACTION);p->a.animation=&cancel_clip;c2_input(p,0,IB_FIRE2,1);player_actions(p,&keys);assertion(!strcmp(p->a.animation->name,"freespecial3"),"production tap classification routes authored attack cancels during committed actions");
 /* Independent real collision masks for wall, top identity and blocked climb tests. */
 image_t* previous=image_drawing_target();image_t* im=image_create(128,128);image_set_drawing_target(im);image_clear(color_rgb(255,255,255));image_set_drawing_target(previous);
 collisionmask_t* mask=collisionmask_create(im,0,0,128,128,0);collisionmask_t* thin=collisionmask_create(im,0,0,128,8,0);
 int top=(int)floor-200;obstacle_t* wall=obstacle_create(mask,(point2d_t){300,top},OL_DEFAULT,0);obstacle_t* ceiling=obstacle_create(thin,(point2d_t){270,top-40},OL_DEFAULT,0);
 obstaclemap_t* map=obstaclemap_create();obstaclemap_add(map,wall);obstaclemap_build(map);
 c2_fixture(floor);test_terrain=map;c2_air(p,290,top+70,0,20);assertion(!wall_jump(p,0,map),"wall proximity without deliberate input cannot trigger a wall jump");assertion(wall_jump(p,1,map)&&p->a.state==BOR_WALL_JUMP&&fabs(p->a.vx+426.6666666667)<1e-6,"deliberate pressure against a contacted wall starts the reduced wall kick");
 double vx=p->a.vx;for(int i=0;i<14;i++){c2_input(p,1,-1,1);move_actor(p);}assertion(p->a.state==BOR_WALL_JUMP&&fabs(p->a.vx-vx)<1e-6&&p->a.vy<0,"opposing input cannot steer the fourteen locked wall-jump ticks");move_actor(p);assertion(p->a.state==BOR_ACTION&&p->job_tick==15,"wall-jump control lock releases after fifteen steps");
 c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,-10);p->input_direction=1;assertion(!ledge_acquire(p,map),"ascending motion cannot acquire a ledge");motion_apply(p,BOR_MOTION_SET,2,0,10);p->input_direction=0;assertion(!ledge_acquire(p,map),"automatic ledge acquisition requires deliberate approach");p->input_direction=1;assertion(ledge_acquire(p,map)&&p->a.state==BOR_LEDGE&&p->a.vx==0&&p->a.vy==0,"descending approach attaches to the actual solid wall-top pair");
 double hangy=p->a.y;for(int i=0;i<20;i++){move_actor(p);advance_host(p);}assertion(p->a.state==BOR_LEDGE&&p->a.y==hangy,"hang ownership survives non-looping pose completion without gravity drift");
 assertion(climb_begin(p,map),"clear up-and-over path starts the authored climb");for(int i=0;i<29;i++)move_actor(p);assertion(p->a.state==BOR_CLIMB&&p->job_tick==29,"climb remains owned for its actual thirty-tick timeline");move_actor(p);assertion(p->a.state==BOR_FREE&&p->ledge_id==0&&p->a.vx==80&&p->a.vy==-80&&p->a.x>300,"completed climb releases safely with the documented C2 helper impulse");
 c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);c2_input(p,-1,-1,1);assertion(p->a.state==BOR_FREE&&!p->ledge_id&&p->a.vy==100,"away input drops the ledge and starts a reacquisition cooldown");
 c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);obstaclemap_clear(map);obstaclemap_add(map,wall);obstaclemap_add(map,ceiling);obstaclemap_build(map);assertion(!climb_begin(p,map)&&p->a.state==BOR_LEDGE,"full-body clearance refuses a climb obstructed above the hanging actor");
 obstaclemap_clear(map);obstaclemap_add(map,wall);obstaclemap_build(map);climb_begin(p,map);obstaclemap_clear(map);obstaclemap_add(map,wall);obstaclemap_add(map,ceiling);obstaclemap_build(map);for(int i=0;i<30&&p->a.state==BOR_CLIMB;i++)move_actor(p);assertion(p->a.state==BOR_FREE&&!p->ledge_id,"new obstruction during climbing cancels the swept path safely");
 obstaclemap_clear(map);obstaclemap_add(map,wall);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);obstaclemap_clear(map);obstaclemap_build(map);move_actor(p);assertion(p->a.state==BOR_FREE&&!p->ledge_id,"removing ledge support releases attachment instead of allowing infinite climbing");
 obstaclemap_clear(map);obstaclemap_add(map,wall);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);damage_reaction(p,slots[1],&impact);assertion(!p->ledge_id&&p->a.state==BOR_PAIN,"damage reaction releases traversal ownership immediately");
 c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);die(p);assertion(!p->ledge_id&&p->a.state==BOR_DEAD,"death safely clears ledge support and state jobs");
 obstaclemap_clear(map);obstaclemap_add(map,wall);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);reset_all();assertion(!p->ledge_id&&p->a.state==BOR_FREE&&p->jumps_used==0,"reset discards attachment and traversal timers");
 // A different collider above the wall is not silently treated as that wall's top.
 obstacle_t* cap=obstacle_create(thin,(point2d_t){300,top-8},OL_DEFAULT,0);obstaclemap_clear(map);obstaclemap_add(map,wall);obstaclemap_add(map,cap);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;assertion(!ledge_acquire(p,map),"wall and candidate top must belong to the same obstacle");obstacle_destroy(cap);

 // Persistent native brick and recreated SurgeScript-style masks carry both hang and climb.
 obstaclemap_clear(map);obstacle_t* moving=obstacle_create(mask,(point2d_t){300,top},OL_DEFAULT,OF_NONSTATIC);obstaclemap_add(map,moving);obstaclemap_build(map);
 c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;assertion(ledge_acquire(p,map),"a nonstatic solid support permits ledge acquisition");
 v2d_t oldfoot=v2d_new(p->a.x,p->a.y);uint64_t moving_id=p->ledge_id;
 obstaclemap_clear(map);obstacle_set_position(moving,(point2d_t){308,top-4});obstaclemap_add(map,moving);obstaclemap_build(map);total_frames++;move_actor(p);
 assertion(p->a.state==BOR_LEDGE&&fabs(p->a.x-oldfoot.x-8)<1e-6&&fabs(p->a.y-oldfoot.y+4)<1e-6,"moving support carries a hanging actor horizontally and vertically");
 double rx,ry,rw,rh;bor_rect(&p->a,false,&rx,&ry,&rw,&rh);const BorFrame* pose=bor_frame(&p->a);
 assertion(fabs(rx-(p->ledge.x+(pose->bx-65)))<1e-6&&fabs(ry-(p->ledge.y+pose->by-40))<1e-6,"hurtbox matches the committed hand anchor immediately after support relocation");
 obstaclemap_clear(map);obstacle_t* replacement=obstacle_create(mask,(point2d_t){310,top-3},OL_DEFAULT,OF_NONSTATIC);obstacle_set_id(replacement,moving_id);obstacle_destroy(moving);moving=replacement;obstaclemap_add(map,moving);obstaclemap_build(map);total_frames++;move_actor(p);
 assertion(p->a.state==BOR_LEDGE&&p->ledge_id==moving_id&&fabs(p->a.x-oldfoot.x-10)<1e-6,"stable identity survives replacement of a platform's per-frame collision object");
 assertion(climb_begin(p,map),"moving support starts a relative climb");
 for(int i=0;i<30;i++){
  obstaclemap_clear(map);point2d_t at=obstacle_get_position(moving);at.x+=1;at.y-=1;obstacle_set_position(moving,at);obstaclemap_add(map,moving);obstaclemap_build(map);total_frames++;move_actor(p);
 }
 assertion(p->a.state==BOR_FREE&&!p->ledge_id&&fabs(p->a.x-364)<1e-6&&fabs(p->a.y-(top-35))<1e-6&&p->a.vx==140&&p->a.vy==-140,"climb follows a translating platform and retains its velocity on release");
 obstaclemap_clear(map);obstacle_set_position(moving,(point2d_t){300,top});obstaclemap_add(map,moving);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);
 obstacle_t* stopper=obstacle_create(thin,(point2d_t){260,top+24},OL_DEFAULT,0);obstaclemap_clear(map);obstacle_set_position(moving,(point2d_t){300,top-8});obstaclemap_add(map,moving);obstaclemap_add(map,stopper);obstaclemap_build(map);total_frames++;move_actor(p);
 assertion(p->a.state==BOR_FREE&&!p->ledge_id&&body_clear(p,map,p->a.x,p->a.y),"blocked carry releases in clear space instead of crushing the hanging actor");
 obstaclemap_clear(map);obstacle_destroy(stopper);obstacle_destroy(moving);
 moving=obstacle_create(thin,(point2d_t){300,top},OL_DEFAULT,OF_NONSTATIC|OF_CLOUD);obstaclemap_add(map,moving);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+40,0,10);p->input_direction=1;assertion(ledge_acquire(p,map)&&!p->support_solid,"one-way moving-platform edges support ledge actions without becoming walls");
 obstaclemap_clear(map);obstacle_set_position(moving,(point2d_t){304,top+2});obstaclemap_add(map,moving);obstaclemap_build(map);total_frames++;move_actor(p);assertion(p->a.state==BOR_LEDGE&&p->ledge.x==304&&p->ledge.y==top+2,"one-way support carries the hanging actor through translation");
 obstaclemap_clear(map);obstacle_destroy(moving);moving=obstacle_create(thin,(point2d_t){304,top+2},OL_DEFAULT,OF_NONSTATIC|OF_CLOUD);obstaclemap_add(map,moving);obstaclemap_build(map);total_frames++;move_actor(p);assertion(p->a.state==BOR_FREE&&!p->ledge_id,"a new platform at the same location cannot steal the old attachment identity");
 obstaclemap_clear(map);obstacle_destroy(moving);

 collisionmask_t* single=collisionmask_create(im,0,0,128,1,0);moving=obstacle_create(single,(point2d_t){300,top},OL_DEFAULT,OF_NONSTATIC|OF_CLOUD);obstaclemap_add(map,moving);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+35,0,10);p->input_direction=1;assertion(ledge_acquire(p,map),"one-pixel cloud edge can be acquired");total_frames++;move_actor(p);assertion(p->a.state==BOR_LEDGE,"one-pixel cloud support remains valid on the following tick");obstaclemap_clear(map);obstacle_destroy(moving);collisionmask_destroy(single);
 // Horizontal motion can cover the OLD body while a different obstacle blocks its destination.
 moving=obstacle_create(mask,(point2d_t){300,top},OL_DEFAULT,OF_NONSTATIC);obstaclemap_add(map,moving);obstaclemap_build(map);c2_fixture(floor);test_terrain=map;c2_air(p,280,top+45,0,10);p->input_direction=1;ledge_acquire(p,map);
 stopper=obstacle_create(mask,(point2d_t){220,top+32},OL_DEFAULT,0);obstaclemap_clear(map);obstacle_set_position(moving,(point2d_t){280,top});obstaclemap_add(map,moving);obstaclemap_add(map,stopper);obstaclemap_build(map);total_frames++;move_actor(p);assertion(p->a.state==BOR_FREE&&!p->ledge_id&&body_clear(p,map,p->a.x,p->a.y),"blocked horizontal carry depenetrates or uses a clear spawn fallback");obstaclemap_clear(map);obstacle_destroy(stopper);obstacle_destroy(moving);
 bool palette_ok=true;int palette_frames=0;
 for(int clip=0;clip<p->a.model->count;clip++)for(int frame=0;frame<p->a.model->animations[clip].count;frame++){
  const BorFrame* f=p->a.model->animations[clip].frames+frame;char page[160];snprintf(page,sizeof(page),"%s",p->a.model->sprite);if(f->sprite>=256)snprintf(page,sizeof(page),"%s@%d",p->a.model->sprite,f->sprite/256);
  if(!sprite_animation_exists(page,f->sprite%256))palette_ok=false;
  if(f->cw>0){snprintf(page,sizeof(page),"%s#%d",p->a.model->sprite,f->sprite);if(!sprite_animation_exists(page,0))palette_ok=false;}
  palette_frames++;
 }
 assertion(palette_ok&&palette_frames==383,"all BOR and complete C2 Michio frames resolve through valid native sprite pages");
 c2_fixture(floor);start(p,"roll",BOR_ROLL);p->roll_angle=.7;render_frame(p);const BorFrame* roll=bor_frame(&p->a);
 assertion(image_width(actor_image(p->visual))==32&&image_height(actor_image(p->visual))==32&&p->visual->hot_spot.x==16&&p->visual->hot_spot.y==16&&roll->cw==32,"roll uses a square32px source crop with a centered hotspot");
 assertion(fabs(p->a.pose_angle-.7)<1e-6&&p->a.pose_y==-16,"roll rotation keeps its center one radius above the physical feet");
 for(int facing=-1;facing<=1;facing+=2){c2_fixture(floor);motion_tangent(p,facing*100);start(p,"roll",BOR_ROLL);p->job=p->physics;p->down_held=true;p->roll_angle=0;p->roll_entry_speed=p->a.x;double prior_x=p->a.x;move_actor(p);assertion(p->a.state==BOR_ROLL&&(p->a.x-prior_x)*p->roll_angle<0,"roll spin follows travel in both directions under Surge angle conventions");}
 c2_fixture(floor);HostActor* later=slots[1];BorFrame source_box=*bor_frame(&p->a),target_box=*bor_frame(&later->a);
 source_box.ox=source_box.oy=target_box.ox=target_box.oy=0;source_box.ax=10;source_box.ay=-25;source_box.aw=source_box.ah=2;target_box.bx=target_box.by=0;target_box.bw=20;target_box.bh=2;
 BorAnimation source_pose=*p->a.animation,target_pose=*later->a.animation;source_pose.frames=&source_box;target_pose.frames=&target_box;source_pose.count=target_pose.count=1;p->a.animation=&source_pose;later->a.animation=&target_pose;p->a.frame=later->a.frame=0;p->a.facing=later->a.facing=1;p->a.x=p->a.y=0;later->a.x=10;later->a.y=0;later->a.state=BOR_ROLL;later->a.grounded=false;later->roll_angle=0;render_frame(p);render_frame(later);
 assertion(!bor_overlaps(&p->a,&later->a),"previous later-slot roll pose misses the earlier-slot attack fixture");later->roll_angle=3.141592653589793/2;prepare_combat_poses();
 assertion(bor_overlaps(&p->a,&later->a),"pre-combat pose barrier refreshes a later rolling target before an earlier slot attacks");
 // Verify the actual host bridge assigns Brick-component IDs, not frame allocation addresses.
 test_terrain=NULL;surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_var_t* name=surgescript_var_set_string(surgescript_var_create(),"BOR Moving Ledge");const surgescript_var_t* args[]={name};surgescript_var_t* found=surgescript_var_create();surgescript_object_call_function(level,"findEntity",args,1,found);
 surgescript_objectmanager_t* manager=surgescript_object_manager(level);surgescript_objecthandle_t owner=surgescript_var_get_objecthandle(found);uint64_t actual_id=0;
 if(owner&&surgescript_objectmanager_exists(manager,owner)){surgescript_object_t* entity=surgescript_objectmanager_get(manager,owner);actual_id=scripting_brick_identity(surgescript_objectmanager_get(manager,surgescript_object_child(entity,"Brick")));}
 assertion(actual_id&&obstaclemap_find_by_id(terrain_map(),actual_id,OL_DEFAULT)!=NULL,"actual SurgeScript moving fixture exposes a persistent Brick lifetime identity in the host map");surgescript_var_destroy(name);surgescript_var_destroy(found);
 test_terrain=NULL;obstaclemap_destroy(map);obstacle_destroy(ceiling);obstacle_destroy(wall);collisionmask_destroy(thin);collisionmask_destroy(mask);image_destroy(im);
 p->physics.enabled=false;total_frames=tick;test_fixture(floor);logfile_message("BOR_C2_PROFILE_CHECKS complete");
}

static void c2_region_checks(double floor){
 HostActor* p=hero();int tick=total_frames;
 for(int type=0;type<2;type++){
  BorState state=type?BOR_LADDER:BOR_POLE;ClimbRegion* region=NULL;
  for(int i=0;i<64;i++)if(regions[i]&&regions[i]->state==state){region=regions[i];break;}
  assertion(region!=NULL,type?"ladder region registers with its own state":"pole region registers independently");if(!region)continue;
  v2d_t at=scripting_util_world_position(region->entity);double bottom=at.y+region->height;
  c2_fixture(floor);test_place(p,at.x,floor);c2_input(p,0,-1,1);assertion(p->a.state==BOR_FREE,"climb region proximity alone cannot capture ordinary movement");c2_input(p,0,IB_UP,1);
  assertion(p->a.state==state&&p->region_id==region->id,type?"Up enters the separate LADDER state from the real floor":"Up attaches to the bounded POLE region from the real floor");
  double y=p->a.y,age=p->a.age;p->climb_direction=0;for(int i=0;i<6;i++){move_actor(p);advance_host(p);}assertion(p->a.y==y&&p->a.age==age,"neutral climbing holds position and pauses the climb animation");
  p->climb_direction=-1;move_actor(p);assertion(fabs(y-p->a.y-(type?p->physics.ladder_speed:p->physics.pole_speed)/60)<.01,"each climb state uses its independent actor physics speed");
  for(int i=0;i<600&&p->a.state==state;i++){move_actor(p);advance_host(p);}assertion(p->a.state==BOR_FREE&&!p->region_id&&p->a.y>=at.y-1.01&&p->a.y<=at.y+.01,"upper endpoint releases within the authored bounds instead of climbing infinitely");
  c2_fixture(floor);c2_air(p,at.x,at.y+region->height*.5,0,0);c2_input(p,0,IB_DOWN,1);p->climb_direction=1;for(int i=0;i<600&&p->a.state==state;i++)move_actor(p);
  assertion(p->a.state==BOR_FREE&&!p->region_id&&p->a.y<=bottom+1.01&&p->a.y>=bottom-4,"lower endpoint drops within bounds without continuing below the region");
  c2_fixture(floor);c2_air(p,at.x,at.y+region->height*.5,0,0);c2_input(p,0,IB_UP,1);c2_input(p,1,IB_FIRE1,1);assertion(!p->region_id&&p->a.state==BOR_ACTION&&p->a.vy==-500&&p->a.vx==p->physics.speed,"jump detaches climb ownership and launches through the Surge body");c2_input(p,0,IB_UP,1);assertion(!p->region_id,"holding a vertical input after release cannot immediately recapture the climb region");
  c2_fixture(floor);c2_air(p,at.x,at.y+region->height*.5,0,0);c2_input(p,0,IB_UP,1);v2d_t region_original=scripting_util_world_position(region->entity);scripting_util_set_world_position(region->entity,v2d_new(region_original.x+10000,region_original.y));move_actor(p);assertion(!p->region_id&&p->a.state==BOR_FREE&&body_clear(p,terrain_map(),p->a.x,p->a.y),"teleported climb region releases safely without unbounded pixel sweeps");scripting_util_set_world_position(region->entity,region_original);
  c2_fixture(floor);c2_air(p,at.x,at.y+region->height*.5,0,0);c2_input(p,0,IB_UP,1);region->enabled=false;move_actor(p);assertion(!p->region_id&&p->a.state==BOR_FREE,"disabling a climb region releases its actor safely");region->enabled=true;
  c2_fixture(floor);c2_air(p,at.x,at.y+region->height*.5,0,0);c2_input(p,0,IB_UP,1);BorFrame impact=*bor_frame(&slots[1]->a);impact.drop=impact.pause=0;damage_reaction(p,slots[1],&impact);assertion(!p->region_id&&p->a.state==BOR_PAIN,"damage releases pole or ladder state ownership");
 }
 c2_fixture(floor);int before=total_sound_events;start(p,"jump",BOR_ACTION);bor_advance(&p->a,2,hit_segment,p);int after=total_sound_events;for(int i=0;i<20;i++)bor_advance(&p->a,1.0/60,hit_segment,p);assertion(after==before+1&&total_sound_events==after,"jump entry sound occurs once and never retriggers on held terminal frames");
 p->physics.enabled=false;total_frames=tick;test_fixture(floor);
}

/* Continuous-network, directional-command and real bound-attack regressions. Signed Atlas. */
static void continuous_combat_checks(double floor){
 HostActor* p=hero();int tick=total_frames;ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));
 for(int facing=-1;facing<=1;facing+=2){
  c2_fixture(floor);c2_air(p,100,floor-300,100,0);p->a.facing=facing;
  c2_input(p,0,IB_UP,1);total_frames++;c2_input(p,facing,IB_UP,1);total_frames++;c2_input(p,facing,-1,1);total_frames++;c2_input(p,facing,IB_FIRE2,1);c2_input(p,0,-1,1);
  assertion(!strcmp(p->a.animation->name,"runjumpattack"),"U/UF/F+sword tap starts the BOR rolling attack in either facing");
  start(p,"jump",BOR_ACTION);c2_input(p,0,IB_FIRE2,1);c2_input(p,0,-1,1);assertion(!strcmp(p->a.animation->name,"c2_swordvair"),"consumed motion cannot retrigger and falls back to modifier-aware normal air attack");
 }
 for(int final_direction=-1;final_direction<=0;final_direction++){
  c2_fixture(floor);c2_air(p,100,floor-300,0,0);p->a.facing=1;c2_input(p,0,IB_UP,1);c2_input(p,1,IB_UP,1);c2_input(p,1,-1,1);c2_input(p,final_direction,IB_FIRE2,1);c2_input(p,0,-1,1);assertion(strcmp(p->a.animation->name,"runjumpattack")!=0,"neutral or backward sword cannot complete the required Forward+Sword motion");
 }
 c2_fixture(floor);c2_air(p,100,floor-300,0,0);c2_input(p,0,IB_UP,1);total_frames+=16;c2_input(p,1,IB_UP,1);c2_input(p,1,-1,1);c2_input(p,1,IB_FIRE2,1);c2_input(p,0,-1,1);assertion(!strcmp(p->a.animation->name,"c2_swordhair"),"expired motion history falls back to normal horizontal air attack");
 c2_fixture(floor);c2_air(p,100,floor-300,0,0);c2_input(p,0,IB_UP,1);c2_input(p,1,IB_UP,1);c2_input(p,1,-1,1);uint64_t action=p->a.action_id;c2_input(p,1,IB_FIRE2,12);c2_input(p,0,-1,1);assertion(p->a.action_id!=action&&!strcmp(p->a.animation->name,"runjumpattack"),"forward+sword launches the captured motion immediately even when held");
 c2_fixture(floor);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames+=8;c2_input(p,1,-1,1);c2_input(p,1,IB_FIRE2,1);c2_input(p,1,-1,1);assertion(!strcmp(p->a.animation->name,"runattack"),"sword tap during dash restores the original BOR dash attack");
 c2_fixture(floor);c2_input(p,0,IB_DOWN,1);input_simulate_button_down(controls,IB_DOWN);input_simulate_button_down(controls,IB_FIRE6);player_actions(p,&keys);input_reset(controls);input_simulate_button_down(controls,IB_DOWN);player_actions(p,&keys);input_reset(controls);assertion(!strcmp(p->a.animation->name,"sweepkick"),"crouched unarmed tap resolves the sweep-kick template");
 // Every authored combo binding must actually damage, not merely select an animation.
 HostActor* dummy=NULL;for(int i=0;i<64;i++)if(slots[i]&&!strcmp(slots[i]->a.model->name,"Training_Log"))dummy=slots[i];assertion(dummy!=NULL,"source C2 training log is present in the opening arena");
 bool all_damage=true,all_commit=true;
 for(int rule=0;rule<p->a.model->combo_count;rule++){
  c2_fixture(floor);BorActor target;bor_init(&target,dummy->a.model,5);BorFrame box=*bor_frame(&target);box.ox=box.oy=0;box.bx=box.by=-4096;box.bw=box.bh=8192;BorAnimation pose=*target.animation;pose.frames=&box;pose.count=1;target.animation=&pose;target.serial=63;target.x=p->a.x;target.y=p->a.y;
  const BorComboRule* binding=p->a.model->combos+rule;if(!bor_combo_attack(&p->a,binding->animation,NULL,NULL)){all_damage=false;continue;}uint64_t owned=p->a.action_id;bool hit=false;
  for(int frame=0;frame<p->a.animation->count;frame++){p->a.frame=frame;p->a.active=bor_frame(&p->a)->aw>0;p->a.epoch=50000+rule*100+frame;if(p->a.active&&bor_damage(&p->a,&target,world_time)){hit=true;break;}}
  if(!hit||target.health>=2147483647)all_damage=false;
  c2_input(p,0,IB_FIRE2,12);if(p->a.action_id!=owned)all_commit=false;
 }
 assertion(all_damage,"every registered sword/unarmed ground-air-duck-dash-motion variant inflicts real damage");assertion(all_commit,"action-only holds cannot replace any newly bound committed attack");
 c2_fixture(floor);BorFrame impact=*bor_frame(&p->a);impact.pause=4;int old_hits=dummy->training_hits;double old_damage=dummy->training_damage;dummy->a.health-=17;damage_reaction(dummy,p,&impact);assertion(dummy->a.health==2147483647&&dummy->training_hits==old_hits+1&&dummy->training_damage==old_damage+17,"training log records connected damage while retaining unlimited health");
 c2_fixture(floor);test_place(dummy,p->a.x+30,p->a.y);int before_sound=total_impact_events;requested_attack(p,0,false,0,false);bor_advance(&p->a,.061,hit_segment,p);int after_sound=total_impact_events;assertion(after_sound==before_sound+1&&last_impact_material==1&&last_impact_kind==1,"confirmed sword hit plays the C2 blade-on-wood profile on the actual log");dummy->a.invulnerable=0;for(int i=0;i<12;i++)hit_segment(&p->a,bor_frame(&p->a),false,p);assertion(total_impact_events==after_sound,"held attack samples never retrigger impact audio for the same activation and target");
 int initial_refs=resourcemanager_ref_sample(bor_sound_paths[0])-1;resourcemanager_unref_sample(bor_sound_paths[0]);release_sounds();int released_refs=resourcemanager_ref_sample(bor_sound_paths[0])-1;resourcemanager_unref_sample(bor_sound_paths[0]);load_sounds();int reloaded_refs=resourcemanager_ref_sample(bor_sound_paths[0])-1;resourcemanager_unref_sample(bor_sound_paths[0]);assertion(released_refs==initial_refs-1&&reloaded_refs==initial_refs,"audio unload and reload balance the resource-manager sample references");
 bool impact_assets=true;for(int material=0;material<bor_impact_material_count;material++)for(int kind=0;kind<4;kind++)for(int strength=0;strength<3;strength++){int index=bor_impact_sounds[material][kind][strength];if(index<0||index>=bor_sound_count||!sounds[index])impact_assets=false;}assertion(impact_assets&&bor_impact_material_count==6,"all six material/type/strength and block profiles resolve to loaded native sound samples");
 c2_fixture(floor);test_place(dummy,p->a.x+12,p->a.y);start(dummy,"idle",BOR_FREE);c2_input(p,1,IB_FIRE6,1);c2_input(p,1,-1,1);assertion(p->a.partner<0&&dummy->a.partner<0&&dummy->a.model->grabbable==false,"toward+unarmed cannot grab or throw the passive training log");dummy->a.health=1;die(dummy);assertion(dummy->a.health==2147483647&&dummy->a.state==BOR_FREE,"training prop recovery retains unlimited health and returns it to its authored spawn");
 c2_fixture(floor);HostActor* red=slots[1];red->wake_distance=800;red->enemy_awake=false;red->a.x=p->a.x+2000;assertion(!encounter_ready(red),"later enemy encounter remains dormant during the opening training arena");red->a.x=p->a.x+700;assertion(encounter_ready(red),"later encounter activates when the player approaches its configured range");red->wake_distance=0;red->enemy_awake=true;
 assertion(enemy_target(slots[4])!=dummy,"hostile enemies never select the passive log as an AI target");
 c2_fixture(floor);red->a.x=p->a.x+4000;red->a.y=floor-100;red->a.health=17;commit_pose(red);rebase(red);uint64_t identity=red->a.action_id;double age=red->a.age,y=red->a.y;update_actor_activity();assertion(red->suspended,"distant native actor suspends outside the camera terrain envelope");for(int i=0;i<180;i++){move_actor(red);advance_host(red);}assertion(red->a.health==17&&red->a.y==y&&red->a.age==age&&red->a.action_id==identity,"off-screen actor preserves health, position, timeline and identity without falling through absent terrain");test_place(red,p->a.x+100,floor);update_actor_activity();assertion(!red->suspended&&red->a.health==17&&red->a.action_id==identity,"returning actor resumes its retained state rather than resetting or respawning");

 p->physics.enabled=false;total_frames=tick;test_fixture(floor);
}
static void continuous_region_checks(double floor){
 HostActor* p=hero();int tick=total_frames;ClimbRegion* pole=NULL;ClimbRegion* ladder=NULL;ClimbRegion* bar=NULL;ClimbRegion* other=NULL;
 for(int i=0;i<64;i++)if(regions[i]){if(regions[i]->state==BOR_POLE&&!pole)pole=regions[i];else if(regions[i]->state==BOR_LADDER&&!ladder)ladder=regions[i];else if(regions[i]->state==BOR_BARCLIMB){if(!bar)bar=regions[i];else if(!other)other=regions[i];}}
 assertion(pole&&ladder&&bar&&other,"continuous traversal fixtures register poles, ladders and multiple bars");if(!pole||!ladder||!bar||!other)return;
 v2d_t pole_at=scripting_util_world_position(pole->entity),ladder_at=scripting_util_world_position(ladder->entity),bar_at=scripting_util_world_position(bar->entity),other_at=scripting_util_world_position(other->entity);double ph=pole->height,lh=ladder->height;char saved_path[1024],saved_other[1024];strcpy(saved_path,bar->path);strcpy(saved_other,other->path);
 scripting_util_set_world_position(pole->entity,v2d_new(700,floor-300));scripting_util_set_world_position(ladder->entity,v2d_new(700,floor-200));pole->height=ladder->height=200;
 c2_fixture(floor);c2_air(p,700,floor-250,0,0);c2_input(p,0,IB_DOWN,1);uint64_t action=p->a.action_id;for(int i=0;i<105;i++)move_actor(p);assertion(p->region_id==ladder->id&&p->a.action_id==action&&p->a.state==BOR_POLE,"overlapping pole/ladder pieces hand off continuously without state or animation reentry");
 p->climb_direction=-1;for(int i=0;i<100;i++)move_actor(p);assertion(p->region_id==pole->id&&p->a.action_id==action,"overlapping vertical network reverses across the same join without retriggering");
 pole->height=ph;ladder->height=lh;scripting_util_set_world_position(pole->entity,pole_at);scripting_util_set_world_position(ladder->entity,ladder_at);
 const char* paths[]={"0,0;160,0","0,0;80,-32;160,0","0,0;32,-16;64,-24;96,-25;128,-32;160,-48"};
 for(int example=0;example<3;example++){
  scripting_util_set_world_position(bar->entity,v2d_new(500,floor-240));region_path(bar,paths[example]);c2_fixture(floor);c2_air(p,500,floor-168,0,0);c2_input(p,0,IB_UP,1);action=p->a.action_id;p->climb_direction=0;p->input_direction=1;for(int i=0;i<180;i++){move_actor(p);advance_host(p);}v2d_t end=region_sample(bar,region_length(bar),NULL);
  logfile_message("BAR_CASE %d state=%d action=%llu expected=%llu distance=%.9f length=%.9f at=%.6f,%.6f end=%.6f,%.6f",example,p->a.state,(unsigned long long)p->a.action_id,(unsigned long long)action,p->region_distance,region_length(bar),p->a.x,p->a.y,end.x,end.y);
  assertion(p->a.state==BOR_BARCLIMB&&p->a.action_id==action&&fabs(p->a.x-end.x)<.01&&fabs(p->a.y-end.y-72)<.01,"straight, peaked and curved bars traverse to their final endpoint without reentry");
  p->input_direction=-1;for(int i=0;i<180;i++)move_actor(p);assertion(p->a.action_id==action&&fabs(p->a.x-500)<.01,"bar path reverses continuously across all internal angled joints");
 }
 region_path(bar,"0,0;100,0;100,100");c2_fixture(floor);c2_air(p,500,floor-168,0,0);c2_input(p,0,IB_UP,1);action=p->a.action_id;p->climb_direction=0;p->input_direction=1;for(int i=0;i<50;i++)move_actor(p);p->input_direction=0;p->climb_direction=1;for(int i=0;i<25;i++)move_actor(p);assertion(p->a.action_id==action&&fabs(p->a.x-600)<.01&&fabs(p->a.y-(floor-118))<.01,"exact right-angle bar junction accepts the adjacent axis without sticking");
 p->climb_direction=-1;for(int i=0;i<25;i++)move_actor(p);p->climb_direction=0;p->input_direction=-1;for(int i=0;i<50;i++)move_actor(p);assertion(p->a.action_id==action&&fabs(p->a.x-500)<.01,"right-angle bar junction is reversible without restarting its action");
 region_path(bar,"0,0;100,0");region_path(other,"0,0;100,-40");scripting_util_set_world_position(other->entity,v2d_new(600,floor-240));c2_fixture(floor);c2_air(p,500,floor-168,0,0);c2_input(p,0,IB_UP,1);action=p->a.action_id;p->climb_direction=0;p->input_direction=1;for(int i=0;i<100;i++)move_actor(p);assertion(p->region_id==other->id&&p->a.action_id==action&&p->a.x>680,"separate connected bar objects transfer ownership without restarting barclimb");
 region_path(bar,saved_path);region_path(other,saved_other);scripting_util_set_world_position(bar->entity,bar_at);scripting_util_set_world_position(other->entity,other_at);
 p->physics.enabled=false;total_frames=tick;test_fixture(floor);
}

/* Confirmed LT/Ctrl defense/target/evasion contract. Signed: Atlas. */
static void modifier_checks(double floor){
 HostActor* p=hero();HostActor* red=slots[1];int tick=total_frames;
 assertion(!bsl_trigger_down(.54f,false)&&bsl_trigger_down(.55f,false)&&bsl_trigger_down(.45f,true)&&!bsl_trigger_down(.35f,true),"LT threshold uses distinct press and release levels");
 assertion(!bsl_trigger_down(NAN,true)&&!bsl_trigger_down(INFINITY,true)&&!bsl_trigger_down(-1,true)&&!bsl_trigger_down(2,true),"invalid trigger samples fail released");
 bool held=bsl_trigger_down(1,false);held=false;assertion(!bsl_trigger_down(.45f,held),"reset trigger history requires a fresh press threshold after reconnect");
 c2_fixture(floor);c2_input(p,-1,IB_FIRE2,1);assertion(p->a.state==BOR_ACTION,"back plus attack remains an attack without LT");
 c2_fixture(floor);c2_guard(p,0,-1,1);assertion(p->a.state==BOR_FREE&&p->lock_target&&p->guard_kind==0,"LT alone acquires a live target without entering guard");
 c2_guard(p,0,IB_FIRE2,1);assertion(p->a.state==BOR_GUARD&&p->guard_kind==1&&!p->a.active&&p->a.facing==1,"LT plus sword guards immediately without an attack");
 c2_input(p,0,IB_FIRE2,1);assertion(p->a.state==BOR_FREE&&p->lock_target==0,"LT release closes guard and clears its target");c2_input(p,0,IB_FIRE2,2);assertion(p->a.state==BOR_FREE,"still-held sword cannot attack after guard release");
 c2_fixture(floor);c2_input(p,0,IB_FIRE6,1);c2_guard(p,0,IB_FIRE6,1);assertion(p->a.state==BOR_GUARD&&p->guard_kind==2,"LT can cancel an already held unarmed action without another press");
 input_reset(controls);input_simulate_button_down(controls,IB_FIRE7);input_simulate_button_down(controls,IB_FIRE2);input_simulate_button_down(controls,IB_FIRE6);ALLEGRO_KEYBOARD_STATE keys;memset(&keys,0,sizeof(keys));player_actions(p,&keys);input_reset(controls);assertion(p->guard_kind==2,"first entered guard retains ownership while both actions remain held");
 c2_fixture(floor);test_place(red,p->a.x+50,floor);test_place(slots[2],p->a.x+90,floor);red->suspended=false;slots[2]->suspended=false;lock_update(p,true);assertion(p->lock_target==red->lifetime,"lock-on selects the closest eligible enemy");red->a.health=0;lock_update(p,true);assertion(p->lock_target==slots[2]->lifetime,"dead targets are replaced safely");slots[2]->suspended=true;lock_update(p,true);assertion(p->lock_target!=slots[2]->lifetime,"suspended targets do not receive a lock");lock_update(p,false);assertion(p->lock_target==0,"modifier release clears lifetime target identity");
 c2_fixture(floor);c2_input(p,1,-1,1);c2_input(p,0,-1,1);total_frames++;c2_guard(p,1,-1,1);assertion(p->a.state!=BOR_EVADE,"a tap made without LT cannot seed an evasion");
 for(int direction=-1;direction<=1;direction+=2){
  c2_fixture(floor);c2_guard(p,0,-1,1);c2_guard(p,direction,-1,1);c2_guard(p,0,-1,1);total_frames++;c2_guard(p,direction,-1,1);
  assertion(p->a.state==BOR_EVADE&&p->a.vx*direction>0&&p->a.facing==1,"LT double tap starts forward/backward evasion without changing facing");
  int duration=(int)p->job.evasion_ticks;for(int i=0;i<duration-1;i++){traversal_after_motion(p);evade_finish(p);}assertion(p->a.state==BOR_EVADE,"evasion retains its exact authored active ticks");traversal_after_motion(p);assertion(p->a.state==BOR_EVADE,"the final evasion tick remains immune through all combat callbacks");evade_finish(p);assertion(p->a.state==BOR_FREE,"evasion ends after its final combat phase even without displacement");
 }
 c2_fixture(floor);c2_guard(p,0,IB_FIRE2,1);uint64_t guard=p->a.action_id;c2_guard(p,-1,IB_FIRE2,1);c2_guard(p,0,IB_FIRE2,1);total_frames++;c2_guard(p,-1,IB_FIRE2,1);assertion(p->a.state==BOR_EVADE&&p->a.action_id!=guard,"evasion can interrupt owned guard");
 c2_fixture(floor);p->a.state=BOR_PAIN;uint64_t protected_action=p->a.action_id;assertion(!evade_begin(p,1)&&p->a.action_id==protected_action,"evasion cannot escape a protected damage reaction");
 c2_fixture(floor);assertion(evade_begin(p,-1),"grounded evasion preflight accepts the harmless roll clip");
 BorFrame hurt=*bor_frame(&p->a);hurt.ox=hurt.oy=0;hurt.bx=-20;hurt.by=-40;hurt.bw=hurt.bh=40;BorAnimation target_pose=*p->a.animation;target_pose.frames=&hurt;target_pose.count=1;p->a.animation=&target_pose;p->a.frame=0;
 BorFrame impact=*bor_frame(&red->a);impact.ox=impact.oy=0;impact.ax=-20;impact.ay=-40;impact.aw=impact.ah=40;impact.damage=7;BorAnimation attack=*red->a.animation;attack.frames=&impact;attack.count=1;red->a.animation=&attack;red->a.frame=0;red->a.x=p->a.x;red->a.y=p->a.y;red->a.active=true;red->a.epoch=987654;red->a.state=BOR_ACTION;
 int hp=p->a.health;assertion(bor_overlaps(&red->a,&p->a)&&!bor_damage(&red->a,&p->a,world_time)&&p->a.health==hp&&p->a.last_hit[red->a.serial]!=red->a.epoch,"evasion rejects a real overlapping damage activation without consuming it");
 p->job_tick=(int)p->job.evasion_ticks;evade_finish(p);p->a.animation=&target_pose;p->a.frame=0;assertion(bor_damage(&red->a,&p->a,world_time)&&p->a.health==hp-7,"the same activation can damage immediately after evasion ends");
 c2_fixture(floor);total_frames=tick;
}
