/* Runs from the unpaused host heartbeat so actual menu pause can be observed. Signed: Atlas. */
void bsl_bor_calendar_test_tick(void){
 const char* mode=getenv("BSL_CALENDAR_TEST");if(!mode||bsl_host_boot_busy())return;
 static int stage,wait,checks,failures;static double elapsed,simulation,ui,saved_elapsed;
 HostActor* p=hero();if(!p||!p->placed)return;if(++wait<60)return;
 BslCalendarProbe clock={0};BslProgressProbe progress={0};BslGameplayProbe bag={0};bsl_host_calendar_probe(&clock);bsl_host_progress_probe(&progress);bsl_host_gameplay_probe(&bag);
#define CAL_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("CALENDAR %s: %s",ok?"PASS":"FAIL",message);if(!ok)logfile_message("CALENDAR observed stage=%d level=%s clock=%.9f simulation=%.9f factor=%.2f date=%lld-%d-%d %02d:%02d:%02d",stage,bsl_engine_level_file(),clock.elapsed,clock.simulation,clock.factor,(long long)clock.year,clock.month,clock.day,clock.hour,clock.minute,clock.second);}while(0)
 if(stage==0){
  checkpoint_park_enemies();CAL_CHECK(clock.enabled&&(!strcmp(mode,"read")||(clock.year==1326&&clock.month==12&&clock.day==30)),"shared elapsed clock projects authored date for new or resumed play");
  if(!strcmp(mode,"read")){
   double expected=getenv("BSL_CALENDAR_EXPECT")?atof(getenv("BSL_CALENDAR_EXPECT")):0;
   CAL_CHECK(progress.loads==1&&clock.elapsed>=expected&&clock.elapsed<expected+10,"fresh Continue restores saved elapsed clock without offline passage");CAL_CHECK(clock.year==1327&&clock.month==1&&clock.day==1&&!strcmp(clock.month_name,"Janure"),"saved clock reconstructs date/month labels from immutable rules");
   elapsed=clock.elapsed;CAL_CHECK(bsl_host_anchor_start(0xb51f05,"event:calendar_hour",p->lifetime)==0,"continued player can request an authored calendar-only skip");bsl_host_simulation_tick();bsl_host_calendar_probe(&clock);CAL_CHECK(fabs(clock.elapsed-elapsed-120)<1e-5,"one game hour changes only the policy clock by rate-converted duration");stage=99;
  }else{
   elapsed=clock.elapsed;CAL_CHECK(bsl_host_anchor_start(0xb51f05,"event:calendar_days",p->lifetime)==0,"two-day skip queues through captured-player common event");bsl_host_simulation_tick();bsl_host_calendar_probe(&clock);CAL_CHECK(fabs(clock.elapsed-elapsed-5760)<1e-5&&clock.year==1327&&clock.month==1&&clock.day==1,"explicit skip crosses the month/year boundary using authored month lengths");
   elapsed=clock.elapsed;simulation=clock.simulation;ui=clock.ui;CAL_CHECK(bsl_host_calendar_test_policy(1)==0,"open actual Full Status menu/pause policy");stage=1;wait=0;
  }
 }else if(stage==1){
  CAL_CHECK(clock.paused&&clock.elapsed==elapsed&&clock.simulation==simulation&&clock.ui>ui,"menu pauses calendar and simulation while UI animation time continues");CAL_CHECK(bsl_host_calendar_test_policy(2)==0,"switch from menu to normal running-world conversation");bsl_host_calendar_probe(&clock);elapsed=clock.elapsed;simulation=clock.simulation;stage=2;wait=0;
 }else if(stage==2){
  CAL_CHECK(!clock.paused&&fabs(clock.factor-.1)<1e-9&&clock.simulation>simulation&&fabs((clock.elapsed-elapsed)/(clock.simulation-simulation)-.1)<.01,"dialogue slows calendar to10percent while simulation remains normal rate");CAL_CHECK(bsl_host_calendar_test_policy(0)==0,"close message and release its policy owner");bsl_host_calendar_probe(&clock);elapsed=clock.elapsed;simulation=clock.simulation;stage=3;wait=0;
 }else if(stage==3){
  CAL_CHECK(clock.factor==1&&clock.simulation>simulation&&fabs((clock.elapsed-elapsed)/(clock.simulation-simulation)-1)<.01,"last dialogue close restores normal calendar rate without a time backlog");checkpoint_place(p,120,p->a.y);stage=4;wait=0;
 }else if(stage==4){
  CAL_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0,"permanent activation captures shared clock with player progress");bsl_host_calendar_probe(&clock);saved_elapsed=clock.elapsed;
  CAL_CHECK(bsl_host_anchor_start(0xb51f05,"event:calendar_hour",p->lifetime)==0,"advance time after checkpoint capture");bsl_host_simulation_tick();CAL_CHECK(bsl_host_anchor_start(0xb51f04,"event:door_unlock",p->lifetime)==0,"enable actual cross-level journey");bsl_host_simulation_tick();stage=5;wait=0;
 }else if(stage==5){
  checkpoint_place(p,200,p->a.y);elapsed=clock.elapsed;CAL_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"travel to another scene with live shared calendar");stage=6;wait=0;
 }else if(stage==6){
  checkpoint_park_enemies();CAL_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&clock.elapsed>=elapsed&&clock.elapsed<elapsed+10&&clock.year==1327,"level change retains current calendar instead of restarting the date");CAL_CHECK(bsl_host_checkpoint_request()==0,"request return to earlier permanent activation in other level");stage=7;wait=0;
 }else if(stage==7){
  checkpoint_park_enemies();CAL_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&clock.elapsed>=saved_elapsed&&clock.elapsed<saved_elapsed+10,"cross-level checkpoint rewind restores its captured calendar atomically");BslCheckpointProbe restored={0};bsl_host_checkpoint_probe(&restored);CAL_CHECK(restored.save_offer,"automatic entry fallback does not revoke restored permanent save offer");CAL_CHECK(bsl_host_progress_save()==0,"explicit compact save publishes captured clock through verified temporary slot");CAL_CHECK(bsl_host_anchor_start(0xb51f05,"event:calendar_hour",p->lifetime)==0,"make unsaved time change");bsl_host_simulation_tick();CAL_CHECK(bsl_host_progress_load()==0,"request fresh saved-level reconstruction");stage=8;wait=0;
 }else if(stage==8){
  checkpoint_park_enemies();CAL_CHECK(progress.loads==1&&clock.elapsed>=saved_elapsed&&clock.elapsed<saved_elapsed+10&&clock.year==1327&&bag.items==3,"fresh reconstruction removes unsaved skip and retains player-owned state");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_CALENDAR_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"saved_elapsed\":%.17g}\n",failures?"false":"true",checks,failures,saved_elapsed);fclose(f);}engine_quit();}
#undef CAL_CHECK
}
