/* Connected native editor/identity/visibility proof, opt-in only. Signed: Atlas. */
static int terrain_test_alive(void){
 int count=0;iterator_t* it=brickmanager_retrieve_all_bricks(brick_manager);
 while(iterator_has_next(it))if(brick_is_alive(iterator_next(it)))count++;
 iterator_destroy(it);return count;
}
static void terrain_test_filter(int definition){
 for(int i=0;i<editor_brick_filter_count;i++)if(editor_brick_filter_roles[i]==brick_role_preview(definition)&&!strcmp(editor_brick_filter_categories[i],brick_category_preview(definition))){editor_brick_refresh_filter(i);return;}
}
struct terrain_acoustic_probe {const obstacle_t* target;int found;};
static void terrain_acoustic_find(const obstacle_t* obstacle,void* data){struct terrain_acoustic_probe* probe=data;if(obstacle==probe->target)probe->found++;}
static bool bsl_terrain_test_poll(void){
 const char* mode=getenv("BSL_TERRAIN_TEST");static int frames;
 if(!mode||++frames<60)return false;
 static bool finished;if(finished)return true;finished=true;
 int checks=0,failures=0;uint64_t collider_uid=0,decor_uid=0,group_collision_uid=0,group_decor_uid=0;
#define TERRAIN_CHECK(expr,msg) do{bool ok=(expr);checks++;if(!ok)failures++;logfile_message("TERRAIN %s: %s",ok?"PASS":"FAIL",msg);}while(0)
 int shape=brick_resolve_definition("shape:full_square"),grass=brick_resolve_definition("decoration:snakegrass"),tree=brick_resolve_definition("decoration:cherry");
 TERRAIN_CHECK(shape==0&&grass==256&&tree==259,"stable definition registry resolves pinned native numbers");
 TERRAIN_CHECK(brick_role_preview(shape)==BRR_COLLISION&&brick_role_preview(grass)==BRR_DECORATION,"logical roles are separate from numeric IDs");
 brick_t* high=bsl_brick_by_uid(UINT64_MAX-1);brick_t* highest=bsl_brick_by_uid(UINT64_MAX);
 TERRAIN_CHECK(high&&highest&&brick_id(high)==shape&&brick_id(highest)==shape,"maximum saved IDs reload without exhausting fresh allocation; stable definition overrides numeric fallback");
 TERRAIN_CHECK(brick_fresh_placement_id()<1000000,"fresh placement allocation skips reserved IDs instead of following their maximum");
 editor_enable();editor_selected_entity_type=EDT_BRICK;
 if(!strcmp(mode,"write")){
  int initial=terrain_test_alive();v2d_t site=v2d_new(9000,640);
  terrain_test_filter(shape);editor_layer=BRL_GREEN;editor_flip=BRF_HFLIP;
  editor_action_t solid=editor_action_entity_new(TRUE,EDT_BRICK,shape,site);solid=editor_action_commit(solid);editor_action_register(solid);collider_uid=solid.brick_uid;
  terrain_test_filter(grass);editor_layer=BRL_DEFAULT;editor_flip=BRF_NOFLIP;
  editor_action_t deco=editor_action_entity_new(TRUE,EDT_BRICK,grass,site);deco=editor_action_commit(deco);editor_action_register(deco);decor_uid=deco.brick_uid;
  brick_t* a=bsl_brick_by_uid(collider_uid);brick_t* b=bsl_brick_by_uid(decor_uid);
  TERRAIN_CHECK(a&&b&&collider_uid!=decor_uid&&terrain_test_alive()==initial+2,"collider and decoration coexist at identical coordinates with distinct placement IDs");
  TERRAIN_CHECK(a&&b&&brick_obstacle(a)&&!brick_obstacle(b),"decoration creates no physical obstacle");
  brick_set_role_visible(BRR_COLLISION,false,false);brick_set_role_visible(BRR_DECORATION,false,false);
  struct terrain_acoustic_probe probe={brick_obstacle(a),0};bsl_level_acoustic_visit(terrain_acoustic_find,&probe);
  TERRAIN_CHECK(!brick_role_visible(BRR_COLLISION,false)&&brick_obstacle(a)&&probe.found==1,"hidden off-screen collider stays physically and acoustically active");
  TERRAIN_CHECK(!brick_role_visible(BRR_DECORATION,false)&&!brick_obstacle(b),"scenery visibility is independent from collision");
  brick_set_role_visible(BRR_COLLISION,true,true);brick_set_role_visible(BRR_DECORATION,true,true);
  TERRAIN_CHECK(!editor_brick_matches_filter(a)&&editor_brick_matches_filter(b)&&editor_brick_count==3,"decoration category filters both palette and selection");
  editor_action_list_t* cursor=editor_action_buffer_cursor;
  editor_action_t refused=editor_action_commit(editor_action_brick_delete(a));editor_action_register(refused);
  TERRAIN_CHECK(refused.rejected&&editor_action_buffer_cursor==cursor&&bsl_brick_by_uid(collider_uid),"decoration placer filter refuses deletion of underlying collider");
  terrain_test_filter(tree);TERRAIN_CHECK(!editor_brick_matches_filter(b)&&editor_brick_count==1,"another decoration category cannot erase terrain details");
  terrain_test_filter(grass);editor_action_t deleted=editor_action_commit(editor_action_brick_delete(b));editor_action_register(deleted);
  TERRAIN_CHECK(!bsl_brick_by_uid(decor_uid)&&bsl_brick_by_uid(collider_uid),"deleting decoration preserves the co-located collider");
  terrain_test_filter(shape);editor_layer=BRL_GREEN;editor_action_undo();
  TERRAIN_CHECK(bsl_brick_by_uid(decor_uid)&&bsl_brick_by_uid(collider_uid),"undo restores original decoration ID even after changing filter");
  editor_action_redo();TERRAIN_CHECK(!bsl_brick_by_uid(decor_uid)&&bsl_brick_by_uid(collider_uid),"redo remains scoped to the original placement ID");editor_action_undo();
  a=bsl_brick_by_uid(collider_uid);editor_layer=BRL_DEFAULT;
  TERRAIN_CHECK(!editor_brick_matches_filter(a),"collider route filter protects a different terrain route");
  editor_layer=BRL_GREEN;editor_flip=BRF_VFLIP;
  editor_action_t copy=editor_action_entity_new(TRUE,EDT_BRICK,shape,site);copy=editor_action_commit(copy);editor_action_register(copy);
  TERRAIN_CHECK(copy.brick_uid!=collider_uid&&bsl_brick_by_uid(copy.brick_uid),"same-definition same-coordinate copies receive fresh IDs");
  deleted=editor_action_commit(editor_action_brick_delete(bsl_brick_by_uid(copy.brick_uid)));editor_action_register(deleted);
  TERRAIN_CHECK(!bsl_brick_by_uid(copy.brick_uid)&&bsl_brick_by_uid(collider_uid)&&bsl_brick_by_uid(decor_uid),"deletion removes only the selected overlapping instance");
  deleted=editor_action_commit(editor_action_brick_delete(bsl_brick_by_uid(collider_uid)));editor_action_register(deleted);
  TERRAIN_CHECK(!bsl_brick_by_uid(collider_uid)&&bsl_brick_by_uid(decor_uid),"deleting collider preserves decoration");editor_action_undo();
  a=bsl_brick_by_uid(collider_uid);TERRAIN_CHECK(a&&brick_layer(a)==BRL_GREEN&&brick_flip(a)==BRF_HFLIP,"undo restores collider ID, route and flip");
  brick_set_role_visible(BRR_COLLISION,true,false);TERRAIN_CHECK(!editor_brick_matches_filter(a)&&brick_obstacle(a),"hidden editor collision cannot be inadvertently picked or erased");brick_set_role_visible(BRR_COLLISION,true,true);
  editor_layer=BRL_DEFAULT;editor_flip=BRF_NOFLIP;int before_group=terrain_test_alive();
  TERRAIN_CHECK(editorgrp_group_count()>0,"bounded mixed-role group fixture is available");
  if(editorgrp_group_count()>0){
   editor_action_t group=editor_action_entity_new(TRUE,EDT_GROUP,0,v2d_new(9200,640));group=editor_action_commit(group);editor_action_register(group);
   iterator_t* it=brickmanager_retrieve_all_bricks(brick_manager);while(iterator_has_next(it)){brick_t* br=iterator_next(it);v2d_t at=brick_spawnpoint(br);if(brick_is_alive(br)&&at.x==9200&&at.y==640){if(brick_id(br)==shape)group_collision_uid=brick_placement_id(br);if(brick_id(br)==grass)group_decor_uid=brick_placement_id(br);}}iterator_destroy(it);
   TERRAIN_CHECK(terrain_test_alive()==before_group+2&&group_collision_uid&&group_decor_uid&&group_collision_uid!=group_decor_uid,"mixed-role group commits each component once with its actual ID");
   editor_action_undo();TERRAIN_CHECK(terrain_test_alive()==before_group&&!bsl_brick_by_uid(group_collision_uid)&&!bsl_brick_by_uid(group_decor_uid),"group undo removes only its own components");
   editor_action_redo();TERRAIN_CHECK(bsl_brick_by_uid(group_collision_uid)&&bsl_brick_by_uid(group_decor_uid),"group redo restores component identities");
  }
  TERRAIN_CHECK(level_save(file),"atomic authored level save succeeds with brick placement IDs");
 }
 else {
  collider_uid=str_to_x64(getenv("BSL_TERRAIN_COLLIDER")?getenv("BSL_TERRAIN_COLLIDER"):"0");decor_uid=str_to_x64(getenv("BSL_TERRAIN_DECOR")?getenv("BSL_TERRAIN_DECOR"):"0");
  group_collision_uid=str_to_x64(getenv("BSL_TERRAIN_GROUP_COLLIDER")?getenv("BSL_TERRAIN_GROUP_COLLIDER"):"0");group_decor_uid=str_to_x64(getenv("BSL_TERRAIN_GROUP_DECOR")?getenv("BSL_TERRAIN_GROUP_DECOR"):"0");
  brick_t* a=bsl_brick_by_uid(collider_uid);brick_t* b=bsl_brick_by_uid(decor_uid);
  TERRAIN_CHECK(a&&b&&brick_id(a)==shape&&brick_id(b)==grass,"fresh process reload preserves both placement IDs and definitions");
  TERRAIN_CHECK(a&&b&&brick_position(a).x==brick_position(b).x&&brick_position(a).y==brick_position(b).y,"fresh reload preserves same-coordinate coexistence");
  TERRAIN_CHECK(a&&brick_layer(a)==BRL_GREEN&&brick_flip(a)==BRF_HFLIP,"fresh reload preserves collider route and flip");
  TERRAIN_CHECK(a&&b&&brick_obstacle(a)&&!brick_obstacle(b),"fresh reload preserves physical participation");
  TERRAIN_CHECK(bsl_brick_by_uid(group_collision_uid)&&bsl_brick_by_uid(group_decor_uid),"fresh reload retains group component placement identities");
  terrain_test_filter(grass);TERRAIN_CHECK(a&&b&&!editor_brick_matches_filter(a)&&editor_brick_matches_filter(b),"reloaded editor still scopes deletion by placer filter");
 }
 editor_disable();
 const char* report=getenv("BSL_TERRAIN_REPORT");FILE* out=report?fopen(report,"w"):NULL;
 if(out){fprintf(out,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"collider\":\"%016llx\",\"decor\":\"%016llx\",\"group_collider\":\"%016llx\",\"group_decor\":\"%016llx\"}\n",failures?"false":"true",checks,failures,(unsigned long long)collider_uid,(unsigned long long)decor_uid,(unsigned long long)group_collision_uid,(unsigned long long)group_decor_uid);fclose(out);}
 engine_quit();return true;
#undef TERRAIN_CHECK
}
