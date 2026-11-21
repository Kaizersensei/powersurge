/* Embedded instance authoring for the GPL Surge host. GPL-3.0-or-later. Signed: Atlas. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

static bool bsl_event_copy_has_capacity(uint64_t source);
static void bsl_event_copy(uint64_t source, uint64_t target);
static void bsl_event_finish_load(void);
static void bsl_event_save(surgescript_object_t* object, ALLEGRO_FILE* fp);
static bool bsl_event_inspect_click(surgescript_object_t* object, int x, int y);
static void bsl_event_inspect_render(surgescript_object_t* object, int x);
static void bsl_editor_poll(void);
static bool bsl_editor_bridge_click(surgescript_object_t*, int, int);
static void bsl_editor_bridge_render(surgescript_object_t*, int);

typedef struct { double x,y,width,height,origin_x,origin_y; bool require_action,enabled; } BslSequenceExitView;
static bool bsl_sequence_exit_view(surgescript_object_t*,BslSequenceExitView*);
static void bsl_sequence_exit_note(surgescript_object_t*);
static void bsl_sequence_exit_reset(void);
static void bsl_sequence_exit_suspend(void);

typedef struct {
    const char* object_name;
    const char* key;
    bool boolean;
    double initial, minimum, maximum, step;
} bsl_instance_field_t;
#define BSL_FIELD(c,k,b,d,l,h,s) { c,k,b,d,l,h,s },
static const bsl_instance_field_t bsl_instance_fields[] = {
#include "level_instance_fields.def"
};
#undef BSL_FIELD
#define BSL_INSTANCE_FIELDS ((int)(sizeof bsl_instance_fields / sizeof bsl_instance_fields[0]))
#define BSL_INSTANCE_LIMIT 4096
static struct { uint64_t id; char name[128], key[64]; bool boolean, present; double value; } bsl_instance_values[BSL_INSTANCE_LIMIT];
static int bsl_instance_count;
static uint64_t bsl_loaded_ids[16384];
static int bsl_loaded_count;
static uint64_t bsl_inspect_id;
static int bsl_inspect_page;
static font_t* bsl_inspect_font;
static bool bsl_inspect_unsaved;
static bool bsl_instance_test_poll(void);

static uint64_t bsl_instance_id(const char* text)
{
    size_t size = strlen(text);
    if(size == 0 || size > 16) return 0;
    for(size_t i = 0; i < size; i++) if(!isxdigit((unsigned char)text[i])) return 0;
    return str_to_x64(text);
}

static void bsl_instance_note_id(const char* text)
{
    uint64_t id = bsl_instance_id(text);
    if(!id) fatal_error("Invalid zero/non-hex placement ID: %s", text);
    for(int i = 0; i < bsl_loaded_count; i++)
        if(bsl_loaded_ids[i] == id) fatal_error("Duplicate placement ID: %s", text);
    if(bsl_loaded_count == 16384) fatal_error("Too many authored placement IDs");
    bsl_loaded_ids[bsl_loaded_count++] = id;
}

bool bsl_level_instance_id_reserved(uint64_t id)
{
    for(int i = 0; i < bsl_loaded_count; i++) if(bsl_loaded_ids[i] == id) return true;
    return false;
}

static int bsl_instance_field(const char* name, const char* key)
{
    for(int i = 0; i < BSL_INSTANCE_FIELDS; i++)
        if(!strcmp(name, bsl_instance_fields[i].object_name) && !strcmp(key, bsl_instance_fields[i].key)) return i;
    return -1;
}

static int bsl_instance_find(uint64_t id, const char* key)
{
    for(int i = 0; i < bsl_instance_count; i++)
        if(bsl_instance_values[i].id == id && !strcmp(bsl_instance_values[i].key, key)) return i;
    return -1;
}

static bool bsl_instance_put(uint64_t id, const char* name, const char* key, bool boolean, bool present, double value)
{
    int i = bsl_instance_find(id, key);
    if(i < 0) {
        if(!present) return true;
        if(bsl_instance_count == BSL_INSTANCE_LIMIT) return false;
        i = bsl_instance_count++;
    }
    bsl_instance_values[i].id = id;
    str_cpy(bsl_instance_values[i].name, name, sizeof bsl_instance_values[i].name);
    str_cpy(bsl_instance_values[i].key, key, sizeof bsl_instance_values[i].key);
    bsl_instance_values[i].boolean = boolean;
    bsl_instance_values[i].present = present;
    bsl_instance_values[i].value = value;
    return true;
}

static double bsl_instance_authored(uint64_t id, int field, bool* overridden)
{
    int i = bsl_instance_find(id, bsl_instance_fields[field].key);
    *overridden = i >= 0 && bsl_instance_values[i].present;
    if(*overridden) return bsl_instance_values[i].value;
    double value = bsl_instance_fields[field].initial;
    if(!strcmp(bsl_instance_fields[field].object_name, "BSL Teleporter"))
        bsl_host_teleport_default(id, bsl_instance_fields[field].key, &value);
    return value;
}

static void bsl_instance_apply(surgescript_object_t* object, int field)
{
    const bsl_instance_field_t* f = &bsl_instance_fields[field];
    if(strcmp(surgescript_object_name(object), f->object_name) || !surgescript_object_has_function(object, "onInstanceParameter")) return;
    bool overridden;
    double value = bsl_instance_authored(entity_info_id(object), field, &overridden);
    surgescript_var_t* key = surgescript_var_set_string(surgescript_var_create(), f->key);
    surgescript_var_t* val = surgescript_var_create();
    if(f->boolean) surgescript_var_set_bool(val, value != 0); else surgescript_var_set_number(val, value);
    const surgescript_var_t* args[] = { key, val };
    surgescript_object_call_function(object, "onInstanceParameter", args, 2, NULL);
    surgescript_var_destroy(val);
    surgescript_var_destroy(key);
}

static void bsl_instance_apply_all(surgescript_object_t* object)
{
    for(int i = 0; i < BSL_INSTANCE_FIELDS; i++) bsl_instance_apply(object, i);
    bsl_sequence_exit_note(object);
}

static void bsl_instance_load(int count, const char** param)
{
    if(count != 6 || strcmp(param[0], "1")) fatal_error("Unsupported instance_parameter version/arity; refusing lossy load");
    uint64_t id = bsl_instance_id(param[1]);
    if(!id || strlen(param[2]) >= 128 || strlen(param[3]) == 0 || strlen(param[3]) >= 64 || strpbrk(param[2], "\"\\\n\r"))
        fatal_error("Invalid instance_parameter identity");
    for(const char* c = param[3]; *c; c++)
        if(!isalnum((unsigned char)*c) && *c != '_') fatal_error("Invalid instance_parameter key");
    bool boolean = !strcmp(param[4], "bool");
    if(!boolean && strcmp(param[4], "number")) fatal_error("Unsupported instance_parameter type");
    char* end;
    double value = strtod(param[5], &end);
    if(end == param[5] || *end || !isfinite(value) || (boolean && value != 0 && value != 1)) fatal_error("Invalid instance_parameter value");
    int field = bsl_instance_field(param[2], param[3]);
    if(field >= 0 && (boolean != bsl_instance_fields[field].boolean || value < bsl_instance_fields[field].minimum || value > bsl_instance_fields[field].maximum))
        fatal_error("Instance parameter outside declared type/bounds: %s", param[3]);
    if(bsl_instance_find(id, param[3]) >= 0) fatal_error("Duplicate instance_parameter: %s/%s", param[1], param[3]);
    if(!bsl_instance_put(id, param[2], param[3], boolean, true, value)) fatal_error("Too many instance parameters");
    if(field < 0) logfile_message("Preserving unknown authored parameter %s/%s without applying it", param[2], param[3]);
}

static bool bsl_instance_seed_defaults(surgescript_object_t* object, void* unused)
{
    (void)unused;
    if(!surgescript_object_is_killed(object) && entity_info_exists(object) &&
       (!strcmp(surgescript_object_name(object), "BSL Teleporter") || !strcmp(surgescript_object_name(object), "BSL Sequence Exit"))) bsl_instance_apply_all(object);
    return true;
}

static void bsl_instance_finish_load(void)
{
    bsl_sequence_exit_reset();
    for(int i = 0; i < bsl_instance_count; i++) {
        surgescript_object_t* object = level_get_entity_by_id(x64_to_str(bsl_instance_values[i].id, NULL, 0));
        if(!object || strcmp(surgescript_object_name(object), bsl_instance_values[i].name))
            fatal_error("Instance parameter references absent/mismatched placement: %s", bsl_instance_values[i].key);
        bsl_instance_apply_all(object);
    }
    /* Definition inheritance also applies when this placement has no parameter lines. */
    surgescript_object_traverse_tree_ex(level_ssobject(), NULL, bsl_instance_seed_defaults);
    bsl_event_finish_load();
}

