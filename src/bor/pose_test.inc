/* Bounded native presentation check, opt-in only; no authored level edits. Signed: Atlas. */
static void pose_test_tick(void){
 if(!getenv("BSL_POSE_TEST"))return;
 static int waited;if(++waited<60)return;
 HostActor* h=hero();if(!h)return;
 int checked=0,failed=0;
#define POSE_CHECK(expr,msg) do{bool ok=(expr);checked++;if(!ok)failed++;logfile_message("POSE %s: %s",ok?"PASS":"FAIL",msg);}while(0)
 const double pi=3.141592653589793;
 double first=bor_surface_angle_step(0,pi/4,1.0/60),settled=0;
 POSE_CHECK(first>0&&first<pi/4,"surface response interpolates rather than snapping");
 for(int i=0;i<60;i++)settled=bor_surface_angle_step(settled,pi/4,1.0/60);
 POSE_CHECK(fabs(settled-pi/4)<.00001,"interpolation settles to the target angle");
 double wrapped=bor_surface_angle_step(359*pi/180,pi/180,1.0/60);
 POSE_CHECK(fabs(remainder(wrapped-359*pi/180,2*pi))<pi/180,"359 to1 degrees follows the short arc");
 double together=bor_surface_angle_step(0,pi/4,1.0/30);
 POSE_CHECK(fabs(together-bor_surface_angle_step(first,pi/4,1.0/60))<1e-12,"response uses elapsed time consistently");
 double old_surface=h->surface_angle;BorState old_state=h->a.state;
 bool old_frozen=h->frozen_tick;double old_freeze=h->a.freeze;
 h->a.state=BOR_FREE;h->surface_angle=.4;render_frame(h);double pose=h->a.pose_angle;double corners[8]={0};bool has_corners=bor_corners(&h->a,false,corners);
 POSE_CHECK(fabs(h->visual->angle-pose)<1e-6&&fabs(pose-.4)<1e-12&&has_corners&&fabs(corners[3]-corners[1])>1,"sprite and combat corners use the same smoothed pose");
 render_frame(h);render_frame(h);
 POSE_CHECK(h->surface_angle==.4&&h->a.pose_angle==pose,"repeated render/frame preparation does not advance interpolation");
 v2d_t before=physicsactor_get_position(h->body);double vx=physicsactor_get_xsp(h->body),vy=physicsactor_get_ysp(h->body),gsp=physicsactor_get_gsp(h->body);int angle=physicsactor_get_angle(h->body);
 h->frozen_tick=false;h->a.freeze=0;update_surface_rotation(h);
 v2d_t after=physicsactor_get_position(h->body);
 POSE_CHECK(before.x==after.x&&before.y==after.y&&vx==physicsactor_get_xsp(h->body)&&vy==physicsactor_get_ysp(h->body)&&gsp==physicsactor_get_gsp(h->body)&&angle==physicsactor_get_angle(h->body),"smoothing leaves physics position, speed and terrain angle unchanged");
 h->frozen_tick=true;double frozen=h->surface_angle;update_surface_rotation(h);
 POSE_CHECK(h->surface_angle==frozen,"hitstop freezes presentation interpolation");
 h->frozen_tick=false;h->a.freeze=0;h->a.state=BOR_ROLL;h->roll_angle=.7;render_frame(h);
 POSE_CHECK(fabs(h->a.pose_angle-h->surface_angle-.7)<1e-12,"roll spin remains separate from surface interpolation");
 h->a.state=old_state;h->roll_angle=0;h->surface_angle=.8;rebase(h);
 POSE_CHECK(h->surface_angle==0,"rebase discards stale surface rotation");
 h->surface_angle=old_surface;h->frozen_tick=old_frozen;h->a.freeze=old_freeze;render_frame(h);
 const char* path=getenv("BSL_POSE_REPORT");FILE* f=path?fopen(path,"w"):NULL;
 if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"half_life_seconds\":%.6f}\n",failed?"false":"true",checked,failed,(double)BOR_SURFACE_ROTATION_HALF_LIFE);fclose(f);}
 engine_quit();
#undef POSE_CHECK
}
