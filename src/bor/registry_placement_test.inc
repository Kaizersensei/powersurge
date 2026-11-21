/* Exact spawn placement through real terrain, script obstacles and soft-body contact. Signed: Mech. */
static surgescript_object_t* placement_entity(const char* name){
 surgescript_object_t* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_objectmanager_t* manager=surgescript_object_manager(level);
 surgescript_var_t* id=surgescript_var_set_string(surgescript_var_create(),name);const surgescript_var_t* args[]={id};surgescript_var_t* result=surgescript_var_create();
 surgescript_object_call_function(level,"findEntity",args,1,result);surgescript_objecthandle_t h=surgescript_var_get_objecthandle(result);surgescript_var_destroy(id);surgescript_var_destroy(result);
 return h&&surgescript_objectmanager_exists(manager,h)?surgescript_objectmanager_get(manager,h):NULL;
}
static surgescript_object_t* placement_registry(void){
 surgescript_object_t* setup=placement_entity("BSL Runtime Setup");if(!setup)setup=placement_entity("BOR Setup");if(!setup)return NULL;
 surgescript_objecthandle_t child=surgescript_object_child(setup,"BSL Character Registry");return child?surgescript_objectmanager_get(surgescript_object_manager(setup),child):NULL;
}
static void placement_gate_type(surgescript_object_t* gate,const char* type){
 surgescript_object_t* brick=surgescript_objectmanager_get(surgescript_object_manager(gate),surgescript_object_child(gate,"Brick"));
 surgescript_var_t* value=surgescript_var_set_string(surgescript_var_create(),type);const surgescript_var_t* args[]={value};surgescript_object_call_function(brick,"set_type",args,1,NULL);surgescript_var_destroy(value);level_set_obstaclemap_dirty();
}
// Derive occupied rows from collision data, not the stock sprite's visual rectangle.
static bool placement_solid_rows(double x,int top,int bottom,int* first,int* last){
 struct checkpoint_terrain_t* query=level_checkpoint_terrain_create(v2d_new(x,(top+bottom)*.5));if(!query)return false;
 const obstaclemap_t* map=level_checkpoint_terrain_map(query);*first=*last=-1;
 for(int y=top;y<bottom;y++)if(solid_at(hero(),map,(int)x,y)){if(*first<0)*first=y;*last=y;}
 level_checkpoint_terrain_destroy(query);return *first>=0;
}
static void registry_placement_test_tick(void){
 if(!getenv("BSL_REGISTRY_PLACEMENT_TEST"))return;
 static int stage,wait,checks,failures,initial_count;static v2d_t gate_origin;
 if(++wait<60)return;
 HostActor* p=hero();HostActor* primary=journey_character("character:michio");HostActor* alternate=journey_character("character:michio_alternate");
 surgescript_object_t* registry=placement_registry();surgescript_object_t* gate=level_get_entity_by_id("b51d01");BslGameplayProbe bag={0};BslAnchorRun run={0};BslStatProbe xp={0};BslCharacterDefinition d={0};
#define PLACE_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("PLACEMENT %s: %s",ok?"PASS":"FAIL",message);}while(0)
 if(!p||!primary||!registry||!gate){PLACE_CHECK(false,"loaded level provides live character, canonical registry and dynamic obstacle");stage=99;}
 if(stage==0){
  initial_count=factory_live_count();checkpoint_park_enemies();PLACE_CHECK(alternate&&bsl_host_character_definition("character:michio_arcade",&d)==0&&d.actor,"all existing fixture characters still spawn at their authored clear positions");
  surgescript_object_t* setup=placement_entity("BSL Spawn Clearance Setup");surgescript_var_t* value=surgescript_var_create();if(setup)surgescript_object_call_function(setup,"get_rolledBack",NULL,0,value);
  PLACE_CHECK(setup&&surgescript_var_get_bool(value),"failed first creation rolls back without closing startup choice");surgescript_var_destroy(value);
  if(!alternate){stage=99;return;}
  PLACE_CHECK(bsl_host_select_character("character:michio_alternate")==0,"existing safe selection still reaches the alternate");alternate->a.health=37;
  PLACE_CHECK(bsl_host_reward(alternate->lifetime,"source:lab_exploration","placement/one")==0&&bsl_host_reward(alternate->lifetime,"source:lab_exploration","placement/two")==0&&bsl_host_use_item("item:strawberry")==0,"character progress and player inventory are prepared through existing services");
  PLACE_CHECK(bsl_host_select_character("character:michio")==0,"primary retains control while a detached character is recreated");
  surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);gate_origin=scripting_util_world_position(gate);stage=1;wait=0;
 }else if(stage==1){
  int count=factory_live_count();v2d_t camera=camera_get_position();double px=primary->a.x,py=primary->a.y;
  int floor_first,floor_last,block_first,block_last;
  bool geometry=placement_solid_rows(380,9850,10032,&floor_first,&floor_last)&&placement_solid_rows(12064,8000,8128,&block_first,&block_last);
  PLACE_CHECK(geometry,"fixture derives blocking points from actual collision-mask occupancy");if(!geometry){stage=99;return;}
  int width,height;v2d_t center,raw=physicsactor_get_position(primary->body);physicsactor_bounding_box(primary->body,&width,&height,&center);
  double bottom=center.y+height*.5-(raw.y+20),top=center.y-height*.5-(raw.y+20),right=center.x+width*.5-raw.x;
  bool refused=!alternate&&!factory_body(registry,"create","character:michio_alternate",380,floor_first+4-bottom);
  PLACE_CHECK(refused,"embedded solid-floor spawn refuses at the requested coordinates");if(!refused){logfile_message("PLACEMENT observed floor=%d bottom=%.2f top=%.2f",floor_first,bottom,top);stage=99;return;}
  refused=!factory_body(registry,"create","character:michio_alternate",12064,block_first+4-bottom);
  PLACE_CHECK(refused,"off-screen solid terrain vetoes a spawn without rendering the brick");if(!refused){stage=99;return;}
  refused=!factory_body(registry,"create","character:michio_alternate",12004-right,block_first+4-bottom)&&!factory_body(registry,"create","character:michio_alternate",12064,block_last-2-top);
  PLACE_CHECK(refused,"side and head overlap are checked using the full movement body even when the origin is outside terrain");if(!refused){stage=99;return;}
  refused=!factory_body(registry,"create","character:michio_alternate",px,py-3);
  PLACE_CHECK(refused&&primary->a.x==px&&primary->a.y==py,"occupied soft-solid space refuses without pushing the existing actor");if(!refused){stage=99;return;}
  PLACE_CHECK(factory_live_count()==count&&bsl_host_character_definition("character:michio_alternate",&d)==0&&!d.actor&&controlled(primary),"repeated failed initialization leaves no body binding or live-controller leak");
  v2d_t after=camera_get_position();PLACE_CHECK(camera.x==after.x&&camera.y==after.y,"off-screen placement queries do not move the playback camera");
  PLACE_CHECK(factory_body(registry,"create","character:michio_alternate",11980,8064)!=0,"clear airborne placement remains valid without a forced floor snap");
  alternate=journey_character("character:michio_alternate");PLACE_CHECK(alternate&&alternate->a.x==11980&&alternate->a.y==8064&&alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"successful placement uses exact coordinates and retains detached health and learning");
  if(!alternate){stage=99;return;}surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);
  scripting_util_set_world_position(gate,v2d_new(340,9820));level_set_obstaclemap_dirty();stage=2;wait=0;
 }else if(stage==2){
  PLACE_CHECK(!alternate&&bsl_host_anchor_start(0xca0080,"event:registry_spawn",primary->lifetime)==0,"authored spawn can run against a changed live solid obstacle");stage=3;wait=0;
 }else if(stage==3){
  bsl_host_anchor_probe(0xca0080,&run);PLACE_CHECK(run.status==5&&run.next==1&&run.error==3&&!alternate,"blocked event spawn reports denied at its command with no partial character binding");
  bsl_host_gameplay_probe(&bag);PLACE_CHECK(bag.items==2&&primary->a.health==83&&bsl_host_progress_variable("variable:training_choices")==0&&controlled(primary),"denied placement preserves inventory, existing actors and following-event state");
  placement_gate_type(gate,"cloud");stage=4;wait=0;
 }else if(stage==4){
  PLACE_CHECK(factory_body(registry,"create","character:michio_alternate",380,9850)!=0,"one-way cloud terrain keeps its existing transparent placement semantics");
  alternate=journey_character("character:michio_alternate");if(!alternate){stage=99;return;}surgescript_object_call_function(alternate->entity,"destroy",NULL,0,NULL);
  scripting_util_set_world_position(gate,gate_origin);placement_gate_type(gate,"solid");stage=5;wait=0;
 }else if(stage==5){
  PLACE_CHECK(!alternate&&bsl_host_anchor_start(0xca0081,"event:registry_spawn",primary->lifetime)==0,"same authored event can retry explicitly after the obstruction clears");stage=6;wait=0;
 }else if(stage==6){
  bsl_host_anchor_probe(0xca0081,&run);PLACE_CHECK(run.status==3&&run.error==0&&alternate&&fabs(alternate->a.x-380)<.01,"clear retry completes at the authored horizontal position");
  PLACE_CHECK(alternate&&alternate->a.health==42&&bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump")&&factory_live_count()==initial_count,"failed attempts and successful retries preserve character state without accumulating controllers");
  PLACE_CHECK(bsl_host_select_character("character:michio_alternate")==0,"accepted replacement remains eligible for ordinary safe switching");bsl_host_stat_probe("stat:experience",&xp);bsl_host_gameplay_probe(&bag);
  PLACE_CHECK(xp.total==5000&&bag.items==2&&bsl_host_progress_variable("variable:training_choices")==7,"earned XP, player bag and committed following commands agree after retry");
  PLACE_CHECK(bsl_host_select_character("character:michio")==0&&primary->a.health==83,"return to the unchanged primary character succeeds");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_REGISTRY_PLACEMENT_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef PLACE_CHECK
}