static void bsl_instance_save(surgescript_object_t* object, ALLEGRO_FILE* fp)
{
    uint64_t id = entity_info_id(object);
    for(int i = 0; i < bsl_instance_count; i++) {
        if(bsl_instance_values[i].id == id && bsl_instance_values[i].present)
            al_fprintf(fp, "instance_parameter 1 \"%s\" \"%s\" \"%s\" %s %.17g\n", x64_to_str(id, NULL, 0), bsl_instance_values[i].name,
                bsl_instance_values[i].key, bsl_instance_values[i].boolean ? "bool" : "number", bsl_instance_values[i].value);
    }
    bsl_event_save(object, fp);
}

/* Reserve IDs for the entire edit lifetime, including deleted placements. */
static uint64_t bsl_instance_fresh_id(void)
{
    static uint64_t next = UINT64_C(0xb510000000000000);
    for(;;) {
        uint64_t id = ++next;
        bool used = false;
        for(int i = 0; i < bsl_loaded_count; i++) if(bsl_loaded_ids[i] == id) used = true;
        for(int i = 0; i < bsl_instance_count; i++) if(bsl_instance_values[i].id == id) used = true;
        if(!used && !level_get_entity_by_id(x64_to_str(id, NULL, 0))) return id;
    }
}

static bool bsl_instance_copy_has_capacity(uint64_t source)
{
    int needed = 0;
    for(int i = 0; i < bsl_instance_count; i++)
        if(bsl_instance_values[i].id == source && bsl_instance_values[i].present) needed++;
    return needed <= BSL_INSTANCE_LIMIT - bsl_instance_count && bsl_event_copy_has_capacity(source);
}

