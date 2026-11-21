/* Regression for Surge low-speed control lock with high C2 braking friction.
 * Uses actual native collision masks and the same BOR movement bridge. Signed: Atlas. */
static void slope_fixture(HostActor* h,const obstaclemap_t* map,double x,double y,double angle){
 physicsactor_t* old=h->body;h->body=physicsactor_create(v2d_new(x,y-20));physicsactor_destroy(old);
 reconstruct_actor(h,x,y,h->a.model->health,true);physicsactor_detach_from_ground(h->body);h->a.grounded=false;
 input_reset(h->drive_input);physicsactor_capture_input(h->body,h->drive_input);
 for(int i=0;i<100;i++){physicsactor_update(h->body,map);if(!physicsactor_is_midair(h->body))break;}
 if(fabs(angle)>45){
  physicsactor_set_gsp(h->body,angle>0?-500:500);
  for(int i=0;i<90;i++){physicsactor_update(h->body,map);if(fabs(fabs(remainder(physicsactor_get_angle(h->body),360))-fabs(angle))<3)break;}
 }
 physicsactor_set_gsp(h->body,0);physicsactor_set_xsp(h->body,0);physicsactor_set_ysp(h->body,0);
 for(int i=0;i<30;i++)physicsactor_update(h->body,map);
 v2d_t p=physicsactor_get_position(h->body);h->a.x=p.x;h->a.y=p.y+20;h->a.grounded=!physicsactor_is_midair(h->body);h->was_ground=h->a.grounded;motion_query(h);commit_pose(h);
}
static void slope_test_tick(void){
 if(!getenv("BSL_SLOPE_TEST"))return;
 static int waited;if(++waited<60)return;
 int checks=0,failures=0;HostActor* h=hero();if(!h)return;
 for(int i=0;i<64;i++)if(slots[i]!=h&&slots[i])slots[i]->suspended=true;
#define SLOPE_CHECK(expr,msg) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("SLOPE %s: %s",ok?"PASS":"FAIL",msg);if(!ok)logfile_message("SLOPE observed case=%d angle=%d ground=%d x=%.3f y=%.3f gsp=%.3f lock=%.3f frc=%.3f",which,physicsactor_get_angle(h->body),h->a.grounded,h->a.x,h->a.y,physicsactor_get_gsp(h->body),physicsactor_hlock_timer(h->body),physicsactor_get_frc(h->body));}while(0)
 const double angles[]={45,-45,60,-50,45,-45,0,30,-30,40};
 for(int which=0;which<10;which++){
  double angle=angles[which],gradient=tan(fabs(angle)*3.141592653589793/180);bool filled=which==4||which==5||which==6;
  image_t* before=image_drawing_target();image_t* im=image_create(480,1024);image_set_drawing_target(im);image_clear(color_rgba(0,0,0,0));
  for(int x=0;x<480;x++){double u=angle>=0?x:479-x;int top=(int)lround(900-gradient*fmin(u,300)-(fabs(angle)>45?tan(3.141592653589793/6):gradient)*fmax(0,u-300));image_line(x,top,x,filled?1023:top+7,color_rgb(255,255,255));}image_set_drawing_target(before);
  image_lock(im,"r");collisionmask_t* mask=collisionmask_create(im,0,0,480,1024,0);image_unlock(im);obstacle_t* ramp=obstacle_create(mask,(point2d_t){100,8000},OL_DEFAULT,0);obstaclemap_t* map=obstaclemap_create();obstaclemap_add(map,ramp);obstaclemap_build(map);test_terrain=map;
  double u=fabs(angle)>45?420:240;double top=900-gradient*fmin(u,300)-(fabs(angle)>45?tan(3.141592653589793/6):gradient)*fmax(0,u-300);double x=100+(angle>=0?u:479-u),y=8000+top-80;
  slope_fixture(h,map,x,y,angle);bool steep=which<6;double begin=h->a.x;
  SLOPE_CHECK(h->a.grounded&&fabs(fabs(remainder(physicsactor_get_angle(h->body),360))-fabs(angle))<4,"actual terrain solver lands on the fixture at its authored incline");
  if(steep){
   for(int i=0;i<120;i++){input_reset(h->drive_input);if(fabs(angle)>45)input_simulate_button_down(h->drive_input,angle>0?IB_LEFT:IB_RIGHT);physicsactor_capture_input(h->body,h->drive_input);physicsactor_update(h->body,map);}
   v2d_t at=physicsactor_get_position(h->body);h->a.x=at.x;h->a.y=at.y+20;h->a.grounded=!physicsactor_is_midair(h->body);SLOPE_CHECK(fabs(at.x-begin)<1&&(fabs(angle)<=45||physicsactor_hlock_timer(h->body)>0),"high friction holds steep actor still; above45 the solver renews its lock despite downhill input");
   slope_fixture(h,map,x,y,angle);begin=h->a.x;
   for(int i=0;i<90;i++){h->steering=0;h->input_direction=0;move_actor(h);}
   SLOPE_CHECK(fabs(h->a.x-begin)>8,"steep actor slides under native gravity without jump or dash");
   slope_fixture(h,map,x,y,angle);begin=h->a.x;
   for(int i=0;i<90;i++){drive(h,angle>0?-1:1,h->physics.speed);move_actor(h);}
   SLOPE_CHECK(fabs(h->a.x-begin)>15&&h->a.state==BOR_FREE,"normal downhill steering escapes the lock without entering a special move");
  }else{
   for(int i=0;i<90;i++){h->steering=0;h->input_direction=0;move_actor(h);}
   SLOPE_CHECK(h->a.grounded&&fabs(h->a.x-begin)<.1&&physicsactor_get_frc(h->body)==h->physics.deceleration,"flat and shallow terrain retain authored stopping friction");
   begin=h->a.x;for(int i=0;i<40;i++){drive(h,angle>=0?-1:1,h->physics.speed);move_actor(h);}
   SLOPE_CHECK(fabs(h->a.x-begin)>5,"ordinary walking still starts on flat and shallow surfaces");
  }
  test_terrain=NULL;obstaclemap_destroy(map);obstacle_destroy(ramp);collisionmask_destroy(mask);image_destroy(im);
 }
 const char* path=getenv("BSL_SLOPE_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();
#undef SLOPE_CHECK
}
