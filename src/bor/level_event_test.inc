/* Opt-in actual native Events/editor checks. GPL-3.0-or-later. Signed: Atlas. */
static bool bsl_event_test_poll(void)
{
    const char* mode = getenv("BSL_EVENT_TEST");
    static int frames;
    if(!mode || ++frames < 12) return false;
    static bool done;
    if(done) return true;
    done = true;
    int checks = 0, failures = 0;
#define EVENT_CHECK(c,m) do { bool passed = (c); checks++; if(!passed) failures++; logfile_message("ANCHOR %s: %s", passed ? "PASS" : "FAIL", m); } while(0)
    double x, y;
    uint64_t subject = bsl_host_interactor(&x, &y);
    EVENT_CHECK(bsl_host_event_count() >= 2 && bsl_host_event_exists("event:training_reward"), "compiled common-event catalog available independently of scene services");
    if(!strcmp(mode, "catalog")) {
        EVENT_CHECK(!subject && bsl_host_anchor_start(0x55ee, "event:training_reward", 1) == 1, "catalog lookup does not activate a session or apply a lab loadout");
    }
    else if(!strcmp(mode, "write")) {
        EVENT_CHECK(subject != 0, "explicit registered player lifetime resolves as interaction subject");
        editor_enable();
        editor_selected_entity_type = EDT_EVENTS;
        editor_selected_entity_id = editor_ssobj_index("BSL Event Anchor");
        bsl_event_change(0x55ee, "event:anchor_counter");
        EVENT_CHECK(!strcmp(bsl_event_definition(0x55ee), "event:anchor_counter"), "Inspect binds stable common-event ID");
        editor_action_undo(); EVENT_CHECK(!*bsl_event_definition(0x55ee), "binding undo restores unbound state");
        editor_action_redo(); bsl_event_change(0x55ee, ""); editor_action_undo();
        EVENT_CHECK(!strcmp(bsl_event_definition(0x55ee), "event:anchor_counter"), "clear and redo retain binding history");
        editor_disable();
        EVENT_CHECK(bsl_host_anchor_start(0x55ee, "event:anchor_counter", subject) == 0, "placement-owned run queues through existing Events service");
        bsl_host_simulation_tick();
        BslAnchorRun run; bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 2 && run.next == 2 && !strcmp(run.owner, "placement/55ee") && !strcmp(run.player, "player/1"), "wait cursor has distinct placement owner and explicit player subject");
        EVENT_CHECK(bsl_host_anchor_start(0x55ee, "event:anchor_counter", subject) == 5, "active anchor rejects reentrant activation");
        editor_enable();
        bsl_event_change(0x55ee, "event:training_reward");
        bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 4 && run.next == 2, "rebinding cancels remaining commands and retains committed prefix");
        editor_action_undo();
        bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 4, "binding undo does not resurrect cancelled continuations");
        char long_id[257]; strcpy(long_id, "event:"); memset(long_id+6, 'z', 250); long_id[256]='\0';
        bsl_event_change(0x55ee, long_id); editor_action_undo();
        EVENT_CHECK(!strcmp(bsl_event_definition(0x55ee), "event:anchor_counter"), "maximum256-byte common-event ID participates in binding undo");
        editor_action_redo();
        int index = editor_ssobj_index("BSL Event Anchor");
        editor_ssobj_picked_entity.id = 0x55ee;
        str_cpy(editor_ssobj_picked_entity.name, "BSL Event Anchor", sizeof editor_ssobj_picked_entity.name);
        editor_action_t copy = editor_action_entity_new(TRUE, EDT_SSOBJ, index, v2d_new(480, 9950));
        copy = editor_action_commit(copy); editor_action_register(copy);
        bsl_host_anchor_probe(copy.ssobj_id, &run);
        EVENT_CHECK(copy.ssobj_id != 0x55ee && !strcmp(bsl_event_definition(copy.ssobj_id), long_id) && run.status == -1, "duplicate copies binding with fresh identity and no live run");
        bsl_event_change(0x55ee, "event:anchor_counter");
        surgescript_object_t* anchor = level_get_entity_by_id("55ee");
        editor_action_t deleted = editor_action_entity_new(FALSE, EDT_SSOBJ, index, entity_info_spawnpoint(anchor));
        deleted.ssobj_id = 0x55ee; editor_action_register(editor_action_commit(deleted)); editor_action_undo();
        EVENT_CHECK(bsl_event_is_anchor(level_get_entity_by_id("55ee")) && !strcmp(bsl_event_definition(0x55ee), "event:anchor_counter"), "delete undo restores anchor identity and authored binding");
        int count = bsl_event_count; bsl_event_count = BSL_EVENT_LIMIT;
        editor_ssobj_picked_entity.id = 0x55ee;
        str_cpy(editor_ssobj_picked_entity.name, "BSL Event Anchor", sizeof editor_ssobj_picked_entity.name);
        editor_action_t rejected = editor_action_entity_new(TRUE, EDT_SSOBJ, index, v2d_new(600, 9950));
        rejected = editor_action_commit(rejected);
        EVENT_CHECK(rejected.rejected, "event capacity refuses duplicate before partial placement");
        bsl_event_count = count; editor_ssobj_picked_entity.id = 0; editor_ssobj_picked_entity.name[0] = '\0';
        editor_disable();
        bsl_host_anchor_start(0x55ee, "event:anchor_counter", subject); bsl_host_simulation_tick();
        editor_enable();
        surgescript_object_t* owned = level_get_entity_by_id("55ee");
        surgescript_var_t* yes = surgescript_var_set_bool(surgescript_var_create(), true);
        const surgescript_var_t* yes_args[] = { yes }; surgescript_object_call_function(owned, "set_enabled", yes_args, 1, NULL); surgescript_var_destroy(yes);
        int enabled_field = bsl_instance_field("BSL Event Anchor", "enabled");
        bsl_inspect_change(0x55ee, enabled_field, true, 0); bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 2, "runtime-enabled override masks authored disable without cancelling");
        editor_action_undo(); surgescript_object_call_function(owned, "resetInstanceParameters", NULL, 0, NULL);
        bsl_inspect_change(0x55ee, enabled_field, true, 0); editor_action_undo(); bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 4, "effective paused disable cancels immediately; undo cannot revive it");
        editor_disable();
        EVENT_CHECK(bsl_host_anchor_start(0x55ee, "event:anchor_counter", subject) == 0, "explicit retry gets fresh run identity");
        bsl_host_simulation_tick();
        anchor = level_get_entity_by_id("55ee");
        surgescript_var_t* value = surgescript_var_set_bool(surgescript_var_create(), false);
        const surgescript_var_t* args[] = { value };surgescript_object_call_function(anchor, "set_enabled", args, 1, NULL);surgescript_var_destroy(value);
        bsl_host_simulation_tick(); bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 4, "runtime disabling cancels before further event execution");
        surgescript_object_call_function(anchor, "resetInstanceParameters", NULL, 0, NULL);
        bsl_host_anchor_start(0x55ee, "event:anchor_counter", subject); bsl_host_simulation_tick(); bsl_host_hit(subject); bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 4, "participant hit cancels placement-owned run through player context");
        editor_enable();
        int radius = bsl_instance_field("BSL Event Anchor", "radius");
        bsl_inspect_change(0x55ee, radius, true, 256);
        EVENT_CHECK(level_save(file), "native level saves binding and typed parameters together");
        editor_disable();
        bsl_host_preupdate();
    }
    else if(!strcmp(mode, "read")) {
        EVENT_CHECK(subject != 0 && !strcmp(bsl_event_definition(0x55ee), "event:anchor_counter"), "fresh process reconstructs event binding from native level");
        bool long_binding = false; for(int i=0;i<bsl_event_count;i++) if(strlen(bsl_event_bindings[i].definition)==256) long_binding=true;
        EVENT_CHECK(long_binding, "maximum-length event ID survives copied placement native save/reload");
        EVENT_CHECK(bsl_instance_test_get(level_get_entity_by_id("55ee"), "get_radius") == 256, "numeric parameters and event binding share the authored round trip");
        EVENT_CHECK(bsl_host_anchor_start(0x55ee, "event:training_reward", subject) == 0, "existing common event accepts placement owner and player subject");
        bsl_host_simulation_tick(); BslAnchorRun run; bsl_host_anchor_probe(0x55ee, &run);
        EVENT_CHECK(run.status == 3 && run.next == 3, "existing reward commands complete in shared native runner");
        bsl_host_preupdate();
    }
    const char* path = getenv("BSL_EVENT_REPORT");FILE* report = path ? fopen(path, "w") : NULL;
    if(report) { fprintf(report, "{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n", failures ? "false" : "true", checks, failures); fclose(report); }
    engine_quit();
#undef EVENT_CHECK
    return true;
}