/* ID-keyed tombstones remain for undo. Only live placements are serialized. */
static void bsl_instance_copy(uint64_t source, uint64_t target)
{
    if(!source || source == target) return;
    int count = bsl_instance_count;
    for(int i = 0; i < count; i++) {
        if(bsl_instance_values[i].id == source && bsl_instance_values[i].present)
            bsl_instance_put(target, bsl_instance_values[i].name, bsl_instance_values[i].key, bsl_instance_values[i].boolean, true, bsl_instance_values[i].value);
    }
    bsl_event_copy(source, target);
}

/* PhysFS virtual paths must publish inside its configured physical write directory. */
static bool bsl_instance_native_path(const char* virtual_path, char* dest, size_t size)
{
    if(!asset_writedir() || !*asset_writedir() || !*virtual_path || virtual_path[0] == '/' || virtual_path[0] == '\\' || strchr(virtual_path, ':')) return false;
    ALLEGRO_PATH* path = al_create_path(virtual_path);
    if(!path) return false;
    bool safe = true;
    for(int i = 0; i < al_get_path_num_components(path); i++)
        if(!strcmp(al_get_path_component(path, i), "..")) safe = false;
    al_destroy_path(path);
    if(!safe) return false;
    int length = snprintf(dest, size, "%s/%s", asset_writedir(), virtual_path);
    return length > 0 && (size_t)length < size;
}

/* Exclusive same-directory staging; Allegro's temp helper only accepts basenames. */
static ALLEGRO_FILE* bsl_instance_open_staged(const char* destination, char* staged, size_t size)
{
    for(int attempt = 0; attempt < 32; attempt++) {
        int length = snprintf(staged, size, "%s.bsl-%016llx.tmp", destination, (unsigned long long)random64());
        if(length < 0 || (size_t)length >= size) return NULL;
#ifdef _WIN32
        wchar_t path[PATH_MAXLEN * 3];
        if(!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, staged, -1, path, PATH_MAXLEN * 3)) return NULL;
        int fd = _wopen(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT, _S_IREAD | _S_IWRITE);
#else
        int fd = open(staged, O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif
        if(fd < 0) { if(errno == EEXIST) continue; return NULL; }
        ALLEGRO_FILE* fp = al_fopen_fd(fd, "wb");
        if(!fp) {
#ifdef _WIN32
            _close(fd);
#else
            close(fd);
#endif
            al_remove_filename(staged);
        }
        return fp;
    }
    return NULL;
}

static bool bsl_instance_publish(const char* staged, const char* destination)
{
#ifdef _WIN32
    wchar_t from[PATH_MAXLEN * 2], to[PATH_MAXLEN * 2];
    if(!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, staged, -1, from, PATH_MAXLEN * 2) ||
       !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, destination, -1, to, PATH_MAXLEN * 2)) return false;
    return MoveFileExW(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(staged, destination) == 0;
#endif
}

