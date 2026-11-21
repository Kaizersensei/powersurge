/* Opt-in checks against the actual native editor/VM/load-save path. GPL-3.0-or-later. Signed: Atlas. */
static double bsl_instance_test_get(surgescript_object_t* object, const char* getter)
{
    surgescript_var_t* value = surgescript_var_create();
    surgescript_object_call_function(object, getter, NULL, 0, value);
    double result = surgescript_var_get_number(value);
    surgescript_var_destroy(value);
    return result;
}
#include "level_teleporter_test.inc"
#include "level_sequence_exit_test.inc"

static bool bsl_instance_test_poll(void)
{
    if(bsl_sequence_exit_test_poll()) return true;
    if(bsl_teleporter_test_poll()) return true;
    const char* mode = getenv("BSL_INSTANCE_TEST");
    static int frames;
    if(!mode || ++frames < 10) return false;
    if(!strcmp(mode, "drag-ui")) {
        static bool ready;
        if(!ready) {
            ready = true;
            editor_enable();
            editor_selected_entity_type = EDT_INSPECT;
            editor_camera = v2d_new(430, 9850);
            editor_grid_size = 16;
        }
        surgescript_object_t* object = level_get_entity_by_id("11aa");
        const char* path = getenv("BSL_INSTANCE_REPORT");
        FILE* report = path ? fopen(path, "w") : NULL;
        if(report && object) {
            v2d_t spawn = entity_info_spawnpoint(object), world = scripting_util_world_position(object);
            v2d_t screen = editor_world2screen(v2d_add(world, v2d_new(0, 40)));
            fprintf(report, "{\"spawn\":[%.3f,%.3f],\"world\":[%.3f,%.3f],\"screen\":[%.3f,%.3f],\"size\":[%d,%d],\"drag\":%s,\"selected\":%s}\n",
                spawn.x, spawn.y, world.x, world.y, screen.x, screen.y, VIDEO_SCREEN_W, VIDEO_SCREEN_H,
                bsl_inspect_drag.id ? "true" : "false", bsl_inspect_id == 0x11aa ? "true" : "false");
        }
        if(report) fclose(report);
        return false;
    }
    static bool finished;
    if(finished) return true;
    finished = true;
    int checks = 0, failures = 0;
#define BSL_CHECK(c,m) do { checks++; bool passed = (c); if(!passed) failures++; logfile_message("INSTANCE %s: %s", passed ? "PASS" : "FAIL", m); } while(0)
    surgescript_object_t* pole = level_get_entity_by_id("11aa");
    surgescript_object_t* other = level_get_entity_by_id("22bb");
    BSL_CHECK(pole && other, "stable placements available after initial native ticks");
    if(pole && other) {
        int field = bsl_instance_field("BOR Pole", "width");
        editor_enable();
        editor_selected_entity_type = EDT_INSPECT;
        bsl_inspect_id = 0x11aa;
        if(!strcmp(mode, "write")) {
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 24, "template default uses runtime adapter");
            bsl_inspect_change(0x11aa, field, true, 40);
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 40, "Inspect edit reaches owning object immediately");
            editor_action_undo();
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 24, "undo removes authored override");
            editor_action_redo();
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 40, "redo restores authored override");
            surgescript_var_t* value = surgescript_var_set_number(surgescript_var_create(), 88);
            const surgescript_var_t* args[] = { value };
            surgescript_object_call_function(pole, "set_width", args, 1, NULL);
            surgescript_var_destroy(value);
            bsl_inspect_change(0x11aa, field, true, 52);
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 88, "explicit runtime override survives authored edit");
            editor_action_undo(); editor_action_redo();
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 88, "undo redo preserve runtime override");
            surgescript_object_call_function(pole, "resetInstanceParameters", NULL, 0, NULL);
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 52, "explicit reset reveals latest authored default");
            BSL_CHECK(bsl_instance_test_get(other, "get_width") == 24, "sibling instances stay isolated");
            bsl_inspect_change(0x11aa, field, false, 0);
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 24, "Reset inherits template");
            editor_action_undo();
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 52, "reset is undoable");
            int index = editor_ssobj_index("BOR Pole");
            editor_action_t deleted = editor_action_entity_new(FALSE, EDT_SSOBJ, index, entity_info_spawnpoint(pole));
            deleted.ssobj_id = 0x11aa;
            editor_action_register(editor_action_commit(deleted));
            editor_action_undo();
            pole = level_get_entity_by_id("11aa");
            BSL_CHECK(pole && bsl_instance_test_get(pole, "get_width") == 52, "deletion undo restores identity and authored values");
            editor_ssobj_picked_entity.id = 0x11aa;
            str_cpy(editor_ssobj_picked_entity.name, "BOR Pole", sizeof editor_ssobj_picked_entity.name);
            editor_action_t copy = editor_action_entity_new(TRUE, EDT_SSOBJ, index, v2d_new(880, 9850));
            copy = editor_action_commit(copy); editor_action_register(copy);
            surgescript_object_t* duplicate = level_get_entity_by_id(x64_to_str(copy.ssobj_id, NULL, 0));
            BSL_CHECK(copy.ssobj_id != 0x11aa && duplicate && bsl_instance_test_get(duplicate, "get_width") == 52, "duplicate copies authored values under a fresh ID");
            bsl_inspect_change(copy.ssobj_id, field, true, 60);
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 52, "duplicate edits do not alias original");
            int retained_count = bsl_instance_count;
            editor_action_list_t* retained_cursor = editor_action_buffer_cursor;
            bsl_instance_count = BSL_INSTANCE_LIMIT;
            editor_ssobj_picked_entity.id = 0x11aa;
            str_cpy(editor_ssobj_picked_entity.name, "BOR Pole", sizeof editor_ssobj_picked_entity.name);
            editor_action_t rejected = editor_action_entity_new(TRUE, EDT_SSOBJ, index, v2d_new(960, 9850));
            rejected = editor_action_commit(rejected); editor_action_register(rejected);
            BSL_CHECK(rejected.rejected && editor_action_buffer_cursor == retained_cursor, "capacity rejection preserves placement history without partial copy");
            bsl_inspect_change(0x22bb, field, true, 44);
            BSL_CHECK(editor_action_buffer_cursor == retained_cursor && bsl_instance_test_get(other, "get_width") == 24, "full table rejects new parameter before modifying runtime or undo");
            bsl_instance_count = retained_count;
            editor_ssobj_picked_entity.id = 0;
            editor_ssobj_picked_entity.name[0] = '\0';
            uint64_t fresh = bsl_instance_fresh_id();
            bsl_loaded_ids[bsl_loaded_count++] = fresh;
            BSL_CHECK(bsl_instance_fresh_id() != fresh, "reserved deleted placement IDs cannot be reused");
            int reserved_count = bsl_loaded_count;
            bsl_loaded_count = 16384;
            editor_action_t at_capacity = editor_action_entity_new(FALSE, EDT_SSOBJ, index, entity_info_spawnpoint(pole));
            at_capacity.ssobj_id = 0x11aa;
            editor_action_register(editor_action_commit(at_capacity));
            editor_action_undo();
            pole = level_get_entity_by_id("11aa");
            BSL_CHECK(pole && bsl_instance_test_get(pole, "get_width") == 52 && bsl_loaded_count == 16384, "deletion undo at reservation capacity restores existing identity without allocation");
            bsl_loaded_count = reserved_count;
            entity_info_set_persistent(pole, false);
            editor_cursor.world_position = scripting_util_world_position(pole);
            surgescript_object_t* candidate = NULL;
            bsl_inspect_pick(pole, &candidate);
            BSL_CHECK(candidate == NULL, "Inspect excludes nonpersistent climb objects");
            entity_info_set_persistent(pole, true);
            editor_cursor.world_position = v2d_add(scripting_util_world_position(pole), v2d_new(0, 100));
            candidate = NULL; bsl_inspect_pick(pole, &candidate);
            BSL_CHECK(candidate == pole, "climb-region picking follows its actual downward axis");
            /* Unknown bounded fields are retained without applying arbitrary setters. */
            BSL_CHECK(bsl_instance_put(0x11aa, "BOR Pole", "futureFlag", true, true, 1), "opaque future parameter retained");
            BSL_CHECK(level_save(file), "checked native level publication succeeds");
        }
        else if(!strcmp(mode, "read")) {
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 52, "fresh process reconstructs authored width");
            BSL_CHECK(bsl_instance_test_get(other, "get_width") == 24, "fresh process retains sibling default");
            BSL_CHECK(bsl_instance_find(0x11aa, "futureFlag") >= 0, "unknown field survives native file round trip");
            BSL_CHECK(level_save(file), "second save preserves unknown field");
        }
        else if(!strcmp(mode, "drag")) {
            v2d_t spawn = entity_info_spawnpoint(pole), world = v2d_add(spawn, v2d_new(7, -3));
            scripting_util_set_world_position(pole, world);
            bsl_inspect_change(0x11aa, field, true, 52);
            editor_action_list_t* before = editor_action_buffer_cursor;
            editor_grid_size = 16;
            editor_cursor.world_position = v2d_new(430, 9890);
            editor_cursor.screen_position = v2d_new(160, 200);
            bsl_inspect_drag_begin(pole);
            for(int i = 0; i < 8; i++) {
                editor_cursor.world_position = v2d_new(430 + i * 10, 9890 + i * 4);
                editor_cursor.screen_position = v2d_new(160 + i * 10, 200 + i * 4);
                bsl_inspect_drag_update(true);
            }
            BSL_CHECK(editor_action_buffer_cursor == before, "preview frames do not create undo entries");
            BSL_CHECK(entity_info_spawnpoint(pole).x == spawn.x + 64 && entity_info_spawnpoint(pole).y == spawn.y + 32,
                "grid drag preserves grab offset and moves in grid steps");
            BSL_CHECK(scripting_util_world_position(pole).x == world.x + 64 && scripting_util_world_position(pole).y == world.y + 32,
                "runtime displacement relative to authored origin is preserved");
            bsl_inspect_drag_update(false);
            BSL_CHECK(editor_action_buffer_cursor != before, "release commits an undoable move");
            editor_action_undo();
            BSL_CHECK(editor_action_buffer_cursor == before && entity_info_spawnpoint(pole).x == spawn.x &&
                scripting_util_world_position(pole).x == world.x, "one undo restores whole drag and both positions");
            editor_action_redo();
            BSL_CHECK(entity_info_spawnpoint(pole).x == spawn.x + 64 && level_get_entity_by_id("11aa") == pole &&
                bsl_instance_test_get(pole, "get_width") == 52, "redo preserves entity identity and authored parameters");
            before = editor_action_buffer_cursor;
            bsl_inspect_drag_begin(pole); bsl_inspect_drag_update(false);
            BSL_CHECK(editor_action_buffer_cursor == before, "selection click adds no empty undo step");
            bsl_inspect_drag_begin(pole);
            editor_cursor.world_position.x += 32; editor_cursor.screen_position.x += 32;
            bsl_inspect_drag_update(true); bsl_inspect_drag_cancel();
            BSL_CHECK(editor_action_buffer_cursor == before && entity_info_spawnpoint(pole).x == spawn.x + 64,
                "cancel restores preview without changing history");
            editor_grid_size = 1;
            bsl_inspect_drag_begin(pole);
            editor_cursor.world_position.x += 5; editor_cursor.screen_position.x += 5;
            bsl_inspect_drag_update(false);
            BSL_CHECK(entity_info_spawnpoint(pole).x == spawn.x + 69, "grid-off drag permits pixel movement");
            editor_action_undo();
            surgescript_object_t* anchor = level_get_entity_by_id("55ee");
            if(anchor) {
                bsl_inspect_drag_begin(anchor);
                editor_cursor.world_position.x += 19; editor_cursor.screen_position.x += 19;
                bsl_inspect_drag_update(false);
            }
            BSL_CHECK(anchor && !strcmp(bsl_event_definition(0x55ee), "event:training_reward"),
                "moving event anchor retains shared event binding");
            surgescript_object_t* player = level_get_entity_by_id("1234");
            v2d_t player_spawn = entity_info_spawnpoint(player);
            bsl_inspect_drag_begin(player);
            editor_cursor.world_position.x += 16; editor_cursor.screen_position.x += 16;
            bsl_inspect_drag_update(false);
            BSL_CHECK(bsl_bor_editor_origin_test(player, player_spawn.x + 16, player_spawn.y, false),
                "committed actor drag synchronizes BOR reset origin");
            editor_action_undo();
            BSL_CHECK(bsl_bor_editor_origin_test(player, player_spawn.x, player_spawn.y, false),
                "undo restores BOR reset origin");
            editor_action_redo();
            bsl_inspect_drag_begin(player);
            editor_cursor.world_position.x += 16; editor_cursor.screen_position.x += 16;
            bsl_inspect_drag_update(true); bsl_inspect_drag_cancel();
            BSL_CHECK(bsl_bor_editor_origin_test(player, player_spawn.x + 16, player_spawn.y, false),
                "cancel leaves BOR reset origin at last committed position");
            bsl_inspect_drag_begin(pole);
            editor_cursor.world_position.x += 32; editor_cursor.screen_position.x += 32;
            bsl_inspect_drag_update(true);
            editor_disable();
            BSL_CHECK(entity_info_spawnpoint(pole).x == spawn.x + 64 && !bsl_inspect_drag.id,
                "leaving editor cancels unfinished preview before gameplay resumes");
            BSL_CHECK(bsl_bor_editor_origin_test(player, player_spawn.x + 16, player_spawn.y, true),
                "actor reset after leaving editor uses relocated origin");
            BSL_CHECK(level_save(file), "native save publishes moved authored origins");
        }
        else if(!strcmp(mode, "drag-read")) {
            BSL_CHECK(entity_info_spawnpoint(pole).x == 494 && entity_info_spawnpoint(pole).y == 9882 &&
                bsl_instance_test_get(pole, "get_width") == 52, "fresh process restores moved placement and parameters");
            surgescript_object_t* anchor = level_get_entity_by_id("55ee");
            BSL_CHECK(anchor && entity_info_spawnpoint(anchor).x == 259 && !strcmp(bsl_event_definition(0x55ee), "event:training_reward"),
                "fresh process preserves moved anchor identity and event");
        }
        else if(!strcmp(mode, "fail")) {
            bsl_inspect_change(0x11aa, field, true, 100);
            BSL_CHECK(!level_save(file), "failed staged save reports failure");
            BSL_CHECK(bsl_instance_test_get(pole, "get_width") == 100, "failed save retains in-memory edit for retry");
        }
        if(editor_enabled) editor_disable();
    }
    const char* path = getenv("BSL_INSTANCE_REPORT");
    FILE* report = path ? fopen(path, "w") : NULL;
    if(report) { fprintf(report, "{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n", failures ? "false" : "true", checks, failures); fclose(report); }
    engine_quit();
#undef BSL_CHECK
    return true;
}
