
/* Native level adapter: reuse placement actions, fields, selection and protected writers. Signed: President. */
static int shell_tool=0;
static bool shell_level_dirty=false;
static uint64_t shell_brick=0;
static void shell_mark_dirty(void){shell_level_dirty=true;}
static char shell_level_message[512]="";
extern const char* bsl_shell_material_name(int);
extern void* bsl_shell_material_image(int);
static brick_t* shell_selected_brick(void){
 iterator_t* it=brickmanager_retrieve_all_bricks(brick_manager);brick_t* result=NULL;
 while(iterator_has_next(it)){brick_t* b=iterator_next(it);if(brick_placement_id(b)==shell_brick){result=b;break;}}iterator_destroy(it);return result;
}
void bsl_shell_level_begin(void){shell_level_message[0]=0;shell_level_dirty=false;shell_brick=0;shell_tool=0;bsl_shell_file_capture(file);}
void bsl_shell_level_end(void){bsl_shell_detach();shell_brick=0;shell_tool=0;}
void bsl_shell_enter(void){shell_tool=0;editor_selected_entity_type=EDT_INSPECT;bsl_shell_terrain_tool(false);editor_brush_reset();}
void bsl_shell_snapshot(BslShellState* s){
 memset(s,0,sizeof *s);s->editing=editor_is_enabled();s->presentation=level_is_setup_object("BSL Presentation Only");s->ready_kind=-1;s->ready_index=-1;s->tool=shell_tool;s->grid=editor_grid_size;
 s->placements_dirty=shell_level_dirty||bsl_inspect_unsaved;s->readonly=readonly;s->zoom=editor_zoom;
 snprintf(s->level,sizeof s->level,"%s",file?file:"");
 s->x=(editor_cursor.world_position.x-4096)/32;s->y=(editor_cursor.world_position.y-4096)/32;
 surgescript_object_t* o=bsl_inspect_object();
 if(o){s->selection=bsl_inspect_id;snprintf(s->name,sizeof s->name,"%s",surgescript_object_name(o));v2d_t p=entity_info_spawnpoint(o);s->selected_x=p.x;s->selected_y=p.y;if(bsl_event_is_anchor(o))str_cpy(s->definition,bsl_event_definition(entity_info_id(o)),sizeof s->definition);}
 else if(shell_selected_brick()){brick_t* b=shell_selected_brick();s->selection=shell_brick;snprintf(s->name,sizeof s->name,"Decoration / brick %d",brick_id(b));v2d_t p=brick_position(b);s->selected_x=p.x;s->selected_y=p.y;s->brightness=brick_brightness(b);}
 if((shell_tool==2||shell_tool==3)&&(editor_selected_entity_type==EDT_SSOBJ||editor_selected_entity_type==EDT_EVENTS)){s->ready_kind=0;s->ready_index=editor_selected_entity_id;}
 else if(shell_tool==4&&editor_selected_entity_type==EDT_BRICK){s->ready_kind=1;for(int i=0;i<editor_brick_count;i++)if(editor_brick_id(i)==editor_selected_entity_id){s->ready_index=i;break;}}
 bsl_shell_terrain_snapshot(s);
 if(shell_tool==1&&s->terrain){s->ready_kind=2;s->ready_index=s->material;}
 if(*shell_level_message)snprintf(s->message,sizeof s->message,"%s",shell_level_message);
}
int bsl_shell_palette_count(int kind){return kind==3?bsl_host_event_count():kind==0?editor_ssobj_count:kind==1?editor_brick_count:kind==2?({BslShellState s={0};bsl_shell_terrain_snapshot(&s);s.materials;}):0;}
const char* bsl_shell_palette_name(int kind,int index){
 static char text[128];
 if(index<0||index>=bsl_shell_palette_count(kind))return "";
 if(kind==3)return bsl_host_event_id(index);
 if(kind==0)return editor_ssobj_name(index);
 if(kind==2)return bsl_shell_material_name(index);
 int id=editor_brick_id(index);snprintf(text,sizeof text,"Brick %d / %s",id,brick_role_preview(id)==BRR_DECORATION?"decoration":"collision");return text;
}
void* bsl_shell_palette_image(int kind,int index){
 if(index<0||index>=bsl_shell_palette_count(kind))return NULL;
 if(kind==2)return bsl_shell_material_image(index);
 const image_t* image;
 if(kind==0){const char* name=editor_ssobj_name(index);image=animation_image(sprite_get_animation(sprite_animation_exists(name,0)?name:NULL,0),0);}
 else image=brick_image_preview(editor_brick_id(index));
 return image?IMAGE2BITMAP(image):NULL;
}
bool bsl_shell_field(int index,BslShellField* out){
 surgescript_object_t* o=bsl_inspect_object();if(!o)return false;
 int n=0;
 for(int i=0;i<BSL_INSTANCE_FIELDS;i++){
  const bsl_instance_field_t* f=&bsl_instance_fields[i];if(strcmp(f->object_name,surgescript_object_name(o)))continue;
  if(n++!=index)continue;
  memset(out,0,sizeof *out);out->index=i;out->boolean=f->boolean;out->minimum=f->minimum;out->maximum=f->maximum;out->step=f->step;
  out->value=bsl_instance_authored(bsl_inspect_id,i,&out->overridden);snprintf(out->name,sizeof out->name,"%s",f->key);
  char getter[128];snprintf(getter,sizeof getter,"get_%s",f->key);out->live_available=surgescript_object_has_function(o,getter);
  if(out->live_available){surgescript_var_t* v=surgescript_var_create();surgescript_object_call_function(o,getter,NULL,0,v);out->effective=f->boolean?surgescript_var_get_bool(v):surgescript_var_get_number(v);surgescript_var_destroy(v);}
  return true;
 }return false;
}
bool bsl_shell_level_preflight(void){
 if(readonly){bsl_shell_note("save","This level is read-only.",2);return false;}
 if(!bsl_shell_file_unchanged(file)){bsl_shell_note("save","Level changed externally; save refused. Your draft is retained.",2);return false;}
 return true;
}
static bool shell_save_all(void){
 if(bsl_inspect_drag.id)bsl_inspect_drag_update(false);editor_brush_reset();
 if(!bsl_shell_terrain_preflight()||!bsl_shell_level_preflight())return false;
 if(!bsl_shell_terrain_save()){bsl_shell_note("save","Terrain save failed; placements were not saved.",2);return false;}
 bsl_shell_note("save","Terrain saved / unchanged.",0);
 if((shell_level_dirty||bsl_inspect_unsaved)&&!level_save(file)){bsl_shell_note("save","Terrain saved; placements FAILED and remain unsaved.",2);return false;}
 shell_level_dirty=bsl_inspect_unsaved=false;bsl_shell_file_capture(file);bsl_shell_note("save","All level edits saved.",0);return true;
}
static void shell_pick_brick(void){
 shell_brick=0;float z=-1e9;iterator_t* it=brickmanager_retrieve_all_bricks(brick_manager);
 while(iterator_has_next(it)){brick_t* b=iterator_next(it);v2d_t p=brick_position(b),size=brick_size(b);
  if(editor_cursor.world_position.x>=p.x&&editor_cursor.world_position.x<p.x+size.x&&editor_cursor.world_position.y>=p.y&&editor_cursor.world_position.y<p.y+size.y&&brick_zindex(b)>=z){shell_brick=brick_placement_id(b);z=brick_zindex(b);}
 }iterator_destroy(it);
}
static void shell_sample(v2d_t point){
 editor_cursor.world_position=point;
 if(shell_tool==1){bsl_cell_cancel_line();if(bsl_shell_terrain_pick(point.x,point.y))bsl_shell_note("sample","Terrain shape and material sampled.",0);return;}
 if(shell_tool==4||shell_tool==0){
  shell_pick_brick();brick_t* b=shell_selected_brick();
  if(b&&shell_tool==4&&!editor_brick_matches_filter(b)){shell_brick=0;b=NULL;}
  if(b){
   editor_selected_entity_id=brick_id(b);editor_layer=brick_layer(b);editor_flip=brick_flip(b);
   editor_stamp_brightness=brick_brightness(b);editor_stamp_z=brick_z_override(b);str_cpy(editor_stamp_texture,brick_texture(b),sizeof editor_stamp_texture);
   shell_tool=4;editor_selected_entity_type=EDT_BRICK;bsl_shell_terrain_tool(false);return;
  }
 }
 if(shell_tool==0||shell_tool==2||shell_tool==3){
  surgescript_object_t* picked=NULL;surgescript_object_t* manager=entitymanager_ssobject();
  iterator_t* it=entitymanager_activeentities_iterator(manager);
  while(iterator_has_next(it)){
   surgescript_var_t** var=iterator_next(it);surgescript_objecthandle_t h=surgescript_var_get_objecthandle(*var);
   if(!surgescript_objectmanager_exists(surgescript_object_manager(manager),h))continue;
   surgescript_object_t* o=surgescript_objectmanager_get(surgescript_object_manager(manager),h);
   if(shell_tool!=3||surgescript_object_has_tag(o,"bsl-event"))editor_pick_entity(o,&picked);
  }iterator_destroy(it);
  if(picked){
   int index=editor_ssobj_index(surgescript_object_name(picked));if(index<0)return;
   editor_selected_entity_id=index;editor_ssobj_picked_entity.id=entity_info_id(picked);
   str_cpy(editor_ssobj_picked_entity.name,surgescript_object_name(picked),sizeof editor_ssobj_picked_entity.name);
   shell_tool=surgescript_object_has_tag(picked,"bsl-event")?3:2;editor_selected_entity_type=shell_tool==3?EDT_EVENTS:EDT_SSOBJ;
   bsl_shell_terrain_tool(false);bsl_shell_note("sample","Entity template and instance overrides sampled.",0);return;
  }
  if(shell_tool==0&&bsl_shell_terrain_pick(point.x,point.y)){shell_tool=1;editor_selected_entity_type=EDT_SSOBJ;bsl_shell_terrain_tool(true);}
 }
}
static bool shell_view_command(const char* command,double value){
 if(!strncmp(command,"view.pan_",9)){
  v2d_t zero=video_convert_window_to_screen(v2d_new(0,0));
  v2d_t delta=v2d_subtract(video_convert_window_to_screen(v2d_new(command[9]=='x'?value:0,command[9]=='y'?value:0)),zero);
  editor_camera=v2d_subtract(editor_camera,v2d_multiply(delta,1.0f/editor_zoom));return true;
 }
 if(!strcmp(command,"view.zoom")){
  ALLEGRO_MOUSE_STATE mouse;al_get_mouse_state(&mouse);
  v2d_t anchor=video_convert_window_to_screen(v2d_new(mouse.x,mouse.y));
  editor_view_zoom(editor_zoom*powf(1.2f,clip(value,-32,32)),anchor);return true;
 }
 if(!strcmp(command,"view.set")){editor_view_zoom(value,v2d_multiply(video_get_screen_size(),.5f));return true;}
 if(!strcmp(command,"view.reset")){editor_view_zoom(1.0f,v2d_multiply(video_get_screen_size(),.5f));return true;}
 if(!strcmp(command,"view.sample")){
  BslShellDrop sample;if(bsl_shell_drop_payload(&sample))shell_sample(editor_screen2world(video_convert_window_to_screen(v2d_new(sample.x,sample.y))));return true;
 }
 return false;
}
bool bsl_shell_level_update(void){
 static char previous_bridge[512]="";
 bsl_editor_poll();
 if(strcmp(previous_bridge,bsl_editor_status)){snprintf(previous_bridge,sizeof previous_bridge,"%s",bsl_editor_status);if(*previous_bridge)bsl_shell_note("bridge",previous_bridge,strstr(previous_bridge,"Cannot")||strstr(previous_bridge,"missing")?2:0);}
 bool gesture=false;
 char command[128];double value;
 while(bsl_shell_pop(command,sizeof command,&value)){
  if(!isfinite(value)){bsl_shell_note("commands","Non-finite editor input refused.",2);continue;}
  if(shell_view_command(command,value)){gesture=true;continue;}
  if(!strcmp(command,"cancel")){bsl_inspect_drag_cancel();editor_brush_reset();bsl_shell_terrain_command("finish",0);continue;}
  if(!strcmp(command,"tool")){
   BslShellState availability={0};bsl_shell_terrain_snapshot(&availability);if((int)value==1&&!availability.terrain){bsl_shell_note("tools","Terrain is unavailable: this level has no cell attachment.",1);continue;}
   bsl_inspect_drag_cancel();editor_brush_reset();shell_tool=clip((int)value,0,4);if(shell_tool==1){bsl_inspect_id=0;shell_brick=0;}
   bsl_shell_terrain_tool(shell_tool==1);
   editor_selected_entity_type=shell_tool==0?EDT_INSPECT:shell_tool==3?EDT_EVENTS:shell_tool==4?EDT_BRICK:EDT_SSOBJ;
   if(shell_tool==3)editor_selected_entity_id=editor_ssobj_index("BSL Event Anchor");
   if(shell_tool==4)editor_selected_entity_id=editor_brick_id(0);
  }else if(!strcmp(command,"palette.entity")){if(value<0||value>=editor_ssobj_count)continue;bsl_inspect_drag_cancel();editor_brush_reset();bsl_inspect_id=0;shell_brick=0;shell_tool=2;bsl_shell_terrain_tool(false);editor_selected_entity_type=EDT_SSOBJ;editor_selected_entity_id=(int)value;}
  else if(!strcmp(command,"palette.brick")){if(value<0||value>=editor_brick_count)continue;bsl_inspect_drag_cancel();editor_brush_reset();bsl_inspect_id=0;shell_brick=0;shell_tool=4;bsl_shell_terrain_tool(false);editor_selected_entity_type=EDT_BRICK;editor_selected_entity_id=editor_brick_id((int)value);}
  else if(!strcmp(command,"palette.drop")){
   gesture=true;
   BslShellDrop drop={0};if(!bsl_shell_drop_payload(&drop)||readonly||!editor_is_enabled()||!isfinite(drop.x)||!isfinite(drop.y))continue;
   v2d_t window=video_get_window_size();if(drop.x<0||drop.y<0||drop.x>=window.x||drop.y>=window.y)continue;
   if(drop.kind<0||drop.kind>2||drop.index<0||drop.index>=bsl_shell_palette_count(drop.kind))continue;
   v2d_t screen=video_convert_window_to_screen(v2d_new(drop.x,drop.y));
   v2d_t at=editor_screen2world(editor_grid_screen_snap(screen));
   if(drop.kind==2){if(bsl_shell_terrain_pick(at.x,at.y)){bsl_shell_terrain_command("material",drop.index);bsl_shell_terrain_tool(true);shell_tool=1;}else bsl_shell_note("placement","No terrain shape under this drop; material remains ready.",1);continue;}
   bsl_inspect_drag_cancel();editor_brush_reset();bsl_shell_terrain_tool(false);bsl_inspect_id=0;shell_brick=0;
   shell_tool=drop.kind==0?2:4;editor_selected_entity_type=drop.kind==0?EDT_SSOBJ:EDT_BRICK;
   editor_selected_entity_id=drop.kind==0?drop.index:editor_brick_id(drop.index);
   editor_action_t placed=editor_action_entity_new(true,editor_selected_entity_type,editor_selected_entity_id,at);placed=editor_action_commit(placed);
   if(!placed.rejected){editor_action_register(placed);if(drop.kind==0)bsl_inspect_id=placed.ssobj_id;bsl_shell_note("placement","Placed. Undo is available.",0);}
  }
  else if(!strncmp(command,"terrain.",8))bsl_shell_terrain_command(command+8,value);
  else if(!strcmp(command,"grid")){editor_grid_size=clip((int)value,8,128);bsl_shell_terrain_command("brush",editor_grid_size);}
  else if(!strcmp(command,"undo")){if(shell_tool==1)bsl_shell_terrain_command("undo",0);else editor_action_undo();}
  else if(!strcmp(command,"redo")){if(shell_tool==1)bsl_shell_terrain_command("redo",0);else editor_action_redo();}
  else if(!strcmp(command,"save")){shell_save_all();ps_lab_mark_saved();}
  else if(!strncmp(command,"lab.",4)){editor_disable();if(!ps_lab_spawn(command+4))bsl_shell_note("lab","Cannot switch to the requested laboratory.",2);return true;}
  else if(!strcmp(command,"resume")){editor_disable();}
  else if(!strcmp(command,"reload")){char path[PATH_MAXLEN];str_cpy(path,file,sizeof path);editor_disable();if(!editor_is_enabled()){scenestack_pop();scripting_reload();scenestack_push(storyboard_get_scene(SCENE_LEVEL),path);}return true;}
  else if(!strcmp(command,"home")){editor_camera=spawn_point;}
  else if(!strcmp(command,"door"))bsl_door_navigate(value>=0?1:-1);
  else if(!strcmp(command,"event")){if(value>=-1&&value<bsl_host_event_count()&&bsl_event_is_anchor(bsl_inspect_object()))bsl_event_change(bsl_inspect_id,value<0?"":bsl_host_event_id((int)value));}
  else if(!strcmp(command,"database")||!strcmp(command,"external")){bsl_editor_launch(bsl_inspect_object(),"");}
  else if(!strcmp(command,"level")){BslLevelProbe p={0};bsl_host_level_probe(&p);if(p.registered)bsl_editor_launch(NULL,p.id);else bsl_shell_note("inspector","This level has no registered properties.",1);}
  else if(!strcmp(command,"definition")){surgescript_object_t* o=bsl_inspect_object();if(!o||bsl_inspect_id!=bsl_shell_command_target())bsl_shell_note("inspector","Selection changed; definition request cancelled.",1);else if(bsl_event_is_anchor(o)){const char* id=bsl_event_definition(entity_info_id(o));if(*id)bsl_editor_launch(o,id);else bsl_shell_note("inspector","Bind a common event before editing its definition.",1);}else bsl_shell_note("inspector","No supported definition binding for this selection.",1);}
  else if(!strncmp(command,"field.",6)){int index=atoi(command+6);if(index>=0&&index<BSL_INSTANCE_FIELDS&&bsl_inspect_id==bsl_shell_command_target()&&bsl_inspect_object()&&!strcmp(bsl_instance_fields[index].object_name,surgescript_object_name(bsl_inspect_object())))bsl_inspect_change(bsl_inspect_id,index,true,bsl_instance_fields[index].boolean?(value!=0):clip(value,bsl_instance_fields[index].minimum,bsl_instance_fields[index].maximum));}
  else if(!strncmp(command,"reset.",6)){int index=atoi(command+6);if(index>=0&&index<BSL_INSTANCE_FIELDS&&bsl_inspect_id==bsl_shell_command_target()&&bsl_inspect_object()&&!strcmp(bsl_instance_fields[index].object_name,surgescript_object_name(bsl_inspect_object())))bsl_inspect_change(bsl_inspect_id,index,false,0);}
  else if(!strcmp(command,"brightness")){brick_t* b=shell_selected_brick();if(b&&brick_role_preview(brick_id(b))==BRR_DECORATION){editor_action_t a=bsl_brick_style_action(b,clip((int)value,0,15),brick_texture(b));editor_action_register(editor_action_commit(a));}}
  else if(!strcmp(command,"delete")){if(bsl_inspect_object()){int index=editor_ssobj_index(surgescript_object_name(bsl_inspect_object()));if(index>=0){editor_action_t a=editor_action_entity_new(false,EDT_SSOBJ,index,entity_info_spawnpoint(bsl_inspect_object()));a.ssobj_id=bsl_inspect_id;editor_action_register(editor_action_commit(a));bsl_inspect_id=0;}}}
 }
 if(!editor_is_enabled())return true;
 if(getenv("BSL_SHELL_CHECK"))return true;
 if(bsl_shell_block_editor())return true;
 if(gesture)return true;
 if(shell_tool==0&&!getenv("BSL_SHELL_CHECK")){
  editor_cursor_update();
  if(editorcmd_is_triggered(editor_cmd,"put-item"))shell_brick=0;
  bsl_inspect_update();
  if(editorcmd_is_triggered(editor_cmd,"put-item")&&!bsl_inspect_id){shell_pick_brick();if(!shell_brick)bsl_shell_terrain_pick(editor_cursor.world_position.x,editor_cursor.world_position.y);}
  return true;
 }
 return false;
}

