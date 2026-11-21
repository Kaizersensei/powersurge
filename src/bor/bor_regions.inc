/* Continuous non-solid climb networks. Bars use editable polyline paths. Signed: Atlas. */
typedef struct {surgescript_object_t* entity;uint64_t id;double width,height,multiplier;BorState state;bool enabled;v2d_t points[32];int count;char path[1024];} ClimbRegion;
static ClimbRegion* regions[64];static uint64_t next_region_id;
static bool climb_state(BorState state){return state==BOR_POLE||state==BOR_LADDER||state==BOR_BARCLIMB;}
static ClimbRegion* region_find(uint64_t id){for(int i=0;i<64;i++)if(regions[i]&&regions[i]->id==id&&regions[i]->enabled)return regions[i];return NULL;}
static bool region_bar(const ClimbRegion* r){return r->state==BOR_BARCLIMB;}
static int region_points(const ClimbRegion* r){return region_bar(r)?r->count:2;}
static v2d_t region_point(const ClimbRegion* r,int i){return region_bar(r)?r->points[i]:v2d_new(0,i?r->height:0);}
static double region_length(const ClimbRegion* r){double length=0;for(int i=1;i<region_points(r);i++){v2d_t d=v2d_subtract(region_point(r,i),region_point(r,i-1));length+=hypot(d.x,d.y);}return length;}
static v2d_t region_sample(const ClimbRegion* r,double distance,v2d_t* tangent){
 int n=region_points(r);v2d_t at=scripting_util_world_position(r->entity);
 for(int i=1;i<n;i++){v2d_t a=region_point(r,i-1),b=region_point(r,i),d=v2d_subtract(b,a);double length=hypot(d.x,d.y);if(distance<=length||i==n-1){if(tangent)*tangent=v2d_multiply(d,1/length);return v2d_add(at,v2d_add(a,v2d_multiply(d,fmax(0,fmin(1,distance/length)))));}distance-=length;}
 return at;
}
static double region_project(const ClimbRegion* r,v2d_t point,double* separation){
 double best=1e30,result=0,offset=0;v2d_t at=scripting_util_world_position(r->entity);
 for(int i=1;i<region_points(r);i++){v2d_t a=v2d_add(at,region_point(r,i-1)),d=v2d_subtract(region_point(r,i),region_point(r,i-1));double length=hypot(d.x,d.y),t=fmax(0,fmin(1,((point.x-a.x)*d.x+(point.y-a.y)*d.y)/(length*length)));v2d_t q=v2d_add(a,v2d_multiply(d,t));double gap=hypot(point.x-q.x,point.y-q.y);if(gap<best){best=gap;result=offset+t*length;}offset+=length;}
 if(separation)*separation=best;
 return result;
}
static bool region_path(ClimbRegion* region,const char* text){
 if(!text||strlen(text)>=sizeof(region->path))return false;
 if(!strcmp(region->path,text))return true;
 v2d_t points[32];int count=0;const char* at=text;double total=0;
 while(*at){if(count==32)return false;char* end;double x=strtod(at,&end);if(end==at||*end!=','||!isfinite(x)||fabs(x)>8192)return false;at=end+1;double y=strtod(at,&end);if(end==at||!isfinite(y)||fabs(y)>8192)return false;points[count++]=v2d_new(x,y);if(count>1){double length=hypot(x-points[count-2].x,y-points[count-2].y);if(length<.1)return false;total+=length;}if(!*end)break;if(*end!=';')return false;at=end+1;if(!*at)return false;
 }
 if(count<2||total>16384)return false;
 memcpy(region->points,points,count*sizeof(*points));region->count=count;strcpy(region->path,text);return true;
}
static v2d_t region_foot(const ClimbRegion* r,v2d_t anchor){return v2d_add(anchor,v2d_new(0,region_bar(r)?72:0));}
static void region_release(HostActor* h,double vx,double vy){
 h->region_id=0;h->climb_direction=0;h->climb_ready_tick=total_frames+15;h->climb_needs_release=true;
 if(!body_clear(h,terrain_map(),h->a.x,h->a.y))carry_fallback(h,terrain_map());
 if(h->a.timeline_fault)return;
 h->a.grounded=false;physicsactor_detach_from_ground(h->body);motion_apply(h,BOR_MOTION_SET,3,vx,vy);start(h,"walkoff",BOR_FREE);
}
static bool region_acquire(HostActor* h,int direction){
 if(!direction||h->climb_needs_release||total_frames<h->climb_ready_tick)return false;
 if(h->a.state!=BOR_FREE&&!(h->a.state==BOR_ACTION&&!(h->a.animation->flags&BOR_ANIM_ATTACK)&&(h->a.animation->flags&BOR_ANIM_AIR_CONTROL)))return false;
 ClimbRegion* selected=NULL;double chosen=0,best=1e30;
 for(int i=0;i<64;i++)if(regions[i]&&regions[i]->enabled){ClimbRegion* region=regions[i];double gap;v2d_t anchor=v2d_new(h->a.x,h->a.y-(region_bar(region)?72:0));double distance=region_project(region,anchor,&gap);if(gap>region->width*.5+8||gap>=best)continue;
  // Preserve the bounded vertical foot range; capsule reach must not extend endpoints.
  if(!region_bar(region)){v2d_t at=scripting_util_world_position(region->entity);if(h->a.y<at.y||h->a.y>at.y+region->height)continue;}
  const char* pose=region_bar(region)?"barclimb":"c2_pole";if(!bor_animation(h->a.model,pose))continue;
  v2d_t foot=region_foot(region,region_sample(region,distance,NULL));if(!body_sweep(h,terrain_map(),v2d_new(h->a.x,h->a.y),foot))continue;selected=region;chosen=distance;best=gap;
 }
 if(!selected)return false;
 h->region_id=selected->id;h->region_distance=chosen;h->region_position=scripting_util_world_position(selected->entity);h->climb_direction=direction;h->jump_cut_active=false;
 if(!start(h,region_bar(selected)?"barclimb":"c2_pole",selected->state)){h->region_id=0;return false;}
 h->a.grounded=false;h->was_ground=false;physicsactor_detach_from_ground(h->body);motion_apply(h,BOR_MOTION_STOP,3,0,0);traversal_position(h,region_foot(selected,region_sample(selected,chosen,NULL)));return true;
}
static void region_player(HostActor* h,int direction,bool jump){if(jump){region_release(h,direction*h->physics.speed,0);if(!h->a.timeline_fault&&player_jump(h,direction,false,false))h->jumps_used=1;}}
static double region_input(HostActor* h,const ClimbRegion* r,v2d_t tangent){return region_bar(r)?h->input_direction*tangent.x+h->climb_direction*tangent.y:h->climb_direction;}
/* Choose either tangent at a vertex and stop each sweep at that vertex. */
static double region_axis(HostActor* h,const ClimbRegion* r,double distance,double* available){
 if(!region_bar(r)){*available=h->climb_direction>0?region_length(r)-distance:distance;return h->climb_direction;}
 double offset=0,best=-1,result=0;*available=0;
 for(int i=1;i<r->count;i++){v2d_t d=v2d_subtract(r->points[i],r->points[i-1]);double length=hypot(d.x,d.y),end=offset+length;
  if(distance>=offset-.00001&&distance<=end+.00001){double input=region_input(h,r,v2d_multiply(d,1/length)),room=input>0?end-distance:distance-offset;double score=fabs(input)+(room>.00001?2:0);if(fabs(input)>.05&&score>best){best=score;result=input;*available=fmax(0,room);}}
  offset=end;
 }return result;
}
static ClimbRegion* region_neighbor(HostActor* h,ClimbRegion* current,v2d_t anchor,double* distance){
 ClimbRegion* selected=NULL;double best=1e30;
 for(int i=0;i<64;i++)if(regions[i]&&regions[i]->enabled&&regions[i]!=current&&region_bar(regions[i])==region_bar(current)){
  ClimbRegion* r=regions[i];double gap,s=region_project(r,anchor,&gap);if(gap>fmin(current->width,r->width)*.5||gap>=best)continue;double room;double input=region_axis(h,r,s,&room),length=region_length(r);
  if(fabs(input)<.05||(input>0&&s>=length-.001)||(input<0&&s<=.001))continue;
  // The next piece must lead away from the exhausted endpoint, not back into it.
  double test=fmax(0,fmin(length,s+copysign(1,input)));v2d_t next=region_sample(r,test,NULL),out=v2d_subtract(next,anchor);if((region_bar(r)?out.x*h->input_direction+out.y*h->climb_direction:out.y*h->climb_direction)<=.01)continue;
  v2d_t foot=region_foot(r,region_sample(r,s,NULL));if(!body_sweep(h,terrain_map(),v2d_new(h->a.x,h->a.y),foot))continue;selected=r;*distance=s;best=gap;
 }return selected;
}
static void region_step(HostActor* h,const obstaclemap_t* map){
 ClimbRegion* r=region_find(h->region_id);if(!r||region_bar(r)!=(h->a.state==BOR_BARCLIMB)){region_release(h,0,100);return;}
 v2d_t from=v2d_new(h->a.x,h->a.y),at=scripting_util_world_position(r->entity);double length=region_length(r);if(!isfinite(at.x)||!isfinite(at.y)||length<=0||h->region_distance>length+.01){region_release(h,0,100);return;}
 v2d_t carried=region_foot(r,region_sample(r,h->region_distance,NULL));if(!body_sweep(h,map,from,carried)){region_release(h,0,100);return;}h->region_position=at;traversal_position(h,carried);
 double speed=(region_bar(r)?h->physics.bar_speed:r->state==BOR_LADDER?h->physics.ladder_speed:h->physics.pole_speed)*r->multiplier,budget=speed/60;
 for(int transfer=0;transfer<64;transfer++){
  double available;double input=region_axis(h,r,h->region_distance,&available);if(fabs(input)<.05)return;int sign=input>0?1:-1;
  double travel=fmin(budget,fmax(0,available));
  if(travel>0){double next=h->region_distance+sign*travel;v2d_t to=region_foot(r,region_sample(r,next,NULL));if(!body_sweep(h,map,v2d_new(h->a.x,h->a.y),to))return;h->region_distance=next;traversal_position(h,to);budget-=travel;}
  if(available>travel+.00001||budget<.00001)return;
  if(region_bar(r)&&h->region_distance>.00001&&h->region_distance<region_length(r)-.00001)continue;
  v2d_t anchor=region_sample(r,h->region_distance,NULL);double next_distance=0;ClimbRegion* next=region_neighbor(h,r,anchor,&next_distance);
  if(next){r=next;h->region_id=r->id;h->region_distance=next_distance;h->region_position=scripting_util_world_position(r->entity);traversal_position(h,region_foot(r,region_sample(r,next_distance,NULL)));continue;}
  if(!region_bar(r)){v2d_t exit=v2d_new(h->a.x,h->a.y+sign);if(body_sweep(h,map,v2d_new(h->a.x,h->a.y),exit)){traversal_position(h,exit);region_release(h,0,sign>0?100:0);}}return;
 }
}
static surgescript_var_t* region_init(surgescript_object_t* object,const surgescript_var_t** args,int count){
 if(surgescript_object_userdata(object)||!surgescript_var_is_string(args[0]))return NULL;
 const char* kind=surgescript_var_fast_get_string(args[0]);if(strcmp(kind,"pole")&&strcmp(kind,"ladder")&&strcmp(kind,"bar"))return NULL;
 int index=0;while(index<64&&regions[index])index++;if(index==64){scripting_warning(object,"Climb region limit64 reached");return NULL;}ClimbRegion* r=calloc(1,sizeof(*r));if(!r)return NULL;r->id=++next_region_id;r->entity=surgescript_objectmanager_get(surgescript_object_manager(object),surgescript_object_parent(object));r->state=!strcmp(kind,"bar")?BOR_BARCLIMB:!strcmp(kind,"pole")?BOR_POLE:BOR_LADDER;r->width=24;r->height=192;r->multiplier=1;r->enabled=true;region_path(r,"0,0;192,0");regions[index]=r;surgescript_object_set_userdata(object,r);return NULL;
}
static surgescript_var_t* region_configure(surgescript_object_t* object,const surgescript_var_t** args,int count){ClimbRegion* r=surgescript_object_userdata(object);double width=surgescript_var_get_number(args[0]),height=surgescript_var_get_number(args[1]),multiplier=surgescript_var_get_number(args[2]);if(r&&isfinite(width)&&isfinite(height)&&isfinite(multiplier)&&width>=4&&width<=256&&height>=16&&height<=4096&&multiplier>=.1&&multiplier<=4){r->width=width;r->height=height;r->multiplier=multiplier;r->enabled=surgescript_var_get_bool(args[3]);}return NULL;}
static surgescript_var_t* region_setpath(surgescript_object_t* object,const surgescript_var_t** args,int count){ClimbRegion* r=surgescript_object_userdata(object);if(r&&surgescript_var_is_string(args[0])&&!region_path(r,surgescript_var_fast_get_string(args[0])))scripting_warning(object,"Invalid bar path: expected2..32 distinct x,y points separated by semicolons");return NULL;}
static surgescript_var_t* region_destroy(surgescript_object_t* object,const surgescript_var_t** args,int count){ClimbRegion* r=surgescript_object_userdata(object);if(r){for(int i=0;i<64;i++)if(regions[i]==r)regions[i]=NULL;free(r);surgescript_object_set_userdata(object,NULL);}return NULL;}
static surgescript_var_t* region_render(surgescript_object_t* object,const surgescript_var_t** args,int count){
 ClimbRegion* r=surgescript_object_userdata(object);if(!r||!r->enabled)return NULL;v2d_t at=scripting_util_world_position(r->entity),center=v2d_multiply(video_get_screen_size(),.5);double dx=at.x-surgescript_var_get_number(args[0])+center.x,dy=at.y-surgescript_var_get_number(args[1])+center.y; color_t color=region_bar(r)?color_rgb(220,70,90):r->state==BOR_LADDER?color_rgb(220,170,85):color_rgb(120,200,145);
 for(int i=1;i<region_points(r);i++){v2d_t a=region_point(r,i-1),b=region_point(r,i);image_line(a.x+dx,a.y+dy,b.x+dx,b.y+dy,color);if(debug_mask&16){image_ellipse(a.x+dx,a.y+dy,r->width*.5,r->width*.5,color_rgb(245,100,255));}}
 if(r->state==BOR_LADDER)for(int rung=0;rung<=(int)r->height;rung+=16)image_line(dx-r->width*.5,dy+rung,dx+r->width*.5,dy+rung,color);
 return NULL;
}
static surgescript_var_t* region_zindex(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_number(surgescript_var_create(),.6);}
static surgescript_var_t* region_main(surgescript_object_t* object,const surgescript_var_t** args,int count){return NULL;}
static void register_climb_regions(surgescript_vm_t* vm){surgescript_tagsystem_add_tag(surgescript_vm_tagsystem(vm),"BORClimbRegion","renderable");surgescript_vm_bind(vm,"BORClimbRegion","state:main",region_main,0);surgescript_vm_bind(vm,"BORClimbRegion","init",region_init,1);surgescript_vm_bind(vm,"BORClimbRegion","configure",region_configure,4);surgescript_vm_bind(vm,"BORClimbRegion","setPath",region_setpath,1);surgescript_vm_bind(vm,"BORClimbRegion","destructor",region_destroy,0);surgescript_vm_bind(vm,"BORClimbRegion","onRender",region_render,2);surgescript_vm_bind(vm,"BORClimbRegion","onRenderGizmos",region_render,2);surgescript_vm_bind(vm,"BORClimbRegion","get_zindex",region_zindex,0);}
