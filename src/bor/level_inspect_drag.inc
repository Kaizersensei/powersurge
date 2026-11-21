/* Stable-placement movement in the embedded inspector. GPL-3.0-or-later. Signed: Atlas. */
#include "bor_editor.h"

static struct {
    uint64_t id;
    v2d_t cursor, screen, spawn, world;
    int grid;
    bool moved;
} bsl_inspect_drag;

static surgescript_object_t* bsl_inspect_drag_object(uint64_t id)
{
    surgescript_object_t* object = id ? level_get_entity_by_id(x64_to_str(id, NULL, 0)) : NULL;
    return object && !surgescript_object_is_killed(object) && entity_info_exists(object) &&
        entity_info_is_persistent(object) && !surgescript_object_has_tag(object, "detached") ? object : NULL;
}

static void bsl_inspect_move(surgescript_object_t* object, v2d_t spawn, v2d_t world)
{
    /* Both positions matter: saving uses spawn, while paused previews use the live transform. */
    entitymanager_set_entity_spawn_point(entitymanager_ssobject(), surgescript_object_handle(object), spawn);
    scripting_util_set_world_position(object, world);
}

static void bsl_inspect_drag_cancel(void)
{
    surgescript_object_t* object = bsl_inspect_drag_object(bsl_inspect_drag.id);
    if(object) bsl_inspect_move(object, bsl_inspect_drag.spawn, bsl_inspect_drag.world);
    bsl_inspect_drag.id = 0;
}

static void bsl_inspect_move_commit(editor_action_t action, bool undo)
{
    surgescript_object_t* object = bsl_inspect_drag_object(action.ssobj_id);
    if(!object) return;
    bsl_inspect_move(object, undo ? action.obj_old_position : action.obj_position,
        undo ? action.move_world_before : action.move_world_after);
    v2d_t origin = undo ? action.obj_old_position : action.obj_position;
    bsl_bor_editor_origin(object, origin.x, origin.y);
    bsl_inspect_unsaved = true;
}

static void bsl_inspect_drag_begin(surgescript_object_t* object)
{
    bsl_inspect_drag_cancel();
    if(!object) return;
    bsl_inspect_drag.id = entity_info_id(object);
    bsl_inspect_drag.cursor = editor_cursor.world_position;
    bsl_inspect_drag.screen = editor_cursor.screen_position;
    bsl_inspect_drag.spawn = entity_info_spawnpoint(object);
    bsl_inspect_drag.world = scripting_util_world_position(object);
    bsl_inspect_drag.grid = editor_grid_is_enabled() ? editor_grid_size : 1;
    bsl_inspect_drag.moved = false;
}

static void bsl_inspect_drag_update(bool held)
{
    if(!bsl_inspect_drag.id) return;
    surgescript_object_t* object = bsl_inspect_drag_object(bsl_inspect_drag.id);
    if(!object) { bsl_inspect_drag.id = 0; return; }
    v2d_t screen_delta = v2d_subtract(editor_cursor.screen_position, bsl_inspect_drag.screen);
    if(fabsf(screen_delta.x) >= 3 || fabsf(screen_delta.y) >= 3) bsl_inspect_drag.moved = true;
    if(bsl_inspect_drag.moved) {
        v2d_t delta = v2d_subtract(editor_cursor.world_position, bsl_inspect_drag.cursor);
        int grid = bsl_inspect_drag.grid;
        delta.x = roundf(delta.x / grid) * grid;
        delta.y = roundf(delta.y / grid) * grid;
        bsl_inspect_move(object, v2d_add(bsl_inspect_drag.spawn, delta), v2d_add(bsl_inspect_drag.world, delta));
    }
    if(!held) {
        editor_action_t action = {0};
        action.type = EDA_MOVEINSTANCE;
        action.obj_type = EDT_INSPECT;
        action.ssobj_id = bsl_inspect_drag.id;
        action.obj_old_position = bsl_inspect_drag.spawn;
        action.obj_position = entity_info_spawnpoint(object);
        action.move_world_before = bsl_inspect_drag.world;
        action.move_world_after = scripting_util_world_position(object);
        bsl_inspect_drag.id = 0;
        if(!nearly_zero(action.obj_position.x - action.obj_old_position.x) ||
           !nearly_zero(action.obj_position.y - action.obj_old_position.y))
            editor_action_register(editor_action_commit(action));
    }
}