static void bsl_inspect_change(uint64_t id, int field, bool present, double value)
{
    bool before;
    double old = bsl_instance_authored(id, field, &before);
    if(before == present && (!present || fabs(old - value) < 1e-9)) return;
    if(present && bsl_instance_find(id, bsl_instance_fields[field].key) < 0 && bsl_instance_count == BSL_INSTANCE_LIMIT) {
        editor_status_display("Instance parameter limit reached; edit not applied", 0, NULL);
        return;
    }
    editor_action_t action = {0};
    action.type = EDA_INSTANCEPARAM;
    action.obj_type = EDT_INSPECT;
    action.ssobj_id = id;
    action.parameter_field = field;
    action.parameter_before = old;
    action.parameter_after = value;
    action.parameter_had_before = before;
    action.parameter_has_after = present;
    editor_action_register(editor_action_commit(action));
}

static void bsl_instance_commit(editor_action_t action, bool undo)
{
    int field = action.parameter_field;
    const bsl_instance_field_t* f = &bsl_instance_fields[field];
    if(!bsl_instance_put(action.ssobj_id, f->object_name, f->key, f->boolean,
        undo ? action.parameter_had_before : action.parameter_has_after,
        undo ? action.parameter_before : action.parameter_after)) {
        editor_status_display("Instance parameter limit reached", 0, NULL);
        return;
    }
    surgescript_object_t* object = level_get_entity_by_id(x64_to_str(action.ssobj_id, NULL, 0));
    if(object && !surgescript_object_is_killed(object)) bsl_instance_apply(object, field);
    bsl_inspect_unsaved = true;
}

static surgescript_object_t* bsl_inspect_object(void)
{
    surgescript_object_t* object = bsl_inspect_id ? level_get_entity_by_id(x64_to_str(bsl_inspect_id, NULL, 0)) : NULL;
    return object && !surgescript_object_is_killed(object) ? object : NULL;
}

static bool bsl_inspect_pick(surgescript_object_t* object, void* data)
{
    surgescript_object_t** candidate = data;
    if(surgescript_object_has_tag(object, "entity") && entity_info_exists(object) && entity_info_is_persistent(object) && !surgescript_object_has_tag(object, "detached") && !surgescript_object_is_killed(object)) {
        editor_pick_entity(object, candidate);
        if(!strcmp(surgescript_object_name(object), "BSL Teleporter")) {
            BslDoorLive live;
            if(bsl_bor_teleport_live(entity_info_id(object), &live)) {
                double x = editor_cursor.world_position.x, y = editor_cursor.world_position.y;
                if(x >= live.x-live.origin_x && x <= live.x-live.origin_x+live.width &&
                   y >= live.y-live.origin_y && y <= live.y-live.origin_y+live.height)
                    if(!*candidate || scripting_util_object_zindex(object) >= scripting_util_object_zindex(*candidate)) *candidate = object;
            }
        }
        if(!strcmp(surgescript_object_name(object), "BSL Sequence Exit")) {
            BslSequenceExitView view;
            if(bsl_sequence_exit_view(object,&view)) {
                double x=editor_cursor.world_position.x,y=editor_cursor.world_position.y;
                if(x>=view.x-view.origin_x&&x<=view.x-view.origin_x+view.width&&y>=view.y-view.origin_y&&y<=view.y-view.origin_y+view.height)
                    if(!*candidate||scripting_util_object_zindex(object)>=scripting_util_object_zindex(*candidate))*candidate=object;
            }
        }
        /* Climb regions do not have actor sprites matching their configurable dimensions. */
        if(!strcmp(surgescript_object_name(object), "BOR Pole") || !strcmp(surgescript_object_name(object), "BOR Ladder")) {
            bool ignored;
            int wf = bsl_instance_field(surgescript_object_name(object), "width");
            int hf = bsl_instance_field(surgescript_object_name(object), "height");
            double w = bsl_instance_authored(entity_info_id(object), wf, &ignored);
            double h = bsl_instance_authored(entity_info_id(object), hf, &ignored);
            v2d_t at = scripting_util_world_position(object);
            if(fabs(editor_cursor.world_position.x - at.x) <= w / 2 && editor_cursor.world_position.y >= at.y && editor_cursor.world_position.y <= at.y + h)
                if(!*candidate || scripting_util_object_zindex(object) >= scripting_util_object_zindex(*candidate)) *candidate = object;
        }
    }
    return true;
}