/* Explicitly opt-in native fixture; only used by the private verification runner. */
static bool shell_count_pole_visit(surgescript_object_t* o,void* data){if(!strcmp(surgescript_object_name(o),"BOR Pole"))(*(int*)data)++;return true;}
static int shell_count_poles(void){int n=0;surgescript_object_traverse_tree_ex(level_ssobject(),&n,shell_count_pole_visit);return n;}
bool bsl_shell_native_fixture(int phase){
 static uint64_t fixture=0,physics_fixture=0,bridge_fixture=0;static int before_drop=-1;
 if(!getenv("BSL_SHELL_CHECK"))return false;
 if(phase==8){
  int index=editor_ssobj_index("BSL Event Anchor");if(index<0)return false;
  editor_action_t a=editor_action_entity_new(true,EDT_SSOBJ,index,v2d_add(spawn_point,v2d_new(224,0)));a=editor_action_commit(a);if(a.rejected||!a.ssobj_id)return false;editor_action_register(a);bridge_fixture=a.ssobj_id;bsl_event_change(bridge_fixture,"event:training_reward");bsl_inspect_id=bridge_fixture;return bsl_event_is_anchor(bsl_inspect_object())&&!strcmp(bsl_event_definition(bridge_fixture),"event:training_reward");
 }
 if(phase==9){extern bool ps_hub_native_origin(const char*,const char*,const char*);return bridge_fixture&&ps_hub_native_origin("event:training_reward",file,x64_to_str(bridge_fixture,NULL,0));}
 if(phase==10){if(!bridge_fixture)return false;bsl_event_change(bridge_fixture,"");bsl_inspect_id=bridge_fixture;return !*bsl_event_definition(bridge_fixture);}
 if(phase==0){
  int index=editor_ssobj_index("BOR Pole");if(index<0)return false;
  v2d_t at=v2d_add(spawn_point,v2d_new(160,0));
  editor_action_t a=editor_action_entity_new(true,EDT_SSOBJ,index,at);a=editor_action_commit(a);editor_action_register(a);fixture=a.ssobj_id;
  surgescript_object_t* object=fixture?level_get_entity_by_id(x64_to_str(fixture,NULL,0)):NULL;if(!object)return false;
  if(getenv("BSL_PHYSICS_LAB")){
   int p=editor_ssobj_index("BSL Physics Ball");if(p<0)return false;
   editor_action_t body=editor_action_entity_new(true,EDT_SSOBJ,p,v2d_add(at,v2d_new(128,-80)));body=editor_action_commit(body);editor_action_register(body);physics_fixture=body.ssobj_id;if(!physics_fixture)return false;
  }
  int f=bsl_instance_field("BOR Pole","width");if(f<0)return false;
  bsl_inspect_change(fixture,f,true,64);
  editor_action_t move={0};move.type=EDA_MOVEINSTANCE;move.obj_type=EDT_INSPECT;move.ssobj_id=fixture;
  move.obj_old_position=entity_info_spawnpoint(object);move.obj_position=v2d_add(move.obj_old_position,v2d_new(32,0));
  move.move_world_before=scripting_util_world_position(object);move.move_world_after=v2d_add(move.move_world_before,v2d_new(32,0));
  editor_action_register(editor_action_commit(move));bool over=false;
  return shell_level_dirty&&bsl_instance_authored(fixture,f,&over)==64&&over&&entity_info_spawnpoint(object).x==at.x+32;
 }
 if(phase==1){
  ALLEGRO_FILE* f=al_fopen(asset_path(file),"rb");if(!f)return false;int64_t n=al_fsize(f);if(n<0||n>16000000){al_fclose(f);return false;}
  char* bytes=mallocx((size_t)n+1);bool okay=al_fread(f,bytes,(size_t)n)==(size_t)n;bytes[n]=0;al_fclose(f);
  char id[64];snprintf(id,sizeof id,"\"%s\"",x64_to_str(fixture,NULL,0));okay=okay&&strstr(bytes,id)&&strstr(bytes,"\"BOR Pole\"")&&strstr(bytes,"\"width\" number 64");free(bytes);return okay;
 }
 if(phase==2){
  shell_level_dirty=true;
#ifdef _WIN32
  _putenv_s("BSL_INSTANCE_FAIL_SAVE","1");
#endif
  bool refused=!shell_save_all();
#ifdef _WIN32
  _putenv_s("BSL_INSTANCE_FAIL_SAVE","");
#endif
  return refused&&shell_level_dirty&&bsl_shell_file_unchanged(file);
 }
 if(phase==3){
  ALLEGRO_FILE* f=al_fopen(asset_path(file),"rb");if(!f)return false;
  int64_t n=al_fsize(f);if(n<0||n>16000000){al_fclose(f);return false;}
  char* bytes=mallocx((size_t)n);bool read=al_fread(f,bytes,(size_t)n)==(size_t)n;al_fclose(f);
  char native[PATH_MAXLEN*2];if(!read||!bsl_instance_native_path(file,native,sizeof native)){free(bytes);return false;}
  FILE* out=fopen(native,"ab");if(!out){free(bytes);return false;}fputs("\n// private external conflict fixture\n",out);fclose(out);
  bool refused=!shell_save_all()&&shell_level_dirty;
  out=fopen(native,"wb");if(!out){free(bytes);return false;}bool restored=fwrite(bytes,1,(size_t)n,out)==(size_t)n;restored=fclose(out)==0&&restored;free(bytes);
  return refused&&restored&&bsl_shell_file_unchanged(file);
 }
 if(phase==6){before_drop=shell_count_poles();return true;}
 if(phase==7)return before_drop>=0&&shell_count_poles()==before_drop+1;
 if(phase==5){
  uint64_t id=bsl_inspect_id;surgescript_object_t* o=bsl_inspect_object();if(!o||strcmp(surgescript_object_name(o),"BOR Pole"))return false;
  editor_action_undo();bool removed=level_get_entity_by_id(x64_to_str(id,NULL,0))==NULL;
  editor_action_redo();bsl_inspect_id=id;return removed&&bsl_inspect_object()!=NULL;
 }
 if(phase==4){
  const uint64_t ids[]={UINT64_C(0xb5010001),UINT64_C(0xb5010002),UINT64_C(0xb5010003),UINT64_C(0xb5010004),UINT64_C(0xb5010005)};
  for(int i=0;i<5;i++)if(!bsl_shell_physics_body(x64_to_str(ids[i],NULL,0)))return false;
  if(physics_fixture&&!bsl_shell_physics_body(x64_to_str(physics_fixture,NULL,0)))return false;
  return true;
 }
 return false;
}

