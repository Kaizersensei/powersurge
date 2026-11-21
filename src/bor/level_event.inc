/* Embedded spatial event bindings. GPL-3.0-or-later. Signed: Atlas. */
#define BSL_EVENT_LIMIT 1024
static struct { uint64_t id; char definition[257]; } bsl_event_bindings[BSL_EVENT_LIMIT];
static int bsl_event_count;
static uint64_t bsl_event_nearest;
static bool bsl_event_test_poll(void);

static int bsl_event_find(uint64_t id)
{
    for(int i = 0; i < bsl_event_count; i++) if(bsl_event_bindings[i].id == id) return i;
    return -1;
}
static const char* bsl_event_definition(uint64_t id)
{
    int index = bsl_event_find(id);
    return index < 0 ? "" : bsl_event_bindings[index].definition;
}
static bool bsl_event_put(uint64_t id, const char* definition)
{
    int index = bsl_event_find(id);
    if(index < 0) {
        if(!*definition) return true;
        if(bsl_event_count == BSL_EVENT_LIMIT) return false;
        index = bsl_event_count++;
        bsl_event_bindings[index].id = id;
    }
    str_cpy(bsl_event_bindings[index].definition, definition, sizeof bsl_event_bindings[index].definition);
    return true;
}
static bool bsl_event_is_anchor(surgescript_object_t* object)
{
    return object && !surgescript_object_is_killed(object) && entity_info_exists(object) && entity_info_is_persistent(object) &&
        !strcmp(surgescript_object_name(object), "BSL Event Anchor");
}
static void bsl_event_load(int count, const char** param)
{
    if(count != 4 || strcmp(param[0], "1") || strcmp(param[2], "interact")) fatal_error("Unsupported instance_event version/hook/arity");
    uint64_t id = bsl_instance_id(param[1]);
    if(!id || !*param[3] || strlen(param[3]) > 256 || !bsl_host_event_exists(param[3])) fatal_error("Invalid instance_event definition or placement ID");
    if(bsl_event_find(id) >= 0) fatal_error("Duplicate instance_event binding");
    if(!bsl_event_put(id, param[3])) fatal_error("Too many authored event bindings");
}
static void bsl_event_finish_load(void)
{
    for(int i = 0; i < bsl_event_count; i++) {
        surgescript_object_t* object = level_get_entity_by_id(x64_to_str(bsl_event_bindings[i].id, NULL, 0));
        if(!bsl_event_is_anchor(object)) fatal_error("Event binding requires a persistent BSL Event Anchor owner");
    }
}
static void bsl_event_save(surgescript_object_t* object, ALLEGRO_FILE* fp)
{
    const char* definition = bsl_event_definition(entity_info_id(object));
    if(*definition) al_fprintf(fp, "instance_event 1 \"%s\" interact \"%s\"\n", x64_to_str(entity_info_id(object), NULL, 0), definition);
}
static bool bsl_event_copy_has_capacity(uint64_t source)
{
    return !*bsl_event_definition(source) || bsl_event_count < BSL_EVENT_LIMIT;
}
static void bsl_event_copy(uint64_t source, uint64_t target)
{
    const char* definition = bsl_event_definition(source);
    if(*definition) bsl_event_put(target, definition);
}
static void bsl_event_commit(editor_action_t action, bool undo)
{
    const char* definition = undo ? action.event_before : action.event_after;
    if(*definition && !bsl_host_event_exists(definition)) return;
    if(!bsl_event_put(action.ssobj_id, definition)) return;
    bsl_host_anchor_cancel(action.ssobj_id);
    bsl_inspect_unsaved = true;
}
static void bsl_event_change(uint64_t id, const char* definition)
{
    if(strlen(definition) > 256 || (*definition && !bsl_host_event_exists(definition))) return;
    if(!strcmp(bsl_event_definition(id), definition)) return;
    if(*definition && bsl_event_find(id) < 0 && bsl_event_count == BSL_EVENT_LIMIT) {
        editor_status_display("Event binding capacity reached; edit not applied", 0, NULL); return;
    }
    editor_action_t action = {0};
    action.type = EDA_EVENTBIND;
    action.obj_type = EDT_INSPECT;
    action.ssobj_id = id;
    str_cpy(action.event_before, bsl_event_definition(id), sizeof action.event_before);
    str_cpy(action.event_after, definition, sizeof action.event_after);
    editor_action_register(editor_action_commit(action));
}
static bool bsl_event_inspect_click(surgescript_object_t* object, int x, int y)
{
    if(!bsl_event_is_anchor(object) || y < 202 || y >= 240) return false;
    const char* current = bsl_event_definition(entity_info_id(object));
    int selected = -1, count = bsl_host_event_count();
    for(int i = 0; i < count; i++) if(!strcmp(current, bsl_host_event_id(i))) selected = i;
    if(x >= 274) bsl_event_change(entity_info_id(object), "");
    else if(x >= 220 && x < 268) {
        selected = (selected + 1 + (x < 244 ? -1 : 1) + count + 1) % (count + 1) - 1;
        bsl_event_change(entity_info_id(object), bsl_host_event_id(selected));
    }
    return true;
}
static void bsl_event_inspect_render(surgescript_object_t* object, int x)
{
    if(!bsl_event_is_anchor(object)) return;
    const char* definition = bsl_event_definition(entity_info_id(object));
    bsl_inspect_text(x + 10, 202, "Interact common event:");
    char label[48]; snprintf(label, sizeof label, "%.40s%s", *definition ? definition : "(unbound)", strlen(definition) > 40 ? "..." : "");
    bsl_inspect_text(x + 10, 222, label);
    image_rect(x + 220, 202, x + 241, 220, color_rgb(122, 156, 181));
    image_rect(x + 245, 202, x + 267, 220, color_rgb(122, 156, 181));
    bsl_inspect_text(x + 227, 205, "-"); bsl_inspect_text(x + 252, 205, "+"); bsl_inspect_text(x + 276, 205, "Clear");
    BslAnchorRun run; bsl_host_anchor_probe(entity_info_id(object), &run);
    const char* states[] = { "queued", "running", "waiting", "complete", "cancelled", "failed" };
    char text[128]; snprintf(text, sizeof text, "Last run: %s | command %u", run.status >= 0 && run.status < 6 ? states[run.status] : "none", run.next);
    bsl_inspect_text(x + 10, 250, text);
    bsl_inspect_text(x + 10, 268, "Rebind cancels remaining commands.");
}
static bool bsl_event_enabled(surgescript_object_t* object, double* radius)
{
    surgescript_var_t* value = surgescript_var_create();
    surgescript_object_call_function(object, "get_radius", NULL, 0, value);
    *radius = surgescript_var_get_number(value);
    surgescript_object_call_function(object, "get_enabled", NULL, 0, value);
    bool enabled = surgescript_var_get_bool(value) && isfinite(*radius) && *radius >= 16 && *radius <= 512;
    surgescript_var_destroy(value);
    return enabled;
}
void bsl_level_events_tick(void)
{
    if(editor_is_enabled()) return;
    double x = 0, y = 0;
    uint64_t subject = bsl_host_interaction_take(&x, &y);
    const bool requested = subject != 0;
    if(!subject) subject = bsl_host_interactor(&x, &y);
    bsl_event_nearest = 0;
    double best = INFINITY;
    for(int i = 0; i < bsl_event_count; i++) {
        uint64_t id = bsl_event_bindings[i].id;
        surgescript_object_t* object = level_get_entity_by_id(x64_to_str(id, NULL, 0));
        double radius;
        if(!*bsl_event_bindings[i].definition || !bsl_event_is_anchor(object) || !bsl_event_enabled(object, &radius)) { bsl_host_anchor_cancel(id); continue; }
        if(!subject) continue;
        v2d_t at = scripting_util_world_position(object);
        double distance = (at.x-x)*(at.x-x) + (at.y-y)*(at.y-y);
        if(distance <= radius*radius && (distance < best || (distance == best && id < bsl_event_nearest))) { best = distance; bsl_event_nearest = id; }
    }
    if(requested)
        bsl_host_interaction_dispatch(subject, bsl_event_nearest, bsl_event_definition(bsl_event_nearest), best);
    double actor_distance = INFINITY;
    if(bsl_host_actor_interaction_nearest(subject, &actor_distance) && actor_distance < best) bsl_event_nearest = 0;
}
static bool bsl_event_render_visit(surgescript_object_t* object, void* data)
{
    (void)data;
    if(!bsl_event_is_anchor(object)) return true;
    uint64_t id = entity_info_id(object);
    if(!editor_is_enabled() && id != bsl_event_nearest) return true;
    v2d_t at = editor_is_enabled() ? editor_world2screen(scripting_util_world_position(object)) :
        v2d_add(v2d_subtract(scripting_util_world_position(object), camera_get_position()), v2d_multiply(video_get_screen_size(), 0.5f));
    color_t color = color_rgb(240, 198, 78);
    image_rect(at.x-8, at.y-8, at.x+8, at.y+8, color);
    image_line(at.x-12, at.y, at.x+12, at.y, color);
    image_line(at.x, at.y-12, at.x, at.y+12, color);
    if(bsl_inspect_font) {
        char label[160];
        snprintf(label, sizeof label, "%s %.80s", editor_is_enabled() ? "EVENT" : "E:", *bsl_event_definition(id) ? bsl_event_definition(id) : "(unbound)");
        bsl_inspect_text(at.x + 12, at.y - 10, label);
    }
    return true;
}
static void bsl_event_render(void)
{
    surgescript_object_traverse_tree_ex(level_ssobject(), NULL, bsl_event_render_visit);
}
static void bsl_event_draw_cursor(v2d_t at)
{
    image_rect(at.x-8, at.y-8, at.x+8, at.y+8, color_rgb(240, 198, 78));
    image_line(at.x-12, at.y, at.x+12, at.y, color_rgb(240, 198, 78));
    image_line(at.x, at.y-12, at.x, at.y+12, color_rgb(240, 198, 78));
}

#include "level_actor_binding.inc"
