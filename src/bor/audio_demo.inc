/* Isolated acoustic fixture interaction; no ordinary course mutation. Signed: Atlas. */
static uint64_t audio_station_handles[64],audio_airflow,audio_player;
static bool audio_door_open,audio_initialized;
static unsigned audio_debug=254;
static bool audio_enabled(void){return getenv("BSL_AUDIO_DEMO")!=NULL;}
static void audio_demo_clear(void){memset(audio_station_handles,0,sizeof audio_station_handles);audio_airflow=audio_player=0;audio_door_open=false;audio_initialized=false;}
void bsl_bor_audio_reset(void){audio_demo_clear();}
static void audio_demo_tick(const ALLEGRO_KEYBOARD_STATE* keys){
 if(!audio_enabled())return;
 HostActor* p=hero();if(!p)return;
 if(!audio_initialized){for(unsigned i=0;i<bsl_audio_materials_count;++i){const BslAudioMaterial* m=&bsl_audio_materials[i];if(!strcmp(m->scene,level_file()))bsl_acoustic_blocker(0xa0000000+i,m->x,m->y,m->w,m->h,m->material,true);}audio_initialized=true;}
 if(audio_player!=p->lifetime){if(audio_airflow)bsl_spatial_stop(audio_airflow);audio_player=p->lifetime;audio_airflow=0;}
 if(!audio_airflow)audio_airflow=bsl_spatial_play("audio:airflow",p->lifetime,p->a.x,p->a.y-24);
 if(audio_airflow)bsl_spatial_move(audio_airflow,p->a.x,p->a.y-24,hypot(p->a.vx,p->a.vy),!p->a.grounded);
 if(!bsl_host_captures_commands()){
  for(int k=0;k<8;++k)if(pressed(keys,ALLEGRO_KEY_1+k)){audio_debug^=1u<<k;bsl_acoustic_debug(audio_debug);}
  if(pressed(keys,ALLEGRO_KEY_N))bsl_spatial_play("audio:pulse",p->lifetime,p->a.x,p->a.y-24);
  if(pressed(keys,ALLEGRO_KEY_M))bsl_spatial_play("audio:quiet",p->lifetime,p->a.x,p->a.y-24);
  if(pressed(keys,ALLEGRO_KEY_E))for(unsigned i=0;i<bsl_audio_stations_count;++i){const BslAudioStation* s=&bsl_audio_stations[i];if(strcmp(s->scene,level_file())||hypot(p->a.x-s->x,p->a.y-24-s->y)>90)continue;
   if(s->kind==3){audio_door_open=!audio_door_open;bsl_acoustic_geometry_dirty();}
   else if(s->kind==0){if(audio_station_handles[i]){bsl_spatial_stop(audio_station_handles[i]);audio_station_handles[i]=0;}else audio_station_handles[i]=bsl_spatial_play(s->recipe,0xa1000000+i,s->x,s->y);}
   else bsl_spatial_play(s->recipe,0xa1000000+i,s->x,s->y);
   break;
  }
 }
 for(unsigned i=0;i<bsl_audio_listeners_count;++i){const BslAudioListener* l=&bsl_audio_listeners[i];if(!strcmp(l->scene,level_file())){BslAcousticResult r;bsl_acoustic_hear(l->id,l->x,l->y,1,l->threshold,&r);}}
}
void bsl_bor_audio_render(void){
 if(!audio_enabled()||level_editmode())return;
 for(unsigned i=0;i<bsl_audio_stations_count;++i){const BslAudioStation* s=&bsl_audio_stations[i];if(!strcmp(s->scene,level_file()))bsl_ui_loot_marker(s->x,s->y,s->label);}
 for(unsigned i=0;i<bsl_audio_listeners_count;++i){const BslAudioListener* l=&bsl_audio_listeners[i];if(!strcmp(l->scene,level_file())){char label[96];snprintf(label,sizeof label,"Receiver %u threshold %.3f",l->id,l->threshold);bsl_ui_loot_marker(l->x,l->y-96,label);}}
 for(int i=0;i<64;++i)if(slots[i]&&slots[i]->listening){HostActor* h=slots[i];char label[192];snprintf(label,sizeof label,"HEARING %.3f %s | perceived %.0f %.0f%s",h->hearing_threshold,h->investigating?"investigate/search":"listen",h->hearing_at.x,h->hearing_at.y,h->hearing_pending?" (queued)":"");bsl_ui_loot_marker(h->a.x+64,h->a.y-112,label);}
 bsl_ui_loot_marker(64,970,"E: previous room");bsl_ui_loot_marker(3960,970,"E: next room");
}
static surgescript_var_t* audio_door_view(surgescript_object_t* obj,const surgescript_var_t** p,int n){return surgescript_var_set_bool(surgescript_var_create(),audio_door_open);}

