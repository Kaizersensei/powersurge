/* C2-intended controls/traversal on the single BOR FSM and Surge body. Signed: Atlas. */
static void apply_physics(HostActor* h){
 physicsactor_reset_model_parameters(h->body);physicsactor_set_airdrag(h->body,1);physicsactor_set_capspeed(h->body,3000);
 physicsactor_set_topyspeed(h->body,h->physics.enabled?h->physics.max_fall:960);physicsactor_set_topspeed(h->body,h->physics.enabled?h->physics.speed:3000);physicsactor_set_grv(h->body,h->physics.enabled?h->physics.gravity:1000);
 if(h->physics.enabled){physicsactor_set_acc(h->body,h->physics.acceleration);physicsactor_set_dec(h->body,h->physics.acceleration+h->physics.deceleration);physicsactor_set_frc(h->body,h->physics.deceleration);physicsactor_set_air(h->body,h->physics.acceleration*h->physics.air_control);}
}
/* Steep surfaces cannot support an idle stop. High C2 braking friction cancels
 * Surge's slope force while its low-speed horizontal lock keeps renewing. */
static double terrain_friction(HostActor* h,double requested){
 if(!physicsactor_is_midair(h->body)&&!h->actor_support&&fabs(remainder(physicsactor_get_angle(h->body),360.0))>=45.0)return 0;
 return requested;
}
static void traversal_clear(HostActor* h){
 contact_clear(h);h->guard_facing=0;h->defense_held=false;h->lock_target=0;h->aim_position=v2d_new(0,0);
 h->motion_step=h->motion_deadline=h->motion_octant=0;h->armed_motion=false;h->region_id=0;h->climb_direction=0;h->climb_ready_tick=0;h->climb_needs_release=false;h->ledge_id=0;h->support_velocity=v2d_new(0,0);h->job_tick=0;h->job_direction=0;h->guard_kind=h->guard_age=0;h->roll_angle=0;
 h->jump_cut_active=false;h->down_held=false;h->jump_held=false;
 for(int i=0;i<2;i++){h->hold_ticks[i]=0;h->channel_captured[i]=false;h->channel_down[i]=false;h->press_ticks[i]=0;h->press_captured[i]=false;}
 h->a.pose_x=h->a.pose_y=h->a.pose_angle=0;h->a.pose_scale=1;
}
static const obstacle_t* solid_at(HostActor* h,const obstaclemap_t* map,int x,int y){
 if(!map)return NULL;
 const obstacle_t* o=obstaclemap_get_best_obstacle_at(map,x,y,x+1,y+1,MM_FLOOR,physicsactor_get_layer(h->body));
 return o&&obstacle_is_solid(o)&&obstacle_point_collision(o,(point2d_t){x,y})?o:NULL;
}
static const obstacle_t* ledge_at(HostActor* h,const obstaclemap_t* map,int x,int y){
 if(!map)return NULL;
 const obstacle_t* o=obstaclemap_get_best_obstacle_at(map,x,y,x+1,y+1,MM_FLOOR,physicsactor_get_layer(h->body));
 return o&&obstacle_point_collision(o,(point2d_t){x,y})?o:NULL;
}
static bool body_clear_except(HostActor* h,const obstaclemap_t* map,double x,double y,uint64_t ignored){
 if(!map)return true;
 int width,height;v2d_t center,raw=physicsactor_get_position(h->body);physicsactor_bounding_box(h->body,&width,&height,&center);
 center.x+=x-raw.x;center.y+=y-(raw.y+20);
 return !obstaclemap_solid_rect_except(map,(int)floor(center.x-width*.5),(int)floor(center.y-height*.5),(int)ceil(center.x+width*.5)-1,(int)ceil(center.y+height*.5)-1,physicsactor_get_layer(h->body),ignored);
}
static bool body_clear(HostActor* h,const obstaclemap_t* map,double x,double y){return body_clear_except(h,map,x,y,0);}
static bool carry_sweep(HostActor* h,const obstaclemap_t* map,v2d_t from,v2d_t to){
 double distance=fmax(fabs(to.x-from.x),fabs(to.y-from.y));
 if(!isfinite(from.x)||!isfinite(from.y)||!isfinite(to.x)||!isfinite(to.y)||!isfinite(distance)||distance>512)return false;
 int steps=(int)ceil(distance);if(steps<1)steps=1;
 if(steps>512)return false; /* Teleports require an explicit relocation policy. */
 for(int i=1;i<=steps;i++){double t=(double)i/steps;if(!body_clear_except(h,map,from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t,h->ledge_id))return false;}
 return body_clear(h,map,to.x,to.y);
}
static bool body_sweep(HostActor* h,const obstaclemap_t* map,v2d_t from,v2d_t to){
 double distance=fmax(fabs(to.x-from.x),fabs(to.y-from.y));
 if(!isfinite(from.x)||!isfinite(from.y)||!isfinite(to.x)||!isfinite(to.y)||!isfinite(distance)||distance>512)return false;
 int steps=(int)ceil(distance);if(steps<1)steps=1;
 for(int i=1;i<=steps;i++){double t=(double)i/steps;if(!body_clear(h,map,from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t))return false;}
 return soft_path_clear(h,from,to);
}
static void traversal_position(HostActor* h,v2d_t foot){h->a.x=foot.x;h->a.y=foot.y;physicsactor_set_position(h->body,v2d_new(foot.x,foot.y-20));render_frame(h);commit_pose(h);}
static bool carry_fallback(HostActor* h,const obstaclemap_t* map){
 v2d_t from=v2d_new(h->a.x,h->a.y);if(body_clear(h,map,from.x,from.y))return true;
 // Emergency depenetration when a moved support already covers the previous position.
 for(int distance=1;distance<=128;distance++){
  const int offsets[][2]={{-h->a.facing,0},{0,1},{0,-1},{h->a.facing,0},{-h->a.facing,1},{-h->a.facing,-1}};
  for(unsigned i=0;i<sizeof(offsets)/sizeof(*offsets);i++){v2d_t to=v2d_new(from.x+offsets[i][0]*distance,from.y+offsets[i][1]*distance);if(body_clear(h,map,to.x,to.y)){traversal_position(h,to);return true;}}
 }
 if(body_clear(h,map,h->a.spawn_x,h->a.spawn_y)){traversal_position(h,v2d_new(h->a.spawn_x,h->a.spawn_y));return true;}
 h->a.timeline_fault=true;h->steering=0;logfile_message("BOR support relocation has no clear fallback: %s; reset required",h->a.model->name);return false;
}
static bool support_valid(HostActor* h,const obstaclemap_t* map){
 if(!map||!h->ledge_id)return false;
 const obstacle_t* found=obstaclemap_find_by_id(map,h->ledge_id,physicsactor_get_layer(h->body));
 if(!found||obstacle_is_solid(found)!=h->support_solid)return false;
 point2d_t at=obstacle_get_position(found);double dx=at.x-h->support_position.x,dy=at.y-h->support_position.y;
 v2d_t edge=v2d_new(h->ledge.x+dx,h->ledge.y+dy);
 if(!obstacle_point_collision(found,(point2d_t){(int)edge.x+h->a.facing,(int)edge.y})||ledge_at(h,map,(int)edge.x+h->a.facing,(int)edge.y-1))return false;
 v2d_t to=v2d_new(h->a.x+dx,h->a.y+dy);
 if((dx||dy)&&!carry_sweep(h,map,v2d_new(h->a.x,h->a.y),to)){h->support_velocity=v2d_new(0,0);carry_fallback(h,map);return false;}
 if(h->support_sample_tick!=total_frames){h->support_velocity=v2d_new(dx*60,dy*60);h->support_sample_tick=total_frames;}h->support_position=at;h->ledge=edge;h->hang_start=v2d_add(h->hang_start,v2d_new(dx,dy));
 if(dx||dy)traversal_position(h,to);
 return true;
}
static void ledge_release(HostActor* h,double vx,double vy){
 if(!body_clear(h,terrain_map(),h->a.x,h->a.y)){h->support_velocity=v2d_new(0,0);carry_fallback(h,terrain_map());}
 vx+=h->support_velocity.x;vy+=h->support_velocity.y;h->support_velocity=v2d_new(0,0);h->ledge_id=0;h->job_tick=0;h->ledge_ready_tick=total_frames+(int)h->physics.ledge_cooldown_ticks;h->jump_cut_active=false;
 if(h->a.timeline_fault)return;
 physicsactor_detach_from_ground(h->body);h->a.grounded=false;h->was_ground=false;motion_apply(h,BOR_MOTION_SET,3,vx,vy);start(h,"walkoff",BOR_FREE);
}
static bool ledge_acquire(HostActor* h,const obstaclemap_t* map){
 BorActor* a=&h->a;BorPhysics* p=&h->physics;
 bool allowed=a->state==BOR_FREE||(a->state==BOR_ACTION&&!(a->animation->flags&BOR_ANIM_ATTACK)&&(a->animation->flags&BOR_ANIM_AIR_ATTACK));
 if(!allowed||a->grounded||a->vy<0||!h->input_direction||h->down_held||total_frames<h->ledge_ready_tick||!bor_animation(a->model,"ledge")||!bor_animation(a->model,"climb"))return false;
 int direction=h->input_direction,chest=(int)floor(a->y-p->chest_height),wallx=0;const obstacle_t* wall=NULL;
 for(int step=1;step<=(int)p->ledge_reach;step++){
  int x=(int)floor(a->x)+direction*step;wall=ledge_at(h,map,x,chest);if(wall){wallx=x;break;}
 }
 if(!wall)return false;
 int top=chest;
 for(int i=0;i<=(int)p->ledge_scan;i++,top--){
  const obstacle_t* candidate=ledge_at(h,map,wallx+direction,top);
  if(candidate!=wall)return false; /* Never join a wall to a different collider's top. */
  if(!ledge_at(h,map,wallx+direction,top-1))break;
 }
 if(chest-top>p->ledge_scan)return false;
 v2d_t ledge=v2d_new(wallx,top),hang=v2d_new(wallx-direction*p->hang_back,top+p->hang_drop);
 v2d_t stand=v2d_new(wallx+direction*p->climb_inset,top-2);
 if(!body_clear(h,map,hang.x,hang.y)||!body_clear(h,map,stand.x,stand.y)||!body_sweep(h,map,v2d_new(a->x,a->y),hang))return false;
 h->job=*p;h->job_tick=0;h->ledge=ledge;h->hang_start=hang;h->ledge_id=obstacle_get_id(wall);h->support_solid=obstacle_is_solid(wall);h->support_velocity=v2d_new(0,0);h->support_sample_tick=total_frames;h->support_position=obstacle_get_position(wall);a->facing=direction;h->jump_cut_active=false;
 if(!start(h,"ledge",BOR_LEDGE)){h->ledge_id=0;return false;}
 physicsactor_detach_from_ground(h->body);a->grounded=false;motion_apply(h,BOR_MOTION_STOP,3,0,0);traversal_position(h,hang);return true;
}
static bool climb_begin(HostActor* h,const obstaclemap_t* map){
 if(!support_valid(h,map))return false;
 v2d_t above=v2d_new(h->hang_start.x,h->ledge.y-2),target=v2d_new(h->ledge.x+h->a.facing*h->job.climb_inset,h->ledge.y-2);
 if(!body_sweep(h,map,h->hang_start,above)||!body_sweep(h,map,above,target))return false;
 if(!start(h,"climb",BOR_CLIMB))return false;
 h->job_tick=0;return true;
}
static void traversal_step(HostActor* h,const obstaclemap_t* map){
 if(!support_valid(h,map)){ledge_release(h,0,100);return;}
 if(h->a.state==BOR_LEDGE)return;
 v2d_t from=v2d_new(h->a.x,h->a.y),above=v2d_new(h->hang_start.x,h->ledge.y-2),target=v2d_new(h->ledge.x+h->a.facing*h->job.climb_inset,h->ledge.y-2);
 int next=h->job_tick+1;double t=fmin(1,next/h->job.climb_ticks);v2d_t to=t<=.8?v2d_new(h->hang_start.x,h->hang_start.y+(above.y-h->hang_start.y)*t/.8):v2d_new(above.x+(target.x-above.x)*(t-.8)/.2,above.y);
 bool crossed=h->job_tick/h->job.climb_ticks<.8&&t>.8;
 if((crossed&&(!body_sweep(h,map,from,above)||!body_sweep(h,map,above,to)))||(!crossed&&!body_sweep(h,map,from,to))){ledge_release(h,0,100);return;}
 h->job_tick=next;traversal_position(h,to);
 if(next>=(int)h->job.climb_ticks)ledge_release(h,h->a.facing*80,-80);
}
static void traversal_after_motion(HostActor* h){
 if(h->a.state==BOR_DASH||h->a.state==BOR_WALL_JUMP){
  h->job_tick++;int limit=h->a.state==BOR_DASH?(int)h->job.dash_ticks:(int)h->job.wall_lock_ticks;
  if(h->job_tick>=limit)start(h,h->a.grounded?"idle":"jump",h->a.grounded?BOR_FREE:BOR_ACTION);
 }
 if(h->a.state==BOR_ROLL&&(!h->down_held||!h->a.grounded||fabs(h->a.vx)<=h->job.roll_entry*h->job.roll_stop_ratio))start(h,h->a.grounded?"duck":"walkoff",BOR_FREE);
 if(h->a.state==BOR_EVADE){if(!h->a.grounded)start(h,"walkoff",BOR_FREE);else ++h->job_tick;}
 if(h->a.state!=BOR_LEDGE&&h->a.state!=BOR_CLIMB)ledge_acquire(h,terrain_map());
}
/* Finish only after every actor's hit callbacks, so slot order cannot shorten immunity. */
static void evade_finish(HostActor* h){
 if(h->a.state==BOR_EVADE&&h->job_tick>=(int)h->job.evasion_ticks)start(h,"idle",BOR_FREE);
}
static bool player_jump(HostActor* h,int direction,bool double_jump,bool dash_jump){
 if(double_jump&&!bsl_host_capability_allowed(h->lifetime,"traversal.double_jump"))return false; // Signed: Mech.
 const char* name=double_jump?"doublejump":dash_jump?"runjump":direction?"forwardjump":"jump";
 if(!bor_animation(h->a.model,name))return false;
 double strength=double_jump?h->physics.second_jump:h->physics.jump;if(dash_jump)strength=h->physics.dash_jump_strength;
 if(!start(h,name,BOR_ACTION))return false;
 motion_apply(h,BOR_MOTION_SET,BOR_AXIS_Y,0,-strength);h->jumps_used++;h->jump_cut_active=true;return true;
}
static bool wall_jump(HostActor* h,int direction,const obstaclemap_t* map){
 if(!direction||h->a.grounded||!bor_animation(h->a.model,"walljump"))return false;
 int width,height;physicsactor_bounding_box(h->body,&width,&height,NULL);int x=(int)round(h->a.x+direction*(width*.5+2));
 if(test_mode)logfile_message("BOR_C2_WALL x=%.2f y=%.2f width=%d probe=%d solid=%d",h->a.x,h->a.y,width,x,solid_at(h,map,x,(int)h->a.y-15)!=NULL);
 if(!solid_at(h,map,x,(int)h->a.y-15)&&!solid_at(h,map,x,(int)h->a.y-30))return false;
 if(!start(h,"walljump",BOR_WALL_JUMP))return false;
 h->job=h->physics;h->job_tick=0;h->job_direction=-direction;h->a.facing=-direction;h->jumps_used=1;h->jump_cut_active=false;
 motion_apply(h,BOR_MOTION_SET,3,-direction*h->job.wall_jump_x,-h->job.wall_jump_y);return true;
}
static void locomotion_update(HostActor* h){
 BorActor* a=&h->a;if(!h->physics.enabled)return;
 if(a->state==BOR_DASH){h->locomotion=3;return;}
 if(a->state!=BOR_FREE||!a->grounded)return;
 double speed=fabs(a->vx);h->locomotion=speed<=h->physics.idle_speed?0:speed>h->physics.speed+.01?3:speed>=h->physics.speed*h->physics.run_ratio?2:1;
 const char* wanted=h->down_held?"duck":h->locomotion==0?"idle":h->locomotion==1?"c2_walk":h->locomotion==2?"c2_run":"dash";
 if(!bor_animation(a->model,wanted))wanted=h->locomotion?"walk":"idle";
 if(strcmp(a->animation->name,wanted))start(h,wanted,BOR_FREE);
}
static bool contextual_grab(HostActor*,int);
static bool guard_hit(HostActor* source,HostActor* target,const BorFrame* frame){
 BorActor* a=&source->a;BorActor* b=&target->a;
 if(b->state!=BOR_GUARD||frame->no_block||!a->active||!a->epoch||a->health<=0||a->state==BOR_DEAD||a->state==BOR_FALL||b->health<=0||b->invulnerable>0||!(a->model->damage_mask&b->model->type)||a->serial>=64||b->last_hit[a->serial]==a->epoch||!bor_overlaps(a,b)||(a->x-b->x)*b->facing<0)return false;
 bool parry=target->guard_kind==2&&target->guard_age<(int)target->job.parry_ticks;
 if(target->guard_kind!=1&&!parry)return false;
 b->last_hit[a->serial]=a->epoch;play_impact(source,target,frame,true,0,false);b->freeze=fmax(b->freeze,.04);a->freeze=fmax(a->freeze,parry?.12:.04);return true;
}
/* Ordered face-relative input history; neutral gaps are allowed inside each15-tick window. */
static void armed_history(HostActor* h,int direction,bool up,bool down_input,bool pressed_sword){
 int code=down_input?4:up?(direction?2:1):direction?3:0;
 if(h->motion_step&&direction&&direction!=h->motion_facing)h->motion_step=0;
 if(total_frames>h->motion_deadline)h->motion_step=0;
 if(code&&code!=h->motion_octant){
  if(code==1){h->motion_step=1;h->motion_facing=h->a.facing;h->motion_deadline=total_frames+15;}
  else if(code==2&&direction==h->motion_facing&&h->motion_step==1){h->motion_step=2;h->motion_deadline=total_frames+15;}
  else if(code==3&&direction==h->motion_facing&&h->motion_step==2){h->motion_step=3;h->motion_deadline=total_frames+15;}
  else h->motion_step=0;
 }
 h->motion_octant=code;
 if(pressed_sword){h->armed_motion=h->motion_step==3&&total_frames<=h->motion_deadline&&direction==h->motion_facing&&!up&&!down_input;h->armed_direction=direction;h->armed_up=up;h->armed_down=down_input;h->motion_step=0;}
}
static bool requested_attack(HostActor* h,int channel,bool crouch,int direction,bool motion){
 BorActor* a=&h->a;int context=motion&&!a->grounded?4:a->state==BOR_DASH?3:!a->grounded?1:crouch?2:0;
 int axis=crouch&&!a->grounded?2:direction?1:0;const char* clip=bor_combo_select(a,channel,context,axis);
 if(!clip){if(channel)clip="unarmed";else if(context==1)clip=air_attack(a,crouch,false);else if(context==4)clip="runjumpattack";else if(context==3)clip="runattack";}
 int previous=h->action_channel;h->action_channel=channel;
 bool accepted=clip?bor_combo_attack(a,clip,hit_segment,h):bor_chain_attack(a,hit_segment,h);
 if(!accepted)h->action_channel=previous;
 return accepted;
}
/* Guard is an explicit defensive cancel; other action replacements stay authored. */
static bool guard_begin(HostActor* h,int kind){
 BorActor* a=&h->a;
 if(kind<1||kind>2||!a->grounded||a->health<=0||a->timeline_fault||h->frozen_tick||a->freeze>0||!(a->state==BOR_FREE||(a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_ATTACK))))return false;
 const char* name=kind==1?"armedguard":"unarmedguard";const BorAnimation* pose=bor_animation(a->model,name);
 if(!pose||!pose->frames||pose->count<1||pose->count>4096||pose->jump_frame>=0||(pose->flags&BOR_ANIM_ATTACK))return false;
 for(int i=0;i<pose->count;i++)if((pose->frames[i].aw>0&&pose->frames[i].ah>0)||pose->frames[i].move||pose->frames[i].movea||pose->frames[i].movez)return false;
 if(!start(h,name,BOR_GUARD))return false;
 unlink_actor(h);h->guard_kind=kind;h->guard_age=0;h->job=h->physics;h->steering=0;h->jump_cut_active=false;
 h->armed_motion=false;h->motion_step=h->motion_deadline=h->motion_octant=0;
 a->combo=a->combo_stage=a->combo_hits=0;a->combo_until=0;a->combo_owner=0;a->combo_arming=false;
 for(int i=0;i<2;i++)h->press_captured[i]=false;
 motion_tangent(h,0);return true;
}
/* Live target hint; no movement authority or projectile implementation. */
static void lock_update(HostActor* h,bool held){
 h->lock_target=0;h->aim_position=v2d_new(h->a.x,h->a.y-20);if(!held||h->a.health<=0||h->a.state==BOR_DEAD)return;
 double best=HUGE_VAL;
 for(int i=0;i<64;i++)if(slots[i]&&slots[i]!=h){
  HostActor* other=slots[i];BorActor* target=&other->a;
  if(!other->placed||other->suspended||target->model->training_dummy||!bor_can_target(&h->a,target))continue;
  double dx=target->x-h->a.x,dy=target->y-h->a.y,distance=dx*dx+dy*dy;
  if(distance<best||(distance==best&&other->lifetime<h->lock_target)){
   best=distance;h->lock_target=other->lifetime;double x,y,w,height;bor_rect(target,false,&x,&y,&w,&height);
   h->aim_position=w>0&&height>0?v2d_new(x+w*.5,y+height*.5):v2d_new(target->x,target->y-20);
  }
 }
}
static bool evade_begin(HostActor* h,int direction){
 BorActor* a=&h->a;
 if(!direction||!a->grounded||a->health<=0||h->frozen_tick||a->freeze>0||a->timeline_fault||!(a->state==BOR_FREE||a->state==BOR_GUARD||(a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_ATTACK))))return false;
 const BorAnimation* pose=bor_animation(a->model,"roll");
 if(!pose||!pose->frames||pose->count<1||pose->count>4096||pose->jump_frame>=0||(pose->flags&BOR_ANIM_ATTACK))return false;
 for(int i=0;i<pose->count;i++)if((pose->frames[i].aw>0&&pose->frames[i].ah>0)||pose->frames[i].move||pose->frames[i].movea||pose->frames[i].movez)return false;
 if(!start(h,"roll",BOR_EVADE))return false;
 unlink_actor(h);h->job=h->physics;h->job_direction=direction;h->job_tick=0;h->guard_kind=0;h->steering=0;h->jump_cut_active=false;h->roll_entry_speed=a->x;h->roll_angle=0;
 h->armed_motion=false;h->motion_step=h->motion_deadline=h->motion_octant=0;
 a->combo=a->combo_stage=a->combo_hits=0;a->combo_until=0;a->combo_owner=0;a->combo_arming=false;
 for(int i=0;i<2;i++)h->press_captured[i]=false;
 motion_tangent(h,direction*h->job.evasion_speed);return true;
}
static void player_actions(HostActor* h,const ALLEGRO_KEYBOARD_STATE* keys){
 if(!h->physics.enabled){legacy_player_actions(h,keys);return;}
 BorActor* a=&h->a;motion_query(h);
 int direction=(down(keys,ALLEGRO_KEY_D)||down(keys,ALLEGRO_KEY_RIGHT)||input_button_down(controls,IB_RIGHT))-(down(keys,ALLEGRO_KEY_A)||down(keys,ALLEGRO_KEY_LEFT)||input_button_down(controls,IB_LEFT));
 bool jump_down=down(keys,ALLEGRO_KEY_L)||down(keys,ALLEGRO_KEY_SPACE)||input_button_down(controls,IB_FIRE1);
 bool jump=jump_down&&!h->jump_held;
 bool crouch=down(keys,ALLEGRO_KEY_S)||down(keys,ALLEGRO_KEY_DOWN)||input_button_down(controls,IB_DOWN);
 bool up=down(keys,ALLEGRO_KEY_W)||down(keys,ALLEGRO_KEY_UP)||input_button_down(controls,IB_UP);
 bool special=pressed(keys,ALLEGRO_KEY_K)||input_button_pressed(controls,IB_FIRE5);
 bool held[]={down(keys,ALLEGRO_KEY_H)||input_button_down(controls,IB_FIRE2),down(keys,ALLEGRO_KEY_J)||input_button_down(controls,IB_FIRE6)},tap[2]={false,false};
 bool defense=down(keys,ALLEGRO_KEY_LCTRL)||input_button_down(controls,IB_FIRE7);
 bool ranged_held=down(keys,ALLEGRO_KEY_K)||input_button_down(controls,IB_FIRE5);
 bool pickup_edge=defense&&!h->defense_held&&crouch&&!held[0]&&!held[1]&&!special&&!ranged_held&&!jump_down;
 if(defense!=h->defense_held)h->tap_tick[0]=h->tap_tick[1]=-1000;
 if(defense&&!h->defense_held)h->guard_facing=a->facing;
 h->defense_held=defense;lock_update(h,defense);
 armed_history(h,direction,up,crouch,!defense&&held[0]&&!h->channel_down[0]);
 bool air_idle=a->state==BOR_ACTION&&!(a->animation->flags&BOR_ANIM_ATTACK)&&(a->animation->flags&BOR_ANIM_AIR_ATTACK);
 bool committed=a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_ATTACK);
 bool accepts=!h->frozen_tick&&a->freeze<=0&&(bor_can_act(a)||air_idle||a->state==BOR_GRAB);
 bool guard_eligible=!h->frozen_tick&&a->freeze<=0&&!a->timeline_fault&&a->grounded&&(a->state==BOR_FREE||committed);
 bool edge_direction=direction&&direction!=h->tap_direction;
 bool dash=edge_direction&&total_frames-h->tap_tick[direction>0]<=(int)h->physics.double_tap_ticks;
 if(edge_direction)h->tap_tick[direction>0]=total_frames;
 h->tap_direction=direction;
 bool evasion=defense&&dash;
 dash|=pressed(keys,ALLEGRO_KEY_LSHIFT)||pressed(keys,ALLEGRO_KEY_RSHIFT);
 for(int i=0;i<2;i++){
  bool edge=held[i]&&!h->channel_down[i];
  tap[i]=edge&&!defense&&(accepts||committed||a->state==BOR_DASH||a->state==BOR_ROLL);
  h->press_captured[i]=false;h->press_ticks[i]=0;h->hold_ticks[i]=defense&&held[i]&&guard_eligible?1:0;
  h->channel_captured[i]=defense&&held[i];h->channel_down[i]=held[i];
 }
 if(!defense)h->guard_facing=0;
 if(h->frozen_tick||a->freeze>0)return;
 if(!up&&!crouch)h->climb_needs_release=false;
 bool released=h->jump_held&&!jump_down;h->jump_held=jump_down;h->down_held=crouch;h->input_direction=direction;
 if(pickup_edge&&bsl_host_pickup_try(h->lifetime)>=0){h->steering=h->input_direction=0;return;}
 if(climb_state(a->state)){h->climb_direction=(crouch?1:0)-(up?1:0);region_player(h,direction,jump);return;}
 if(evasion&&evade_begin(h,direction))return;
 if(a->state==BOR_EVADE)return;
 if(a->state==BOR_GUARD){h->guard_age+=(h->guard_age<(int)h->job.parry_ticks);if(!defense||h->guard_kind<1||h->guard_kind>2||!held[h->guard_kind-1]){h->guard_kind=0;h->hold_ticks[0]=h->hold_ticks[1]=0;start(h,"idle",BOR_FREE);}return;}
 if(a->state==BOR_LEDGE){if(!support_valid(h,terrain_map())){ledge_release(h,0,100);return;}if(crouch||direction==-a->facing)ledge_release(h,0,100);else if(up||jump)climb_begin(h,terrain_map());return;}
 if(a->state==BOR_CLIMB||a->state==BOR_WALL_JUMP)return;
 if(a->state==BOR_DASH){if(tap[0]){requested_attack(h,0,h->armed_down,h->armed_direction,false);h->armed_motion=false;return;}if(tap[1]){requested_attack(h,1,crouch,direction,false);return;}if(jump&&a->grounded&&total_frames-h->dash_started_tick<=(int)h->job.dash_jump_ticks)player_jump(h,direction,false,true);return;}
 if(a->state==BOR_ROLL){if(tap[1]){requested_attack(h,1,true,direction,false);return;}if(jump&&a->grounded)player_jump(h,direction,false,false);return;}
 if(a->state==BOR_DEAD||a->state==BOR_PAIN||a->state==BOR_FALL||a->state==BOR_RISE||a->state==BOR_GRABBED)return;
 if(released&&h->jump_cut_active&&a->vy<0){motion_apply(h,BOR_MOTION_SET,2,0,a->vy*h->physics.jump_cut);h->jump_cut_active=false;}
 if(a->state==BOR_GRAB){
  HostActor* target=linked_partner(h);if(!target){a->partner=-1;start(h,"idle",BOR_FREE);return;}
  bool away=direction==-a->facing;if(!away)h->grab_release=world_time+.5;
  if(jump||((tap[0]||tap[1])&&away)){
   if(bor_animation(a->model,"throw")){unlink_actor(h);target->a.facing=a->facing;motion_apply(target,BOR_MOTION_SET,3,-a->facing*target->a.model->throw_distance,-target->a.model->throw_height);target->landing_damage=a->model->throw_damage;target->lie_time=0;target->reaction_tick=total_frames;start(target,"fall",BOR_FALL);h->action_channel=1;start(h,"throw",BOR_ACTION);}
  }else if(tap[0]||tap[1]){bool finishing=crouch||a->grabs>=2;const char* move=finishing?"grabattack2":"grabattack";if(bor_animation(a->model,move)){a->grabs++;h->grab_finisher=finishing;h->action_channel=1;start(h,move,BOR_ACTION);}}
  else if(away&&world_time>h->grab_release){unlink_actor(h);start(h,"idle",BOR_FREE);}
  return;
 }
 if(guard_eligible&&defense){
  for(int i=0;i<2;i++)if(held[i]&&guard_begin(h,i+1))return;
 }
 if(a->state==BOR_ACTION&&(a->animation->flags&BOR_ANIM_ATTACK)){
  h->jump_cut_active=false;
  if((!a->grounded&&(a->animation->flags&BOR_ANIM_AIR_CONTROL))||(a->animation->flags&BOR_ANIM_RANGED))drive(h,direction,h->physics.speed);
  int previous=h->action_channel;if(tap[0])h->action_channel=0;
  if(!bor_try_cancel(a,(jump?BOR_INPUT_JUMP:0)|(special?BOR_INPUT_SPECIAL:0)|(tap[0]?BOR_INPUT_ATTACK:0),hit_segment,h))h->action_channel=previous;
  return;
 }
 if(a->state==BOR_ACTION&&!air_idle)return;
 if(!defense&&pressed(keys,ALLEGRO_KEY_U)&&bsl_host_ability_use(h->lifetime,0)==0)return;
 if((up||crouch)&&!h->climb_needs_release&&region_acquire(h,(crouch?1:0)-(up?1:0)))return;
 if(direction&&!defense)a->facing=direction;
 drive(h,crouch&&a->grounded?0:direction,h->physics.speed);
 if(dash&&!defense&&direction&&a->grounded&&!crouch&&total_frames>=h->dash_ready_tick&&start(h,"dash",BOR_DASH)){
  h->job=h->physics;h->job_tick=0;h->job_direction=direction;h->dash_started_tick=total_frames;h->dash_ready_tick=total_frames+(int)(h->job.dash_ticks+h->job.dash_cooldown_ticks);h->steering=0;motion_tangent(h,direction*h->job.dash_speed);if(tap[0]){requested_attack(h,0,h->armed_down,h->armed_direction,false);h->armed_motion=false;}else if(tap[1])requested_attack(h,1,crouch,direction,false);return;
 }
 if(tap[1]&&contextual_grab(h,direction))return;
 if(tap[0]||tap[1]){
  if(tap[0]){int previous=h->action_channel;h->action_channel=0;if(bor_try_cancel(a,BOR_INPUT_ATTACK,hit_segment,h)){h->steering=0;return;}h->action_channel=previous;}
  h->jump_cut_active=false;
  if(tap[1])requested_attack(h,1,crouch,direction,false);
  else {requested_attack(h,0,h->armed_down,h->armed_direction,h->armed_motion);h->armed_motion=false;}
  return;
 }
 if(special){h->jump_cut_active=false;start(h,bor_animation(a->model,"c2_shoot")?"c2_shoot":"special",BOR_ACTION);return;}
 if(jump){
  if(!a->grounded&&wall_jump(h,direction,terrain_map()))return;
  if(a->grounded){player_jump(h,direction,false,false);return;}
  if(!h->jumps_used)h->jumps_used=1;
  if(h->jumps_used<(int)h->physics.jump_count){player_jump(h,direction,true,false);return;}
 }
 if(crouch&&a->grounded&&fabs(a->vx)>h->physics.roll_entry&&start(h,"roll",BOR_ROLL)){h->job=h->physics;h->roll_entry_speed=a->x;h->roll_angle=0;h->steering=0;return;}
 if(a->grounded){locomotion_update(h);return;}
 const char* wanted="jump";
 if(strcmp(a->animation->name,wanted)&&!(air_idle&&!a->grounded))start(h,wanted,a->grounded?BOR_FREE:BOR_ACTION);
}
