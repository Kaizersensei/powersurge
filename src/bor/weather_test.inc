/* One connected weather/owned-audio lifecycle proof; opt-in, no disk saves. Signed: Atlas. */
void bsl_bor_weather_test_tick(void){
 if(!getenv("BSL_WEATHER_TEST")||bsl_host_boot_busy())return;
 static int stage,checks,failures;static double wait,paused_sim,old_clock;static unsigned paused_position,paused_particles,old_starts;static int old_items;static uint64_t voice,external,source_actor;
 HostActor* p=hero();if(!p||!p->placed)return;checkpoint_park_enemies();wait+=bsl_engine_delta();if(wait<.8)return;wait=0;
 BslWeatherProbe w={0};BslParticleProbe fx={0};BslAmbientProbe audio={0};BslCalendarProbe clock={0};BslGameplayProbe bag={0};
 bsl_host_weather_probe(&w);bsl_host_particle_probe(&fx);bsl_host_ambient_probe(&audio);bsl_host_calendar_probe(&clock);bsl_host_gameplay_probe(&bag);
#define W_CHECK(expr,msg) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("WEATHER %s: %s",ok?"PASS":"FAIL",msg);if(!ok)logfile_message("WEATHER observed stage=%d level=%s kind=%s particles=%u instances=%u voices=%u playing=%u voice=%llu gain=%.5f starts=%u stops=%u",stage,bsl_engine_level_file(),w.kind,fx.particles,fx.instances,audio.voices,audio.playing,(unsigned long long)audio.first_voice,audio.gain,audio.starts,audio.stops);}while(0)
 BslWeatherOverride change={0};change.wind_factor=change.precipitation_factor=1;
 if(stage==0){
  W_CHECK(!strcmp(w.kind,"snow")&&fx.particles>0&&audio.voices==1&&audio.playing==1,"baseline outdoor winter drives particles and one owned wind loop");
  source_actor=p->lifetime;p->a.health=77;old_items=bag.items;old_clock=clock.elapsed;voice=audio.first_voice;old_starts=audio.starts;
  BslParticleSpawn spawn={p->lifetime,7,p->a.x-50,p->a.y-100,100,100,1,true};external=bsl_host_particle_start("effect:weather_ash",&spawn);W_CHECK(external!=0,"independent actor-owned effect starts beside weather emitter");
  change.preset="weather:rain";change.wind_override=true;change.wind=8;W_CHECK(bsl_host_weather_override(&change)==0,"external rain and wind override accepted");++stage;
 }else if(stage==1){
  W_CHECK(!strcmp(w.kind,"rain")&&!strcmp(fx.weather_effect,"effect:weather_rain")&&fx.batches>0,"live native rain effect replaces winter snow");
  W_CHECK(audio.first_voice==voice&&audio.starts==old_starts&&audio.playing==1&&audio.gain>.1,"wind gain changes without restarting loop");
  change.preset="weather:missing";W_CHECK(bsl_host_weather_override(&change)!=0,"invalid weather override refused");BslWeatherProbe after={0};bsl_host_weather_probe(&after);W_CHECK(!strcmp(after.preset,w.preset),"refused override retains current weather");
  sound_swap_mixers();++stage;
 }else if(stage==2){
  W_CHECK(audio.voices==1&&audio.playing==0&&audio.first_voice==voice,"engine SFX mixer swap pauses dedicated wind without host pause update");paused_position=audio.position;++stage;
 }else if(stage==3){
  W_CHECK(audio.position==paused_position&&audio.starts==old_starts,"engine mixer pause retains ambient playhead and voice");sound_swap_mixers();
  change.preset="weather:ash";change.wind_override=true;change.wind=6;W_CHECK(bsl_host_weather_override(&change)==0,"external ash recipe accepted");++stage;
 }else if(stage==4){
  W_CHECK(!strcmp(w.kind,"ash")&&!strcmp(fx.weather_effect,"effect:weather_ash")&&fx.batches>0,"live native ash effect uses registered recipe");
  W_CHECK(audio.first_voice==voice&&audio.starts==old_starts,"switching precipitation preserves independent wind voice");
  W_CHECK(bsl_host_calendar_test_policy(1)==0,"open actual pause policy");++stage;
 }else if(stage==5){
  W_CHECK(audio.paused&&audio.voices==1&&!audio.playing&&audio.first_voice==voice,"pause retains dedicated voice without playback");
  paused_position=audio.position;paused_particles=fx.particles;paused_sim=clock.simulation;++stage;
 }else if(stage==6){
  W_CHECK(audio.position==paused_position&&audio.first_voice==voice&&audio.starts==old_starts,"paused audio playhead and voice identity remain unchanged");
  W_CHECK(fx.particles==paused_particles&&clock.simulation==paused_sim,"paused particle count and simulation clock remain unchanged");
  W_CHECK(bsl_host_calendar_test_policy(2)==0,"replace menu pause with running-world dialogue");++stage;
 }else if(stage==7){
  W_CHECK(!clock.paused&&fabs(clock.factor-.1)<1e-9&&audio.playing==1&&audio.first_voice==voice&&audio.starts==old_starts,"dialogue slows calendar only and resumes same ambient voice");
  W_CHECK(bsl_host_calendar_test_policy(0)==0,"release dialogue input/time leases");change.preset="weather:clear";W_CHECK(bsl_host_weather_override(&change)==0,"clear weather accepted");++stage;
 }else if(stage==8){
  W_CHECK(!strcmp(w.kind,"none")&&fx.weather_handle==0&&fx.instances==1,"clear weather cancels only its emitter");
  W_CHECK(audio.voices==0&&audio.playing==0,"calm wind fades out and releases owned voice");W_CHECK(bsl_host_weather_reset()==0,"reset external weather to level baseline");++stage;
 }else if(stage==9){
  W_CHECK(!strcmp(w.kind,"snow")&&fx.instances==2&&audio.voices==1,"weather reset retains unrelated owner effect and rebuilds baseline");
  W_CHECK(bsl_host_anchor_start(0xb51f04,"event:door_unlock",p->lifetime)==0,"unlock registered level door through event service");bsl_host_simulation_tick();checkpoint_place(p,200,p->a.y);++stage;
 }else if(stage==10){
  W_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"request real cross-level travel");++stage;
 }else if(stage==11){
  W_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&w.indoor&&fx.instances==0&&audio.voices==0,"live indoor transition clears both particle owners and owned audio");
  W_CHECK(p->lifetime!=source_actor&&p->a.health==77&&bag.items==old_items&&clock.elapsed>=old_clock,"scene change retains gameplay resources and clock with fresh actor lifetime");
  W_CHECK(!bsl_host_particle_stop(external,true),"source-level cosmetic handle is stale after unload");checkpoint_place(p,200,p->a.y);W_CHECK(bsl_host_teleport_request(p->lifetime,"door:return",true)==0,"origin-aware return door starts");++stage;
 }else if(stage==12){
  W_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&!w.indoor&&!strcmp(w.kind,"snow")&&fx.instances==1&&audio.voices==1,"return loads new outdoor baseline without source effects");
  W_CHECK(p->a.health==77&&bag.items==old_items&&audio.failures==0,"return preserves resources and reports no audio failures");
  bsl_ambient_release();bsl_ambient_release();bsl_particles_release();bsl_host_ambient_probe(&audio);bsl_host_particle_probe(&fx);W_CHECK(audio.voices==0&&fx.instances==0,"explicit release is idempotent and drops owned resources");
  const char* path=getenv("BSL_WEATHER_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"starts\":%u,\"stops\":%u}\n",failures?"false":"true",checks,failures,audio.starts,audio.stops);fclose(f);}engine_quit();++stage;
 }
#undef W_CHECK
}