static void audio_capture(const char* name){const char* dir=getenv("BSL_AUDIO_CAPTURE");if(!dir)return;char path[2048];snprintf(path,sizeof path,"%s/%s.png",dir,name);image_t* img=video_take_snapshot();if(img){const ALLEGRO_FILE_INTERFACE* files=al_get_new_file_interface();al_set_standard_file_interface();bool ok=al_save_bitmap(path,IMAGE2BITMAP(img));al_set_new_file_interface(files);logfile_message("ACOUSTIC capture %s: %s",name,ok?"saved":"failed");image_destroy(img);}}
/* One connected disposable journey; only enabled by the dedicated check command. */
void bsl_bor_audio_test_tick(void){
 if(!getenv("BSL_AUDIO_TEST")||bsl_host_boot_busy())return;
 static int stage,checks,failures,old_items;static double wait,clock_before,cpu_sum,cpu_max;static unsigned samples,max_work,max_deferred,position,max_chunks,max_saturated;static uint64_t max_cache,max_invalidations,max_node_limit;static BslAcousticProbe peak_work;static int peak_stage;static char peak_level[256];
 static uint64_t direct,loop,event,thin,thick,room,old_revision,stress[8];static bool paused_captured,movement_checked;static int search_wait,dead_end_stage;static double room_gain,ninja_start;
 HostActor* p=hero();if(!p||!p->placed)return;
 BslAcousticProbe work={0};bsl_acoustic_probe(&work);cpu_sum+=work.update_ms;if(work.update_ms>cpu_max){cpu_max=work.update_ms;peak_work=work;peak_stage=stage;snprintf(peak_level,sizeof peak_level,"%s",level_file());}samples++;if(work.expansions>max_work)max_work=work.expansions;if(work.deferred>max_deferred)max_deferred=work.deferred;if(work.chunks>max_chunks)max_chunks=work.chunks;if(work.saturated>max_saturated)max_saturated=work.saturated;if(work.cache_hits>max_cache)max_cache=work.cache_hits;if(work.invalidations>max_invalidations)max_invalidations=work.invalidations;if(work.node_limit>max_node_limit)max_node_limit=work.node_limit;
 wait+=bsl_engine_delta();if(wait<.55)return;wait=0;
 BslVoiceProbe voice={0};BslAcousticResult result={0},other={0};BslGameplayProbe bag={0};BslCalendarProbe clock={0};bsl_host_gameplay_probe(&bag);bsl_host_calendar_probe(&clock);
#define A_CHECK(expr,msg) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("ACOUSTIC %s stage%d: %s",ok?"PASS":"FAIL",stage,msg);}while(0)
 if(stage==0){
  A_CHECK(!strcmp(level_file(),"levels/audio-direction.lev"),"dedicated direction level loaded");
  p->a.health=77;old_items=bag.items;clock_before=clock.elapsed;
  direct=bsl_voice_create("samples/audio-demo/stereo.wav",0xad001,true);A_CHECK(direct!=0,"owned stereo voice created");A_CHECK(bsl_voice_create("samples/audio-demo/missing.wav",0xad099,true)==0,"missing audio asset is refused without terminating");A_CHECK(bsl_voice_set(direct,.6,1,0,20000,true,1.0/60),"start direct stereo voice");
  loop=bsl_spatial_play("audio:stereo",0xad002,560,992);event=bsl_acoustic_emit(0xad003,160,992,1,1,1,1,1000,20,true);A_CHECK(loop&&event,"spatial recipe and explicit noise are independent");++stage;
 }else if(stage==1){
  bsl_voice_probe(direct,&voice);A_CHECK(voice.position>0&&voice.playing,"playhead advanced before parameter edit");position=voice.position;
  for(int i=0;i<8;++i){stress[i]=bsl_voice_create("samples/audio-demo/limiter.wav",0xad080+i,true);bsl_voice_set(stress[i],1,1,0,20000,true,.1);}
  A_CHECK(bsl_voice_set(direct,.25,1.3,-.7,900,true,.1),"live gain rate balance cutoff update accepted");++stage;
 }else if(stage==2){
  float reduction=0,peak=0;bsl_master_probe(&reduction,&peak);A_CHECK(reduction>.01&&peak<=.95001,"master limiter restrains simultaneous stacked voices");for(int i=0;i<8;++i)bsl_voice_destroy(stress[i]);
  bsl_voice_probe(direct,&voice);A_CHECK(voice.position>position&&voice.rate>1.1&&voice.cutoff==900&&voice.balance==-.7,"live parameters retain advancing playhead");
  A_CHECK(bsl_acoustic_sample(event,256,992,&result)&&result.low>0,"source field resolves a listener");
  A_CHECK(bsl_acoustic_hear(0xad011,256,992,1,.01,&result)&&bsl_acoustic_hear(0xad012,256,992,1,.01,&other)&&result.event==other.event,"one noise reaches several independent listeners");
  camera_set_position(v2d_new(1000,700));audio_set_muted(true);audio_set_master_volume(0);
  A_CHECK(bsl_acoustic_hear(0xad013,256,992,1,.01,&other)&&fabs(result.mid-other.mid)<1e-9,"camera move and mute do not alter AI received bands");video_set_resolution(VIDEORESOLUTION_2X);A_CHECK(bsl_acoustic_hear(0xad014,256,992,1,.01,&other)&&fabs(result.mid-other.mid)<1e-9,"window resize does not alter AI detection");video_set_resolution(VIDEORESOLUTION_1X);audio_set_muted(false);audio_set_master_volume(1);audio_capture("direction");
  bsl_spatial_probe(loop,&voice);position=voice.position;A_CHECK(position>0,"nonzero spatial playhead before pause");
  bsl_acoustic_tick(0,true);bsl_spatial_probe(loop,&voice);A_CHECK(!voice.playing&&voice.position==position,"pause immediately preserves nonzero spatial playhead");
  A_CHECK(bsl_host_calendar_test_policy(1)==0,"real pause menu policy acquired");++stage;
 }else if(stage==3){
  if(!paused_captured){bsl_spatial_probe(loop,&voice);position=voice.position;paused_captured=true;return;}
  bsl_spatial_probe(loop,&voice);A_CHECK(!voice.playing&&voice.position==position,"paused mixer does not advance or restart voice");
  A_CHECK(bsl_host_calendar_test_policy(0)==0,"pause menu policy released");++stage;
 }else if(stage==4){
  bsl_spatial_probe(loop,&voice);A_CHECK(voice.playing&&voice.position!=0,"resume preserves same voice and nonzero timeline");sound_swap_mixers();bsl_voice_probe(direct,&voice);position=voice.position;A_CHECK(!voice.playing,"stock parent mixer swap pauses owned voice");++stage;
 }else if(stage==5){
  bsl_voice_probe(direct,&voice);A_CHECK(voice.position==position&&!voice.playing,"stock pause preserves playhead without host heartbeat");sound_swap_mixers();
  bsl_acoustic_cancel_owner(0xad002);A_CHECK(!bsl_spatial_probe(loop,&voice),"owner cancellation removes its voice and event");bsl_voice_destroy(direct);
  A_CHECK(bsl_bor_teleport(p->lifetime,4000,1024,0,0,1),"move to registered first door");A_CHECK(bsl_host_teleport_request(p->lifetime,"door:audio-0-1",true)==0,"registered travel to transmission level accepted");++stage;
 }else if(stage==6){
  A_CHECK(!strcmp(level_file(),"levels/audio-transmission.lev")&&p->a.health==77&&bag.items==old_items&&clock.elapsed>=clock_before,"real scene travel retains HP inventory and calendar");
  A_CHECK(work.voices<=1&&work.sources<=1,"scene unload clears prior voices and noise fields");
  bsl_bor_teleport(p->lifetime,900,1024,0,0,1);
  thin=bsl_acoustic_emit(0xad101,640,992,1,1,1,1,1200,20,true);thick=bsl_acoustic_emit(0xad102,1104,992,1,1,1,1,1200,20,true);room=bsl_acoustic_emit(0xad103,2640,992,1,1,1,1,1400,20,true);++stage;
 }else if(stage==7){
  A_CHECK(bsl_acoustic_sample(thin,800,992,&result)&&result.walls==1&&result.high<result.low,"thin wood transmission is muffled exactly once");
  A_CHECK(bsl_acoustic_sample(thick,1328,992,&other)&&other.high<result.high,"thick stone / indirect path reduces brightness more");
  A_CHECK(bsl_acoustic_sample(room,2304,992,&result),"closed door acoustic path resolves off-screen");room_gain=result.mid;old_revision=result.revision;audio_capture("transmission");bsl_bor_teleport(p->lifetime,2304,1024,0,0,1);audio_door_open=true;bsl_acoustic_geometry_dirty();
  A_CHECK(!bsl_acoustic_sample(room,2304,992,&result),"dirty door path fails closed before recomputation");++stage;
 }else if(stage==8){
  A_CHECK(bsl_acoustic_sample(room,2304,992,&result)&&result.revision!=old_revision&&result.mid>room_gain,"actual door opening invalidates cache and increases transmission");
  audio_capture("open-door");A_CHECK(bsl_bor_teleport(p->lifetime,4000,1024,0,0,1),"move to second registered door");A_CHECK(bsl_host_teleport_request(p->lifetime,"door:audio-1-1",true)==0,"registered travel to hearing level accepted");++stage;
 }else if(stage==9){
  A_CHECK(!strcmp(level_file(),"levels/audio-hearing.lev")&&p->a.health==77&&bag.items==old_items,"hearing scene loaded with unchanged player state");
  HostActor* ninja=NULL;for(int i=0;i<64;++i)if(slots[i]&&slots[i]->listening)ninja=slots[i];A_CHECK(ninja!=NULL,"only opt-in red ninja has hearing profile");
  if(ninja){bsl_bor_teleport(p->lifetime,1536,1024,0,0,-1);ninja_start=ninja->a.x;start(ninja,"attack1",BOR_ACTION);uint64_t action=ninja->a.action_id;ninja->pending_hearing=v2d_new(1088,992);ninja->hearing_pending=true;hearing_actions(ninja,1.0/60);A_CHECK(ninja->a.action_id==action&&ninja->a.state==BOR_ACTION&&ninja->hearing_pending,"investigation intent waits for committed BOR action");}
  event=bsl_acoustic_emit(0xad201,992,992,1,1,1,1,1200,20,true);++stage;
 }else if(stage==10){
  HostActor* ninja=NULL;for(int i=0;i<64;++i)if(slots[i]&&slots[i]->listening)ninja=slots[i];
  if(ninja&&!bor_can_act(&ninja->a)){logfile_message("ACOUSTIC wait committed state %d pending=%d perceived=%.0f",ninja->a.state,ninja->hearing_pending,ninja->pending_hearing.x);return;}
  if(ninja)logfile_message("ACOUSTIC ninja x=%.1f perceived=%.1f %.1f pending=%d state=%d suspended=%d",ninja->a.x,ninja->hearing_at.x,ninja->hearing_at.y,ninja->hearing_pending,ninja->a.state,ninja->suspended);
  A_CHECK(ninja&&ninja->hearing_at.x>1072&&fabs(ninja->hearing_at.x-992)>32,"ninja receives listener-facing arrival rather than hidden source");
  A_CHECK(ninja&&ninja->provoked_until==0&&ninja->lock_target==0,"hearing grants investigation without combat lock-on");if(ninja)ninja_start=ninja->a.x;++stage;
 }else if(stage==11){
  HostActor* ninja=NULL;for(int i=0;i<64;++i)if(slots[i]&&slots[i]->listening)ninja=slots[i];if(!movement_checked){A_CHECK(ninja&&ninja->a.x<ninja_start&&ninja->a.x>1080,"investigation uses ordinary movement and remains on reachable wall side");movement_checked=true;audio_capture("hearing");}
  if(ninja&&ninja->investigating&&++search_wait<30)return;
  A_CHECK(ninja&&!ninja->investigating,"reachable investigation ends its search and returns to listening");
  if(ninja&&dead_end_stage==0){
   reconstruct_actor(ninja,1712,1024,ninja->a.model->health,true);bsl_bor_teleport(ninja->lifetime,1712,1024,0,0,1);
   bsl_acoustic_emit(0xad222,1900,992,1,1,1,1,500,10,true);dead_end_stage=1;search_wait=0;return;
  }
  BslReactionProbe reaction={0};A_CHECK(ninja&&ninja->a.x<1760&&bsl_acoustic_reaction_probe(ninja->lifetime,&reaction)&&reaction.kind==1,"ground-only actor stops at unsafe gap and signals dead end with question bubble");audio_capture("dead-end");
  if(ninja){ninja->hearing_pending=ninja->investigating=true;ninja->provoked_until=world_time+10;reconstruct_actor(ninja,ninja->a.spawn_x,ninja->a.spawn_y,ninja->a.model->health,true);A_CHECK(!ninja->hearing_pending&&!ninja->investigating&&ninja->provoked_until==0,"actor reconstruction clears transient hearing intent");}
  A_CHECK(max_work<=4096,"global expansion budget respected through connected journey");
  A_CHECK(bsl_bor_teleport(p->lifetime,4000,1024,0,0,1),"move to third registered door");A_CHECK(bsl_host_teleport_request(p->lifetime,"door:audio-2-1",true)==0,"connected journey returns to direction level");++stage;
 }else{
  A_CHECK(!strcmp(level_file(),"levels/audio-direction.lev")&&p->a.health==77,"completed three-scene loop retains player state");
  bsl_acoustic_clear();bsl_acoustic_probe(&work);A_CHECK(work.sources==0&&work.voices==0,"teardown clears voices sources and hearing records");
  const char* path=getenv("BSL_AUDIO_REPORT");FILE* out=path?fopen(path,"w"):NULL;if(out){fprintf(out,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"frames_sampled\":%u,\"mean_update_ms\":%.6f,\"max_update_ms\":%.6f,\"max_expansions\":%u,\"max_deferred\":%u,\"max_chunks\":%u,\"max_cache_hits\":%llu,\"max_invalidations\":%llu,\"max_saturated_frames\":%u,\"max_node_limits\":%llu,\"peak_stage\":%d,\"peak_level\":\"%s\",\"peak_terrain_ms\":%.6f,\"peak_search_ms\":%.6f,\"peak_playback_ms\":%.6f,\"peak_hearing_ms\":%.6f,\"peak_expansions\":%u,\"peak_geometry_changed\":%s,\"measurement_context\":\"elapsed game-thread timings under concurrent user/system load; not a controlled benchmark\",\"levels\":3,\"limiter\":\"stereo-linked sample-peak .95, release120ms\"}\n",failures?"false":"true",checks,failures,samples,cpu_sum/samples,cpu_max,max_work,max_deferred,max_chunks,(unsigned long long)max_cache,(unsigned long long)max_invalidations,max_saturated,(unsigned long long)max_node_limit,peak_stage,peak_level,peak_work.terrain_ms,peak_work.search_ms,peak_work.playback_ms,peak_work.hearing_ms,peak_work.expansions,peak_work.geometry_changed?"true":"false");fclose(out);}engine_quit();stage=999;
 }
#undef A_CHECK
}