/* Opt-in native view contract check. No authored content is changed. Signed: President. */
bool bsl_shell_view_fixture(void){
 if(!getenv("BSL_SHELL_CHECK")||!editor_is_enabled())return false;
 v2d_t saved_camera=editor_camera,game=camera_get_position();float saved_zoom=editor_zoom;
 bool okay=true;
 const float scales[]={.2f,1.0f,10.0f};
 v2d_t anchor=v2d_multiply(video_get_screen_size(),.37f),point=v2d_new(-96,4096);
 for(int i=0;i<3;i++){
  v2d_t before=editor_screen2world(anchor);editor_view_zoom(scales[i],anchor);
  v2d_t after=editor_screen2world(anchor),roundtrip=editor_screen2world(editor_world2screen(point));
  okay=okay&&fabsf(before.x-after.x)<.01f&&fabsf(before.y-after.y)<.01f&&fabsf(roundtrip.x-point.x)<.01f&&fabsf(roundtrip.y-point.y)<.01f;
  rect_t roi=create_roi(editor_camera,0);(void)roi;
 }
 editor_view_zoom(20,anchor);okay=okay&&editor_zoom==10;
 editor_view_zoom(.01f,anchor);okay=okay&&fabsf(editor_zoom-.2f)<.0001f;
 v2d_t before=editor_camera;shell_view_command("view.pan_x",40);okay=okay&&editor_camera.x<before.x;
 v2d_t unchanged=camera_get_position();okay=okay&&unchanged.x==game.x&&unchanged.y==game.y;
 editor_camera=saved_camera;editor_zoom=saved_zoom;return okay;
}
