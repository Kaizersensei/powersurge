/* Native soft contact vetoes the mover's displacement; it never pushes a neighbour.
 * Geometry is authored independently from per-frame combat boxes. Signed: Atlas. */
static void contact_clear(HostActor* h){
 h->contact_x_blocked=false;h->actor_support=h->ignored_support=h->contact_lifetime=0;h->actor_support_ticks=0;h->support_block_tick=-1;h->contact_slot=-1;h->contact_ticks=0;h->grip_distance=0;
}
static HostActor* contact_actor(uint64_t id){if(!id)return NULL;for(int i=0;i<64;i++)if(slots[i]&&slots[i]->lifetime==id)return slots[i];return NULL;}
static bool contact_live(const HostActor* h){return h->placed&&!h->suspended&&h->a.health>0&&h->a.state!=BOR_DEAD&&h->a.state!=BOR_GRABBED&&h->a.model->soft_solid;}
static bool contact_pair(const HostActor* h,const HostActor* o){return o&&o!=h&&h->placed&&!h->suspended&&h->a.health>0&&h->a.state!=BOR_DEAD&&h->a.model->soft_solid&&contact_live(o)&&!(h->a.partner==(int)o->a.serial&&o->a.partner==(int)h->a.serial)&&h->ignored_support!=o->lifetime;}
static bool contact_overlap(const HostActor* h,const HostActor* o,double x,double y){return fabs(x-o->a.x)<h->a.model->soft_radius+o->a.model->soft_radius-.01&&y>o->a.y-o->a.model->soft_height+.5&&y-h->a.model->soft_height<o->a.y-.5;}
static v2d_t soft_clip(HostActor* h,v2d_t from,v2d_t to,bool attach){
 h->contact_x_blocked=false;if(!contact_live(h))return to;
 HostActor* ignored=contact_actor(h->ignored_support);
 if(ignored&&(fabs(from.x-ignored->a.x)>h->a.model->soft_radius+ignored->a.model->soft_radius+1||from.y<ignored->a.y-ignored->a.model->soft_height-1||from.y-h->a.model->soft_height>ignored->a.y+1))h->ignored_support=0;
 double distance=fmax(fabs(to.x-from.x),fabs(to.y-from.y));if(!isfinite(distance)||distance>1024){h->contact_x_blocked=to.x!=from.x;return from;}
 int steps=(int)ceil(distance*2);if(steps<1)steps=1;
 /* v2d_t is float. Sample the ideal trajectory in double precision instead of
  * accumulating rounded substeps; untouched axes return the exact candidate. */
 double x=from.x,y=from.y;bool fixed_y=false;
 for(int step=1;step<=steps;step++){
  double t=(double)step/steps,nx=h->contact_x_blocked?x:from.x+((double)to.x-from.x)*t,ny=fixed_y?y:from.y+((double)to.y-from.y)*t;
  for(int i=0;i<64;i++){
   HostActor* o=slots[i];if(!contact_pair(h,o))continue;
   double radius=h->a.model->soft_radius+o->a.model->soft_radius,head=o->a.y-o->a.model->soft_height;
   bool platform=attach&&total_frames>h->support_block_tick&&!o->actor_support&&h->a.state!=BOR_FALL&&h->a.state!=BOR_DEAD;
   if(platform&&ny>=y&&y<=head+.5&&ny>=head&&fabs(nx-o->a.x)<radius-.5&&body_clear(h,terrain_map(),nx,head)){
    ny=head;fixed_y=true;
    if(h->actor_support!=o->lifetime){h->actor_support=o->lifetime;h->actor_support_ticks=0;}
    h->actor_support_position=v2d_new(o->a.x,o->a.y);h->a.grounded=true;physicsactor_set_ysp(h->body,0);
   }
   if(contact_overlap(h,o,nx,ny)){
    bool was=contact_overlap(h,o,x,y);double before=x-o->a.x,after=nx-o->a.x;
    if(!was&&nx!=x&&(y>head+.5&&y-h->a.model->soft_height<o->a.y-.5)){nx=x<=o->a.x?o->a.x-radius:o->a.x+radius;h->contact_x_blocked=true;}
    else if(fabs(after)<fabs(before)||before*after<0){nx=x;h->contact_x_blocked=true;}
   }
  }
  x=nx;y=ny;
 }
 return v2d_new(h->contact_x_blocked?x:to.x,fixed_y?y:to.y);
}
static v2d_t contact_resolve(HostActor* h,v2d_t from,v2d_t candidate){
 uint64_t previous=h->actor_support;int ticks=h->actor_support_ticks;bool grounded=h->a.grounded;double vy=physicsactor_get_ysp(h->body);v2d_t support_at=h->actor_support_position;
 v2d_t resolved=soft_clip(h,from,candidate,true);
 if((resolved.x!=candidate.x||resolved.y!=candidate.y)&&!body_clear(h,terrain_map(),resolved.x,resolved.y)){
  /* Terrain wins at corners/slopes: allow temporary actor overlap instead of
   * rolling back one axis into terrain or forcing an emergency teleport. */
  h->actor_support=previous;h->actor_support_ticks=ticks;h->actor_support_position=support_at;h->a.grounded=grounded;physicsactor_set_ysp(h->body,vy);h->contact_x_blocked=false;return candidate;
 }
 return resolved;
}
static bool soft_path_clear(HostActor* h,v2d_t from,v2d_t to){
 /* Read-only sweep used by traversal and grip alignment: no support acquisition. */
 double distance=fmax(fabs(to.x-from.x),fabs(to.y-from.y));if(!isfinite(distance)||distance>512)return false;
 int steps=(int)ceil(distance*2);if(steps<1)steps=1;
 for(int step=1;step<=steps;step++){
  double t=(double)step/steps;v2d_t at=v2d_new(from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t);
  for(int i=0;i<64;i++){HostActor* o=slots[i];if(!contact_pair(h,o)||h->actor_support==o->lifetime)continue;
   if(contact_overlap(h,o,at.x,at.y)&&(!contact_overlap(h,o,from.x,from.y)||fabs(at.x-o->a.x)<fabs(from.x-o->a.x)-.01))return false;
  }
 }
 return true;
}
static void actor_support_step(HostActor* h){
 if(!h->actor_support)return;
 HostActor* support=contact_actor(h->actor_support);int limit=h->physics.enabled?(int)h->physics.actor_platform_ticks:15;
 bool valid=support&&contact_live(support)&&!support->actor_support&&++h->actor_support_ticks<=limit&&fabs(h->a.x-support->a.x)<h->a.model->soft_radius+support->a.model->soft_radius-.5;
 if(valid){
  v2d_t delta=v2d_new(support->a.x-h->actor_support_position.x,support->a.y-h->actor_support_position.y),from=v2d_new(h->a.x,h->a.y),to=v2d_add(from,delta);
  valid=fmax(fabs(delta.x),fabs(delta.y))<=32&&body_sweep(h,terrain_map(),from,to);
  if(valid){traversal_position(h,to);h->actor_support_position=v2d_new(support->a.x,support->a.y);return;}
 }
 h->support_block_tick=total_frames;h->ignored_support=h->actor_support;h->actor_support=0;h->a.grounded=false;h->was_ground=false;physicsactor_detach_from_ground(h->body);physicsactor_set_ysp(h->body,fmax(0,physicsactor_get_ysp(h->body)));
}
static bool hold_begin(HostActor* h,HostActor* target,int direction){
 BorActor* a=&h->a;BorActor* b=&target->a;
 if(h==target||h->suspended||target->suspended||!a->grounded||!b->grounded||a->state!=BOR_FREE||b->state!=BOR_FREE||a->health<=0||b->health<=0||!direction||!b->model->grabbable||!(a->model->hold_mask&b->model->type)||!bor_animation(a->model,"grab")||!bor_animation(b->model,"pain"))return false;
 double radius=a->model->soft_radius+b->model->soft_radius;
 if(fabs(b->x-a->x)>fmax(radius,a->model->grab_distance)+1||fabs(b->y-a->y)>7||(b->x-a->x)*direction<=0||!body_clear(h,terrain_map(),a->x,a->y)||!body_clear(target,terrain_map(),b->x,b->y))return false;
 /* An eligible pose on either side of a thin wall is still not a hold. */
 double distance=fabs(b->x-a->x);if(distance>512)return false;
 int steps=(int)ceil(distance);double height=fmin(a->model->soft_height,b->model->soft_height);
 for(int step=0;step<=steps;step++)for(int probe=1;probe<=3;probe++){
  double t=steps?(double)step/steps:0;
  if(solid_at(h,terrain_map(),(int)floor(a->x+(b->x-a->x)*t),(int)floor(a->y+(b->y-a->y)*t-height*probe/4)))return false;
 }
 a->facing=direction;a->partner=b->serial;b->partner=a->serial;a->grabs=0;h->grip_distance=fabs(b->x-a->x);h->grab_release=world_time+.5;
 start(target,"pain",BOR_GRABBED);start(h,"grab",BOR_GRAB);motion_tangent(h,0);motion_tangent(target,0);h->steering=target->steering=0;return true;
}
static bool contextual_grab(HostActor* h,int direction){for(int i=0;i<64;i++)if(slots[i]&&hold_begin(h,slots[i],direction))return true;return false;}
static void pressure_hold(HostActor* h){
 if(h->a.state!=BOR_FREE||!h->a.grounded||!h->input_direction){h->contact_slot=-1;h->contact_ticks=0;return;}
 int found=-1;
 for(int i=0;i<64;i++)if(contact_pair(h,slots[i])){
  HostActor* o=slots[i];double gap=h->a.model->soft_radius+o->a.model->soft_radius;
  if((o->a.x-h->a.x)*h->input_direction>0&&fabs(o->a.x-h->a.x)<=gap+1&&fabs(o->a.y-h->a.y)<7&&o->a.model->grabbable&&(h->a.model->hold_mask&o->a.model->type)&&bor_animation(h->a.model,"grab")){found=i;break;}
 }
 if(found<0){h->contact_slot=-1;h->contact_ticks=0;return;}
 if(h->contact_slot!=found||h->contact_lifetime!=slots[found]->lifetime){h->contact_slot=found;h->contact_lifetime=slots[found]->lifetime;h->contact_ticks=0;}
 if(++h->contact_ticks>=(h->physics.enabled?(int)h->physics.hold_pressure_ticks:3))hold_begin(h,slots[found],h->input_direction);
}
static void grip_follow(HostActor* h){
 HostActor* other=linked_partner(h);if(!other||other->a.state!=BOR_GRABBED)return;
 v2d_t from=v2d_new(other->a.x,other->a.y),to=v2d_new(h->a.x+h->a.facing*h->grip_distance,h->a.y);
 if(!body_sweep(other,terrain_map(),from,to)){unlink_actor(h);if(h->a.state==BOR_GRAB)start(h,"idle",BOR_FREE);return;}
 other->a.x=to.x;other->a.y=to.y;other->a.facing=-h->a.facing;commit_pose(other);rebase(other);
}