static int bsl_inspect_field_count(surgescript_object_t* object)
{
    int count = 0;
    if(object) for(int i = 0; i < BSL_INSTANCE_FIELDS; i++)
        if(!strcmp(surgescript_object_name(object), bsl_instance_fields[i].object_name)) count++;
    return count;
}

static int bsl_inspect_panel_x(void) { return max(0, VIDEO_SCREEN_W - 330); }
static bool bsl_inspect_over_panel(void) { return bsl_shell_block_editor(); }

#include "level_inspect_drag.inc"

static void bsl_inspect_update(void)
{
    bsl_editor_poll();
    if(editorcmd_is_triggered(editor_cmd, "put-item")) {
        if(!bsl_inspect_over_panel()) {
            surgescript_object_t* object = NULL;
            surgescript_object_traverse_tree_ex(level_ssobject(), &object, bsl_inspect_pick);
            uint64_t id = object ? entity_info_id(object) : 0;
            if(id != bsl_inspect_id) bsl_inspect_page = 0;
            bsl_inspect_id = id;
            bsl_inspect_drag_begin(object);
        }
        else {
            surgescript_object_t* object = bsl_inspect_object();
            if(bsl_editor_bridge_click(object, (int)editor_cursor.screen_position.x - bsl_inspect_panel_x(), (int)editor_cursor.screen_position.y)) {}
            else if(object) {
                int y = (int)editor_cursor.screen_position.y, x = (int)editor_cursor.screen_position.x - bsl_inspect_panel_x();
                int row = (y - 124) / 38, n = 0, pages = (bsl_inspect_field_count(object) + 3) / 4;
                if(pages > 1 && y >= 274 && y < 292) {
                    if(x >= 10 && x < 70) bsl_inspect_page = max(0, bsl_inspect_page - 1);
                    else if(x >= 260 && x < 320) bsl_inspect_page = min(pages - 1, bsl_inspect_page + 1);
                }
                else if(!bsl_event_inspect_click(object, x, y) && y >= 124 && row < 4 && y - (124 + row * 38) < 19) for(int i = 0; i < BSL_INSTANCE_FIELDS; i++) {
                    if(strcmp(surgescript_object_name(object), bsl_instance_fields[i].object_name)) continue;
                    if(n++ != bsl_inspect_page * 4 + row) continue;
                    bool override;
                    double value = bsl_instance_authored(bsl_inspect_id, i, &override);
                    if(x >= 274) bsl_inspect_change(bsl_inspect_id, i, false, 0);
                    else if(x >= 220 && x < 268) {
                        value = bsl_instance_fields[i].boolean ? !value : clip(value + (x < 244 ? -1 : 1) * bsl_instance_fields[i].step, bsl_instance_fields[i].minimum, bsl_instance_fields[i].maximum);
                        bsl_inspect_change(bsl_inspect_id, i, true, value);
                    }
                    break;
                }
            }
        }
    }
    if(editorcmd_is_triggered(editor_cmd, "delete-item")) {
        bsl_inspect_drag_cancel();
        bsl_inspect_id = 0;
    }
    bsl_inspect_drag_update(editorcmd_is_triggered(editor_cmd, "paint-item"));
    if(editorcmd_is_triggered(editor_cmd, "undo")) editor_action_undo();
    else if(editorcmd_is_triggered(editor_cmd, "redo")) editor_action_redo();
    editor_update_background();
    editor_grid_update();
    editor_scroll();
    editor_status_update();
    editor_topbar_update();
    editor_cursor_update();
}

static void bsl_inspect_text(int x, int y, const char* text)
{
    font_set_text(bsl_inspect_font, "%s", text);
    font_set_position(bsl_inspect_font, v2d_new(x, y));
    font_render(bsl_inspect_font, v2d_multiply(video_get_screen_size(), 0.5f));
}

static void bsl_inspect_render(void)
{
    int x = bsl_inspect_panel_x();
    char text[256];
    image_rectfill(x, 36, VIDEO_SCREEN_W, min(VIDEO_SCREEN_H - 24, 398), color_premul_rgba(23, 29, 39, 240));
    bsl_inspect_text(x + 10, 44, "INSPECT - drag to move / parameters");
    surgescript_object_t* object = bsl_inspect_object();
    if(!object) {
        bsl_inspect_text(x + 10, 68, "Click a placed object in this level.");
        bsl_inspect_text(x + 10, 92, "Drag to move; right-click cancels/clears.");
        bsl_editor_bridge_render(NULL, x);
        return;
    }
    snprintf(text, sizeof text, "%s", surgescript_object_name(object)); bsl_inspect_text(x + 10, 66, text);
    v2d_t spawn = entity_info_spawnpoint(object), world = scripting_util_world_position(object);
    snprintf(text, sizeof text, "ID %s | spawn %.0f, %.0f", x64_to_str(bsl_inspect_id, NULL, 0), spawn.x, spawn.y); bsl_inspect_text(x + 10, 84, text);
    snprintf(text, sizeof text, "Live %.0f, %.0f | %s", world.x, world.y, bsl_inspect_unsaved ? "authored edits pending" : "authored values saved"); bsl_inspect_text(x + 10, 102, text);
    int row = 0, count = 0;
    for(int i = 0; i < BSL_INSTANCE_FIELDS; i++) {
        const bsl_instance_field_t* f = &bsl_instance_fields[i];
        if(strcmp(surgescript_object_name(object), f->object_name)) continue;
        if(count++ / 4 != bsl_inspect_page) continue;
        int y = 124 + row++ * 38;
        bool override;
        double value = bsl_instance_authored(bsl_inspect_id, i, &override);
        char getter[80]; snprintf(getter, sizeof getter, "get_%s", f->key);
        surgescript_var_t* live = surgescript_var_create();
        bool available = surgescript_object_has_function(object, getter);
        if(available) surgescript_object_call_function(object, getter, NULL, 0, live);
        double effective = f->boolean ? surgescript_var_get_bool(live) : surgescript_var_get_number(live);
        surgescript_var_destroy(live);
        snprintf(text, sizeof text, "%s: %.3g %s", f->key, value, override ? "[instance]" : "[template]"); bsl_inspect_text(x + 10, y, text);
        if(available) snprintf(text, sizeof text, "live %.3g%s", effective, fabs(value-effective) > 1e-9 ? " (runtime override)" : "");
        else snprintf(text, sizeof text, "live unavailable (update scripts)");
        bsl_inspect_text(x + 10, y + 15, text);
        image_rect(x + 220, y, x + 241, y + 18, color_rgb(122, 156, 181));
        image_rect(x + 245, y, x + 267, y + 18, color_rgb(122, 156, 181));
        bsl_inspect_text(x + 227, y + 3, "-"); bsl_inspect_text(x + 252, y + 3, "+"); bsl_inspect_text(x + 276, y + 3, "Reset");
    }
    if(count > 4) {
        image_rect(x + 10, 274, x + 69, 291, color_rgb(122, 156, 181));
        image_rect(x + 260, 274, x + 319, 291, color_rgb(122, 156, 181));
        bsl_inspect_text(x + 18, 278, "Prev"); bsl_inspect_text(x + 269, 278, "Next");
        snprintf(text, sizeof text, "Fields %d/%d", bsl_inspect_page + 1, (count + 3) / 4);
        bsl_inspect_text(x + 105, 278, text);
    }
    if(!strcmp(surgescript_object_name(object), "BSL Teleporter")) {
        BslDoorLive live;
        if(bsl_bor_teleport_live(bsl_inspect_id, &live)) {
            v2d_t a = editor_world2screen(v2d_new(live.x-live.origin_x, live.y-live.origin_y));
            v2d_t b = editor_world2screen(v2d_new(live.x-live.origin_x+live.width, live.y-live.origin_y+live.height));
            image_rect(a.x, a.y, b.x, b.y, live.enabled ? color_rgb(80, 230, 180) : color_rgb(135, 135, 135));
        }
    }
    if(!strcmp(surgescript_object_name(object), "BSL Sequence Exit")) {
        BslSequenceExitView view;
        if(bsl_sequence_exit_view(object,&view)) {
            v2d_t a=editor_world2screen(v2d_new(view.x-view.origin_x,view.y-view.origin_y));
            v2d_t b=editor_world2screen(v2d_new(view.x-view.origin_x+view.width,view.y-view.origin_y+view.height));
            image_rect(a.x,a.y,b.x,b.y,view.enabled?color_rgb(240,198,78):color_rgb(135,135,135));
        }
    }
    bsl_event_inspect_render(object, x);
    if(!row) bsl_inspect_text(x + 10, 130, "No declared editable parameters yet.");
    bsl_editor_bridge_render(object, x);
    v2d_t screen = editor_world2screen(world);
    image_rect(screen.x - 18, screen.y - 30, screen.x + 18, screen.y + 5, color_rgb(70, 225, 230));
}

#include "level_sequence_exit.inc"
#include "level_event.inc"
#include "level_editor_bridge.inc"
