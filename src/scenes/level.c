#include "../editor_shell/shell.h"
/*
 * Open Surge Engine
 * level.c - code for the game levels
 * Copyright 2008-2026 Alexandre Martins <alemartf(at)gmail.com>
 * http://opensurge2d.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* BOR consumes one current post-script map per frame. Signed: Atlas. */
extern bool scripting_bor_tick_pending(void);
extern void scripting_update_bor(void);
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "level.h"
#include "../presentation/video_presentation.h"
#include "confirmbox.h"
#include "gameover.h"
#include "pause.h"
#include "../foundation/foundation_host.h"
extern void scripting_prepare_bsl_level(void); /* After-header service defaults. Signed: President. */
#include "quest.h"
#include "util/levparser.h"
#include "util/editorgrp.h"
#include "util/editorcmd.h"
#include "../core/engine.h"
#include "../core/scene.h"
#include "../core/storyboard.h"
#include "../core/global.h"
#include "../core/input.h"
#include "../core/fadefx.h"
#include "../core/video.h"
#include "../core/audio.h"
#include "../core/timer.h"
#include "../core/sprite.h"
#include "../core/asset.h"
#include "../core/logfile.h"
#include "../core/lang.h"
#include "../core/nanoparser.h"
#include "../core/font.h"
#include "../core/prefs.h"
#include "../util/darray.h"
#include "../util/numeric.h"
#include "../util/rect.h"
#include "../util/util.h"
#include "../util/stringutil.h"
#include "../util/iterator.h"
#include "../entities/mobilegamepad.h"
#include "../entities/actor.h"
#include "../entities/brick.h"
#include "../entities/brickmanager.h"
#include "../entities/player.h"
#include "../entities/camera.h"
#include "../entities/waterfx.h"
#include "../entities/background.h"
#include "../entities/renderqueue.h"
#include "../cell_terrain/native.h"
#include "../entities/sfx.h"
#include "../entities/legacy/entitymanager.h"
#include "../entities/legacy/item.h"
#include "../entities/legacy/enemy.h"
#include "../physics/obstacle.h"
#include "../physics/obstaclemap.h"
#include "../scripting/scripting.h"
#include "../scenes/editorpal.h"

/* ------------------------
 * Dialog Regions
 *
 * If the player gets inside
 * these regions, a dialog
 * box appears
 * ------------------------ */
/* constants */
#define DIALOGREGION_MAX 8

/* structure */
typedef struct {
    int rect_x, rect_y, rect_w, rect_h; /* region data */
    char title[128], message[1024]; /* dialog box data */
    int disabled; /* TRUE if this region is disabled */
} dialogregion_t;

/* internal data */
static int dialogregion_size; /* size of the vector */
static dialogregion_t dialogregion[DIALOGREGION_MAX];

/* internal methods */
static void update_dialogregions();



/* ------------------------
 * Setup objects
 * ------------------------ */
static void add_to_setup_object_list(const char *object_name);
static void spawn_setup_objects();
static bool is_setup_object_list_empty();
static bool is_setup_object(const char* object_name);



/* ------------------------
 * Level
 * ------------------------ */
/* constants */
#define DEFAULT_MARGIN          (VIDEO_SCREEN_W/2)
#define MAX_POWERUPS            10
#define DLGBOX_MAXTIME          10000
#define TEAM_MAX                16
#define DEFAULT_GRAVITY         787.5f
#define PATH_MAXLEN             1024
#define LINE_MAXLEN             1024
#define MAX_ACT_NUMBER          99

/* water */
#define DEFAULT_WATERLEVEL()    waterfx_default_ypos()
#define DEFAULT_WATERCOLOR()    waterfx_default_color()

/* level attributes */
static char file[PATH_MAXLEN];
static char musicfile[PATH_MAXLEN];
static double music_repeat_start = 0.0;
static char theme[PATH_MAXLEN];
static char bgtheme[PATH_MAXLEN];
static char grouptheme[PATH_MAXLEN];
static char name[128];
static char author[128];
static char version[128];
static char license[128];
static int act_number;
static int requires[3]; /* this level requires engine version x.y.z */
static bool readonly; /* we can't activate the level editor */
static v2d_t spawn_point;

/* player data */
static player_t *team[TEAM_MAX]; /* players */
static int team_size; /* size of team[] */
static player_t *player; /* reference to the current player */

/* level state */
typedef struct levelstate_t levelstate_t;
struct levelstate_t { /* stores attributes that can be changed during gameplay */
    bool is_valid;
    v2d_t spawn_point;
    int act_number;
    int waterlevel;
    color_t watercolor;
    char bgtheme[PATH_MAXLEN];
};
static levelstate_t saved_state;
static void save_level_state(levelstate_t* state);
static void restore_level_state(const levelstate_t* state);
static void clear_level_state(levelstate_t* state);

/* region of interest */
static rect_t create_roi(v2d_t camera, int margin);
static const int ROI_MARGIN_UPDATE_ENTITY = 256;
static const int ROI_MARGIN_UPDATE_BRICK = ROI_MARGIN_UPDATE_ENTITY + 64; /* bricks should use a larger margin than entities */
static const int ROI_MARGIN_UPDATE_PLAYER = ROI_MARGIN_UPDATE_ENTITY;
static const int ROI_MARGIN_RENDER_ENTITY = ROI_MARGIN_UPDATE_ENTITY; /* use the same ROI of the update cycle to skip updating the entity tree; we don't want to unnecessarily bubble things up and down in the entity tree, as it takes time */
static const int ROI_MARGIN_RENDER_BRICK = 128;
static const int ROI_MARGIN_EDITOR = 128;

/* internal data */
static float level_timer;
static music_t *music;
static bgtheme_t *backgroundtheme;
static image_t *quit_level_img;
static brickmanager_t* brick_manager;
static int quit_level;
static int must_load_another_level;
static int must_restart_this_level;
static int must_push_a_quest;
static int must_quit_with_gameover;
static char quest_to_be_pushed[PATH_MAXLEN];
static float dead_player_timeout;
static actor_t *camera_focus; /* camera */
static int must_render_brick_masks;
static int must_display_gizmos;
static bool was_immersive;

/* scripting controlled variables */
static int level_cleared; /* display the level cleared animation */
static int jump_to_next_stage; /* jumps to the next stage in the quest */
static int wants_to_leave; /* wants to abort the level */
static int wants_to_pause; /* wants to pause the level */

/* dialog box */
static int dlgbox_active;
static uint32_t dlgbox_starttime;
static actor_t *dlgbox;
static font_t *dlgbox_title, *dlgbox_message;

/* level management */
static void level_load(const char *filepath);
static void level_unload();
static int level_save(const char *filepath);
static bool level_interpret_header_line(const char *filepath, int fileline, levparser_command_t command, const char *command_name, int param_count, const char** param, void *data);
static bool level_interpret_body_line(const char *filepath, int fileline, levparser_command_t command, const char *command_name, int param_count, const char** param, void *data);
static bool level_save_ssobject(surgescript_object_t* object, void* param);

/* internal methods */
static int inside_screen(int x, int y, int w, int h, int margin);
static void update_level_size();
static void restart(int preserve_level_state);
static void update_music();
static void render_bricks();
static void render_bricks_debug();
static void render_players();
static void spawn_players();
static void render_level(const item_list_t *major_items, const enemy_list_t *major_enemies); /* render bricks, items, enemies, players, etc. */
static void render_hud(); /* gui / hud related */
static void render_dlgbox(v2d_t camera_position); /* dialog boxes */
static void update_dlgbox(); /* dialog boxes */
static void reconfigure_players_input_devices();

/* obstacle map */
static obstaclemap_t* obstaclemap = NULL; /* obstacle map near the camera */
static bool is_obstaclemap_dirty = false;
STATIC_DARRAY(obstacle_t*, mock_obstacles); /* dynamically generated obstacles */
static void create_obstaclemap();
static void destroy_obstaclemap();
static void clear_obstaclemap();
static void update_obstaclemap(const item_list_t* item_list, const object_list_t* object_list);
static obstacle_t* item2obstacle(const item_t* item);
static obstacle_t* object2obstacle(const object_t* object);
static obstacle_t* bricklike2obstacle(const surgescript_object_t* object);
static collisionmask_t* create_collisionmask_of_bricklike_object(const surgescript_object_t* object);
static void destroy_collisionmask_of_bricklike_object(void* mask);

/* Scripting */
static surgescript_object_t* cached_level_ssobject = NULL;
static surgescript_object_t* cached_entity_manager = NULL;

static inline surgescript_object_t* level_ssobject();
static inline surgescript_object_t* entitymanager_ssobject();
static void update_ssobjects();
static void late_update_ssobjects();
static void render_ssobjects();
static void set_entitymanager_roi(rect_t roi);
static surgescript_object_t* spawn_ssobject(const char* object_name, v2d_t spawn_point);
static void notify_ssobjects(const char* fun_name);

static bool entity_info_exists(const surgescript_object_t* object);
static void entity_info_remove(const surgescript_object_t* object);
static v2d_t entity_info_spawnpoint(const surgescript_object_t* object);
static uint64_t entity_info_id(const surgescript_object_t* object);
static void entity_info_set_id(const surgescript_object_t* object, uint64_t entity_id);
static bool entity_info_is_persistent(const surgescript_object_t* object);
static void entity_info_set_persistent(const surgescript_object_t* object, bool is_persistent);

/* debug mode */
#define debug_mode_want_to_activate() (!mobilegamepad_is_available() && editorcmd_is_triggered(editor_cmd, "enter-debug-mode"))

/* events */
static void handle_switchout_event(const ALLEGRO_EVENT* event, void* data);



/* ------------------------
 * Level Editor
 * ------------------------ */

/* methods */
static void editor_init();
static void editor_release();
static void editor_enable();
static void editor_disable();
static void editor_update();
static void editor_render();
static bool editor_is_enabled();
static bool editor_want_to_activate();
static void editor_update_background();
static void editor_render_waterline();
static void editor_save();
static void editor_scroll();
static bool editor_is_eraser_enabled();

/* utilities */
static v2d_t editor_screen2world(v2d_t screen_position);
static v2d_t editor_world2screen(v2d_t world_position);

/* object type */
enum editor_entity_type {
    EDT_BRICK,
    EDT_ITEM,
    EDT_ENEMY,
    EDT_GROUP,
    EDT_SSOBJ,
    EDT_INSPECT,
    EDT_EVENTS
};
#define EDITORGRP_ENTITY_TO_EDT(t) \
                ( ((t) == EDITORGRP_ENTITY_BRICK) ? EDT_BRICK : \
                ((t) == EDITORGRP_ENTITY_ITEM) ? EDT_ITEM : \
                EDT_ENEMY );

/* internal stuff */
static bool editor_enabled; /* is the level editor enabled? */
static editorcmd_t* editor_cmd;
static v2d_t editor_camera;
static enum editor_entity_type editor_selected_entity_type;
static int editor_selected_entity_id;
static videomode_t editor_previous_videomode = VIDEOMODE_DEFAULT;
static const char* editor_entity_class(enum editor_entity_type objtype);
static const char* editor_entity_info(enum editor_entity_type objtype, int objid);
static void editor_draw_object(enum editor_entity_type obj_type, int obj_id, v2d_t position);
static void editor_next_class();
static void editor_previous_class();
static void editor_next_entity();
static void editor_previous_entity();

/* editor: legacy items */
static int editor_item_list[] = {
    IT_GOAL, IT_BIGRING,
    IT_GLASSESBOX, IT_BLUECOLLECTIBLE,
    IT_DANGER, IT_VDANGER, IT_FIREDANGER, IT_VFIREDANGER,
    IT_LWSPIKES, IT_RWSPIKES, IT_PERLWSPIKES, IT_PERRWSPIKES,
    IT_DNADOOR, IT_DNADOORNEON, IT_DNADOORCHARGE,
    IT_HDNADOOR, IT_HDNADOORNEON, IT_HDNADOORCHARGE,
    -1 /* -1 represents the end of this list */
};
static int editor_selected_item_index; /* an index of editor_item_list[] */
static int editor_item_list_size; /* counted automatically */
static int editor_item_list_get_index(int item_id);
int editor_is_valid_item(int item_id); /* this is used by editorgrp */

/* editor: legacy objects (enemy type) */
static const char** editor_enemy_name;
static int editor_enemy_name_length;
int editor_enemy_name2key(const char *name);
const char* editor_enemy_key2name(int key);
static const char** editor_enemy_category;
static int editor_enemy_category_length;
static int editor_enemy_selected_category_id; /* a value between 0 and editor_enemy_category_length - 1, inclusive */
static const char* editor_enemy_selected_category();
static void editor_next_object_category();
static void editor_previous_object_category();

/* editor: SurgeScript entities */
static char** editor_ssobj; /* the names of all SurgeScript entities; a vector of strings */
static int editor_ssobj_count; /* length of editor_ssobj */
static struct { char name[256]; uint64_t id; } editor_ssobj_picked_entity; /* recently picked entity */
static void editor_ssobj_init();
static void editor_ssobj_release();
static int editor_ssobj_sortfun(const void* a, const void* b);
static void editor_ssobj_register(const char* entity_name, void* data); /* internal */
static int editor_ssobj_index(const char* entity_name); /* an index k such that editor_ssobj[k] is entity_name */
static const char* editor_ssobj_name(int entity_index); /* the inverse of editor_ssobj_index() */
static void editor_remove_entity(uint64_t entity_id);
static void editor_pick_entity(surgescript_object_t* object, surgescript_object_t** best_candidate);

/* editor: bricks */
static int* editor_brick; /* an array of all valid brick numbers */
static int editor_brick_count; /* length of editor_brick */
static bricklayer_t editor_layer; /* current layer */
static brickflip_t editor_flip; /* flip flags */
static void editor_brick_init();
static void editor_brick_release();
static int editor_brick_index(int brick_id); /* index of brick_id at editor_brick[] */
static int editor_brick_id(int index); /* the index-th valid brick - at editor_brick[] */
static brickrole_t* editor_brick_filter_roles;
static char (*editor_brick_filter_categories)[65];
static int editor_brick_filter_count, editor_brick_filter;
static void editor_brick_refresh_filter(int filter);
static void editor_brick_cycle_filter(int delta);
static bool bsl_brick_style_input(void);
static bool bsl_workflow_input(void);
static int editor_stamp_z=-1;
static int editor_stamp_brightness=15;
static char editor_stamp_texture[65];
static bool editor_texture_pending;
static uint64_t editor_texture_target;
static bool editor_texture_release;
static bool editor_brick_matches_filter(const brick_t* brick);


/* editor: UI */
#define EDITOR_UI_COLOR()              color_rgb(40, 44, 52)
#define EDITOR_UI_COLOR_TRANS(alpha)   color_premul_rgba(40, 44, 52, (alpha))

/* editor: grid */
#define EDITOR_GRID_INITIAL_SIZE 16
static const int EDITOR_GRID_SIZES[] = { 128, 64, 32, 16, 8, 1 };
static int editor_grid_size = EDITOR_GRID_INITIAL_SIZE;
STATIC_DARRAY(point2d_t, editor_brush_cells);
static v2d_t editor_brush_origin;
static bool editor_brush_active;
static v2d_t editor_brush_previous;
static int editor_brush_grid, editor_brush_type, editor_brush_id;
static bricklayer_t editor_brush_layer;
static brickflip_t editor_brush_flip;
static void editor_brush_reset();
static void editor_brush_paint(bool clicked);
static vertexcache_t* editor_grid_cache;
static void editor_grid_init();
static void editor_grid_release();
static void editor_grid_update();
static void editor_grid_render();
static void editor_grid_render_ex(color_t color, int grid_width, int grid_height);
static inline v2d_t editor_grid_snap(v2d_t world_pos); /* snap world position to grid with custom grid size */
static inline v2d_t editor_grid_snap_ex(v2d_t world_pos, v2d_t topleft, int grid_width, int grid_height);
static inline v2d_t editor_grid_screen_snap(v2d_t screen_pos); /* snap screen position to grid with custom grid size */
static inline v2d_t editor_grid_screen_snap_ex(v2d_t screen_pos, v2d_t topleft, int grid_width, int grid_height);
static inline bool editor_grid_is_enabled();

/* editor: mouse cursor */
static struct {
    v2d_t screen_position;
    v2d_t world_position;
    v2d_t screen_grid_position; /* snap to grid */
    v2d_t world_grid_position;
} editor_cursor;
static font_t *editor_cursor_font;
static void editor_cursor_init();
static void editor_cursor_release();
static void editor_cursor_update();
static void editor_cursor_render();

/* editor: top bar */
static font_t *editor_topbar_info_font;
static font_t *editor_topbar_help_font;
static void editor_topbar_init();
static void editor_topbar_release();
static void editor_topbar_update();
static void editor_topbar_render();

/* editor: tooltip */
static font_t* editor_tooltip_font;
static void editor_tooltip_init();
static void editor_tooltip_release();
static void editor_tooltip_update();
static void editor_tooltip_render();
static const char* editor_tooltip_ssproperties(surgescript_object_t* object);
static void editor_tooltip_ssproperties_add(const char* object_name, void* data);
static const char* editor_tooltip_ssproperties_file(surgescript_object_t* object);

/* editor: status bar */
#define EDITOR_STATUS_TIMEOUT 5.0f /* how long does a status message last on screen, in seconds */
static font_t *editor_status_font; /* status bar */
static float editor_status_timer;
static void editor_status_init();
static void editor_status_release();
static void editor_status_update();
static void editor_status_render();
static void editor_status_display(const char* message, int argc, const char** argv);

/* implementing UNDO and REDO */

/* in order to implement UNDO and REDO,
 * we must register the actions of the
 * user */

/* action type */
enum editor_action_type {
    EDA_NEWOBJECT,
    EDA_DELETEOBJECT,
    EDA_CHANGESPAWN,
    EDA_RESTORESPAWN,
    EDA_CHANGEWATER,
    EDA_RESTOREWATER,
    EDA_INSTANCEPARAM,
    EDA_RESTOREINSTANCEPARAM,
    EDA_EVENTBIND,
    EDA_RESTOREEVENTBIND,
    EDA_BRICKSTYLE,
    EDA_RESTOREBRICKSTYLE,
    EDA_MOVEINSTANCE,
    EDA_RESTOREMOVEINSTANCE
};

/* action = action type + action properties */
typedef struct editor_action_t {
    enum editor_action_type type;
    enum editor_entity_type obj_type;
    int obj_id;
    v2d_t obj_position;
    v2d_t obj_old_position;
    bricklayer_t layer;
    brickflip_t flip;
    uint64_t ssobj_id;
    v2d_t move_world_before, move_world_after;
    uint64_t brick_uid;
    int brightness, brightness_before;
    int z_override, z_before;
    char texture_id[65],texture_before[65];
    int parameter_field;
    double parameter_before, parameter_after;
    bool parameter_had_before, parameter_has_after;
    bool rejected;
    char event_before[257], event_after[257];
} editor_action_t;

/* action: constructors */
static editor_action_t editor_action_entity_new(int is_new_object, enum editor_entity_type obj_type, int obj_id, v2d_t obj_position);
static editor_action_t editor_action_brick_delete(const brick_t* brick);
static editor_action_t editor_action_spawnpoint_new(int is_changing, v2d_t obj_position, v2d_t obj_old_position);
static editor_action_t editor_action_waterlevel_new(int is_changing, int new_waterlevel, int old_waterlevel);

/* linked list */
typedef struct editor_action_list_t {
    editor_action_t action; /* node data */
    int in_group; /* is this element part of a group? */
    uint32_t group_key; /* internal use (used only if in_group is true) */
    struct editor_action_list_t *prev, *next; /* linked list: pointers */
} editor_action_list_t;

/* data */
editor_action_list_t *editor_action_buffer;
editor_action_list_t *editor_action_buffer_head; /* sentinel */
editor_action_list_t *editor_action_buffer_cursor;
    
/* methods */
static void editor_action_init();
static void editor_action_release();
static void editor_action_undo();
static void shell_mark_dirty(void);
static void editor_action_redo();
static editor_action_t bsl_brick_style_action(brick_t*,int,const char*);
static editor_action_t editor_action_commit(editor_action_t action);
static void editor_action_register(editor_action_t action); /* internal */
static editor_action_list_t* editor_action_delete_list(editor_action_list_t *list); /* internal */












#include "../bor/level_instance.inc"
/* Placement lookup never treats shared coordinates as ownership. Signed: Atlas. */
static brick_t* bsl_brick_create(int id,v2d_t position,bricklayer_t layer,brickflip_t flip,uint64_t uid){
 brick_t* brick=brick_create_with_placement_id(id,position,layer,flip,uid);brickmanager_add_brick(brick_manager,brick);return brick;
}
static const char* bsl_brick_parse_definition(int count,const char** params){
 const char* result=NULL;
 for(int i=3;i<count;i++)if(!strncmp(params[i],"def:",4)){if(result||!params[i][4])fatal_error("Invalid/repeated brick definition reference");result=params[i]+4;}
 return result;
}
static brick_t* bsl_brick_by_uid(uint64_t uid){
 if(!uid)return NULL;
 brick_t* found=NULL;iterator_t* it=brickmanager_retrieve_all_bricks(brick_manager);
 while(iterator_has_next(it)){brick_t* b=iterator_next(it);if(brick_is_alive(b)&&brick_placement_id(b)==uid){found=b;break;}}
 iterator_destroy(it);return found;
}
static uint64_t bsl_brick_parse_uid(int count,const char** params){
 uint64_t result=0;
 for(int i=3;i<count;i++)if(!strncmp(params[i],"uid:",4)){
  const char* hex=params[i]+4;if(result||strlen(hex)!=16)fatal_error("Malformed or repeated brick placement ID");
  for(int j=0;j<16;j++)if(!isxdigit((unsigned char)hex[j]))fatal_error("Malformed brick placement ID");
  result=str_to_x64(hex);if(!result)fatal_error("Invalid brick placement ID");
 }
 return result;
}
static bool bsl_terrain_test_poll(void);


/* level-specific functions */

/*
 * level_load()
 * Loads a level from a file
 */
void level_load(const char *filepath)
{
    logfile_message("Loading level \"%s\"...", filepath);

    brick_reset_placement_ids();
    bsl_instance_count = bsl_loaded_count = 0;
    bsl_event_count = 0;
    bsl_event_nearest = 0;
    bsl_inspect_id = 0;
    bsl_inspect_drag.id = 0;
    bsl_inspect_unsaved = false;

    /* initialize fields with default values */
    str_cpy(file, filepath, sizeof(file)); /* we want the relative filepath */
    str_cpy(name, "Untitled", sizeof(name));
    strcpy(musicfile, "");
    strcpy(theme, "");
    strcpy(bgtheme, "");
    strcpy(author, "");
    strcpy(version, "");
    strcpy(license, "");
    strcpy(grouptheme, "");
    spawn_point = v2d_new(0, 0);
    act_number = 0;
    requires[0] = GAME_VERSION_SUP;
    requires[1] = GAME_VERSION_SUB;
    requires[2] = GAME_VERSION_WIP;
    music_repeat_start = 0.0;
    readonly = false;
    dialogregion_size = 0;

    /* clear pointers */
    backgroundtheme = NULL;
    music = NULL;

    /* clear players */
    team_size = 0;
    for(int i = 0; i < TEAM_MAX; i++)
        team[i] = NULL;

    /* initialize the water effect */
    waterfx_init();

    /* Selected progress must precede level object construction. Signed: Atlas. */
    bsl_host_level_begin(filepath);

    /* scripting: preparing a new Level... */
    surgescript_object_t* level_manager = scripting_util_surgeengine_component(surgescript_vm(), "LevelManager");
    surgescript_object_call_function(level_manager, "onLevelLoad", NULL, 0, NULL);

    /* read the header of the level file */
    if(!levparser_parse(filepath, NULL, level_interpret_header_line))
        fatal_error("Can\'t open level file \"%s\".", filepath);

    scripting_prepare_bsl_level();

    /* load the music */
    music_stop(); /* stop any music that's playing */
    music = *musicfile ? music_load(musicfile) : NULL;
    if(music != NULL)
        music_set_repeat_point(music, music_repeat_start);

    /* load the brickset */
    brickset_load(theme);
    bsl_presentation_loading_pump();

    /* load the background */
    backgroundtheme = background_load(bgtheme);
    bsl_presentation_loading_pump();

    /* players */
    spawn_players();
    player_set_collectibles(0);
    level_change_player(team[0]);
    camera_set_position(player_position(player));
    surgescript_object_call_function(scripting_util_surgeengine_component(surgescript_vm(), "Player"), "__spawnPlayers", NULL, 0, NULL);

    /* read the body of the level file;
       load bricks & entities */
    levparser_parse(filepath, NULL, level_interpret_body_line);
    bsl_instance_finish_load();

    /* recompute the level size */
    update_level_size();

    /* spawn setup objects */
    spawn_setup_objects();

    /* success! */
    logfile_message("The level has been loaded.");
}

/*
 * level_unload()
 * Call manually after level_load() whenever
 * this level has to be released or changed
 */
void level_unload()
{
    logfile_message("Unloading the level...");

    /* scripting */
    if(surgescript_vm_is_active(surgescript_vm())) {
        logfile_message("Clearing up level scripts...");
        surgescript_object_call_function(scripting_util_surgeengine_component(surgescript_vm(), "LevelManager"), "onLevelUnload", NULL, 0, NULL);
        logfile_message("The level scripts have been cleared.");
    }

    /* release the players */
    logfile_message("Unloading the players...");
    for(int i = 0; i < team_size; i++)
        player_destroy(team[i]);
    team_size = 0;
    player = NULL;

    /* misc */
    camera_unlock();
    waterfx_release();

    /* music */
    logfile_message("Stopping the music...");
    if(music != NULL) {
        music_stop();
        music_unref(music);
    }

    /* remove all bricks */
    logfile_message("Removing all bricks...");
    brickmanager_remove_all_bricks(brick_manager);

    /* unload the brickset */
    logfile_message("Unloading the brickset...");
    brickset_unload();

    /* unload the background */
    logfile_message("Unloading the background...");
    backgroundtheme = background_unload(backgroundtheme);

    /* success! */
    logfile_message("The level has been unloaded.");
}


/*
 * level_save()
 * Saves the current level to a file
 * Returns TRUE on success
 */
int level_save(const char *filepath)
{
    char fullpath[PATH_MAXLEN * 2];
    if(!bsl_instance_native_path(asset_path(filepath), fullpath, sizeof fullpath)) return FALSE;
    ALLEGRO_FILE *fp;
    char staged[PATH_MAXLEN * 2 + 64];

    /* skip if the readonly flag is set
       should this be moved to the scripting layer instead? */
    if(readonly) {
        logfile_message("Can't save read-only level \"%s\"", filepath);
        return FALSE;
    }

    /* skip if playing in compatibility mode (it overwrites the version) */
    if(engine_compatibility_version_code() < VERSION_CODE_EX(GAME_VERSION_SUP, GAME_VERSION_SUB, GAME_VERSION_WIP, GAME_VERSION_FIX)) {
        logfile_message("Can't save level \"%s\" while playing in compatibility mode!", filepath);
        return FALSE;
    }

    /* open file for writing */
    logfile_message("level_save(\"%s\")", fullpath);
    if(NULL == (fp = bsl_instance_open_staged(fullpath, staged, sizeof staged))) {
        logfile_message("Warning: could not open \"%s\" for writing.", fullpath);
        video_showmessage("Could not open \"%s\" for writing.", fullpath);
        return FALSE;
    }

    /* level header */
    al_fprintf(fp,
    "// ------------------------------------------------------------\n"
    "// %s %s level\n"
    "// This file was generated automatically.\n"
    "// http://%s\n"
    "// ------------------------------------------------------------\n\n",
    GAME_TITLE, GAME_VERSION_STRING, GAME_WEBSITE);

    /* header */
    al_fprintf(fp,
    "// header\n"
    "name \"%s\"\n",
    str_addslashes(name, NULL, 0));

    /* author */
    al_fprintf(fp, "author \"%s\"\n", str_addslashes(author, NULL, 0));
    if(strcmp(license, "") != 0)
        al_fprintf(fp, "license \"%s\"\n", str_addslashes(license, NULL, 0));

    /* level attributes */
    al_fprintf(fp,
    "version \"%s\"\n"
    "requires \"%d.%d.%d\"\n"
    "act %d\n"
    "theme \"%s\"\n"
    "bgtheme \"%s\"\n"
    "spawn_point %d %d\n",
    str_addslashes(version, NULL, 0),
    GAME_VERSION_SUP, GAME_VERSION_SUB, GAME_VERSION_WIP,
    act_number, theme, bgtheme,
    (int)spawn_point.x, (int)spawn_point.y);

    /* music? */
    if(strcmp(musicfile, "") != 0) {
        if(music_repeat_start == 0.0)
            al_fprintf(fp, "music \"%s\"\n", musicfile);
        else
            al_fprintf(fp, "music \"%s\" %.2lf\n", musicfile, music_repeat_start);
    }

    /* grouptheme? */
    if(strcmp(grouptheme, "") != 0)
        al_fprintf(fp, "grouptheme \"%s\"\n", grouptheme);

    /* setup objects? */
    iterator_t* setup_iterator = scripting_level_setupobjects_iterator(level_ssobject());
    if(iterator_has_next(setup_iterator)) {
        al_fprintf(fp, "setup");
        while(iterator_has_next(setup_iterator)) {
            const char** object_name = iterator_next(setup_iterator);
            al_fprintf(fp, " \"%s\"", str_addslashes(*object_name, NULL, 0));
        }
        al_fprintf(fp, "\n");
    }
    iterator_destroy(setup_iterator);

    /* players */
    al_fprintf(fp, "players");
    for(int i = 0; i < team_size; i++)
        al_fprintf(fp, " \"%s\"", str_addslashes(player_name(team[i]), NULL, 0));
    al_fprintf(fp, "\n");

    /* read only? */
    if(readonly)
        al_fprintf(fp, "readonly\n");

    /* water */
    if(level_waterlevel() != DEFAULT_WATERLEVEL())
        al_fprintf(fp, "waterlevel %d\n", level_waterlevel());
    if(!color_equals(level_watercolor(), DEFAULT_WATERCOLOR())) {
        uint8_t r, g, b, a;
        color_unmap(level_watercolor(), &r, &g, &b, &a);
        al_fprintf(fp, "watercolor %d %d %d %d\n", r, g, b, a);
    }

    /* dialog regions */
    if(dialogregion_size > 0) {
        al_fprintf(fp, "\n// dialogs\n");
        for(int i = 0; i < dialogregion_size; i++) {
            char title[256], message[1024];
            al_fprintf(fp,
                "dialogbox %d %d %d %d \"%s\" \"%s\"\n",
                dialogregion[i].rect_x,
                dialogregion[i].rect_y,
                dialogregion[i].rect_w,
                dialogregion[i].rect_h,
                str_addslashes(dialogregion[i].title, title, sizeof(title)),
                str_addslashes(dialogregion[i].message, message, sizeof(message))
            );
        }
    }

    /* brick list */
    al_fprintf(fp, "\n// bricks\n");
    iterator_t* brick_iterator = brickmanager_retrieve_all_bricks(brick_manager);
    while(iterator_has_next(brick_iterator)) {
        const brick_t* brick = iterator_next(brick_iterator);
        if(!brick_is_alive(brick))continue;
        v2d_t spawn_point = brick_spawnpoint(brick);
        bricklayer_t layer = brick_layer(brick);
        brickflip_t flip = brick_flip(brick);

        al_fprintf(fp,
            "brick %d %d %d%s%s%s%s%s%s brightness:%d texture:%s z:%d uid:%016llx\n",

            brick_id(brick),
            (int)(spawn_point.x), (int)(spawn_point.y),

            layer != BRL_DEFAULT ? " " : "",
            layer != BRL_DEFAULT ? brick_util_layername(layer) : "",

            flip != BRF_NOFLIP ? " " : "",
            flip != BRF_NOFLIP ? brick_util_flipstr(flip) : "",
            *brick_definition_preview(brick_id(brick)) ? " def:" : "",
            brick_definition_preview(brick_id(brick)),
            brick_brightness(brick),*brick_texture(brick)?brick_texture(brick):"default",brick_z_override(brick),
            (unsigned long long)brick_placement_id(brick)
        );
    }
    iterator_destroy(brick_iterator);

    /* SurgeScript entity list */
    al_fprintf(fp, "\n// entities\n");
    surgescript_object_traverse_tree_ex(level_ssobject(), fp, level_save_ssobject);

    /* item list */
    item_list_t* item_list = entitymanager_retrieve_all_items();
    if(item_list) {
        al_fprintf(fp, "\n// legacy items\n");
        for(item_list_t* iti = item_list; iti != NULL; iti = iti->next) {
            if(iti->data->state != IS_DEAD)
                al_fprintf(fp, "item %d %d %d\n", iti->data->type, (int)iti->data->actor->spawn_point.x, (int)iti->data->actor->spawn_point.y);
        }
    }
    item_list = entitymanager_release_retrieved_item_list(item_list);

    /* legacy object list */
    enemy_list_t* object_list = entitymanager_retrieve_all_objects();
    if(object_list) {
        al_fprintf(fp, "\n// legacy objects\n");
        for(enemy_list_t* ite = object_list; ite != NULL; ite = ite->next) {
            if(ite->data->created_from_editor && ite->data->state != ES_DEAD)
                al_fprintf(fp, "object \"%s\" %d %d\n", str_addslashes(ite->data->name, NULL, 0), (int)ite->data->actor->spawn_point.x, (int)ite->data->actor->spawn_point.y);
        }
    }
    object_list = entitymanager_release_retrieved_object_list(object_list);

    /* end of file */
    al_fprintf(fp, "\n// EOF");
    bool saved = !al_ferror(fp) && al_fflush(fp);
    if(!al_fclose(fp)) saved = false;
    if(getenv("BSL_INSTANCE_FAIL_SAVE")) saved = false;
    if(saved && bsl_shell_present() && !strcmp(filepath,file)) saved = bsl_shell_file_unchanged(file);
    if(saved) saved = bsl_instance_publish(staged, fullpath);
    if(!saved) {
        logfile_message("Level save failed; original retained: %s", fullpath);
        al_remove_filename(staged);
        return FALSE;
    }

    /* done! */
    logfile_message("level_save() ok");
    return TRUE;
}

/*
 * level_interpret_header_line()
 * Interprets a line of the header of the .lev file
 */
bool level_interpret_header_line(const char* filepath, int fileline, levparser_command_t command, const char* command_name, int param_count, const char** param, void* data)
{
    switch(command) {
    case LEVCOMMAND_ENTITY:
        if(param_count > 3) bsl_instance_note_id(param[3]);
        break;

    case LEVCOMMAND_NAME: {
        if(param_count >= 1)
            str_cpy(name, param[0], sizeof(name));
        else
            logfile_message("Level loader - command '%s' expects one parameter: level name. Did you forget to double quote the level name?", command_name);

        break;
    }

    case LEVCOMMAND_AUTHOR: {
        if(param_count >= 1)
            str_cpy(author, param[0], sizeof(name));
        else
            logfile_message("Level loader - command '%s' expects one parameter: author name. Did you forget to double quote the author name?", command_name);

        break;
    }

    case LEVCOMMAND_VERSION: {
        if(param_count >= 1)
            str_cpy(version, param[0], sizeof(name));
        else
            logfile_message("Level loader - command '%s' expects one parameter: level version", command_name);

        break;
    }

    case LEVCOMMAND_LICENSE: {
        if(param_count >= 1)
            str_cpy(license, param[0], sizeof(license));
        else
            logfile_message("Level loader - command '%s' expects one parameter: license name. Did you forget to double quote the license parameter?", command_name);

        break;
    }

    case LEVCOMMAND_REQUIRES: {
        if(param_count >= 1) {
            requires[0] = requires[1] = requires[2] = 0;
            sscanf(param[0], "%d.%d.%d", &requires[0], &requires[1], &requires[2]);
            for(int i = 0; i < 3; i++)
                requires[i] = clip(requires[i], 0, 99);

            if(game_version_compare(requires[0], requires[1], requires[2]) < 0) {
                fatal_error(
                    "This level requires version %d.%d.%d or greater of the game engine. Yours is %s. Please check out for new versions at %s",
                    requires[0], requires[1], requires[2], GAME_VERSION_STRING, GAME_WEBSITE
                );
            }
        }
        else
            logfile_message("Level loader - command '%s' expects one parameter: minimum required engine version", command_name);

        break;
    }

    case LEVCOMMAND_ACT: {
        if(param_count >= 1)
            level_set_act(atoi(param[0]));
        else
            logfile_message("Level loader - command '%s' expects one parameter: act number", command_name);

        break;
    }

    case LEVCOMMAND_READONLY: {
        readonly = true;
        break;
    }

    case LEVCOMMAND_THEME: {
        if(param_count >= 1)
            str_cpy(theme, param[0], sizeof(theme));
        else
            logfile_message("Level loader - command '%s' expects one parameter: brickset filepath. Did you forget to double quote the brickset filepath?", command_name);

        break;
    }

    case LEVCOMMAND_BGTHEME: {
        if(param_count >= 1)
            str_cpy(bgtheme, param[0], sizeof(bgtheme));
        else
            logfile_message("Level loader - command '%s' expects one parameter: background filepath. Did you forget to double quote the background filepath?", command_name);

        break;
    }

    case LEVCOMMAND_GROUPTHEME: /* deprecated */ {
        if(param_count >= 1)
            str_cpy(grouptheme, param[0], sizeof(grouptheme));
        else
            logfile_message("Level loader - command '%s' expects one parameter: grouptheme filepath. Did you forget to double quote the grouptheme filepath?", command_name);

        break;
    }

    case LEVCOMMAND_MUSIC: {
        if(param_count == 1) {
            str_cpy(musicfile, param[0], sizeof(musicfile));
            music_repeat_start = 0.0;
        }
        else if(param_count >= 2) {
            str_cpy(musicfile, param[0], sizeof(musicfile));
            music_repeat_start = max(atof(param[1]), 0.0);
        }
        else
            logfile_message("Level loader - command '%s' expects parameters: filepath [, loop_point]", command_name);

        break;
    }

    case LEVCOMMAND_WATERLEVEL: {
        if(param_count >= 1) {
            int waterlevel = atoi(param[0]);
            level_set_waterlevel(waterlevel);
        }
        else
            logfile_message("Level loader - command '%s' expects one parameter: y coordinate", command_name);

        break;
    }

    case LEVCOMMAND_WATERCOLOR: {
        if(param_count == 3) {
            level_set_watercolor(color_rgba(
                clip(atoi(param[0]), 0, 255),
                clip(atoi(param[1]), 0, 255),
                clip(atoi(param[2]), 0, 255),
                128
            ));
        }
        else if(param_count >= 4) {
            level_set_watercolor(color_rgba(
                clip(atoi(param[0]), 0, 255),
                clip(atoi(param[1]), 0, 255),
                clip(atoi(param[2]), 0, 255),
                clip(atoi(param[3]), 0, 255)
            ));
        }
        else
            logfile_message("Level loader - command '%s' expects parameters: red, green, blue [, alpha]", command_name);

        break;
    }

    case LEVCOMMAND_SPAWNPOINT: {
        if(param_count >= 2) {
            int x = atoi(param[0]);
            int y = atoi(param[1]);
            spawn_point = v2d_new(x, y);
        }
        else
            logfile_message("Level loader - command '%s' expects two parameters: xpos, ypos", command_name);

        break;
    }

    case LEVCOMMAND_PLAYERS: {
        if(team_size == 0) {
            if(param_count > 0) {
                for(int i = 0; i < param_count; i++) {
                    if(team_size < TEAM_MAX) {
                        for(int j = 0; j < team_size; j++) {
                            if(strcmp(player_name(team[j]), param[i]) == 0)
                                fatal_error("Level loader - duplicate player entry '%s' in '%s' near line %d", param[i], filepath, fileline);
                        }

                        logfile_message("Loading player '%s'...", param[i]);
                        team[team_size] = player_create(team_size, param[i]);
                        team_size++;
                    }
                    else {
                        logfile_message("Level loader - can't have more than %d players per level in '%s' near line %d", TEAM_MAX, filepath, fileline);
                        video_showmessage("Level loader - can't have more than %d players per level in '%s' near line %d", TEAM_MAX, filepath, fileline);
                        break;
                    }
                }
            }
            else
                logfile_message("Level loader - command '%s' expects one or more parameters: character_name1 [, character_name2 [, ... [, character_nameN] ... ] ]", command_name);
        }
        else
            logfile_message("Level loader - duplicate command '%s' on line %d. Ignoring...", command_name, fileline);

        break;
    }

    case LEVCOMMAND_SETUP: {
        if(is_setup_object_list_empty()) {
            if(param_count > 0) {
                for(int i = 0; i < param_count; i++)
                    add_to_setup_object_list(param[i]);
            }
            else
                logfile_message("Level loader - command '%s' expects one or more parameters: object_name1 [, object_name2 [, ... [, object_nameN] ... ] ]", command_name);
        }
        else
            logfile_message("Level loader - duplicate command '%s' on line %d. Ignoring...", command_name, fileline);

        break;
    }

    case LEVCOMMAND_DIALOGBOX: {
        if(param_count >= 6) {
            if(dialogregion_size < DIALOGREGION_MAX) {
                dialogregion_t *d = &(dialogregion[dialogregion_size++]);
                d->disabled = FALSE;
                d->rect_x = atoi(param[0]);
                d->rect_y = atoi(param[1]);
                d->rect_w = atoi(param[2]);
                d->rect_h = atoi(param[3]);
                str_cpy(d->title, param[4], sizeof(d->title));
                str_cpy(d->message, param[5], sizeof(d->message));
            }
            else
                logfile_message("Level loader - command '%s' has reached %d repetitions.", command_name, dialogregion_size);
        }
        else
            logfile_message("Level loader - command '%s' expects six parameters: rect_xpos, rect_ypos, rect_width, rect_height, title, message. Did you forget to double quote the message?", command_name);

        break;
    }

    case LEVCOMMAND_BRICK:
        brick_reserve_placement_id(bsl_brick_parse_uid(param_count,param));
        break;
    case LEVCOMMAND_INSTANCEPARAM:
    case LEVCOMMAND_EVENTBIND:
    case LEVCOMMAND_LEGACYOBJECT:
    case LEVCOMMAND_LEGACYITEM:
        /* skip the body of the file */
        break;

    default:
        /* warn, but do not crash - forward compatibility? */
        logfile_message("Level loader - unknown command '%s' in '%s' near line %d", command_name, filepath, fileline);
        break;
    }

    /* continue reading */
    return true;
}

/*
 * level_interpret_body_line()
 * Interprets a line of the body of the .lev file
 */
bool level_interpret_body_line(const char* filepath, int fileline, levparser_command_t command, const char* command_name, int param_count, const char** param, void* data)
{
    bsl_presentation_loading_pump();
    switch(command) {
    case LEVCOMMAND_BRICK: {
        if(param_count >= 3) {
            if(*theme != '\0') {
                bricklayer_t layer = BRL_DEFAULT;
                brickflip_t flip = BRF_NOFLIP;
                int id = atoi(param[0]);
                const char* definition=bsl_brick_parse_definition(param_count,param);
                if(definition){id=brick_resolve_definition(definition);if(id<0)fatal_error("Missing brick definition: %s",definition);}
                int x = atoi(param[1]);
                int y = atoi(param[2]);

                for(int j = 3; j < param_count; j++) {
                    if(layer == BRL_DEFAULT && brick_util_layercode(param[j]) != BRL_DEFAULT)
                        layer = brick_util_layercode(param[j]);
                    else if(flip == BRF_NOFLIP && brick_util_flipcode(param[j]) != BRF_NOFLIP)
                        flip = brick_util_flipcode(param[j]);
                }

                if(brick_exists(id)) {
                    uint64_t uid=bsl_brick_parse_uid(param_count,param);
                    if(uid&&bsl_brick_by_uid(uid))fatal_error("Duplicate brick placement ID: %016llx",(unsigned long long)uid);
                    brick_t* placed=bsl_brick_create(id,v2d_new(x,y),layer,flip,uid);
                    bool brightness_seen=false,texture_seen=false,z_seen=false;
                    for(int j=3;j<param_count;j++){
                        if(!strncmp(param[j],"brightness:",11)){
                            char* end;long value=strtol(param[j]+11,&end,10);
                            if(brightness_seen||end==param[j]+11||*end||value<0||value>15)fatal_error("Invalid brick brightness");
                            brick_set_brightness(placed,value);brightness_seen=true;
                        }else if(!strncmp(param[j],"z:",2)){
                            char* end;long value=strtol(param[j]+2,&end,10);
                            if(z_seen||end==param[j]+2||*end||value< -1||value>100)fatal_error("Invalid brick Z override");
                            brick_set_z_override(placed,(int)value);z_seen=true;
                        }else if(!strncmp(param[j],"texture:",8)){
                            const char* id=param[j]+8;
                            if(texture_seen||!brick_set_texture(placed,!strcmp(id,"default")?"":id))fatal_error("Invalid/missing brick texture: %s",id);
                            texture_seen=true;
                        }
                    }
                }
                else
                    logfile_message("Level loader - invalid brick: %d", id);
            }
            else
                logfile_message("Level loader - warning: cannot create a new brick if the theme is not defined");
        }
        else
            logfile_message("Level loader - command '%s' expects three, four or five parameters: id, xpos, ypos [, layer_name [, flip_flags]]", command_name);

        break;
    }

    case LEVCOMMAND_ENTITY: {
        if(param_count >= 3) {
            const char* name = param[0];
            int x = atoi(param[1]);
            int y = atoi(param[2]);
            if(!is_setup_object(name)) {
                surgescript_object_t* obj = level_create_object(name, v2d_new(x, y));
                if(obj != NULL) {
                    if(!surgescript_object_has_tag(obj, "entity"))
                        fatal_error("Level loader - can't spawn \"%s\": object is not an entity", name);
                    else if(param_count > 3 && entity_info_exists(obj)) {
                        surgescript_object_t* existing = level_get_entity_by_id(param[3]);
                        if(existing && existing != obj) fatal_error("Placement ID already occupied: %s", param[3]);
                        entity_info_set_id(obj, str_to_x64(param[3]));
                    }
                }
                else {
                    logfile_message("Level loader - can't spawn \"%s\": entity doesn't exist", name);
                    video_showmessage("Entity \"%s\" doesn't exist!", name);
                }
            }
        }
        else
            logfile_message("Level loader - command '%s' expects three or four parameters: name, xpos, ypos [, id]", command_name);

        break;
    }

    case LEVCOMMAND_INSTANCEPARAM:
        bsl_instance_load(param_count, param);
        break;

    case LEVCOMMAND_EVENTBIND:
        bsl_event_load(param_count, param);
        break;

    case LEVCOMMAND_LEGACYOBJECT: {
        if(param_count >= 3) {
            const char* name = param[0];
            int x = atoi(param[1]);
            int y = atoi(param[2]);
            if(!is_setup_object(name)) {
                surgescript_object_t* obj = level_create_object(name, v2d_new(x, y));
                if(obj != NULL) {
                    if(!surgescript_object_has_tag(obj, "entity"))
                        fatal_error("Level loader - can't spawn \"%s\": object is not an entity", name);
                }
                else if(enemy_exists(name)) {
                    enemy_t* e = level_create_legacy_object(name, v2d_new(x, y)); /* old API */
                    e->created_from_editor = TRUE;
                }
                else {
                    logfile_message("Level loader - can't spawn \"%s\": object doesn't exist", name);
                    video_showmessage("Entity \"%s\" doesn't exist!", name);
                }
            }
        }
        else
            logfile_message("Level loader - command '%s' expects three parameters: name, xpos, ypos", command_name);

        break;
    }

    case LEVCOMMAND_LEGACYITEM: {
        if(param_count >= 3) {
            int type = atoi(param[0]);
            int x = atoi(param[1]);
            int y = atoi(param[2]);
            surgescript_object_t* object = NULL;
            const char* object_name = item2surgescript(type); /* legacy item ported to SurgeScript? */
            if(object_name != NULL && (object = level_create_object(object_name, v2d_new(x, y))) != NULL) {
                /* force this flag, so the port gets persisted */
                entity_info_set_persistent(object, true);
            }
            else
                level_create_legacy_item(type, v2d_new(x, y)); /* no; create legacy item */
        }
        else
            logfile_message("Level loader - command '%s' expects three parameters: type, xpos, ypos", command_name);

        break;
    }

    default:
        /* skip the header */
        break;
    }

    /* continue reading */
    return true;
}

/*
 * level_save_ssobject()
 * Writes an object declaration to a level file
 */
bool level_save_ssobject(surgescript_object_t* object, void* param)
{
    ALLEGRO_FILE* fp = (ALLEGRO_FILE*)param;

    if(surgescript_object_is_killed(object))
        return false;

    if(surgescript_object_has_tag(object, "entity")) {
        if(entity_info_is_persistent(object)) {
            const char* object_name = surgescript_object_name(object);
            v2d_t spawn_point = entity_info_spawnpoint(object);
            uint64_t entity_id = entity_info_id(object);

            al_fprintf(fp, "entity \"%s\" %d %d \"%s\"\n", str_addslashes(object_name, NULL, 0), (int)spawn_point.x, (int)spawn_point.y, x64_to_str(entity_id, NULL, 0));
            bsl_instance_save(object, fp);
        }
    }

    return true;
}



/* scene functions */

/*
 * level_init()
 * Initializes the scene
 */
void level_init(void *path_to_lev_file)
{
    const char *filepath = (const char*)path_to_lev_file;

    logfile_message("level_init()");
    bsl_presentation_loading_begin();
    video_display_loading_screen();

    /* initialize variables */
    player = NULL;
    music = NULL;
    backgroundtheme = NULL;
    quit_level_img = NULL;

    level_timer = 0.0f;
    quit_level = FALSE;
    must_load_another_level = FALSE;
    must_restart_this_level = FALSE;
    must_push_a_quest = FALSE;
    must_quit_with_gameover = FALSE;
    must_render_brick_masks = FALSE;
    must_display_gizmos = FALSE;
    dead_player_timeout = 0.0f;

    level_cleared = FALSE;
    jump_to_next_stage = FALSE;
    wants_to_leave = FALSE;
    wants_to_pause = FALSE;

    /* cached objects */
    cached_level_ssobject = NULL;
    cached_entity_manager = NULL;

    /* create the brick manager */
    brick_manager = brickmanager_create();

    /* dialog box */
    dlgbox_active = FALSE;
    dlgbox_starttime = 0;
    dlgbox = actor_create();
    dlgbox->position.y = VIDEO_SCREEN_H;
    actor_change_animation(dlgbox, sprite_get_animation("Message Box", 0));
    dlgbox_title = font_create("dialogbox");
    dlgbox_message = font_create("dialogbox");

    /* render queue */
    bool want_depth_buffer = (video_get_quality() < VIDEOQUALITY_MEDIUM);
    renderqueue_init(want_depth_buffer);

    /* helpers */
    clear_level_state(&saved_state);
    mobilegamepad_fadein();

    audio_muffler_activate(MUFFLE_NOTHING);
    camera_init();
    entitymanager_init();
    create_obstaclemap();

    /* load level file */
    level_load(filepath);
    bsl_cell_begin(filepath);
    bsl_shell_level_begin();

    /* editor */
    editor_init();

    /* event listeners */
    engine_add_event_listener(ALLEGRO_EVENT_DISPLAY_SWITCH_OUT, NULL, handle_switchout_event);

    /* immersive mode */
    was_immersive = video_is_immersive();
    video_set_immersive(true); /* enable immersive mode during gameplay */

    /* done! */
    bsl_presentation_loading_end();
    logfile_message("level_init() ok");
}



/*
 * level_release()
 * Releases the scene
 */
void level_release()
{
    bsl_shell_detach();
    logfile_message("level_release()");

    /* save prefs */
    extern prefs_t* prefs;
    prefs_save(prefs);

    /* immersive mode */
    video_set_immersive(was_immersive);

    /* event listeners */
    if(!engine_remove_event_listener(ALLEGRO_EVENT_DISPLAY_SWITCH_OUT, NULL, handle_switchout_event))
        logfile_message("Can't remove event listener: switch out");

    /* release the editor */
    editor_release();

    /* Release native UI ownership before actors and scripts are destroyed. */
    bsl_host_shutdown();

    /* unload the level and its scripts */
    level_unload();

    /* dialog box */
    font_destroy(dlgbox_title);
    font_destroy(dlgbox_message);
    actor_destroy(dlgbox);

    /* release the brick manager and all bricks */
    logfile_message("Releasing the brick manager...");
    brick_manager = brickmanager_destroy(brick_manager);

    /* quit level img */
    if(quit_level_img != NULL)
        image_destroy(quit_level_img);

    /* release the helpers */
    cached_level_ssobject = NULL;
    cached_entity_manager = NULL;

    destroy_obstaclemap();
    bsl_shell_level_end();
    bsl_cell_end();
    entitymanager_release();
    camera_release();
    audio_muffler_activate(MUFFLE_NOTHING);

    clear_level_state(&saved_state);

    /* render queue */
    renderqueue_release();

    /* deinitialize the fields */
    strcpy(file, "");
    strcpy(name, ""); /* scripting: Level.name may be accessed on Application.onExit */
    strcpy(musicfile, "");
    strcpy(theme, "");
    strcpy(bgtheme, "");
    strcpy(author, "");
    strcpy(version, "");
    strcpy(license, "");
    strcpy(grouptheme, "");
    spawn_point = v2d_new(0, 0);
    act_number = 0;

    /* done! */
    logfile_message("level_release() ok");
}





/*
 * level_update()
 * Updates the scene (this one runs
 * every cycle of the program)
 */
void level_update()
{
    brick_surface_test_tick();
    int i, cbox;
    bool got_dying_player = false;
    float dt = timer_get_delta();
    brick_list_t *major_bricks;
    item_list_t *major_items, *inode;
    enemy_list_t *major_enemies, *enode;
    v2d_t cam = level_editmode() ? editor_camera : camera_get_position();
    (void)dt;

    if(editorcmd_is_triggered(editor_cmd,"toggle-collision-role"))brick_toggle_role_visible(BRR_COLLISION,level_editmode());
    if(editorcmd_is_triggered(editor_cmd,"toggle-decoration-role"))brick_toggle_role_visible(BRR_DECORATION,level_editmode());
    if(bsl_terrain_test_poll()) return;
    if(bsl_event_test_poll()) return;
    if(bsl_instance_test_poll()) return;

    /* legacy: release entities */
    entitymanager_remove_dead_bricks();
    entitymanager_remove_dead_items();
    entitymanager_remove_dead_objects();

    /* next stage in the quest... */
    if(jump_to_next_stage) {
        jump_to_next_stage = FALSE;
        scenestack_pop();
        return;
    }

    /* must load another level? */
    if(must_load_another_level) {
        must_load_another_level = FALSE;
        restart(FALSE);
        return;
    }

    /* must restart the current level? */
    if(must_restart_this_level) {
        must_restart_this_level = FALSE;
        restart(TRUE);
        return;
    }

    /* must push a quest? */
    if(must_push_a_quest) {
        must_push_a_quest = FALSE;
        scenestack_pop();
        quest_set_next_level(quest_next_level() - 1); /* will return to the current level */
        scenestack_push(storyboard_get_scene(SCENE_QUEST), (void*)quest_to_be_pushed);
        return;
    }

    /* must quit the level and show the game over screen? */
    if(must_quit_with_gameover) {
        must_quit_with_gameover = FALSE;
        scenestack_pop();
        scenestack_push(storyboard_get_scene(SCENE_GAMEOVER), NULL);
        return;
    }

    /* Native UI updates independently while a menu pauses the world. Signed: Atlas. */
    if(!level_is_setup_object("BSL Presentation Only") && bsl_host_preupdate() && !(bsl_host_player_assembly_waiting() && editor_want_to_activate()))
        return;

    /* got a dying player? */
    for(i = 0; i < team_size && !got_dying_player; i++) {
        if(player_is_dying(team[i]) && !player_is_immortal(team[i]))
            got_dying_player = true;
    }

    /* update the brick manager */
    brickmanager_update(brick_manager);

    /* music */
    update_music();

    /* level editor */
    if(editor_is_enabled()) {
        editor_update();
        return;
    }

    /* should we quit due to scripting? */
    if(!surgescript_vm_is_active(surgescript_vm())) {
        engine_quit();
        return;
    }

    /* displaying message: "do you really want to quit?" */
    bool block_quit = got_dying_player;
    if(wants_to_leave && !block_quit) {
        confirmboxdata_t cbd = { "$QUIT_QUESTION", "$QUIT_OPTION1", "$QUIT_OPTION2", 2 };

        if(quit_level_img != NULL)
            image_destroy(quit_level_img);
        quit_level_img = video_take_snapshot();

        wants_to_leave = FALSE;
        music_pause();
        audio_muffler_activate(MUFFLE_NOTHING);
        scenestack_push(storyboard_get_scene(SCENE_CONFIRMBOX), &cbd);
        return;
    }

    cbox = confirmbox_selected_option();
    if(cbox == 1)
        quit_level = TRUE;
    else if(cbox == 2)
        music_resume();

    if(quit_level) {
        music_stop();
        if(fadefx_is_over()) {
            scenestack_pop();
            quest_abort();
            return;
        }
        fadefx_out(color_rgb(0,0,0), 1.0);
        return;
    }

    /* pause game */
    bool block_pause = got_dying_player || (level_timer < 1.0f);
    if(wants_to_pause && !block_pause) {
        wants_to_pause = FALSE;
        audio_muffler_activate(MUFFLE_NOTHING);
        scenestack_push(storyboard_get_scene(SCENE_PAUSE), NULL);
        return;
    }

    /* open level editor */
    if(editor_want_to_activate()) {
        if(readonly || !bsl_host_level_editable()) {
            video_showmessage("No way!");
            sound_play(SFX_DENY);
        }
        else if(!level_is_in_debug_mode()) {
            editor_enable();
            return;
        }
    }

    /* enable the debug mode */
    if(debug_mode_want_to_activate())
        level_enter_debug_mode();

    /* -------------------------------------- */
    /* updating the entities */
    /* -------------------------------------- */

    /* update background */
    background_update(backgroundtheme);

    /* update water */
    waterfx_update();

    /* legacy camera code (runs before scripts) */
    if(!level_is_in_debug_mode()) {
        if(level_cleared)
            camera_move_to(v2d_add(camera_focus->position, v2d_new(0, -90)), 0.17);
        else if(!got_dying_player)
            camera_move_to(camera_focus->position, 0.0f); /* the camera will be locked on its focus (usually, the player) */
    }

    /* getting the major entities */
    rect_t brick_roi = create_roi(cam, ROI_MARGIN_UPDATE_BRICK);
    rect_t entity_roi = create_roi(cam, ROI_MARGIN_UPDATE_ENTITY);

    brickmanager_set_roi(brick_manager, brick_roi);
    set_entitymanager_roi(entity_roi);
    entitymanager_set_active_region(entity_roi); /* legacy */

    major_enemies = entitymanager_retrieve_active_objects();
    major_items = entitymanager_retrieve_active_items();
    major_bricks = major_enemies != NULL || major_items != NULL ? brickmanager_retrieve_active_bricks_as_list(brick_manager) : NULL; /* for backwards compatibility only */

    /* update legacy items */
    for(inode = major_items; inode != NULL; inode = inode->next) {
        float x = inode->data->actor->position.x;
        float y = inode->data->actor->position.y;
        float w = image_width(actor_image(inode->data->actor));
        float h = image_height(actor_image(inode->data->actor));
        int inside_playarea = inside_screen(x, y, w, h, DEFAULT_MARGIN);
        int always_active = inode->data->always_active;

        if(inside_playarea || always_active) {
            /* update this item */
            item_update(inode->data, team, team_size, major_bricks, major_items, major_enemies);
        }
        else if(!inside_playarea) {
            /* this item is outside the screen... (and it's not always active) */
            if(!inode->data->preserve)
                inode->data->state = IS_DEAD;
            else if(!inside_screen(inode->data->actor->spawn_point.x, inode->data->actor->spawn_point.y, w, h, DEFAULT_MARGIN))
                inode->data->actor->position = inode->data->actor->spawn_point;
        }
    }

    /* update legacy objects */
    for(enode = major_enemies; enode != NULL; enode = enode->next) {
        float x = enode->data->actor->position.x;
        float y = enode->data->actor->position.y;
        float w = image_width(actor_image(enode->data->actor));
        float h = image_height(actor_image(enode->data->actor));
        int always_active = enode->data->always_active;
        int inside_playarea = inside_screen(x, y, w, h, DEFAULT_MARGIN);

        if(inside_playarea || always_active) {
            /* update this object */
            enemy_update(enode->data, team, team_size, major_bricks, major_items, major_enemies);
        }
        else if(!inside_playarea) {
            /* this object is outside the play area... (and it's not always active) */
            if(!enode->data->preserve)
                enode->data->state = ES_DEAD;
            else if(!inside_screen(enode->data->actor->spawn_point.x, enode->data->actor->spawn_point.y, w, h, DEFAULT_MARGIN))
                enode->data->actor->position = enode->data->actor->spawn_point;
        }
    }

    /* update bricks */
    iterator_t* brick_iterator = brickmanager_retrieve_active_moving_bricks(brick_manager);
    while(iterator_has_next(brick_iterator)) {
        /* no need to update static bricks.
           We won't even retrieve them! */
        brick_t* brick = iterator_next(brick_iterator);
        brick_update(brick, team, team_size);
    }
    iterator_destroy(brick_iterator);

    /* early update: players */
    if(brickmanager_number_of_bricks(brick_manager) > 0) {
        for(i = 0; i < team_size; i++)
            player_early_update(team[i]);
    }

    /* update the obstacle map */
    update_obstaclemap(major_items, major_enemies);

    /* update scripts */
    update_ssobjects();

    /* update the obstacle map again after updating the scripts */
    if(is_obstaclemap_dirty || scripting_bor_tick_pending()) {
        update_obstaclemap(major_items, major_enemies);
        is_obstaclemap_dirty = false;
    }

    scripting_update_bor();

    /* update players */
    if(brickmanager_number_of_bricks(brick_manager) > 0) {
        for(i = 0; i < team_size; i++) {
            v2d_t cam = camera_get_position();
            v2d_t pos = player_position(team[i]);
            rect_t box = player_bounding_box(team[i]);
            rect_t roi = create_roi(cam, ROI_MARGIN_UPDATE_PLAYER);

            /* updating... */
            if(rect_overlaps(roi, box) || player_is_dying(team[i]) || pos.y < 0) {
                if(!got_dying_player || player_is_dying(team[i]) || player_is_getting_hit(team[i]))
                    player_update(team[i], obstaclemap);
            }
        }
    }

    /* some objects are attached to the player... */
    for(enode = major_enemies; enode != NULL; enode = enode->next) {
        float x = enode->data->actor->position.x;
        float y = enode->data->actor->position.y;
        float w = image_width(actor_image(enode->data->actor));
        float h = image_height(actor_image(enode->data->actor));
        int always_active = enode->data->always_active;
        int inside_playarea = inside_screen(x, y, w, h, DEFAULT_MARGIN);

        if(inside_playarea || always_active) {
            if(enode->data->attached_to_player) {
                enode->data->actor->position = enemy_get_observed_player(enode->data)->actor->position;
                enode->data->actor->position = v2d_add(enode->data->actor->position, enode->data->attached_to_player_offset);
                enode->data->attached_to_player = FALSE;
            }
        }
    }

    /* update camera */
    camera_update();

    /* scripting: late update */
    late_update_ssobjects();

    /* update dialog box */
    update_dialogregions();
    update_dlgbox();

    /* level timer */
    if(!got_dying_player && !level_cleared)
        level_timer += timer_get_delta();

    /* release major entities */
    major_bricks = brickmanager_release_list(major_bricks);
    major_items = entitymanager_release_retrieved_item_list(major_items);
    major_enemies = entitymanager_release_retrieved_object_list(major_enemies);
}



/*
 * level_render()
 * Rendering function
 */
void level_render()
{
    item_list_t *major_items;
    enemy_list_t *major_enemies;

    /* very important (if we restart the level) */
    if(level_timer < 0.05f)
        return;

    /* quit... */
    if(quit_level) {
        image_blit(quit_level_img, 0, 0, 0, 0, image_width(quit_level_img), image_height(quit_level_img));
        return;
    }

    /* render the level editor? */
    if(editor_is_enabled()) {
        editor_render();
        return;
    }

    /* --------------------------- */

    /* FIX: the camera position may have been changed since the last time
       entitymanager_set_active_region() was called (i.e., via scripting).
       Let's make sure that we keep our active region updated. */
    v2d_t cam = camera_get_position(); /* we're not in editor mode */
    rect_t brick_roi = create_roi(cam, ROI_MARGIN_RENDER_BRICK);
    rect_t entity_roi = create_roi(cam, ROI_MARGIN_RENDER_ENTITY);

    brickmanager_set_roi(brick_manager, brick_roi); /* this call is cheap */
    set_entitymanager_roi(entity_roi); /* this call may be expensive if the ROI has changed */
    entitymanager_set_active_region(entity_roi); /* legacy */

    /* retrieve lists of active entities */
    major_items = entitymanager_retrieve_active_items();
    major_enemies = entitymanager_retrieve_active_objects();

    /* clear the screen */
    image_rectfill(0, 0, VIDEO_SCREEN_W, VIDEO_SCREEN_H, color_rgb(0,0,0));

    /* render level */
    render_level(major_items, major_enemies);

    /* render the built-in HUD */
    render_hud();

    /* release lists of active entites */
    major_items = entitymanager_release_retrieved_item_list(major_items);
    major_enemies = entitymanager_release_retrieved_object_list(major_enemies);

    /* Retained gameplay services belong to world scenes. Signed: President. */
    if(!level_is_setup_object("BSL Presentation Only")) {
        bsl_event_render();
        bsl_host_render();
    }
}


/*
 * level_file()
 * Returns the relative path of the level file
 */
const char* level_file()
{
    return file;
}

/*
 * level_name()
 * Returns the level name
 */
const char* level_name()
{
    return name;
}

/*
 * level_version()
 * Returns the version of the level
 */
const char* level_version()
{
    return version;
}

/*
 * level_author()
 * Returns the author of the level
 */
const char* level_author()
{
    return author;
}

/*
 * level_license()
 * Returns the level license
 */
const char* level_license()
{
    return license;
}

/*
 * level_act()
 * Returns the current act number
 */
int level_act()
{
    return act_number;
}

/*
 * level_bgtheme()
 * Returns the relative path to the original background of the level
 */
const char* level_bgtheme()
{
    return bgtheme;
}

/*
 * level_player()
 * Returns the current player
 */
player_t* level_player()
{
    return player;
}

/*
 * level_persist()
 * Persists (saves) the current level
 * Returns TRUE on success
 */
int level_persist()
{
    return level_save(file);
}

/*
 * level_change()
 * Changes the level.
 * Make sure that 'level' is the current scene.
 */
void level_change(const char* path_to_lev_file)
{
    str_cpy(file, path_to_lev_file, sizeof(file));
    must_load_another_level = TRUE;
    logfile_message("Changing level to '%s'...", path_to_lev_file);
}


/*
 * level_change_player()
 * Changes the current player (character switching)
 */
void level_change_player(player_t *new_player)
{
    int player_id = -1;

    if(player != NULL) {
        if(team_size > 1 && !player_is_focusable(new_player))
            return;
    }

    for(int i = 0; i < team_size && player_id < 0; i++)
        player_id = (team[i] == new_player) ? i : player_id;
    
    if(player_id >= 0) {
        player = team[player_id];
        level_set_camera_focus(player->actor);
        reconfigure_players_input_devices();
    }
}

/*
 * level_get_player_by_name()
 * Get a player by its name
 * Returns NULL if there is no such player
 */
player_t* level_get_player_by_name(const char* name)
{
    /* simple search */
    for(int i = 0; i < team_size; i++) {
        if(strcmp(player_name(team[i]), name) == 0)
            return team[i];
    }

    /* not found */
    return NULL;
}

/*
 * level_get_player_by_id()
 * Get a player by its ID
 * Returns NULL if there is no such player
 */
player_t* level_get_player_by_id(int id)
{
    if(id >= 0 && id < team_size) {
        assertx(team[id]->id == id); /* the index in team[] matches the id */
        return team[id];
    }
    else
        return NULL;
}

/*
 * level_create_brick()
 * Creates and adds a brick to the level. This function
 * returns a pointer to the created brick.
 */
brick_t* level_create_brick(int id, v2d_t position, bricklayer_t layer, brickflip_t flip)
{
    brick_t *brick = brick_create(id, position, layer, flip);
    brickmanager_add_brick(brick_manager, brick);
    return brick;
}



/*
 * level_create_legacy_item()
 * Creates and adds a legacy item to the level.
 * Returns the legacy item.
 */
item_t* level_create_legacy_item(int id, v2d_t position)
{
    item_t *item = item_create(id);
    item->actor->spawn_point = position;
    item->actor->position = position;
    entitymanager_store_item(item);

    /* deprecation warning */
    video_showmessage("Legacy item %d will not be supported!", id);
    logfile_message("Legacy item %d will not be supported!", id);

    return item;
}



/*
 * level_create_legacy_object()
 * Creates and adds a legacy object to the level.
 * Returns the legacy object.
 */
enemy_t* level_create_legacy_object(const char *name, v2d_t position)
{
    enemy_t *object = enemy_create(name);
    object->actor->spawn_point = position;
    object->actor->position = position;
    object->created_from_editor = (name[0] != '.');
    entitymanager_store_object(object);

    /* deprecation warning */
    video_showmessage("Legacy \"%s\" will not be supported!", name);
    logfile_message("Legacy \"%s\" will not be supported!", name);

    return object;
}


/*
 * level_create_object()
 * Creates a SurgeScript object and adds it to the level.
 * Returns the new object, or NULL if the object couldn't be found
 */
surgescript_object_t* level_create_object(const char* object_name, v2d_t position)
{
    return spawn_ssobject(object_name, position);
}

/*
 * level_get_entity_by_id()
 * Gets a SurgeScript entity by its ID, a 64-bit hex-string
 * Returns NULL if no such entity is found
 */
surgescript_object_t* level_get_entity_by_id(const char* entity_id)
{
    surgescript_object_t* level = level_ssobject();
    surgescript_objectmanager_t* manager = surgescript_object_manager(level);

    uint64_t id = str_to_x64(entity_id);
    surgescript_object_t* entity_manager = entitymanager_ssobject();
    surgescript_objecthandle_t handle = entitymanager_find_entity_by_id(entity_manager, id);

    if(!surgescript_objectmanager_exists(manager, handle)) /* the object may have been deleted */
        return NULL; /* not found */

    /* success! */
    return surgescript_objectmanager_get(manager, handle);
}


/*
 * level_get_entity_id()
 * Gets the ID, a 64-bit hex-string, of the given entity
 * If no ID exists, an empty string is returned
 */
const char* level_get_entity_id(const surgescript_object_t* entity)
{
    static char buf[17];

    if(entity_info_exists(entity))
        return x64_to_str(entity_info_id(entity), buf, sizeof(buf));

    return "";
}


/*
 * level_child_object()
 * Get a direct child of the SurgeScript Level object with the given name
 * Returns NULL if no such object exists
 */
surgescript_object_t* level_child_object(const char* object_name)
{
    surgescript_vm_t* vm = surgescript_vm();
    surgescript_objectmanager_t* manager = surgescript_vm_objectmanager(vm);
    surgescript_object_t* level = level_ssobject();
    surgescript_objecthandle_t child = surgescript_object_child(level, object_name);

    if(surgescript_objectmanager_exists(manager, child))
        return surgescript_objectmanager_get(manager, child);

    return NULL;
}

/*
 * level_is_setup_object()
 * Checks if the given object name belongs to the setup list
 */
bool level_is_setup_object(const char* object_name)
{
    return is_setup_object(object_name);
}

/*
 * level_obstaclemap()
 * The obstacle map featuring the bricks and brick-like objects that are
 * placed near the camera of the level.
 */
const obstaclemap_t* level_obstaclemap()
{
    assertx(obstaclemap != NULL); /* will be NULL if the level is not loaded */
    return obstaclemap;
}

/* Bounded read-only checkpoint terrain query, independent of the active camera ROI.
 * Caller owns the context and its bricklike masks. Signed: Atlas. */
struct checkpoint_terrain_t {
    obstaclemap_t* map;
    DARRAY(obstacle_t*, owned)
    int left, right;
};
static bool checkpoint_intersects(const obstacle_t* obstacle, v2d_t center)
{
    point2d_t p = obstacle_get_position(obstacle);
    return p.x <= center.x + 512 && p.x + obstacle_get_width(obstacle) >= center.x - 512 &&
           p.y <= center.y + 512 && p.y + obstacle_get_height(obstacle) >= center.y - 512;
}
static void checkpoint_add(struct checkpoint_terrain_t* query, const obstacle_t* obstacle)
{
    point2d_t p = obstacle_get_position(obstacle);
    if(p.x < query->left) query->left = p.x;
    if(p.x + obstacle_get_width(obstacle) > query->right) query->right = p.x + obstacle_get_width(obstacle);
    obstaclemap_add(query->map, obstacle);
}
void level_checkpoint_terrain_destroy(struct checkpoint_terrain_t* query)
{
    if(query == NULL) return;
    obstaclemap_destroy(query->map);
    for(int i = 0; i < darray_length(query->owned); i++) obstacle_destroy(query->owned[i]);
    darray_release(query->owned);
    free(query);
}
struct checkpoint_terrain_t* level_checkpoint_terrain_create(v2d_t center)
{
    struct checkpoint_terrain_t* query = mallocx(sizeof *query);
    query->map = obstaclemap_create(); query->left = (int)center.x; query->right = (int)center.x;
    darray_init(query->owned);
    iterator_t* it = brickmanager_retrieve_all_bricks(brick_manager);
    while(iterator_has_next(it)) {
        const obstacle_t* obstacle = brick_obstacle(iterator_next(it));
        if(obstacle != NULL && checkpoint_intersects(obstacle, center)) checkpoint_add(query, obstacle);
    }
    iterator_destroy(it);
    const surgescript_objectmanager_t* manager = surgescript_object_manager(level_ssobject());
    it = entitymanager_bricklike_iterator(entitymanager_ssobject());
    while(iterator_has_next(it)) {
        surgescript_objecthandle_t handle = *(surgescript_objecthandle_t*)iterator_next(it);
        if(!surgescript_objectmanager_exists(manager, handle)) continue;
        surgescript_object_t* object = surgescript_objectmanager_get(manager, handle);
        if(!scripting_brick_is_valid(object) || !scripting_brick_enabled(object)) continue;
        obstacle_t* obstacle = bricklike2obstacle(object);
        if(checkpoint_intersects(obstacle, center)) {
            darray_push(query->owned, obstacle); checkpoint_add(query, obstacle);
        }
        else obstacle_destroy(obstacle);
    }
    iterator_destroy(it);
    /* This engine map clamps wide bucket spans. Refuse unknown coverage, never assume clear. */
    if(query->right - query->left > 12000) { level_checkpoint_terrain_destroy(query); return NULL; }
    bsl_cell_obstacles(query->map);
    obstaclemap_build(query->map);
    return query;
}
const obstaclemap_t* level_checkpoint_terrain_map(const struct checkpoint_terrain_t* query) { return query->map; }
void level_checkpoint_rebase(v2d_t center)
{
    camera_set_position(center);
    v2d_t at = camera_get_position();
    brickmanager_set_roi(brick_manager, create_roi(at, ROI_MARGIN_UPDATE_BRICK));
    rect_t entity_roi = create_roi(at, ROI_MARGIN_UPDATE_ENTITY);
    set_entitymanager_roi(entity_roi);
    entitymanager_set_active_region(entity_roi);
    update_obstaclemap(NULL, NULL);
    is_obstaclemap_dirty = false;
}

/*
 * level_set_obstaclemap_dirty()
 * Require another update of the obstacle map after updating the scripts
 */
/* Camera-independent read-only acoustic visitor. Borrowed masks never escape callback. Signed: Atlas. */
void bsl_level_acoustic_visit(void (*visit)(const obstacle_t*, void*), void* data)
{
    iterator_t* it = brickmanager_retrieve_all_bricks(brick_manager);
    while(iterator_has_next(it)) {
        const obstacle_t* obstacle = brick_obstacle(iterator_next(it));
        if(obstacle != NULL && obstacle_is_solid(obstacle)) visit(obstacle, data);
    }
    iterator_destroy(it);
    const surgescript_objectmanager_t* manager = surgescript_object_manager(level_ssobject());
    it = entitymanager_bricklike_iterator(entitymanager_ssobject());
    while(iterator_has_next(it)) {
        surgescript_objecthandle_t handle = *(surgescript_objecthandle_t*)iterator_next(it);
        if(!surgescript_objectmanager_exists(manager, handle)) continue;
        surgescript_object_t* object = surgescript_objectmanager_get(manager, handle);
        if(!scripting_brick_is_valid(object) || !scripting_brick_enabled(object)) continue;
        obstacle_t* obstacle = bricklike2obstacle(object);
        if(obstacle_is_solid(obstacle)) visit(obstacle, data);
        obstacle_destroy(obstacle);
    }
    iterator_destroy(it);
}
extern void bsl_acoustic_geometry_dirty(void);
void level_set_obstaclemap_dirty()
{
    is_obstaclemap_dirty = true;
    bsl_acoustic_geometry_dirty();
}


/*
 * level_add_to_score()
 * Adds a value to the player's score.
 * (It also creates a flying text that
 * shows that score)
 */
void level_add_to_score(int score)
{
    /* legacy item */
    item_t *flyingtext = level_create_legacy_item(IT_FLYINGTEXT, player->actor->position);
    flyingtext_set_text(flyingtext, "%d", score);
    player_set_score(player_get_score() + score);
}



/*
 * level_set_camera_focus()
 * Sets a new focus to the camera
 */
void level_set_camera_focus(actor_t *act)
{
    camera_focus = act;
}


/*
 * level_get_camera_focus()
 * Gets the actor being focused by the camera
 */
actor_t* level_get_camera_focus()
{
    return camera_focus;
}




/*
 * level_size()
 * Returns the size of the level
 */
v2d_t level_size()
{
    int level_width, level_height;

    brickmanager_world_size(brick_manager, &level_width, &level_height);

    return v2d_new(level_width, level_height);
}



/*
 * level_height_at()
 * The height of the level at a certain x-position
 */
int level_height_at(int xpos)
{
    if(bsl_cell_world_height())return bsl_cell_world_height();
    int half_window_size = VIDEO_SCREEN_W / 2;
    int left_xpos = xpos - half_window_size;
    int right_xpos = xpos + half_window_size;

    return brickmanager_world_height_at_interval(brick_manager, left_xpos, right_xpos);
}


/*
 * level_music()
 * The music of the level (may be NULL)
 */
music_t* level_music()
{
    return music;
}


/*
 * level_set_spawnpoint()
 * Defines a new spawn point
 * You need to call level_save_state() just
 * after calling this (if you're in gameplay)
 */
void level_set_spawnpoint(v2d_t newpos)
{
    spawn_point = newpos;
}


/*
 * level_spawnpoint()
 * Gets the spawn point
 */
v2d_t level_spawnpoint()
{
    return spawn_point;
}


/*
 * level_set_act()
 * Changes the act number
 */
void level_set_act(int new_act_number)
{
    act_number = clip(new_act_number, 0, MAX_ACT_NUMBER);
}


/*
 * level_clear()
 * Call this when the player clears this level
 * If end_sign is NULL, the camera will be focused on the active player
 */
void level_clear(actor_t *end_sign)
{
    if(level_cleared)
        return;

    /* disable player input */
    for(int i = 0; i < team_size; i++)
        input_disable(team[i]->actor->input);

    /* focus the camera on the end sign */
    if(end_sign != NULL)
        level_set_camera_focus(end_sign);
    else
        level_set_camera_focus(player->actor);

    /* hide dialog box, if any */
    level_hide_dialogbox();

    /* success! */
    level_cleared = TRUE;
}


/*
 * level_undo_clear()
 * Undo a previous call to level_clear()
 */
void level_undo_clear()
{
    if(!level_cleared)
        return;

    /* re-enable player input */
    for(int i = 0; i < team_size; i++)
        input_enable(team[i]->actor->input);

    /* restore the focus */
    level_set_camera_focus(player->actor);

    /* done! */
    level_cleared = FALSE;
}


/*
 * level_call_dialogbox()
 * Calls a dialog box
 */
void level_call_dialogbox(const char *title, const char *message)
{
    if(dlgbox_active && strcmp(font_get_text(dlgbox_title), title) == 0 && strcmp(font_get_text(dlgbox_message), message) == 0)
        return;

    dlgbox_active = TRUE;
    dlgbox_starttime = timer_get_elapsed() * 1000.0;
    font_set_text(dlgbox_title, "%s", title);
    font_set_text(dlgbox_message, "%s", message);
    font_set_width(dlgbox_message, image_width(actor_image(dlgbox)) - 14);
}



/*
 * level_hide_dialogbox()
 * Hides the current dialog box (if any)
 */
void level_hide_dialogbox()
{
    dlgbox_active = FALSE;
}


/*
 * level_inside_screen()
 * Returns TRUE if a given region is
 * inside the screen position (camera-related)
 */
int level_inside_screen(int x, int y, int w, int h)
{
    return inside_screen(x,y,w,h,DEFAULT_MARGIN);
}


/*
 * level_has_been_cleared()
 * Has this level been cleared?
 */
int level_has_been_cleared()
{
    return level_cleared;
}


/*
 * level_jump_to_next_stage()
 * Jumps to the next stage in the quest
 */
void level_jump_to_next_stage()
{
    jump_to_next_stage = TRUE;
}


/*
 * level_ask_to_leave()
 * Asks permission for the user to leave the level
 */
void level_ask_to_leave()
{
    wants_to_leave = TRUE;
}

/*
 * level_pause()
 * Pauses the level
 */
void level_pause()
{
    wants_to_pause = TRUE;
}

/*
 * level_restart()
 * Restarts the current level
 */
void level_restart()
{
    must_restart_this_level = TRUE; /* schedules restart */
}

/*
 * level_waterlevel()
 * Returns the level of the water (the y-coordinate where the water begins)
 */
int level_waterlevel()
{
    return waterfx_ypos();
}

/*
 * level_set_waterlevel()
 * Sets a new waterlevel
 */
void level_set_waterlevel(int ycoord)
{
    waterfx_set_ypos(ycoord);
}

/*
 * level_watercolor()
 * Returns the color of the water
 */
color_t level_watercolor()
{
    return waterfx_color();
}

/*
 * level_set_watercolor()
 * Sets a new watercolor
 */
void level_set_watercolor(color_t color)
{
    waterfx_set_color(color);
}

/*
 * level_push_quest()
 * Pops this level, pushes a new quest.
 * So, you'll have at least two quests on the scene stack,
 * stored in consecutive positions.
 */
void level_push_quest(const char* path_to_qst_file)
{
    /* schedules a quest push */
    must_push_a_quest = TRUE;
    str_cpy(quest_to_be_pushed, path_to_qst_file, sizeof(quest_to_be_pushed));
}

/*
 * level_abort()
 * Pops this level, and also pops the current quest (if any)
 */
void level_abort()
{
    /* schedules a quest pop */
    quest_abort();
    level_jump_to_next_stage();
}

/*
 * level_quit_with_gameover()
 * Quits the level and show the game over screen
 */
void level_quit_with_gameover()
{
    must_quit_with_gameover = TRUE;
}

/*
 * level_change_background()
 * Changes the background
 */
void level_change_background(const char* filepath)
{
    if(str_pathcmp(filepath, background_filepath(backgroundtheme)) != 0) {
        /* string bgtheme (original path) is untouched */
        logfile_message("Changing level background to \"%s\"...", filepath);
        background_unload(backgroundtheme);
        backgroundtheme = background_load(filepath);
    }
}

/*
 * level_background()
 * Returns the background that is currently in use
 */
const struct bgtheme_t* level_background()
{
    return backgroundtheme;
}

/*
 * level_time()
 * Elapsed time since the level has started, in seconds
 */
float level_time()
{
    return level_timer;
}

/*
 * level_gravity()
 * Gravity in px/s^2
 */
float level_gravity()
{
    return DEFAULT_GRAVITY;
}

/*
 * level_save_state()
 * Saves the state of the level (spawn point, waterlevel, etc.)
 * The next time the level is restarted, those properties will
 * be restored to what they were at the moment they were saved
 */
void level_save_state()
{
    save_level_state(&saved_state);
}



/*
 * level_editmode()
 * Is the level editor activated?
 */
int level_editmode()
{
    return editor_is_enabled();
}



/*
 * level_is_readonly()
 * Is the readonly flag enabled?
 */
bool level_is_readonly()
{
    return readonly;
}



/*
 * level_is_displaying_gizmos()
 * Are we displaying gizmos for visual debugging?
 * This may be activated/deactivated in the editor and
 * serves as a clue to objects in the code
 */
int level_is_displaying_gizmos()
{
    return must_display_gizmos;
}


/*
 * level_enter_debug_mode()
 * Enter the Debug Mode
 */
void level_enter_debug_mode()
{
    if(!level_is_in_debug_mode()) {

        if(readonly) {
            video_showmessage("No way!");
            sound_play(SFX_DENY);
            return;
        }

        surgescript_object_t* level = level_ssobject();
        surgescript_var_t* tmp = surgescript_var_create();
        const surgescript_var_t* param[] = { tmp };

        surgescript_var_set_bool(tmp, true);
        surgescript_object_call_function(level, "set_debugMode", param, 1, NULL);

        surgescript_var_destroy(tmp);
    }
}


/*
 * level_is_in_debug_mode()
 * Are we in Debug Mode?
 */
bool level_is_in_debug_mode()
{
    surgescript_object_t* level = level_ssobject();
    surgescript_var_t* ret = surgescript_var_create();
    bool flag = false;

    surgescript_object_call_function(level, "get_debugMode", NULL, 0, ret);
    flag = surgescript_var_get_bool(ret);

    surgescript_var_destroy(ret);
    return flag;
}




/*
 * Private functions
 */


/* renders the entities of the level: bricks, enemies, items, players, etc. */
void render_level(const item_list_t *major_items, const enemy_list_t *major_enemies)
{
    /* starting up the render queue... */
    renderqueue_begin( camera_get_position() );

        /* render the background */
        renderqueue_enqueue_background(backgroundtheme);

        /* render the background of the water */
        if(!editor_is_enabled())
            renderqueue_enqueue_waterbg();

        /* render the bricks */
        if(editor_is_enabled() || level_is_in_debug_mode())
            render_bricks_debug();
        else
            render_bricks();

        /* render SurgeScript objects */
        render_ssobjects();

        /* render the players */
        render_players();

        /* render the foreground of the water */
        if(!editor_is_enabled())
            renderqueue_enqueue_waterfg();

        /* render the foreground */
        renderqueue_enqueue_foreground(backgroundtheme);

        /* render legacy items */
        for(const item_list_t* inode=major_items; inode; inode=inode->next)
            renderqueue_enqueue_item(inode->data);

        /* render legacy objects */
        for(const enemy_list_t* enode=major_enemies; enode; enode=enode->next)
            renderqueue_enqueue_object(enode->data);

    /* okay, enough! let's render */
    renderqueue_end();
    v2d_t cell_camera=camera_get_position();
    bsl_cell_render(cell_camera.x,cell_camera.y,editor_is_enabled());
}


/* true if a given region is inside the screen position */
int inside_screen(int x, int y, int w, int h, int margin)
{
    v2d_t cam = level_editmode() ? editor_camera : camera_get_position();
    float a[4] = { x, y, x+w, y+h };
    float b[4] = {
        cam.x-VIDEO_SCREEN_W/2 - margin,
        cam.y-VIDEO_SCREEN_H/2 - margin,
        cam.x+VIDEO_SCREEN_W/2 + margin,
        cam.y+VIDEO_SCREEN_H/2 + margin
    };

    return bounding_box(a,b);
}


/* recalculates the size of the current level */
void update_level_size()
{
    brickmanager_recalculate_world_size(brick_manager);
}


/* restarts the level preserving the current
 * state (spawn point, waterlevel, etc.) */
void restart(int preserve_level_state)
{
    char path[PATH_MAXLEN];
    levelstate_t state = saved_state;

    /* copy the filepath before the level fields are cleared */
    str_cpy(path, file, sizeof(path));

    /* restart the scene */
    scenestack_pop();
    scenestack_push(
        storyboard_get_scene(SCENE_LEVEL),
        path
    );

    /* restore the saved state */
    if(preserve_level_state) {
        saved_state = state;
        restore_level_state(&saved_state);
        spawn_players(); /* reposition players */
    }
}


/* updates the music */
void update_music()
{
    if(music != NULL && !music_is_playing()) {
        if(music_current() == NULL || (music_current() == music && !music_is_paused())) {
            if(!level_cleared && !(player_is_dying(player) && !player_is_immortal(player)))
                music_play(music, true);
        }
    }
}


/* renders the bricks */
void render_bricks()
{
    iterator_t* it = brickmanager_retrieve_active_bricks(brick_manager);
    while(iterator_has_next(it)) {
        brick_t* brick = iterator_next(it);

        renderqueue_enqueue_brick(brick);

        if(must_render_brick_masks)
            renderqueue_enqueue_brick_mask(brick);
    }
    iterator_destroy(it);
}


/* renders the bricks (level editor) */
void render_bricks_debug()
{
    iterator_t* it = brickmanager_retrieve_active_bricks(brick_manager);
    while(iterator_has_next(it)) {
        brick_t* brick = iterator_next(it);

        renderqueue_enqueue_brick_debug(brick);
        renderqueue_enqueue_brick_path(brick);

        if(must_render_brick_masks)
            renderqueue_enqueue_brick_mask(brick);
    }
    iterator_destroy(it);
}


/* reconfigures the input devices of the players after switching characters */
void reconfigure_players_input_devices()
{
    for(int i = 0; i < team_size; i++) {
        if(NULL == team[i]->actor->input)
            team[i]->actor->input = input_create_user(NULL);

        if(team[i] == player)
            input_unblock(team[i]->actor->input);
        else
            input_block(team[i]->actor->input);
    }
}


/* renders the players */
void render_players()
{
    for(int i = team_size - 1; i >= 0; i--) {
        if(team[i] != player)
            renderqueue_enqueue_player(team[i]);
    }
    renderqueue_enqueue_player(player);
}


/* puts the players at the spawn point */
void spawn_players()
{
    int level_width = level_size().x;
    bool at_left_side = (int)spawn_point.x <= level_width / 2;

    /* default players */
    if(team_size == 0) {
        video_showmessage("No players have been specified!");
        team[0] = player_create(0, asset_exists("borsurgelab.runtime")?"None":"Surge");
        team_size = 1;
    }

    /* set the initial position of the players */
    for(int i = 0; i < team_size; i++) {
        int j = at_left_side ? (team_size - 1) - i : i;
        v2d_t offset = v2d_new(15 * j, 0);
        v2d_t position = v2d_add(spawn_point, offset);

        player_set_position(team[i], position);
        team[i]->actor->spawn_point = position; /* legacy */
    }
}


/* renders the built-in hud */
void render_hud()
{
    v2d_t fixedcam = v2d_new(VIDEO_SCREEN_W/2, VIDEO_SCREEN_H/2);

    /* dialog box */
    render_dlgbox(fixedcam);
}


/* renders the dialog box */
void render_dlgbox(v2d_t camera_position)
{
    /* this legacy component (dlgbox) will be removed */
    if(video_get_mode() != VIDEOMODE_DEFAULT)
        return;

    if(dlgbox->position.y < VIDEO_SCREEN_H) {
        actor_render(dlgbox, camera_position);
        font_render(dlgbox_title, camera_position);
        font_render(dlgbox_message, camera_position);
    }
}

/* updates the dialog box */
void update_dlgbox()
{
    float speed = VIDEO_SCREEN_H / 2; /* y speed */
    float dt = timer_get_delta();
    uint32_t t = timer_get_elapsed() * 1000.0;

    if(dlgbox_active) {
        if(t >= dlgbox_starttime + DLGBOX_MAXTIME) {
            dlgbox_active = FALSE;
            return;
        }
        dlgbox->position.x = (VIDEO_SCREEN_W - image_width(actor_image(dlgbox)))/2;
        dlgbox->position.y = max(dlgbox->position.y - speed*dt, VIDEO_SCREEN_H - image_height(actor_image(dlgbox))*1.3f);
    }
    else {
        dlgbox->position.y = min(dlgbox->position.y + speed*dt, VIDEO_SCREEN_H);
    }

    font_set_position(dlgbox_title, v2d_add(dlgbox->position, v2d_new(7, 8)));
    font_set_position(dlgbox_message, v2d_add(dlgbox->position, v2d_new(7, 20)));
}








/* dialog regions */


/* update_dialogregions(): updates all the dialog regions.
 * This checks if the player enters one region, and if he/she does,
 * this function shows the corresponding dialog box */
void update_dialogregions()
{
    int i;
    float a[4], b[4];

    if(level_timer < 2.0)
        return;

    a[0] = player->actor->position.x;
    a[1] = player->actor->position.y;
    a[2] = a[0] + image_width(actor_image(player->actor));
    a[3] = a[1] + image_height(actor_image(player->actor));

    for(i=0; i<dialogregion_size; i++) {
        if(dialogregion[i].disabled)
            continue;

        b[0] = dialogregion[i].rect_x;
        b[1] = dialogregion[i].rect_y;
        b[2] = b[0]+dialogregion[i].rect_w;
        b[3] = b[1]+dialogregion[i].rect_h;

        if(bounding_box(a, b)) {
            dialogregion[i].disabled = TRUE;
            level_call_dialogbox(dialogregion[i].title, dialogregion[i].message);
            break;
        }
    }
}





/* setup objects */

/* empty list? */
bool is_setup_object_list_empty()
{
    surgescript_object_t* level = level_ssobject();

    iterator_t* it = scripting_level_setupobjects_iterator(level);
    bool is_empty = !iterator_has_next(it);
    iterator_destroy(it);

    return is_empty;
}

/* adds a new object to the setup object list */
void add_to_setup_object_list(const char *object_name)
{
    surgescript_object_t* level = level_ssobject();

    surgescript_var_t* arg = surgescript_var_create();
    const surgescript_var_t* args[] = { arg };

    surgescript_var_set_string(arg, object_name);
    surgescript_object_call_function(level, "__registerSetupObjectName", args, 1, NULL);

    surgescript_var_destroy(arg);

    /* use the legacy API?
       TODO: remove this */
    const surgescript_objectmanager_t* manager = surgescript_object_manager(level);
    if(!surgescript_objectmanager_class_exists(manager, object_name)) {
        if(enemy_exists(object_name)) {
            enemy_t* e = level_create_legacy_object(object_name, v2d_new(0, 0));
            e->created_from_editor = FALSE;
        }
    }
}

/* spawns the setup objects */
void spawn_setup_objects()
{
    surgescript_object_t* level = level_ssobject();
    surgescript_object_call_function(level, "__spawnSetupObjects", NULL, 0, NULL);
}

/* check if object_name is in the setup object list */
bool is_setup_object(const char* object_name)
{
    surgescript_object_t* level = level_ssobject();
    return scripting_level_issetupobjectname(level, object_name);
}




/* level state */

/* save current state */
void save_level_state(levelstate_t* state)
{
    state->spawn_point = spawn_point;
    state->act_number = act_number;
    state->waterlevel = level_waterlevel();
    state->watercolor = level_watercolor();
    str_cpy(
        state->bgtheme,
        (backgroundtheme != NULL) ? background_filepath(backgroundtheme) : bgtheme,
        sizeof(state->bgtheme)
    );
    state->is_valid = true;
}

/* restore previously saved state */
void restore_level_state(const levelstate_t* state)
{
    if(state->is_valid) {
        spawn_point = state->spawn_point;
        act_number = state->act_number;
        level_set_waterlevel(state->waterlevel);
        level_set_watercolor(state->watercolor);
        level_change_background(state->bgtheme);
    }
}

/* clear level state */
void clear_level_state(levelstate_t* state)
{
    state->is_valid = false;
}



/* region of interest */

/* create a region of interest */
rect_t create_roi(v2d_t camera, int margin)
{
    v2d_t screen_size = video_get_screen_size();
    int dx = (int)screen_size.x / 2 + margin;
    int dy = (int)screen_size.y / 2 + margin;

    return rect_new(
        (int)camera.x - dx,
        (int)camera.y - dy,
        2*dx,
        2*dy
    );
}


/* obstacle map */

/* create the obstacle map */
void create_obstaclemap()
{
    is_obstaclemap_dirty = false;
    obstaclemap = obstaclemap_create();
    darray_init(mock_obstacles);
}

/* destroy the obstacle map */
void destroy_obstaclemap()
{
    for(int i = 0; i < darray_length(mock_obstacles); i++)
        obstacle_destroy(mock_obstacles[i]);
    darray_release(mock_obstacles);

    obstaclemap_destroy(obstaclemap);
    obstaclemap = NULL;

    is_obstaclemap_dirty = false;
}

/* clear the obstacle map */
void clear_obstaclemap()
{
    obstaclemap_clear(obstaclemap);

    for(int i = 0; i < darray_length(mock_obstacles); i++)
        obstacle_destroy(mock_obstacles[i]);
    darray_clear(mock_obstacles);
}

/* update the obstacle map */
void update_obstaclemap(const item_list_t* item_list, const object_list_t* object_list)
{
    const surgescript_objectmanager_t* manager = surgescript_object_manager(level_ssobject());

    /* clear the obstacle map */
    clear_obstaclemap();

    /* add bricks */
    iterator_t* brick_iterator = brickmanager_retrieve_active_bricks(brick_manager);
    while(iterator_has_next(brick_iterator)) {
        const brick_t* brick = iterator_next(brick_iterator);
        const obstacle_t* obstacle = brick_obstacle(brick);

        if(obstacle != NULL)
            obstaclemap_add(obstaclemap, obstacle);
    }
    iterator_destroy(brick_iterator);

    /* add brick-like objects */
    iterator_t* bricklike_iterator = entitymanager_bricklike_iterator(entitymanager_ssobject());
    while(iterator_has_next(bricklike_iterator)) {
        surgescript_objecthandle_t* bricklike_handle = iterator_next(bricklike_iterator);

        if(surgescript_objectmanager_exists(manager, *bricklike_handle)) {
            surgescript_object_t* bricklike_object = surgescript_objectmanager_get(manager, *bricklike_handle);

            if(scripting_brick_is_valid(bricklike_object) && scripting_brick_enabled(bricklike_object)) {
                obstacle_t* mock_obstacle = bricklike2obstacle(bricklike_object);

                darray_push(mock_obstacles, mock_obstacle);
                obstaclemap_add(obstaclemap, mock_obstacle);
            }
        }
    }
    iterator_destroy(bricklike_iterator);

    /* add legacy items */
    for(; item_list; item_list = item_list->next) {
        const item_t* item = item_list->data;

        if(item->obstacle && item->mask) {
            obstacle_t* mock_obstacle = item2obstacle(item);

            darray_push(mock_obstacles, mock_obstacle);
            obstaclemap_add(obstaclemap, mock_obstacle);
        }
    }

    /* add legacy objects */
    for(; object_list; object_list = object_list->next) {
        const object_t* object = object_list->data;

        if(object->obstacle && object->mask) {
            obstacle_t* mock_obstacle = object2obstacle(object);

            darray_push(mock_obstacles, mock_obstacle);
            obstaclemap_add(obstaclemap, mock_obstacle);
        }
    }

    /* The signed cell field remains collision-active off screen. */
    bsl_cell_obstacles(obstaclemap);
    /* build the obstacle map */
    obstaclemap_build(obstaclemap);
}

/* converts a legacy item to an obstacle */
obstacle_t* item2obstacle(const item_t* item)
{
    const collisionmask_t* mask = item->mask;
    v2d_t position = v2d_subtract(item->actor->position, item->actor->hot_spot);
    return obstacle_create(mask, point2d_new(position.x, position.y), OL_DEFAULT, OF_NONSTATIC);
}

/* converts a legacy object to an obstacle */
obstacle_t* object2obstacle(const object_t* object)
{
    const collisionmask_t* mask = object->mask;
    v2d_t position = v2d_subtract(object->actor->position, object->actor->hot_spot);
    return obstacle_create(mask, point2d_new(position.x, position.y), OL_DEFAULT, OF_NONSTATIC);
}

/* converts a brick-like SurgeScript object to an obstacle */
obstacle_t* bricklike2obstacle(const surgescript_object_t* object)
{
    v2d_t position = v2d_subtract(scripting_util_world_position(object), scripting_brick_hotspot(object));
    bricklayer_t brick_layer = scripting_brick_layer(object);
    obstaclelayer_t layer = ((brick_layer == BRL_GREEN) ? OL_GREEN : ((brick_layer == BRL_YELLOW) ? OL_YELLOW : OL_DEFAULT));
    int flags = OF_NONSTATIC;

    if(scripting_brick_type(object) == BRK_CLOUD)
        flags |= OF_CLOUD;

    collisionmask_t* clone = create_collisionmask_of_bricklike_object(object);
    obstacle_t* obstacle = obstacle_create_ex(
        clone,
        point2d_new(position.x, position.y),
        layer, flags,
        destroy_collisionmask_of_bricklike_object, clone
    );
    /* Brick lifetime serials survive mock rebuilds without handle-reuse ambiguity. */
    obstacle_set_id(obstacle, scripting_brick_identity(object));
    return obstacle;
}

/* creates a collision mask for a brick-like SurgeScript object */
collisionmask_t* create_collisionmask_of_bricklike_object(const surgescript_object_t* object)
{
    /* the following pointer is guaranteed to be valid during the lifetime of the obstacle_t,
       regardless of what happens with the brick-like object (i.e., it may get destroyed) */
    const collisionmask_t* mask = scripting_brick_mask(object); /* assumed to be valid */
    return collisionmask_clone(mask); /* not very efficient, though */
}

/* destroys a collision mask created for a brick-like SurgeScript object */
void destroy_collisionmask_of_bricklike_object(void* mask)
{
    collisionmask_t* clone = (collisionmask_t*)mask;
    collisionmask_destroy(clone);
}



/*
 * SurgeScript helpers
 */

/* get the Level object (SurgeScript) */
surgescript_object_t* level_ssobject()
{
    if(cached_level_ssobject == NULL)
        cached_level_ssobject = scripting_util_surgeengine_component(surgescript_vm(), "Level");
    return cached_level_ssobject;
}

/* get the EntityManager object (SurgeScript) */
surgescript_object_t* entitymanager_ssobject()
{
    if(cached_entity_manager == NULL)
        cached_entity_manager = scripting_level_entitymanager(level_ssobject());
    return cached_entity_manager;
}

/* update SurgeScript */
void update_ssobjects()
{
    surgescript_vm_t* vm = surgescript_vm();

    if(surgescript_vm_is_active(vm)) {
        surgescript_vm_update(vm);
    }
}

/* call lateUpdate() for each SurgeScript entity that implements it */
void late_update_ssobjects()
{
    surgescript_vm_t* vm = surgescript_vm();

    if(surgescript_vm_is_active(vm)) {
        surgescript_object_t* entity_manager = entitymanager_ssobject();
        surgescript_object_call_function(entity_manager, "lateUpdate", NULL, 0, NULL);
    }
}

/* render objects */
void render_ssobjects()
{
    surgescript_vm_t* vm = surgescript_vm();

    if(surgescript_vm_is_active(vm)) {
        surgescript_object_t* entity_manager = entitymanager_ssobject();
        surgescript_object_call_function(entity_manager, "render", NULL, 0, NULL);
    }
}

/* set the Region of Interest (ROI) of the SurgeScript Entity Manager */
void set_entitymanager_roi(rect_t roi)
{
    surgescript_object_t* entity_manager = entitymanager_ssobject();
    surgescript_var_t* args[4] = {
        surgescript_var_set_number(surgescript_var_create(), roi.x),
        surgescript_var_set_number(surgescript_var_create(), roi.y),
        surgescript_var_set_number(surgescript_var_create(), roi.width),
        surgescript_var_set_number(surgescript_var_create(), roi.height)
    };

    surgescript_object_call_function(
        entity_manager, "setROI",
        (const surgescript_var_t**)args, 4, NULL
    );

    surgescript_var_destroy(args[3]);
    surgescript_var_destroy(args[2]);
    surgescript_var_destroy(args[1]);
    surgescript_var_destroy(args[0]);
}

/* spawns a ssobject */
surgescript_object_t* spawn_ssobject(const char* object_name, v2d_t spawn_point)
{
    surgescript_object_t* level = level_ssobject();
    surgescript_objectmanager_t* manager = surgescript_object_manager(level);
    surgescript_tagsystem_t* tag_system = surgescript_objectmanager_tagsystem(manager);
    surgescript_objecthandle_t new_object_handle;

    /* return NULL if the class of objects doesn't exist */
    if(!surgescript_objectmanager_class_exists(manager, object_name))
        return NULL;

    if(!surgescript_tagsystem_has_tag(tag_system, object_name, "entity")) {
        /* call Level.spawn(object_name) */
        surgescript_var_t* ret = surgescript_var_create();
        surgescript_var_t* tmp = surgescript_var_set_string(surgescript_var_create(), object_name);

        const surgescript_var_t* param[] = { tmp };
        surgescript_object_call_function(level, "spawn", param, 1, ret);
        new_object_handle = surgescript_var_get_objecthandle(ret);

        surgescript_var_destroy(tmp);
        surgescript_var_destroy(ret);
    }
    else {
        /* call Level.spawnEntity(object_name, spawn_point) */
        surgescript_objecthandle_t v2_handle = surgescript_objectmanager_spawn_temp(manager, "Vector2");
        surgescript_object_t* v2 = surgescript_objectmanager_get(manager, v2_handle);
        scripting_vector2_update(v2, spawn_point.x, spawn_point.y);

        surgescript_var_t* ret = surgescript_var_create();
        surgescript_var_t* tmp = surgescript_var_set_string(surgescript_var_create(), object_name);
        surgescript_var_t* tmp2 = surgescript_var_set_objecthandle(surgescript_var_create(), v2_handle);

        const surgescript_var_t* param[] = { tmp, tmp2 };
        surgescript_object_call_function(level, "spawnEntity", param, 2, ret);
        new_object_handle = surgescript_var_get_objecthandle(ret);

        surgescript_var_destroy(tmp2);
        surgescript_var_destroy(tmp);
        surgescript_var_destroy(ret);

        surgescript_object_kill(v2);
    }

    return surgescript_objectmanager_get(manager, new_object_handle);
}

/* notifies all SurgeScript entities of the level */
void notify_ssobjects(const char* fun_name)
{
    surgescript_vm_t* vm = surgescript_vm();

    if(surgescript_vm_is_active(vm)) {
        surgescript_object_t* entity_manager = entitymanager_ssobject();
        surgescript_var_t* arg = surgescript_var_create();
        const surgescript_var_t* args[] = { arg };

        surgescript_var_set_string(arg, fun_name);
        surgescript_object_call_function(entity_manager, "notifyEntities", args, 1, NULL);

        surgescript_var_destroy(arg);
    }
}





/*
 * Additional info of SurgeScript entities
 */

bool entity_info_exists(const surgescript_object_t* object)
{
    return entitymanager_has_entity_info(entitymanager_ssobject(), surgescript_object_handle(object));
}

void entity_info_remove(const surgescript_object_t* object)
{
    entitymanager_remove_entity_info(entitymanager_ssobject(), surgescript_object_handle(object));
}

v2d_t entity_info_spawnpoint(const surgescript_object_t* object)
{
    return entitymanager_get_entity_spawn_point(entitymanager_ssobject(), surgescript_object_handle(object));
}

uint64_t entity_info_id(const surgescript_object_t* object)
{
    return entitymanager_get_entity_id(entitymanager_ssobject(), surgescript_object_handle(object));
}

void entity_info_set_id(const surgescript_object_t* object, uint64_t entity_id)
{
    entitymanager_set_entity_id(entitymanager_ssobject(), surgescript_object_handle(object), entity_id);
}

bool entity_info_is_persistent(const surgescript_object_t* object)
{
    return entitymanager_is_entity_persistent(entitymanager_ssobject(), surgescript_object_handle(object));
}

void entity_info_set_persistent(const surgescript_object_t* object, bool is_persistent)
{
    entitymanager_set_entity_persistent(entitymanager_ssobject(), surgescript_object_handle(object), is_persistent);
}



/*
 * Event listeners
 */

/* a switch out event takes place when the window is no longer active */
void handle_switchout_event(const ALLEGRO_EVENT* event, void* data)
{
#if defined(__ANDROID__)
    const scene_t* level_scene = storyboard_get_scene(SCENE_LEVEL);
    const scene_t* current_scene = scenestack_top();

    if(current_scene == level_scene) {
        /*

        Pause the game when the app goes out of focus.

        We should implement an event system in the scripting layer in order to
        accomplish this. Not all levels / scenes admit pausing; we should pause
        only on the ones that do.

        The solution below is not meant to be permanent.

        FIXME

        */
        if(is_setup_object("Default Pause and Quit")) {
            if(!level_is_in_debug_mode())
                level_pause();
        }
    }
#endif

    (void)event;
    (void)data;
}




/*
                                                                                                              
                                                                                                              
                                                                                                              
                                                                  MZZM                                        
                                                       MMMMMMMMMMMZOOZM MMMMMMMMM                             
             MM                                       M=...........MOOOM=.........M       MM          MM      
             MM                             MM          M7......77MOOOOOOM7.....77M             MM    MM      
             MM                             MM          M=......7MOOOOOMM=.....77M              MM    MM      
             MM    MM     MM     MMMMM    MMMMMM        M=......7MOOOMM==....77M          MM  MMMMMM  MM      
             MM    MM     MM    MMMMMMMM  MMMMMM        M=......7MOOOM==....77M           MM  MMMMMM  MM      
             MM    MM     MM   MM           MM         MM=......7MOM==....77MOOOM         MM    MM    MM      
             MM    MM     MM   MMMM         MM        MZM=......7MM==....77MOOOOOM        MM    MM    MM      
             MM    MM     MM      MMMMMM    MM      MZOOM=......7==....77MOOOOOOOOOM      MM    MM    MM      
     MM      MM    MM     MM           MM   MM       MZOM=......7....77MOOOOOOOOOOM       MM    MM            
      MM    MM      MM   MMM   MM     MMM   MM        MZM=..........MMMOOOOOOOOOOM        MM    MM            
        MMMM          MMM MM     MMMMMM      MMM        M=.........M..MOOOOOOOOM          MM     MMM  MM      
                                                        M=........7MMMOMMMOMMMMMM                             
                                                        M=......77MM..M...........M                           
                                                        M=....77MOM..MM..OM..MM..M                            
                                                        M=...77MOOM..MM..MM..MM..M                            
                                                         M.77M  MM...M..MM..MM...M                            
                                                          MMM    MMMMOMM  MM  MMM                             
                                                                   MM                                         
                                                                                                              
*/





/* Level Editor */



/*
 * editor_init()
 * Initializes the level editor data
 */
void editor_init()
{
    logfile_message("editor_init()");

    /* intializing... */
    editor_enabled = false;
    editor_item_list_size = -1;
    while(editor_item_list[++editor_item_list_size] >= 0);
    editor_selected_entity_type = EDT_BRICK;
    editor_selected_entity_id = 0;
    editor_enemy_name = objects_get_list_of_names(&editor_enemy_name_length);
    editor_enemy_selected_category_id = 0;
    editor_enemy_category = objects_get_list_of_categories(&editor_enemy_category_length);

    /* creating objects */
    editor_cmd = editorcmd_create();

    /* cursor */
    editor_cursor_init();
    bsl_inspect_font = font_create(font_exists("EditorInspect") ? "EditorInspect" : "EditorUI");

    /* grid */
    editor_grid_init();

    /* tooltip */
    editor_tooltip_init();

    /* status bar */
    editor_status_init();

    /* top bar */
    editor_topbar_init();

    /* bricks */
    editor_brick_init();
    editor_selected_entity_type = EDT_BRICK;
    editor_selected_entity_id = editor_brick_id(0);

    /* SurgeScript entities */
    editor_ssobj_init();

    /* groups */
    editorgrp_init(grouptheme);

    /* done */
    logfile_message("editor_init() ok");
}


/*
 * editor_release()
 * Releases the level editor data
 */
void editor_release()
{
    logfile_message("editor_release()");

    /* groups */
    editorgrp_release();

    /* SurgeScript entities */
    editor_ssobj_release();

    /* bricks */
    editor_brick_release();

    /* top bar */
    editor_topbar_release();

    /* status bar */
    editor_status_release();

    /* tooltip */
    editor_tooltip_release();
    
    /* grid */
    editor_grid_release();

    /* cursor */
    editor_cursor_release();
    font_destroy(bsl_inspect_font);
    bsl_inspect_font = NULL;

    /* destroying objects */
    editorcmd_destroy(editor_cmd);

    /* releasing... */
    editor_enabled = false;
    logfile_message("editor_release() ok");
}


/*
 * editor_update()
 * Updates the level editor
 */
/* Grid brush: each cell is placed once per stroke, including skipped mouse cells. */
void editor_brush_reset()
{
    editor_brush_active = false;
    darray_clear(editor_brush_cells);
}

void editor_brush_paint(bool clicked)
{
    v2d_t position = editor_cursor.world_grid_position;
    if(clicked)
        editor_brush_reset();
    if(!editor_brush_active) {
        if(!clicked)
            return;
        editor_brush_grid = editor_grid_size;
        editor_brush_type = editor_selected_entity_type;
        editor_brush_id = editor_selected_entity_id;
        editor_brush_layer = editor_layer;
        editor_brush_flip = editor_flip;
        editor_brush_previous = position;
        editor_brush_origin=v2d_new(position.x-roundf(position.x/editor_grid_size)*editor_grid_size,position.y-roundf(position.y/editor_grid_size)*editor_grid_size);
        editor_brush_active = true;
    }
    if(editor_brush_grid != editor_grid_size || editor_brush_type != (int)editor_selected_entity_type ||
       editor_brush_id != editor_selected_entity_id || editor_brush_layer != editor_layer || editor_brush_flip != editor_flip) {
        editor_brush_reset();
        return;
    }

    v2d_t origin=v2d_new(position.x-roundf(position.x/editor_grid_size)*editor_grid_size,position.y-roundf(position.y/editor_grid_size)*editor_grid_size);
    if(!nearly_zero(origin.x-editor_brush_origin.x)||!nearly_zero(origin.y-editor_brush_origin.y)){editor_brush_reset();return;}
    int x = (int)roundf(editor_brush_previous.x / editor_grid_size);
    int y = (int)roundf(editor_brush_previous.y / editor_grid_size);
    int end_x = (int)roundf(position.x / editor_grid_size);
    int end_y = (int)roundf(position.y / editor_grid_size);
    int dx = abs(end_x - x), dy = -abs(end_y - y);
    int sx = x < end_x ? 1 : -1, sy = y < end_y ? 1 : -1;
    int error = dx + dy;
    for(;;) {
        v2d_t cell = v2d_new(position.x + (x - end_x) * editor_grid_size,
                              position.y + (y - end_y) * editor_grid_size);
        bool visited = false;
        for(int i = 0; i < darray_length(editor_brush_cells); i++) {
            if(x == editor_brush_cells[i].x && y == editor_brush_cells[i].y) {
                visited = true;
                break;
            }
        }
        if(!visited) {
            editor_action_t action = editor_action_entity_new(TRUE,
                editor_selected_entity_type == EDT_EVENTS ? EDT_SSOBJ : editor_selected_entity_type,
                editor_selected_entity_id, cell);
            action = editor_action_commit(action);
            editor_action_register(action);
            darray_push(editor_brush_cells, ((point2d_t){x,y}));
        }
        if(x == end_x && y == end_y)
            break;
        int twice_error = 2 * error;
        if(twice_error >= dy) { error += dy; x += sx; }
        if(twice_error <= dx) { error += dx; y += sy; }
    }
    editor_brush_previous = position;
}

void editor_update()
{
    if(bsl_shell_level_update())return;
    if(bsl_cell_tool()) {
        editor_scroll();
        editor_grid_update();
        editor_cursor_update();
        bsl_cell_input(editor_cursor.world_position.x,editor_cursor.world_position.y,editor_grid_size,editor_cursor.screen_position.x,editor_cursor.screen_position.y);
        if((editorcmd_is_triggered(editor_cmd,"quit") || editorcmd_is_triggered(editor_cmd,"quit-alt"))) editor_disable();
        return;
    }
    /* Disruptive commands cancel an unfinished preview before doing their work. */
    if(bsl_inspect_drag.id && (editorcmd_is_triggered(editor_cmd, "quit") ||
       editorcmd_is_triggered(editor_cmd, "quit-alt") || editorcmd_is_triggered(editor_cmd, "save") ||
       editorcmd_is_triggered(editor_cmd, "reload") || editorcmd_is_triggered(editor_cmd, "help") ||
       editorcmd_is_triggered(editor_cmd, "open-brick-palette") || editorcmd_is_triggered(editor_cmd, "open-entity-palette") ||
       editorcmd_is_triggered(editor_cmd, "next-class") || editorcmd_is_triggered(editor_cmd, "previous-class") ||
       editorcmd_is_triggered(editor_cmd, "undo") || editorcmd_is_triggered(editor_cmd, "redo")))
        bsl_inspect_drag_cancel();
    if(editor_texture_release){ALLEGRO_MOUSE_STATE mouse;al_get_mouse_state(&mouse);if(mouse.buttons&3)return;editor_texture_release=false;}
    brick_list_t* major_bricks;
    item_list_t *major_items;
    enemy_list_t *major_enemies;
    int pick_object, delete_object = FALSE;
    int selected_item;

    /* Palette/menu commands are handled by the shared shell. */
    if(!editorcmd_is_triggered(editor_cmd, "paint-item"))
        editor_brush_reset();

    /* disable the level editor */
    if(editorcmd_is_triggered(editor_cmd, "quit") || editorcmd_is_triggered(editor_cmd, "quit-alt")) {
        editor_disable();
        editor_brush_reset();
        return;
    }

    /* save the level */
    if(editorcmd_is_triggered(editor_cmd, "save")) {
        editor_save();
        editor_brush_reset();
        return;
    }

    /* reload level */
    if(editorcmd_is_triggered(editor_cmd, "reload")) {
        confirmboxdata_t cbd = { "$EDITOR_CONFIRM_RELOAD", "$EDITOR_CONFIRM_YES", "$EDITOR_CONFIRM_NO", 1 };
        scenestack_push(storyboard_get_scene(SCENE_CONFIRMBOX), &cbd);
        editor_brush_reset();
        return;
    }

    if(1 == confirmbox_selected_option()) {
        char path[PATH_MAXLEN];
        logfile_message("Reloading the level via the editor...");
        str_cpy(path, file, sizeof(path));

        editor_disable();
        scenestack_pop();
        scripting_reload();
        scenestack_push(
            storyboard_get_scene(SCENE_LEVEL),
            path
        );

        editor_brush_reset();
        return;
    }

    /* help */
    if(editorcmd_is_triggered(editor_cmd, "help")) {
        scenestack_push(storyboard_get_scene(SCENE_EDITORHELP), NULL);
        editor_brush_reset();
        return;
    }

    if(editorcmd_is_triggered(editor_cmd, "open-inspect")) {
        editor_selected_entity_type = EDT_INSPECT;
        editor_selected_entity_id = 0;
    }

    if(editorcmd_is_triggered(editor_cmd, "open-events")) {
        editor_selected_entity_type = EDT_EVENTS;
        editor_selected_entity_id = editor_ssobj_index("BSL Event Anchor");
    }

    /* open palette */
    if(editorcmd_is_triggered(editor_cmd, "open-brick-palette")) {
        if(editor_brick_count > 0) {
            editorpal_config_t config = {
                .type = EDITORPAL_BRICK,
                .brick = {
                    .id = (const int*)editor_brick,
                    .count = editor_brick_count
                }
            };
            scenestack_push(storyboard_get_scene(SCENE_EDITORPAL), &config);
            editor_selected_entity_type = EDT_BRICK; editor_next_entity();
            editor_brush_reset();
            return;
        }
        else
            sound_play(SFX_DENY);
    }
    else if(editorcmd_is_triggered(editor_cmd, "open-entity-palette")) {
        if(editor_ssobj_count > 0) {
            editorpal_config_t config = {
                .type = EDITORPAL_SSOBJ,
                .ssobj = {
                    .name = (const char**)editor_ssobj,
                    .count = editor_ssobj_count
                }
            };
            scenestack_push(storyboard_get_scene(SCENE_EDITORPAL), &config);
            editor_selected_entity_type = EDT_SSOBJ; editor_next_entity();
            editor_brush_reset();
            return;
        }
        else
            sound_play(SFX_DENY);
    }

    if(editor_texture_pending){
        editor_texture_pending=false;selected_item=editorpal_selected_item();
        if(selected_item>=0&&selected_item<brick_texture_count()){
            const char* texture=brick_texture_id(selected_item);
            brick_t* target=editor_texture_target?bsl_brick_by_uid(editor_texture_target):NULL;
            if(target){editor_action_t action=bsl_brick_style_action(target,brick_brightness(target),texture);action=editor_action_commit(action);editor_action_register(action);editor_status_display("Texture applied to selected placement",0,NULL);}
            else if(!editor_texture_target){str_cpy(editor_stamp_texture,texture,sizeof editor_stamp_texture);editor_status_display("Texture selected for new placements",0,NULL);}
            else editor_status_display("Texture target is no longer available",0,NULL);
        }
        editor_texture_target=0;editor_texture_release=true;return;
    }
    else if(-1 < (selected_item = editorpal_selected_item())) {
        if(editor_selected_entity_type == EDT_BRICK) {
            editor_selected_entity_id = editor_brick_id(selected_item);
            editor_flip = BRF_NOFLIP;
            /*editor_layer = BRL_DEFAULT;*/ /* is this desirable? */
        }
        else if(editor_selected_entity_type == EDT_SSOBJ)
            editor_selected_entity_id = selected_item;
    }

    /* ----------------------------------------- */

    /* set the region of interest */
    v2d_t cam = editor_camera;
    rect_t brick_roi = create_roi(cam, ROI_MARGIN_EDITOR);
    rect_t entity_roi = brick_roi;

    brickmanager_set_roi(brick_manager, brick_roi);
    set_entitymanager_roi(entity_roi);
    entitymanager_set_active_region(entity_roi); /* legacy */

    /* Inspect is a virtual tool; it cannot place, erase or serialize itself. */
    editor_cursor_update();
    if(editorcmd_is_triggered(editor_cmd, "next-class")) editor_next_class();
    else if(editorcmd_is_triggered(editor_cmd, "previous-class")) editor_previous_class();
    if(editor_selected_entity_type != EDT_INSPECT) bsl_inspect_drag_cancel();
    if(editor_selected_entity_type == EDT_INSPECT) {
        bsl_inspect_update();
        editor_brush_reset();
        return;
    }

    /* get legacy entities */
    major_enemies = entitymanager_retrieve_active_objects();
    major_items = entitymanager_retrieve_active_items();
    major_bricks = major_enemies != NULL || major_items != NULL ? brickmanager_retrieve_active_bricks_as_list(brick_manager) : NULL; /* for backwards compatibility only */

    /* update items */
    for(item_list_t* it=major_items; it!=NULL; it=it->next)
        item_update(it->data, team, team_size, major_bricks, major_items, major_enemies);

    /* change class / entity / object category */
    if(editorcmd_is_triggered(editor_cmd, "next-category")) {
        if(editor_selected_entity_type == EDT_BRICK) editor_brick_cycle_filter(1);
        else if(editor_selected_entity_type == EDT_ENEMY) /* change category: legacy objects */
            editor_next_object_category();
    }
    else if(editorcmd_is_triggered(editor_cmd, "previous-category")) {
        if(editor_selected_entity_type == EDT_BRICK) editor_brick_cycle_filter(-1);
        else if(editor_selected_entity_type == EDT_ENEMY)
            editor_previous_object_category();
    }
    else if(editorcmd_is_triggered(editor_cmd, "next-item"))
        editor_next_entity();
    else if(editorcmd_is_triggered(editor_cmd, "previous-item"))
        editor_previous_entity();

    /* change brick layer */
    static const char* layer_message[3] = {
        [BRL_DEFAULT] = "$EDITOR_MESSAGE_DEFAULTLAYER",
        [BRL_GREEN] = "$EDITOR_MESSAGE_GREENLAYER",
        [BRL_YELLOW] = "$EDITOR_MESSAGE_YELLOWLAYER"
    };
    if(editorcmd_is_triggered(editor_cmd, "layer-next")) {
        if(editor_selected_entity_type == EDT_BRICK) {
            editor_layer = (editor_layer + 1) % 3;
            editor_status_display(layer_message[editor_layer], 1, (const char*[]){ color_to_hex(brick_util_layercolor(editor_layer), NULL, 0) });
        }
        else
            sound_play(SFX_DENY);
    }
    else if(editorcmd_is_triggered(editor_cmd, "layer-previous")) {
        if(editor_selected_entity_type == EDT_BRICK) {
            editor_layer = (editor_layer + 2) % 3;
            editor_status_display(layer_message[editor_layer], 1, (const char*[]){ color_to_hex(brick_util_layercolor(editor_layer), NULL, 0) });
        }
        else
            sound_play(SFX_DENY);
    }

    /* change brick flip mode */
    if(editorcmd_is_triggered(editor_cmd, "flip-next")) {
        if(editor_selected_entity_type == EDT_BRICK) {
            int delta = (3 + editor_flip) / 2;
            editor_flip = (editor_flip + delta) & BRF_VHFLIP;
        }
        else
            sound_play(SFX_DENY);
    }
    else if(editorcmd_is_triggered(editor_cmd, "flip-previous")) {
        if(editor_selected_entity_type == EDT_BRICK) {
            int delta = 2 + editor_flip + editor_flip / 2;
            editor_flip = (editor_flip + delta) & BRF_VHFLIP;
        }
        else
            sound_play(SFX_DENY);
    }

    /* show/hide brick masks & gizmos */
    if(editorcmd_is_triggered(editor_cmd, "toggle-masks")) {
        must_render_brick_masks = !must_render_brick_masks;
        must_display_gizmos = must_render_brick_masks; /* NOTE: should we give gizmos their own hotkey? */
    }

    /* change spawn point */
    if(editorcmd_is_triggered(editor_cmd, "change-spawnpoint")) {
        v2d_t new_position = editor_cursor.world_grid_position;
        editor_action_t eda = editor_action_spawnpoint_new(TRUE, new_position, spawn_point);
        eda = editor_action_commit(eda);
        editor_action_register(eda);
    }

    /* change waterlevel */
    if(editorcmd_is_triggered(editor_cmd, "change-waterlevel")) {
        v2d_t new_position = editor_cursor.world_grid_position;
        editor_action_t eda = editor_action_waterlevel_new(TRUE, new_position.y, level_waterlevel());
        eda = editor_action_commit(eda);
        editor_action_register(eda);
    }

    /* put item */
    if(editor_grid_is_enabled() && editorcmd_is_triggered(editor_cmd, "paint-item")) {
        if(editorcmd_is_triggered(editor_cmd, "snap-to-grid"))
            editor_brush_reset();
        else
            editor_brush_paint(editorcmd_is_triggered(editor_cmd, "put-item"));
    }
    else if(editorcmd_is_triggered(editor_cmd, "put-item")) {
        v2d_t position = editor_cursor.world_grid_position;
        editor_action_t eda = editor_action_entity_new(TRUE, editor_selected_entity_type == EDT_EVENTS ? EDT_SSOBJ : editor_selected_entity_type, editor_selected_entity_id, position);
        eda = editor_action_commit(eda);
        editor_action_register(eda);
    }

    /* pick or delete item */
    pick_object = editorcmd_is_triggered(editor_cmd, "pick-item");
    delete_object = editorcmd_is_triggered(editor_cmd, "delete-item") || editor_is_eraser_enabled();
    if(pick_object || delete_object) {
        switch(editor_selected_entity_type) {
        case EDT_INSPECT: break;
            /* brick */
            case EDT_BRICK: {
                const brick_t* candidate = NULL;
                bool candidate_got_collision = false;

                iterator_t* it = brickmanager_retrieve_active_bricks(brick_manager);
                while(iterator_has_next(it)) {
                    const brick_t* brick = iterator_next(it);
                    if(!editor_brick_matches_filter(brick)) continue;
                    v2d_t brk_topleft = brick_position(brick);
                    v2d_t brk_bottomright = v2d_add(brk_topleft, brick_size(brick));
                    float a[4] = { brk_topleft.x, brk_topleft.y, brk_bottomright.x, brk_bottomright.y };
                    float b[4] = { editor_cursor.world_position.x, editor_cursor.world_position.y, editor_cursor.world_position.x, editor_cursor.world_position.y };

                    if(bounding_box(a,b)) {
                        const obstacle_t* obstacle = brick_obstacle(brick);

                        /* pick the best brick that is in front of the others */
                        if(
                            (candidate == NULL) ||
                            (obstacle == NULL && !candidate_got_collision && (
                                brick_obstacle(candidate) != NULL || brick_zindex(brick) >= brick_zindex(candidate)
                            )) ||
                            (obstacle != NULL && obstacle_got_collision(obstacle, b[0], b[1], b[2], b[3]) && (
                                !candidate_got_collision || brick_zindex(brick) >= brick_zindex(candidate)
                            ))
                        ) {
                            candidate = brick;
                            candidate_got_collision = (obstacle != NULL) && obstacle_got_collision(obstacle, b[0], b[1], b[2], b[3]);
                        }
                    }
                }
                iterator_destroy(it);

                if(candidate != NULL) {
                    if(pick_object) {
                        editor_selected_entity_id = brick_id(candidate);
                        editor_layer = brick_layer(candidate);
                        editor_flip = brick_flip(candidate);
                        editor_stamp_brightness=brick_brightness(candidate);editor_stamp_z=brick_z_override(candidate);str_cpy(editor_stamp_texture,brick_texture(candidate),sizeof editor_stamp_texture);
                    }
                    else {
                        editor_action_t eda = editor_action_brick_delete(candidate);
                        eda = editor_action_commit(eda);
                        editor_action_register(eda);
                    }
                }

                break;
            }

            /* item */
            case EDT_ITEM: {
                item_t* candidate = NULL;

                for(item_list_t* iti = major_items; iti != NULL; iti = iti->next) {
                    float a[4] = { iti->data->actor->position.x-iti->data->actor->hot_spot.x, iti->data->actor->position.y-iti->data->actor->hot_spot.y, iti->data->actor->position.x-iti->data->actor->hot_spot.x + image_width(actor_image(iti->data->actor)), iti->data->actor->position.y-iti->data->actor->hot_spot.y + image_height(actor_image(iti->data->actor)) };
                    float b[4] = { editor_cursor.world_position.x, editor_cursor.world_position.y, editor_cursor.world_position.x, editor_cursor.world_position.y };

                    if(bounding_box(a,b)) {
                        if(candidate == NULL || !iti->data->bring_to_back)
                            candidate = iti->data;
                    }
                }

                if(candidate != NULL) {
                    if(pick_object) {
                        int index = editor_item_list_get_index(candidate->type);
                        if(index >= 0) {
                            editor_selected_item_index = index;
                            editor_selected_entity_id = editor_item_list[index];
                        }
                    }
                    else {
                        editor_action_t eda = editor_action_entity_new(FALSE, EDT_ITEM, candidate->type, candidate->actor->position);
                        eda = editor_action_commit(eda);
                        editor_action_register(eda);
                    }
                }

                break;
            }

            /* enemy */
            case EDT_ENEMY: {
                enemy_t *candidate = NULL;
                int candidate_key = 0;

                for(enemy_list_t* ite = major_enemies; ite != NULL; ite = ite->next) {
                    float a[4] = { ite->data->actor->position.x-ite->data->actor->hot_spot.x, ite->data->actor->position.y-ite->data->actor->hot_spot.y, ite->data->actor->position.x-ite->data->actor->hot_spot.x + image_width(actor_image(ite->data->actor)), ite->data->actor->position.y-ite->data->actor->hot_spot.y + image_height(actor_image(ite->data->actor)) };
                    float b[4] = { editor_cursor.world_position.x, editor_cursor.world_position.y, editor_cursor.world_position.x, editor_cursor.world_position.y };

                    int mykey = editor_enemy_name2key(ite->data->name);
                    if(mykey >= 0 && bounding_box(a,b)) {
                        if(candidate == NULL || ite->data->zindex >= candidate->zindex) {
                            candidate = ite->data;
                            candidate_key = mykey;
                        }
                    }
                }

                if(candidate != NULL) {
                    if(pick_object) {
                        editor_selected_entity_id = candidate_key;
                    }
                    else {
                        editor_action_t eda = editor_action_entity_new(FALSE, EDT_ENEMY, candidate_key, candidate->actor->position);
                        eda = editor_action_commit(eda);
                        editor_action_register(eda);
                    }
                }

                break;
            }

            /* can't pick-up/delete a group */
            case EDT_GROUP:
                break;

            /* SurgeScript entity */
            case EDT_EVENTS:
            case EDT_SSOBJ: {
                surgescript_object_t* ssobject = NULL;
                surgescript_object_t* entity_manager = entitymanager_ssobject();
                surgescript_objectmanager_t* manager = surgescript_object_manager(entity_manager);

                iterator_t* it = entitymanager_activeentities_iterator(entity_manager);
                while(iterator_has_next(it)) {
                    surgescript_var_t** var = iterator_next(it);
                    surgescript_objecthandle_t entity_handle = surgescript_var_get_objecthandle(*var);

                    if(surgescript_objectmanager_exists(manager, entity_handle)) {
                        surgescript_object_t* entity = surgescript_objectmanager_get(manager, entity_handle);
                        if(editor_selected_entity_type != EDT_EVENTS || surgescript_object_has_tag(entity, "bsl-event"))
                            editor_pick_entity(entity, &ssobject);
                    }
                }
                iterator_destroy(it);

                if(ssobject != NULL) {
                    int index = editor_ssobj_index(surgescript_object_name(ssobject));
                    if(!pick_object) {
                        editor_action_t eda = editor_action_entity_new(FALSE, EDT_SSOBJ, index, entity_info_spawnpoint(ssobject));
                        eda.ssobj_id = entity_info_id(ssobject);
                        eda = editor_action_commit(eda);
                        editor_action_register(eda);
                    }
                    else if(index >= 0) {
                        editor_selected_entity_id = index;
                        editor_ssobj_picked_entity.id = entity_info_id(ssobject);
                        str_cpy(editor_ssobj_picked_entity.name, surgescript_object_name(ssobject), sizeof(editor_ssobj_picked_entity.name));
                    }
                }
                break;               
            }
        }
    }

    /* undo & redo */
    if(editorcmd_is_triggered(editor_cmd, "undo"))
        editor_action_undo();
    else if(editorcmd_is_triggered(editor_cmd, "redo"))
        editor_action_redo();

    /* background */
    editor_update_background();

    /* grid */
    editor_grid_update();

    /* scrolling */
    editor_scroll();

    /* status bar */
    editor_status_update();

    /* tooltop */
    editor_tooltip_update();

    /* topbar */
    editor_topbar_update();

    /* cursor */
    editor_cursor_update();

    /* release major entities */
    major_bricks = brickmanager_release_list(major_bricks);
    major_items = entitymanager_release_retrieved_item_list(major_items);
    major_enemies = entitymanager_release_retrieved_object_list(major_enemies);
}



/*
 * editor_render()
 * Renders the level editor
 */
void editor_render()
{
    item_list_t* major_items = entitymanager_retrieve_active_items();
    enemy_list_t* major_enemies = entitymanager_retrieve_active_objects();
    
    /* render the level */
    render_level(major_items, major_enemies);

    /* render the grid */
    editor_grid_render();

    /* draw editor water line */
    editor_render_waterline();

    bsl_event_render();

    /* tooltip */
    /* Unified shell owns if(editor_selected_entity_type != EDT_INSPECT) editor_tooltip_render(); */

    /* mouse cursor */
    if(!bsl_cell_painting() && !bsl_shell_block_editor() && editor_selected_entity_type!=EDT_INSPECT) editor_draw_object(editor_selected_entity_type,editor_selected_entity_id,editor_cursor.screen_grid_position);

    /* status bar */
    /* Unified shell owns editor_status_render(); */

    /* top bar */
    /* Unified shell owns editor_topbar_render(); */
    /* Unified shell owns if(editor_selected_entity_type == EDT_INSPECT) bsl_inspect_render(); */
    v2d_t cell_editor_camera=camera_get_position();
    bsl_cell_overlay(cell_editor_camera.x,cell_editor_camera.y);

    /* done */
    major_items = entitymanager_release_retrieved_item_list(major_items);
    major_enemies = entitymanager_release_retrieved_object_list(major_enemies);
}



/*
 * editor_enable()
 * Enables the level editor
 */
void editor_enable()
{
    bsl_sequence_exit_suspend();
    logfile_message("Entering the level editor");
    bsl_host_interaction_reset();
    bsl_event_nearest = 0;

    /* pause the SurgeScript VM */
    scripting_pause_vm();

    /* changing the video mode */
    editor_previous_videomode = video_get_mode();
    video_set_mode(VIDEOMODE_FILL);

    /* activating the editor */
    editor_action_init();
    editor_brush_reset();
    editor_enabled = true;
    bsl_shell_enter();
    editor_camera.x = (int)camera_get_position().x;
    editor_camera.y = (int)camera_get_position().y;
    memset(&editor_cursor, 0, sizeof(editor_cursor));
    editor_cursor_update();

    /* display a warm, welcome message */
    editor_status_display("$EDITOR_MESSAGE_WELCOME", 0, NULL);
}


/*
 * editor_disable()
 * Disables the level editor
 */
void editor_disable()
{
    if(!bsl_cell_leave())return;
    bsl_inspect_drag_cancel();
    logfile_message("Exiting the level editor");
    bsl_host_interaction_reset();

    /* disabling the level editor */
    editor_action_release();
    editor_brush_reset();
    editor_enabled = false;

    /* changing the video mode */
    video_set_mode(editor_previous_videomode);

    /* updating the level size */
    update_level_size();

    /* unpause the SurgeScript VM */
    scripting_resume_vm();

    /* notify the SurgeScript entities */
    notify_ssobjects("onLeaveEditor");
}


/*
 * editor_is_enabled()
 * Is the level editor activated?
 */
bool editor_is_enabled()
{
    return editor_enabled;
}


/*
 * editor_want_to_activate()
 * The level editor wants to be activated!
 */
bool editor_want_to_activate()
{
    return editorcmd_is_triggered(editor_cmd, "enter");
}


/*
 * editor_update_background()
 * Updates the background of the level editor
 */
void editor_update_background()
{
    background_update(backgroundtheme);
}


/*
 * editor_render_waterline()
 * Renders the line of the water
 */
void editor_render_waterline()
{
    color_t color = color_rgb(255, 255, 255);
    v2d_t pos = editor_world2screen(v2d_new(0, level_waterlevel()));
    int x0 = 19 - (int)(timer_get_elapsed() * 40.0) % 20, y = pos.y;

    for(int x = x0 - 10; x < VIDEO_SCREEN_W; x += 20)
        image_line(x, y, x + 10, y, color);
}


/*
 * editor_save()
 * Saves the level
 */
void editor_save()
{
    bsl_inspect_drag_cancel();
    if(level_save(file)) {
        bsl_inspect_unsaved = false;
        sound_play(SFX_SAVE);
        editor_status_display("$EDITOR_MESSAGE_SAVED", 1, (const char*[]){ file });
    }
    else {
        sound_play(SFX_DENY);
        editor_status_display("$EDITOR_MESSAGE_SAVEERROR", 0, NULL);
    }
}


/*
 * editor_scroll()
 * Scrolling - "free-look mode"
 */
void editor_scroll()
{
    v2d_t camera_direction = v2d_new(0, 0);
    float camera_speed = 750.0f;
    float dt = timer_get_delta();

    /* input */
    if(editorcmd_is_triggered(editor_cmd, "UP"))
        camera_direction.y -= 5.0f;
    if(editorcmd_is_triggered(editor_cmd, "RIGHT"))
        camera_direction.x += 5.0f;
    if(editorcmd_is_triggered(editor_cmd, "DOWN"))
        camera_direction.y += 5.0f;
    if(editorcmd_is_triggered(editor_cmd, "LEFT"))
        camera_direction.x -= 5.0f;
    if(editorcmd_is_triggered(editor_cmd, "up"))
        camera_direction.y -= 1.0f;
    if(editorcmd_is_triggered(editor_cmd, "right"))
        camera_direction.x += 1.0f;
    if(editorcmd_is_triggered(editor_cmd, "down"))
        camera_direction.y += 1.0f;
    if(editorcmd_is_triggered(editor_cmd, "left"))
        camera_direction.x -= 1.0f;

    /* scroll */
    if(!nearly_zero(v2d_magnitude(camera_direction)))
        editor_camera = v2d_add(v2d_multiply(camera_direction, camera_speed * dt), editor_camera);

    /* the camera mustn't go off the bounds */
    editor_camera.x = (int)max(editor_camera.x, VIDEO_SCREEN_W/2);
    editor_camera.y = (int)max(editor_camera.y, VIDEO_SCREEN_H/2);
    camera_set_position(editor_camera);
    camera_update();
}

/*
 * editor_is_eraser_enabled()
 * Is the eraser enabled?
 */
bool editor_is_eraser_enabled()
{
    const float hold_time = 0.57f; /* the eraser is activated after this amount of seconds */
    static float timer = 0.0f;

    /* group mode? will erase bricks */
    if(editor_selected_entity_type == EDT_GROUP) {
        if(editorcmd_is_triggered(editor_cmd, "erase-area")) {
            while(editor_selected_entity_type != EDT_BRICK)
                editor_next_class();
        }
    }

    /* check if the eraser is enabled */
    if(editorcmd_is_triggered(editor_cmd, "erase-area")) {
        timer += timer_get_delta();
        return timer >= hold_time;
    }
    else {
        timer = 0.0f;
        return false;
    }
}


/* private stuff (level editor) */

/* convert screen coordinates to world coordinates */
v2d_t editor_screen2world(v2d_t screen_position)
{
    v2d_t half_screen = v2d_multiply(video_get_screen_size(), 0.5f);
    v2d_t topleft = v2d_subtract(editor_camera, half_screen);

    return v2d_add(screen_position, topleft);
}

/* convert world coordinates to screen coordinates */
v2d_t editor_world2screen(v2d_t world_position)
{
    v2d_t half_screen = v2d_multiply(video_get_screen_size(), 0.5f);
    v2d_t topleft = v2d_subtract(editor_camera, half_screen);

    return v2d_subtract(world_position, topleft);
}

/* returns a string containing a text
 * that corresponds to the given editor class
 * object id */
const char *editor_entity_class(enum editor_entity_type objtype)
{
    switch(objtype) {
        case EDT_EVENTS:
            return "EVENTS";
        case EDT_INSPECT:
            return "INSPECT";

        case EDT_BRICK:
            return "BRICK";

        case EDT_GROUP:
            return "GROUP";

        case EDT_SSOBJ:
            return "ENTITY";

        case EDT_ITEM:
            return "LEGACYITEM";

        case EDT_ENEMY:
            return "LEGACYOBJECT";
    }

    return "UNKNOWN";
}


/* returns a string containing data
 * about a given object */
const char *editor_entity_info(enum editor_entity_type objtype, int objid)
{
    static char buf[256];
    *buf = 0;

    switch(objtype) {
        case EDT_EVENTS: break;
        case EDT_INSPECT: break;
        case EDT_BRICK:
            if(brick_exists(objid)) {
                static char tmp[3][128];
                snprintf(tmp[0], sizeof(tmp[0]), "EDITOR_BRICK_TYPE_%s", brick_util_typename(brick_type_preview(objid)));
                snprintf(tmp[1], sizeof(tmp[1]), "EDITOR_BRICK_BEHAVIOR_%s", brick_util_behaviorname(brick_behavior_preview(objid)));
                snprintf(tmp[2], sizeof(tmp[2]), "EDITOR_BRICK_FLIP_%s", str_to_upper(brick_util_flipstr(editor_flip), NULL, 0));
                snprintf(buf, sizeof(buf),
                    "%4d %10s %12s    %3dx%-3d    z=%.2f    %6s", objid,
                    lang_getstring(tmp[0], tmp[0], sizeof(tmp[0])),
                    lang_getstring(tmp[1], tmp[1], sizeof(tmp[1])),
                    image_width(brick_image_preview(objid)),
                    image_height(brick_image_preview(objid)),
                    brick_zindex_preview(objid),
                    lang_getstring(tmp[2], tmp[2], sizeof(tmp[2]))
                );
            }
            else
                str_cpy(buf, "$EDITOR_UI_MISSING", sizeof(buf));
            break;

        case EDT_ITEM:
            snprintf(buf, sizeof(buf), "%2d", objid);
            break;

        case EDT_ENEMY: {
            int len = 0;
            if(strcmp(editor_enemy_selected_category(), "*") != 0) {
                snprintf(buf, sizeof(buf), "[%s] ", editor_enemy_selected_category());
                len = strlen(buf);
            }
            str_cpy(buf + len, editor_enemy_key2name(editor_selected_entity_id), sizeof(buf) - len);
            break;
        }

        case EDT_GROUP:
            break;

        case EDT_SSOBJ:
            str_cpy(buf, editor_ssobj_name(objid), sizeof(buf));
            break;
    }

    return buf;
}

/* returns the name of the selected category */
const char* editor_enemy_selected_category()
{
    return editor_enemy_category[editor_enemy_selected_category_id];
}

/* next object category */
void editor_next_object_category()
{
    editor_enemy_selected_category_id = (editor_enemy_selected_category_id + 1) % editor_enemy_category_length;

    /* drop the entity if it's not in the correct category */
    editor_next_entity();
    editor_previous_entity();
}

/* previous object category */
void editor_previous_object_category()
{
    editor_enemy_selected_category_id = ((editor_enemy_selected_category_id - 1) + editor_enemy_category_length) % editor_enemy_category_length;

    /* drop the entity if it's not in the correct category */
    editor_next_entity();
    editor_previous_entity();
}


/* jump to next class */
void editor_next_class()
{
    editor_selected_entity_type =
    (editor_selected_entity_type == EDT_BRICK) ? EDT_SSOBJ :
    (editor_selected_entity_type == EDT_SSOBJ) ? EDT_ITEM  :
    (editor_selected_entity_type == EDT_ITEM)  ? EDT_ENEMY :
    (editor_selected_entity_type == EDT_ENEMY) ? EDT_GROUP :
    (editor_selected_entity_type == EDT_GROUP) ? EDT_EVENTS :
    (editor_selected_entity_type == EDT_EVENTS) ? EDT_INSPECT :
    (editor_selected_entity_type == EDT_INSPECT) ? EDT_BRICK :
    editor_selected_entity_type;

    editor_selected_entity_id = editor_selected_entity_type == EDT_EVENTS ? editor_ssobj_index("BSL Event Anchor") : 0;
    editor_selected_item_index = 0;

    if(editor_selected_entity_type == EDT_GROUP && editorgrp_group_count() == 0)
        editor_next_class();

    if(editor_selected_entity_type == EDT_ENEMY && editor_enemy_name_length == 0)
        editor_next_class();

    if(editor_selected_entity_type == EDT_SSOBJ && editor_ssobj_count == 0)
        editor_next_class();

    if(editor_selected_entity_type == EDT_BRICK && !brick_exists(editor_selected_entity_id))
        editor_next_entity(); /* it's guaranteed that we'll always have a brick (see brickset_load) */

    if(editor_selected_entity_type == EDT_ITEM)
        editor_selected_entity_id = editor_item_list[editor_selected_item_index];
}


/* jump to previous class */
void editor_previous_class()
{
    editor_selected_entity_type =
    (editor_selected_entity_type == EDT_BRICK) ? EDT_INSPECT :
    (editor_selected_entity_type == EDT_INSPECT) ? EDT_EVENTS :
    (editor_selected_entity_type == EDT_EVENTS) ? EDT_GROUP :
    (editor_selected_entity_type == EDT_SSOBJ) ? EDT_BRICK :
    (editor_selected_entity_type == EDT_ITEM)  ? EDT_SSOBJ :
    (editor_selected_entity_type == EDT_ENEMY) ? EDT_ITEM  :
    (editor_selected_entity_type == EDT_GROUP) ? EDT_ENEMY :
    editor_selected_entity_type;

    editor_selected_entity_id = editor_selected_entity_type == EDT_EVENTS ? editor_ssobj_index("BSL Event Anchor") : 0;
    editor_selected_item_index = 0;

    if(editor_selected_entity_type == EDT_GROUP && editorgrp_group_count() == 0)
        editor_previous_class();

    if(editor_selected_entity_type == EDT_ENEMY && editor_enemy_name_length == 0)
        editor_previous_class();

    if(editor_selected_entity_type == EDT_SSOBJ && editor_ssobj_count == 0)
        editor_previous_class();

    if(editor_selected_entity_type == EDT_BRICK && !brick_exists(editor_selected_entity_id))
        editor_previous_entity();

    if(editor_selected_entity_type == EDT_ITEM)
        editor_selected_entity_id = editor_item_list[editor_selected_item_index];
}


/* select the next object */
void editor_next_entity()
{
    switch(editor_selected_entity_type) {
        case EDT_EVENTS: editor_selected_entity_id = editor_ssobj_index("BSL Event Anchor"); break;
        case EDT_INSPECT: break;
        /* brick */
        case EDT_BRICK: {
            editor_selected_entity_id = editor_brick_id(
                (editor_brick_index(editor_selected_entity_id) + 1) % editor_brick_count
            );
            break;
        }

        /* group */
        case EDT_GROUP: {
            int size = editorgrp_group_count();
            editor_selected_entity_id = (editor_selected_entity_id + 1) % size;
            break;
        }

        /* SurgeScript entity */
        case EDT_SSOBJ: {
            int size = editor_ssobj_count;
            editor_selected_entity_id = (editor_selected_entity_id + 1) % size;
            break;
        }

        /* item */
        case EDT_ITEM: {
            int size = editor_item_list_size;
            editor_selected_item_index = (editor_selected_item_index + 1) % size;
            editor_selected_entity_id = editor_item_list[editor_selected_item_index];
            break;
        }

        /* enemy */
        case EDT_ENEMY: {
            enemy_t *enemy;
            int size = editor_enemy_name_length;
            editor_selected_entity_id = (editor_selected_entity_id + 1) % size;

            enemy = enemy_create(editor_enemy_key2name(editor_selected_entity_id));
            if(!enemy_belongs_to_category(enemy, editor_enemy_selected_category())) {
                enemy_destroy(enemy);
                editor_next_entity(); /* invalid category? */
            }
            else
                enemy_destroy(enemy);

            break;
        }
    }
}


/* select the previous object */
void editor_previous_entity()
{
    switch(editor_selected_entity_type) {
        case EDT_EVENTS: editor_selected_entity_id = editor_ssobj_index("BSL Event Anchor"); break;
        case EDT_INSPECT: break;
        /* brick */
        case EDT_BRICK: {
            editor_selected_entity_id = editor_brick_id(
                ((editor_brick_index(editor_selected_entity_id) - 1) + editor_brick_count) % editor_brick_count
            );
            break;
        }

        /* group */
        case EDT_GROUP: {
            int size = editorgrp_group_count();
            editor_selected_entity_id = ((editor_selected_entity_id - 1) + size) % size;
            break;
        }

        /* SurgeScript entity */
        case EDT_SSOBJ: {
            int size = editor_ssobj_count;
            editor_selected_entity_id = ((editor_selected_entity_id - 1) + size) % size;
            break;
        }

        /* item */
        case EDT_ITEM: {
            int size = editor_item_list_size;
            editor_selected_item_index = ((editor_selected_item_index - 1) + size) % size;
            editor_selected_entity_id = editor_item_list[editor_selected_item_index];
            break;
        }

        /* enemy */
        case EDT_ENEMY: {
            enemy_t *enemy;
            int size = editor_enemy_name_length;
            editor_selected_entity_id = ((editor_selected_entity_id - 1) + size) % size;

            enemy = enemy_create(editor_enemy_key2name(editor_selected_entity_id));
            if(!enemy_belongs_to_category(enemy, editor_enemy_selected_category())) {
                enemy_destroy(enemy);
                editor_previous_entity(); /* invalid category? */
            }
            else
                enemy_destroy(enemy);

            break;
        }
    }
}


/* returns the index of item_id on
 * editor_item_list or -1 if the search fails */
int editor_item_list_get_index(int item_id)
{
    int i;

    for(i=0; i<editor_item_list_size; i++) {
        if(item_id == editor_item_list[i])
            return i;
    }

    return -1;
}


/*
 * editor_is_valid_item()
 * Is the given item valid to be used in
 * the level editor?
 */
int editor_is_valid_item(int item_id)
{
    return (editor_item_list_get_index(item_id) != -1);
}



/* draws the given object at [position] */
void editor_draw_object(enum editor_entity_type obj_type, int obj_id, v2d_t position)
{
    const image_t *cursor = NULL;
    v2d_t offset = v2d_new(0, 0);
    float alpha = 0.75f;
    int flags = IF_NONE;

    /* getting the image of the current object */
    switch(obj_type) {
        case EDT_EVENTS: bsl_event_draw_cursor(position); break;
        case EDT_INSPECT: break;
        case EDT_BRICK: {
            if(brick_exists(obj_id)) {
                cursor = brick_image_preview(obj_id);
                flags = brick_image_flags(editor_flip);
            }
            break;
        }

        case EDT_ITEM: {
            item_t *item = item_create(obj_id);
            if(item != NULL) {
                cursor = actor_image(item->actor);
                offset = item->actor->hot_spot;
                item_destroy(item);
            }
            break;
        }

        case EDT_ENEMY: {
            enemy_t *enemy = enemy_create(editor_enemy_key2name(obj_id));
            if(enemy != NULL) {
                cursor = actor_image(enemy->actor);
                offset = enemy->actor->hot_spot;
                enemy_destroy(enemy);
            }
            break;
        }

        case EDT_GROUP: {
            editorgrp_entity_list_t *list, *it;
            list = editorgrp_get_group(obj_id);
            for(it=list; it; it=it->next) {
                enum editor_entity_type my_type = EDITORGRP_ENTITY_TO_EDT(it->entity.type);
                editor_draw_object(my_type, it->entity.id, v2d_add(position, it->entity.position));
            }
            break;
        }

        case EDT_SSOBJ: {
            const animation_t* anim = NULL;
            const char* object_name = editor_ssobj_name(obj_id);
            if(sprite_animation_exists(object_name, 0))
                anim = sprite_get_animation(object_name, 0);
            else
                anim = sprite_get_animation(NULL, 0);
            cursor = animation_image(anim, 0);
            offset = animation_hot_spot(anim);
            break;
        }
    }

    /* drawing the object */
    if(cursor != NULL)
        image_draw_trans(cursor, (int)(position.x-offset.x), (int)(position.y-offset.y), alpha, flags);
}


/* level editor: enemy name list */
int editor_enemy_name2key(const char *name)
{
    int i;

    for(i=0; i<editor_enemy_name_length; i++) {
        if(strcmp(name, editor_enemy_name[i]) == 0)
            return i;
    }

    return -1; /* not found */
}

const char* editor_enemy_key2name(int key)
{
    key = clip(key, 0, editor_enemy_name_length-1);
    return editor_enemy_name[key];
}


/* level editor: SurgeScript entities */
void editor_ssobj_init()
{
    surgescript_vm_t* vm = surgescript_vm();
    surgescript_tagsystem_t* tag_system = surgescript_vm_tagsystem(vm);
    int fill_counter = 0;

    editor_ssobj = NULL;
    editor_ssobj_count = 0;
    editor_ssobj_picked_entity.id = 0;
    editor_ssobj_picked_entity.name[0] = '\0';

    /* count SurgeScript objects tagged as "entity" */
    surgescript_tagsystem_foreach_tagged_object(tag_system, "entity", &editor_ssobj_count, editor_ssobj_register);

    /* register entities */
    editor_ssobj = mallocx(editor_ssobj_count * sizeof(*editor_ssobj));
    surgescript_tagsystem_foreach_tagged_object(tag_system, "entity", &fill_counter, editor_ssobj_register);
    qsort(editor_ssobj, editor_ssobj_count, sizeof(const char*), editor_ssobj_sortfun);
}

void editor_ssobj_register(const char* entity_name, void* data)
{
    surgescript_vm_t* vm = surgescript_vm();
    surgescript_tagsystem_t* tag_system = surgescript_vm_tagsystem(vm);
    if(!surgescript_tagsystem_has_tag(tag_system, entity_name, "private")) {
        int *counter = (int*)data;
        if(editor_ssobj != NULL)
            editor_ssobj[*counter] = str_dup(entity_name);
        (*counter)++;
    }
}

void editor_ssobj_release()
{
    for(int i = 0; i < editor_ssobj_count; i++)
        free(editor_ssobj[i]);
    free(editor_ssobj);
    editor_ssobj = NULL;
}

/* comparison function between entity names (for sorting) */
int editor_ssobj_sortfun(const void* a, const void* b)
{
    surgescript_vm_t* vm = surgescript_vm();
    surgescript_tagsystem_t* tag_system = surgescript_vm_tagsystem(vm);
    const char* x = *((const char**)a);
    const char* y = *((const char**)b);

    /* put "gimmick"-tagged entities last */
    bool x_is_gimmick = surgescript_tagsystem_has_tag(tag_system, x, "gimmick");
    bool y_is_gimmick = surgescript_tagsystem_has_tag(tag_system, y, "gimmick");
    if(x_is_gimmick == y_is_gimmick) {
        /* then, put "boss"-tagged entites after the others */
        bool x_is_boss = surgescript_tagsystem_has_tag(tag_system, x, "boss");
        bool y_is_boss = surgescript_tagsystem_has_tag(tag_system, y, "boss");
        if(x_is_boss == y_is_boss) {
            /* then, put "enemy"-tagged entites after the others */
            bool x_is_enemy = surgescript_tagsystem_has_tag(tag_system, x, "enemy");
            bool y_is_enemy = surgescript_tagsystem_has_tag(tag_system, y, "enemy");
            if(x_is_enemy == y_is_enemy) {
                /* then, put "basic"-tagged entites BEFORE the others */
                bool x_is_basic = surgescript_tagsystem_has_tag(tag_system, x, "basic");
                bool y_is_basic = surgescript_tagsystem_has_tag(tag_system, y, "basic");
                if(x_is_basic == y_is_basic) {
                    /* then, put "special"-tagged entities before the others */
                    bool x_is_special = surgescript_tagsystem_has_tag(tag_system, x, "special");
                    bool y_is_special = surgescript_tagsystem_has_tag(tag_system, y, "special");
                    if(x_is_special == y_is_special) {
                        /* then, sort alphabetically with a caveat:

                           if two entities have almost equal names, differing only by a numeric
                           suffix preceded by a space, then sort by the suffix.

                           example: "Event Trigger 2" appears before "Event Trigger 10" */
                        const char *sx, *sy;
                        if((sx = strrchr(x, ' ')) && isdigit(sx[1]) && (sy = strrchr(y, ' ')) && isdigit(sy[1])) {
                            ptrdiff_t x_prefix_length = sx - x;
                            ptrdiff_t y_prefix_length = sy - y;
                            if(x_prefix_length == y_prefix_length && 0 == strncmp(x, y, x_prefix_length)) {
                                const char* x_suffix = sx + 1;
                                const char* y_suffix = sy + 1;
                                if(0 == strcspn(x_suffix, "0123456789") && 0 == strcspn(y_suffix, "0123456789")) {
                                    /* sort by the numeric suffix */
                                    return atoi(x_suffix) - atoi(y_suffix);
                                }
                            }
                        }

                        /* sort alphabetically */
                        return strcmp(x, y);
                    }
                    else
                        return (x_is_special && !y_is_special) ? -1 : 1;
                }
                else
                    return (x_is_basic && !y_is_basic) ? -1 : 1;
            }
            else
                return (x_is_enemy && !y_is_enemy) ? 1 : -1;
        }
        else
            return (x_is_boss && !y_is_boss) ? 1 : -1;
    }
    else
        return (x_is_gimmick && !y_is_gimmick) ? 1 : -1;
}

/* associates an integer to each SurgeScript entity */
int editor_ssobj_index(const char* entity_name)
{
    /* find i such that editor_ssobj[i] == entity_name */
    for(int i = 0; i < editor_ssobj_count; i++) {
        if(strcmp(entity_name, editor_ssobj[i]) == 0)
            return i;
    }

    /* not found */
    return -1;
}

const char* editor_ssobj_name(int entity_index)
{
    int idx = clip(entity_index, 0, editor_ssobj_count - 1);
    return editor_ssobj[idx];
}


/* level editor: bricks */
void editor_brick_init()
{
    editor_stamp_brightness=15;editor_stamp_z=-1;editor_stamp_texture[0]=0;editor_texture_pending=false;editor_texture_target=0;editor_texture_release=false;
    editor_layer=BRL_DEFAULT;editor_flip=BRF_NOFLIP;
    editor_brick_filter_count=0;editor_brick_filter=0;
    editor_brick=brickset_loaded()?mallocx(brickset_size()*sizeof(*editor_brick)):NULL;
    editor_brick_filter_roles=brickset_loaded()?mallocx(brickset_size()*sizeof(*editor_brick_filter_roles)):NULL;
    editor_brick_filter_categories=brickset_loaded()?mallocx(brickset_size()*sizeof(*editor_brick_filter_categories)):NULL;
    for(int i=0;i<brickset_size();i++)if(brick_exists(i)){
        brickrole_t role=brick_role_preview(i);const char* category=brick_category_preview(i);int f=0;
        for(;f<editor_brick_filter_count;f++)if(editor_brick_filter_roles[f]==role&&!strcmp(editor_brick_filter_categories[f],category))break;
        if(f==editor_brick_filter_count){
            editor_brick_filter_roles[f]=role;str_cpy(editor_brick_filter_categories[f],category,sizeof editor_brick_filter_categories[f]);editor_brick_filter_count++;
        }
    }
    editor_brick_refresh_filter(0);
}
static int bsl_brick_palette_compare(const void* a,const void* b){int x=*(const int*)a,y=*(const int*)b;int delta=brick_palette_order(x)-brick_palette_order(y);return delta?delta:x-y;}
static void editor_brick_refresh_filter(int filter)
{
    editor_brick_count=0;if(!editor_brick_filter_count)return;
    editor_brick_filter=(filter+editor_brick_filter_count)%editor_brick_filter_count;
    for(int i=0;i<brickset_size();i++)if(brick_exists(i)&&brick_role_preview(i)==editor_brick_filter_roles[editor_brick_filter]&&!strcmp(brick_category_preview(i),editor_brick_filter_categories[editor_brick_filter]))editor_brick[editor_brick_count++]=i;
    qsort(editor_brick,editor_brick_count,sizeof(*editor_brick),bsl_brick_palette_compare);
    if(editor_brick_count)editor_selected_entity_id=editor_brick[0];
    editor_brush_reset();
}
static void editor_brick_cycle_filter(int delta){
    if(editor_brick_filter_count)editor_brick_refresh_filter(editor_brick_filter+delta);
}
static bool editor_brick_matches_filter(const brick_t* brick){
    if(!editor_brick_filter_count||!brick_is_alive(brick))return false;
    int id=brick_id(brick);brickrole_t role=brick_role_preview(id);
    return role==editor_brick_filter_roles[editor_brick_filter]&&!strcmp(brick_category_preview(id),editor_brick_filter_categories[editor_brick_filter])&&(brick_role_visible(role,true)||(role==BRR_COLLISION&&brick_has_surface(brick)&&brick_role_visible(BRR_DECORATION,true)))&&(role!=BRR_COLLISION||brick_layer(brick)==editor_layer);
}

void editor_brick_release()
{
    if(editor_brick != NULL)
        free(editor_brick);

    editor_brick = NULL;
    free(editor_brick_filter_roles); editor_brick_filter_roles=NULL;
    free(editor_brick_filter_categories); editor_brick_filter_categories=NULL;
    editor_brick_filter_count=0;
    editor_brick_count = 0;
}

/* index of brick_id at editor_brick[], or -1 if not found */
int editor_brick_index(int brick_id)
{
    if(editor_brick != NULL) {
        int begin = 0, mid = 0;
        int end = editor_brick_count - 1;
        while(begin <= end) {
            mid = (begin + end) / 2;
            if(brick_id < editor_brick[mid])
                end = mid - 1;
            else if(brick_id > editor_brick[mid])
                begin = mid + 1;
            else
                return mid;
        }
        return -1;
    }
    else
        return -1;    
}

/* the index-th valid brick - at editor_brick[] */
int editor_brick_id(int index)
{
    if(editor_brick != NULL) {
        index = clip(index, 0, editor_brick_count - 1);
        return editor_brick[index];
    }
    else
        return 0;
}




/* level editor: grid */

/* initializes the grid module */
void editor_grid_init()
{
    editor_grid_size = EDITOR_GRID_INITIAL_SIZE;
    darray_init(editor_brush_cells);
    editor_brush_reset();
    editor_grid_cache = vertexcache_create();
}

/* releases the grid module */
void editor_grid_release()
{
    editor_brush_reset();
    darray_release(editor_brush_cells);
    vertexcache_destroy(editor_grid_cache);
}

/* updates the grid module */
void editor_grid_update()
{
    const int n = sizeof(EDITOR_GRID_SIZES) / sizeof(EDITOR_GRID_SIZES[0]);

    /* next grid size */
    if(editorcmd_is_triggered(editor_cmd, "snap-to-grid")) {
        for(int i = 0; i < n; i++) {
            if(editor_grid_size == EDITOR_GRID_SIZES[i]) {
                editor_grid_size = EDITOR_GRID_SIZES[(i + 1) % n];
                break;
            }
        }

        if(editor_grid_is_enabled()) {
            const char* grid_size_str = str_from_int(editor_grid_size, NULL, 0);
            editor_status_display("$EDITOR_MESSAGE_SNAP2GRIDON", 2, (const char*[]){ grid_size_str, grid_size_str });
        }
        else
            editor_status_display("$EDITOR_MESSAGE_SNAP2GRIDOFF", 0, NULL);
    }
}

/* render the grid */
void editor_grid_render()
{
    /* no grid? */
    if(!editor_grid_is_enabled())
        return;

    /* render translucent grid */
    if(editor_grid_size != 128) {
        color_t translucent_grid_color = color_premul_rgba(255, 255, 255, 32);
        editor_grid_render_ex(translucent_grid_color, editor_grid_size, editor_grid_size);
    }

    /* render reference grid (128x128) */
    color_t reference_grid_color = color_rgb(255, 255, 255);
    editor_grid_render_ex(reference_grid_color, 128, 128);
}

/* render a grid with custom parameters */
void editor_grid_render_ex(color_t color, int grid_width, int grid_height)
{
    const float dx[6] = { -1, 1, 1, 1, -1, -1 }, dy[6] = { -1, -1, 1, 1, 1, -1 };
    v2d_t half_screen = v2d_multiply(video_get_screen_size(), 0.5f);
    v2d_t topleft = v2d_subtract(editor_camera, half_screen);
    int max_x = VIDEO_SCREEN_W + grid_width;
    int max_y = VIDEO_SCREEN_H + grid_height;

    /* clear cache */
    vertexcache_clear(editor_grid_cache);

    /* for each point (x,y) */
    for(int x = 0; x < max_x; x += grid_width) {
        for(int y = 0; y < max_y; y += grid_height) {

            /* find a vertex of the grid near (x,y) in screen space */
            v2d_t screen_pos = v2d_new(x, y);
            v2d_t snapped_pos = editor_grid_screen_snap_ex(screen_pos, topleft, grid_width, grid_height);

            /* prepare two triangles per vertex of the grid */
            for(int j = 0; j < 6; j++) {
                int vertex_x = (int)(snapped_pos.x + dx[j]);
                int vertex_y = (int)(snapped_pos.y + dy[j]);
                vertexcache_push(editor_grid_cache, vertex_x, vertex_y, color);
            }

        }
    }

    /* draw triangles */
    image_quick_triangles(editor_grid_cache);
}

/* aligns a position in world space to the grid */
v2d_t editor_grid_snap(v2d_t world_pos)
{
    v2d_t half_screen = v2d_multiply(video_get_screen_size(), 0.5f);
    v2d_t topleft = v2d_subtract(editor_camera, half_screen);

    return editor_grid_snap_ex(world_pos, topleft, editor_grid_size, editor_grid_size);
}

/* aligns a position in screen space to the grid */
v2d_t editor_grid_screen_snap(v2d_t screen_pos)
{
    v2d_t half_screen = v2d_multiply(video_get_screen_size(), 0.5f);
    v2d_t topleft = v2d_subtract(editor_camera, half_screen);

    return editor_grid_screen_snap_ex(screen_pos, topleft, editor_grid_size, editor_grid_size);
}

/* snap world position to grid with custom grid size */
v2d_t editor_grid_snap_ex(v2d_t world_pos, v2d_t topleft, int grid_width, int grid_height)
{
    v2d_t screen_pos = v2d_subtract(world_pos, topleft);

    int x = (int)screen_pos.x - (int)screen_pos.x % grid_width - (int)topleft.x % grid_width;
    int y = (int)screen_pos.y - (int)screen_pos.y % grid_height - (int)topleft.y % grid_height;

    return v2d_add(topleft, v2d_new(x, y));
}

/* snap screen position to grid with custom grid size */
v2d_t editor_grid_screen_snap_ex(v2d_t screen_pos, v2d_t topleft, int grid_width, int grid_height)
{
    v2d_t world_pos = v2d_add(screen_pos, topleft);

    int dx = (int)topleft.x % grid_width;
    int dy = (int)topleft.y % grid_height;

    v2d_t adjusted_world_pos = v2d_add(world_pos, v2d_new(dx, dy));
    v2d_t aligned_world_pos = editor_grid_snap_ex(adjusted_world_pos, topleft, grid_width, grid_height);
    v2d_t aligned_screen_pos = v2d_subtract(aligned_world_pos, topleft);

    return aligned_screen_pos;
}

/* is the grid enabled? */
bool editor_grid_is_enabled()
{
    return editor_grid_size > 1;
}



/* level editor: mouse cursor */

/* initializes the mouse cursor */
void editor_cursor_init()
{
    editor_cursor_font = font_create("EditorCursor");
}

/* releases the mouse cursor */
void editor_cursor_release()
{
    font_destroy(editor_cursor_font);
}

/* updates the mouse cursor */
void editor_cursor_update()
{
    /* read the position of the mouse */
    v2d_t mousepos = editorcmd_mousepos(editor_cmd);

    /* update editor_cursor */
    editor_cursor.screen_position = mousepos;
    editor_cursor.screen_grid_position = editor_grid_screen_snap(editor_cursor.screen_position);
    editor_cursor.world_position = editor_screen2world(editor_cursor.screen_position);
    editor_cursor.world_grid_position = editor_screen2world(editor_cursor.screen_grid_position);

    /* update the font that displays the coordinates of the cursor */
    v2d_t textsize = font_get_textsize(editor_cursor_font);
    v2d_t offset = v2d_compmult(textsize, v2d_new(0, -2));
    v2d_t position = v2d_add(editor_cursor.screen_grid_position, offset);

    v2d_t margin = v2d_new(8.0f, max(8.0f, textsize.y * 0.5f));
    position.x = clip(position.x, margin.x, VIDEO_SCREEN_W - textsize.x - margin.x);
    position.y = clip(position.y, margin.y, VIDEO_SCREEN_H - margin.y);

    font_set_text(editor_cursor_font, "%.0f,%.0f", floorf(editor_cursor.world_grid_position.x), floorf(editor_cursor.world_grid_position.y));
    font_set_position(editor_cursor_font, position);
}

/* renders the mouse cursor */
void editor_cursor_render()
{
    if(editor_selected_entity_type == EDT_INSPECT || !editor_is_eraser_enabled()) {

        /* drawing the object */
        if(editor_selected_entity_type != EDT_INSPECT)
            editor_draw_object(editor_selected_entity_type, editor_selected_entity_id, editor_cursor.screen_grid_position);

        /* drawing the cursor arrow */
        const image_t* cursor = animation_image(sprite_get_animation("Mouse Cursor", 0), 0);
        if(editor_layer == BRL_DEFAULT || (editor_selected_entity_type != EDT_BRICK && editor_selected_entity_type != EDT_GROUP))
            image_draw(cursor, editor_cursor.screen_position.x, editor_cursor.screen_position.y, IF_NONE);
        else
            image_draw_tinted(cursor, editor_cursor.screen_position.x, editor_cursor.screen_position.y, brick_util_layercolor(editor_layer), IF_NONE);

        /* cursor coordinates */
        v2d_t no_camera = editor_world2screen(editor_camera); /* half screen */
        font_render(editor_cursor_font, no_camera);

    }
    else {

        /* drawing an eraser */
        const image_t* cursor = animation_image(sprite_get_animation("Eraser", 0), 0);
        image_draw(cursor, editor_cursor.screen_position.x - image_width(cursor)/2, editor_cursor.screen_position.y - image_height(cursor)/2, IF_NONE);

    }
}



/* level editor: top bar */

/* initializes the top bar */
void editor_topbar_init()
{
    editor_topbar_info_font = font_create("EditorUI");
    editor_topbar_help_font = font_create("EditorUI");
}

/* releases the top bar */
void editor_topbar_release()
{
    font_destroy(editor_topbar_help_font);
    font_destroy(editor_topbar_info_font);
}

/* updates the top bar */
void editor_topbar_update()
{
    const char* info_text = editor_entity_info(editor_selected_entity_type, editor_selected_entity_id);
    char buffer[32];

    if(editor_selected_entity_type == EDT_INSPECT) {
        font_set_position(editor_topbar_info_font, v2d_new(8, 8));
        font_set_text(editor_topbar_info_font, "Tool: INSPECT | click object | Shift+wheel changes tool");
        font_set_visible(editor_topbar_help_font, false);
        return;
    }

    if(editor_selected_entity_type == EDT_EVENTS) {
        font_set_position(editor_topbar_info_font, v2d_new(8, 8));
        font_set_text(editor_topbar_info_font, "Tool: EVENTS | click place | Ctrl+2 Inspect to bind | E interacts in play");
        font_set_visible(editor_topbar_help_font, false);
        return;
    }

    if(editor_selected_entity_type==EDT_BRICK&&editor_brick_filter_count){
        brickrole_t role=editor_brick_filter_roles[editor_brick_filter];
        const char* label=*editor_brick_filter_categories[editor_brick_filter]?editor_brick_filter_categories[editor_brick_filter]:"Legacy bricks";
        font_set_position(editor_topbar_info_font,v2d_new(8,8));
        char grouped_info[512];snprintf(grouped_info,sizeof grouped_info,"%s | %s",brick_shape_group(editor_selected_entity_id),info_text);
        font_set_text(editor_topbar_info_font,"%s / %s | %s | Ctrl+wheel category | Ctrl+M colliders %s | Ctrl+G scenery %s | T texture | [ ] brightness",role==BRR_COLLISION?"Collision":role==BRR_DECORATION?"Decoration":"Legacy",label,grouped_info,brick_role_visible(BRR_COLLISION,true)?"ON":"OFF",brick_role_visible(BRR_DECORATION,true)?"ON":"OFF");
        font_set_visible(editor_topbar_help_font,false);return;
    }

    /* object properties */
    snprintf(buffer, sizeof(buffer), "$EDITOR_UI_%s ", editor_entity_class(editor_selected_entity_type));
    font_set_position(editor_topbar_info_font, v2d_new(8, 8));
    font_set_textarguments(editor_topbar_info_font, 2, buffer, info_text);
    font_set_text(editor_topbar_info_font, "$EDITOR_UI_TOOL");

    /* help label */
    font_set_visible(editor_topbar_help_font, VIDEO_SCREEN_W > 512);
    font_set_align(editor_topbar_help_font, FONTALIGN_RIGHT);
    font_set_position(editor_topbar_help_font, v2d_new(VIDEO_SCREEN_W - 8, 8));
    font_set_text(editor_topbar_help_font, "$EDITOR_UI_HELP");
}

/* renders the top bar */
void editor_topbar_render()
{
    v2d_t no_camera = editor_world2screen(editor_camera); /* half screen */
    image_rectfill(0, 0, VIDEO_SCREEN_W, (int)font_get_textsize(editor_topbar_info_font).y + 16, EDITOR_UI_COLOR_TRANS(160));
    font_render(editor_topbar_info_font, no_camera);
    font_render(editor_topbar_help_font, no_camera);
}



/* level editor: tooltip */

/* initializes the tooltip */
void editor_tooltip_init()
{
    editor_tooltip_font = font_create("EditorUI");
    font_set_visible(editor_tooltip_font, false);
}

/* releases the tooltip */
void editor_tooltip_release()
{
    font_destroy(editor_tooltip_font);
}

/* updates the tooltip */
void editor_tooltip_update()
{
    font_set_visible(editor_tooltip_font, false);
    if(editor_selected_entity_type == EDT_SSOBJ) {
        surgescript_object_t* target = NULL;
        surgescript_object_t* entity_manager = entitymanager_ssobject();
        surgescript_objectmanager_t* manager = surgescript_object_manager(entity_manager);

        /* locate a target (onmouseover) */
        iterator_t* it = entitymanager_activeentities_iterator(entity_manager);
        while(iterator_has_next(it)) {
            surgescript_var_t** var = iterator_next(it);
            surgescript_objecthandle_t entity_handle = surgescript_var_get_objecthandle(*var);

            if(surgescript_objectmanager_exists(manager, entity_handle)) {
                surgescript_object_t* entity = surgescript_objectmanager_get(manager, entity_handle);
                editor_pick_entity(entity, &target);
            }
        }
        iterator_destroy(it);

        /* found a target */
        if(target != NULL && !surgescript_object_is_killed(target)) {
            bool has_data = entity_info_exists(target);
            const char* entity_id = has_data ? x64_to_str(entity_info_id(target), NULL, 0) : "";
            v2d_t sp = has_data ? entity_info_spawnpoint(target) : v2d_new(0, 0);
            font_set_text(editor_tooltip_font, "<color=$COLOR_HIGHLIGHT>%s</color>\n%d,%d\n%s\n%s", surgescript_object_name(target), (int)sp.x, (int)sp.y, entity_id, editor_tooltip_ssproperties(target));
            font_set_position(editor_tooltip_font, scripting_util_world_position(target));
            font_set_visible(editor_tooltip_font, true);
        }
    }
}

/* renders the tooltip */
void editor_tooltip_render()
{
    if(font_is_visible(editor_tooltip_font)) {
        v2d_t margin = v2d_new(8, 8);
        v2d_t rect_position = editor_world2screen(font_get_position(editor_tooltip_font));
        v2d_t rect_size = font_get_textsize(editor_tooltip_font);
        v2d_t rect_topleft = v2d_subtract(rect_position, margin);
        v2d_t rect_bottomright = v2d_add(v2d_add(rect_position, rect_size), margin);

        image_rectfill(rect_topleft.x, rect_topleft.y, rect_bottomright.x, rect_bottomright.y, EDITOR_UI_COLOR_TRANS(160));
        image_rect(rect_topleft.x, rect_topleft.y, rect_bottomright.x, rect_bottomright.y, EDITOR_UI_COLOR());
        font_render(editor_tooltip_font, editor_camera);
    }
}

/* lists the public properties (and their values) of the given object */
struct editor_tooltip_ssproperties_helper_t {
    surgescript_object_t* object;
    surgescript_programpool_t* pool;
    char buf[1024];
    bool rw;
};

const char* editor_tooltip_ssproperties(surgescript_object_t* object)
{
    static struct editor_tooltip_ssproperties_helper_t helper;
    surgescript_objectmanager_t* manager = surgescript_object_manager(object);
    surgescript_programpool_t* pool = surgescript_objectmanager_programpool(manager);
    int len = 0;
    
    /* write public properties to the internal buffer */
    helper.object = object;
    helper.pool = pool;
    helper.buf[0] = '\0';
    helper.rw = true;
    surgescript_programpool_foreach_ex(pool, surgescript_object_name(object), &helper, editor_tooltip_ssproperties_add);
    helper.rw = false;
    surgescript_programpool_foreach_ex(pool, surgescript_object_name(object), &helper, editor_tooltip_ssproperties_add);

    /* write the source file */
    len = strlen(helper.buf);
    snprintf(helper.buf + len, sizeof(helper.buf) - len, "\n%s%s",
        len == 0 ? "" : "\n", editor_tooltip_ssproperties_file(object)
    );

    /* a static buffer is returned */
    return helper.buf;
}

void editor_tooltip_ssproperties_add(const char* fun_name, void* data)
{
    /* this is callback utility function */
    struct editor_tooltip_ssproperties_helper_t* helper =
        (struct editor_tooltip_ssproperties_helper_t*)data;

    /* are we dealing with a property? */
    if(strncmp(fun_name, "get_", 4) == 0) {
        surgescript_program_t* property_getter = surgescript_programpool_get(helper->pool, surgescript_object_name(helper->object), fun_name);
        assertx(property_getter != NULL);

        int fun_arity = surgescript_program_arity(property_getter);
        const char* property_name = fun_name + 4;

        /* properties starting with '_' are hidden
           we also validate the arity just to be sure */
        if(property_name[0] != '_' && fun_arity == 0) {
            static char setter[128] = "set_";
            bool readonly = false;

            /* is it a readonly property? */
            str_cpy(setter + 4, property_name, sizeof(setter) - 4);
            readonly = !(surgescript_programpool_exists(
                helper->pool,
                surgescript_object_name(helper->object),
                setter
            ));

            if((!readonly && helper->rw) || (readonly && !helper->rw)) {
                static char value[32]; /* size must be >= 4 */
                int len = 0;
                char* p;

                /* get the current value of the property */
                surgescript_var_t* ret = surgescript_var_create();
                surgescript_object_call_function(helper->object, fun_name, NULL, 0, ret);
                if(surgescript_var_is_objecthandle(ret)) {
                    const surgescript_objectmanager_t* manager = surgescript_object_manager(helper->object);
                    char* str = surgescript_var_get_string(ret, manager); /* will call toString() */
                    str_cpy(value, str, sizeof(value));
                    ssfree(str);
                }
                else
                    surgescript_var_to_string(ret, value, sizeof(value));
                surgescript_var_destroy(ret);

                /* remove '\n' from the string */
                for(p = value; *p; p++) {
                    if(*p == '\n' || *p == '\r')
                        *p = ' ';
                }

                /* add ellipsis if necessary */
                if(strlen(value) == sizeof(value) - 1)
                    strcpy(value + sizeof(value) - 4, "...");

                /* write property to the buffer */
                len = strlen(helper->buf);
                if(!readonly)
                    snprintf(helper->buf + len, sizeof(helper->buf) - len, "\n<color=fff>%s: %s</color>", property_name, value);
                else
                    snprintf(helper->buf + len, sizeof(helper->buf) - len, "\n<color=aaa>%s: %s</color>", property_name, value);
            }
        }
    }
}

const char* editor_tooltip_ssproperties_file(surgescript_object_t* object)
{
    static char file[128];

    surgescript_var_t* ret = surgescript_var_create();
    surgescript_object_call_function(object, "get___file", NULL, 0, ret);
    surgescript_var_to_string(ret, file, sizeof(file));
    surgescript_var_destroy(ret);

    return file;
}




/* level editor: status bar */

/* initialize the status bar */
void editor_status_init()
{
    editor_status_timer = 0.0f;
    editor_status_font = font_create("EditorUI");
    font_set_text(editor_status_font, " ");
    font_set_visible(editor_status_font, false);
}

/* release the status bar */
void editor_status_release()
{
    font_destroy(editor_status_font);
    editor_status_timer = 0.0f;
}

/* update the status bar */
void editor_status_update()
{
    float dt = timer_get_delta();

    editor_status_timer = max(0.0f, editor_status_timer - dt);
    font_set_visible(editor_status_font, editor_status_timer > 0.0f);
}

/* render the status bar */
void editor_status_render()
{
    if(font_is_visible(editor_status_font)) {
        v2d_t cam = v2d_multiply(video_get_screen_size(), 0.5f);
        int h = (int)(font_get_textsize(editor_status_font).y);
        const int padding = 8;

        image_rectfill(0, VIDEO_SCREEN_H - h - 2*padding, VIDEO_SCREEN_W, VIDEO_SCREEN_H, EDITOR_UI_COLOR_TRANS(160));
        font_set_position(editor_status_font, v2d_new(padding, VIDEO_SCREEN_H - h - padding));
        font_render(editor_status_font, cam);
    }
}

/* display a message on the status bar */
void editor_status_display(const char* message, int argc, const char** argv)
{
    font_set_textargumentsv(editor_status_font, argc, argv);
    font_set_text(editor_status_font, "%s", message);
    bsl_shell_note("editor",message[0]=='$'?lang_getstring(message+1,(char[1024]){0},1024):message,0);
    bsl_shell_note("editor",message[0]=='$'?lang_getstring(message+1,(char[1024]){0},1024):message,0);
    editor_status_timer = EDITOR_STATUS_TIMEOUT;
}



/* level editor actions */

/* action: constructor (entity) */
editor_action_t editor_action_entity_new(int is_new_object, enum editor_entity_type obj_type, int obj_id, v2d_t obj_position)
{
    editor_action_t o = {0};
    o.type = is_new_object ? EDA_NEWOBJECT : EDA_DELETEOBJECT;
    o.obj_type = obj_type;
    o.obj_id = obj_id;
    o.obj_position = obj_position;
    o.obj_old_position = obj_position;
    o.layer = editor_layer;
    o.flip = editor_flip;
    o.brightness=obj_type==EDT_BRICK&&brick_role_preview(obj_id)==BRR_DECORATION?editor_stamp_brightness:15;
    o.z_override=obj_type==EDT_BRICK&&brick_role_preview(obj_id)==BRR_DECORATION?editor_stamp_z:-1;
    if(obj_type==EDT_BRICK&&brick_role_preview(obj_id)==BRR_COLLISION)str_cpy(o.texture_id,editor_stamp_texture,sizeof o.texture_id);
    o.ssobj_id = 0;

    /* Brick deletion needs an explicit selected identity, never coordinates alone. */
    if(!is_new_object && obj_type==EDT_BRICK)o.rejected=true;

    return o;
}

static editor_action_t editor_action_brick_delete(const brick_t* brick){
    editor_action_t action=editor_action_entity_new(FALSE,EDT_BRICK,brick_id(brick),brick_spawnpoint(brick));
    if(!editor_brick_matches_filter(brick))return action;
    action.rejected=false;action.brick_uid=brick_placement_id(brick);action.layer=brick_layer(brick);action.flip=brick_flip(brick);action.brightness=brick_brightness(brick);action.z_override=brick_z_override(brick);str_cpy(action.texture_id,brick_texture(brick),sizeof action.texture_id);return action;
}

/* action: constructor (spawn point) */
editor_action_t editor_action_spawnpoint_new(int is_changing, v2d_t obj_position, v2d_t obj_old_position)
{
    editor_action_t o = {0};
    o.type = is_changing ? EDA_CHANGESPAWN : EDA_RESTORESPAWN;
    o.obj_id = 0;
    o.obj_position = obj_position;
    o.obj_old_position = obj_old_position;
    o.obj_type = EDT_BRICK;
    o.layer = editor_layer;
    o.flip = editor_flip;
    o.ssobj_id = 0;
    return o;
}

/* action: constructor (waterlevel) */
editor_action_t editor_action_waterlevel_new(int is_changing, int new_waterlevel, int old_waterlevel)
{
    editor_action_t o = {0};
    o.type = is_changing ? EDA_CHANGEWATER : EDA_RESTOREWATER;
    o.obj_id = 0;
    o.obj_position = v2d_new(0, new_waterlevel);
    o.obj_old_position = v2d_new(0, old_waterlevel);
    o.obj_type = EDT_BRICK;
    o.layer = editor_layer;
    o.flip = editor_flip;
    o.ssobj_id = 0;
    return o;
}

/* initializes the editor_action module */
void editor_action_init()
{
    /* linked list */
    editor_action_buffer_head = mallocx(sizeof *editor_action_buffer_head);
    editor_action_buffer_head->in_group = FALSE;
    editor_action_buffer_head->prev = NULL;
    editor_action_buffer_head->next = NULL;
    editor_action_buffer = editor_action_buffer_head;
    editor_action_buffer_cursor = editor_action_buffer_head;
}

/* releases the editor_action module */
void editor_action_release()
{
    /* linked list */
    editor_action_buffer_head = editor_action_delete_list(editor_action_buffer_head);
    editor_action_buffer = NULL;
    editor_action_buffer_cursor = NULL;
}

/* registers a new editor_action */
void editor_action_register(editor_action_t action)
{
    if(action.rejected) return;
    shell_mark_dirty();
    /* ugly, but these fancy group stuff
     * shouldn't be availiable on the interface */
    static int registering_group = FALSE;
    static uint32_t group_key;

    if(action.obj_type != EDT_GROUP) {
        editor_action_list_t *c, *it, *node;

        /* creating new node */
        node = mallocx(sizeof *node);
        node->action = action;
        node->in_group = registering_group;
        if(node->in_group)
            node->group_key = group_key;

        /* cursor */
        c = editor_action_buffer_cursor;
        if(c != NULL)
            c->next = editor_action_delete_list(c->next);

        /* inserting the node into the linked list */
        it = editor_action_buffer;
        while(it->next != NULL)
            it = it->next;
        it->next = node;
        node->prev = it;
        node->next = NULL;

        /* updating the cursor */
        editor_action_buffer_cursor = node;
    }
    else {
        static uint32_t auto_increment = 0xbeef; /* dummy value */
        editorgrp_entity_list_t *list, *it;

        /* registering a group of objects */
        registering_group = TRUE;
        group_key = auto_increment++;
        list = editorgrp_get_group(action.obj_id);
        for(it=list; it; it=it->next) {
            editor_action_t a;
            editorgrp_entity_t e = it->entity;
            enum editor_entity_type my_type = EDITORGRP_ENTITY_TO_EDT(e.type);
            bricklayer_t old_layer=editor_layer;brickflip_t old_flip=editor_flip;
            editor_layer=e.layer!=BRL_DEFAULT?e.layer:editor_layer;editor_flip=e.flip!=BRF_NOFLIP?e.flip:editor_flip;
            a = editor_action_entity_new(TRUE, my_type, e.id, v2d_add(e.position, action.obj_position));
            a=editor_action_commit(a);editor_action_register(a);
            editor_layer=old_layer;editor_flip=old_flip;
        }
        registering_group = FALSE;
    }
}

/* deletes an editor_action list */
editor_action_list_t* editor_action_delete_list(editor_action_list_t *list)
{
    editor_action_list_t *p, *next;

    p = list;
    while(p != NULL) {
        next = p->next;
        free(p);
        p = next;
    }

    return NULL;
}

/* undo */
void editor_action_undo()
{
    editor_action_list_t *p;
    editor_action_t a;

    if(editor_action_buffer_cursor != editor_action_buffer_head) {
        /* moving the cursor */
        p = editor_action_buffer_cursor;
        editor_action_buffer_cursor = editor_action_buffer_cursor->prev;

        /* UNDOing a group? */
        if(p->in_group && p->prev && p->prev->in_group && p->group_key == p->prev->group_key)
            editor_action_undo();

        /* undo */
        a = p->action;
        a.type = /* reverse of a.type ??? */
        (a.type == EDA_NEWOBJECT) ? EDA_DELETEOBJECT :
        (a.type == EDA_DELETEOBJECT) ? EDA_NEWOBJECT :
        (a.type == EDA_CHANGESPAWN) ? EDA_RESTORESPAWN :
        (a.type == EDA_RESTORESPAWN) ? EDA_CHANGESPAWN :
        (a.type == EDA_CHANGEWATER) ? EDA_RESTOREWATER :
        (a.type == EDA_RESTOREWATER) ? EDA_CHANGEWATER :
        (a.type == EDA_MOVEINSTANCE) ? EDA_RESTOREMOVEINSTANCE :
        (a.type == EDA_RESTOREMOVEINSTANCE) ? EDA_MOVEINSTANCE :
        (a.type == EDA_INSTANCEPARAM) ? EDA_RESTOREINSTANCEPARAM :
        (a.type == EDA_RESTOREINSTANCEPARAM) ? EDA_INSTANCEPARAM :
        (a.type == EDA_EVENTBIND) ? EDA_RESTOREEVENTBIND :
        (a.type == EDA_RESTOREEVENTBIND) ? EDA_EVENTBIND :
        (a.type == EDA_BRICKSTYLE) ? EDA_RESTOREBRICKSTYLE :
        (a.type == EDA_RESTOREBRICKSTYLE) ? EDA_BRICKSTYLE :
        a.type;
        editor_action_commit(a);
    }
    else
        editor_status_display("$EDITOR_MESSAGE_UNDOERROR", 0, NULL);
}


/* redo */
void editor_action_redo()
{
    editor_action_list_t *p;
    editor_action_t a;

    if(editor_action_buffer_cursor->next != NULL) {
        /* moving the cursor */
        editor_action_buffer_cursor = editor_action_buffer_cursor->next;
        p = editor_action_buffer_cursor;

        /* REDOing a group? */
        if(p->in_group && p->next && p->next->in_group && p->group_key == p->next->group_key)
            editor_action_redo();
        
        /* redo */
        a = p->action;
        editor_action_commit(a);
    }
    else
        editor_status_display("$EDITOR_MESSAGE_REDOERROR", 0, NULL);
}

/* commit action */
editor_action_t editor_action_commit(editor_action_t action)
{
    if(action.type == EDA_MOVEINSTANCE || action.type == EDA_RESTOREMOVEINSTANCE) {
        bsl_inspect_move_commit(action, action.type == EDA_RESTOREMOVEINSTANCE);
        return action;
    }
    if(action.rejected)return action;
    if(action.type==EDA_BRICKSTYLE||action.type==EDA_RESTOREBRICKSTYLE){
        brick_t* brick=bsl_brick_by_uid(action.brick_uid);bool restore=action.type==EDA_RESTOREBRICKSTYLE;
        if(!brick||!brick_set_texture(brick,restore?action.texture_before:action.texture_id)){action.rejected=true;return action;}
        brick_set_brightness(brick,restore?action.brightness_before:action.brightness);brick_set_z_override(brick,restore?action.z_before:action.z_override);return action;
    }
    if(action.type == EDA_EVENTBIND || action.type == EDA_RESTOREEVENTBIND) {
        bsl_event_commit(action, action.type == EDA_RESTOREEVENTBIND);
        return action;
    }
    if(action.type == EDA_INSTANCEPARAM || action.type == EDA_RESTOREINSTANCEPARAM) {
        bsl_instance_commit(action, action.type == EDA_RESTOREINSTANCEPARAM);
        return action;
    }
    if(action.type == EDA_NEWOBJECT) {
        /* new object */
        switch(action.obj_type) {
        case EDT_EVENTS: break;
        case EDT_INSPECT: break;
            case EDT_BRICK: {
                /* new brick */
                if(action.brick_uid&&bsl_brick_by_uid(action.brick_uid)){action.rejected=true;break;}
                brick_t* created=bsl_brick_create(action.obj_id, action.obj_position, action.layer, action.flip,action.brick_uid);
                action.brick_uid=brick_placement_id(created);brick_set_brightness(created,action.brightness);brick_set_z_override(created,action.z_override);
                if(!brick_set_texture(created,action.texture_id)){brick_kill(created);action.rejected=true;}
                break;
            }

            case EDT_ITEM: {
                /* new item */
                level_create_legacy_item(action.obj_id, action.obj_position);
                break;
            }

            case EDT_ENEMY: {
                /* new legacy object */
                level_create_legacy_object(editor_enemy_key2name(action.obj_id), action.obj_position);
                break;
            }

            case EDT_SSOBJ: {
                const char* name = editor_ssobj_name(action.obj_id);
                uint64_t picked = !strcmp(name, editor_ssobj_picked_entity.name) ? editor_ssobj_picked_entity.id : 0;
                uint64_t desired = action.ssobj_id ? action.ssobj_id : picked;
                bool copying = !action.ssobj_id && picked && level_get_entity_by_id(x64_to_str(picked, NULL, 0));
                if(copying && !bsl_instance_copy_has_capacity(picked)) {
                    action.rejected = true;
                    editor_status_display("Instance capacity reached; placement not applied", 0, NULL);
                    return action;
                }
                if(!desired || level_get_entity_by_id(x64_to_str(desired, NULL, 0))) {
                    if(bsl_loaded_count == 16384) {
                        action.rejected = true;
                        editor_status_display("Placement ID capacity reached; placement not applied", 0, NULL);
                        return action;
                    }
                    desired = bsl_instance_fresh_id();
                    bsl_loaded_ids[bsl_loaded_count++] = desired;
                }
                surgescript_object_t* ssobj = level_create_object(name, action.obj_position);
                if(!ssobj || !entity_info_exists(ssobj)) {
                    action.rejected = true;
                    return action;
                }
                entity_info_set_id(ssobj, desired);
                if(copying) bsl_instance_copy(picked, desired);
                bsl_instance_apply_all(ssobj);
                editor_ssobj_picked_entity.id = 0;
                editor_ssobj_picked_entity.name[0] = '\0';
                action.ssobj_id = desired;
                break;
            }

            case EDT_GROUP: {
                /* Registration commits children and retains their actual placement IDs. */
                break;
            }
        }
    }
    else if(action.type == EDA_DELETEOBJECT) {
        /* delete object */
        switch(action.obj_type) {
        case EDT_EVENTS: break;
        case EDT_INSPECT: break;
            case EDT_BRICK: {
                brick_t* brick=bsl_brick_by_uid(action.brick_uid);
                if(!brick||brick_id(brick)!=action.obj_id){action.rejected=true;break;}
                brick_kill(brick);break;
            }

            case EDT_ITEM: {
                /* delete legacy item */
                item_list_t *item_list = entitymanager_retrieve_all_items();
                for(item_list_t *it = item_list; it != NULL; it = it->next) {
                    if(it->data->type == action.obj_id) {
                        float dist = v2d_magnitude(v2d_subtract(it->data->actor->position, action.obj_position));
                        if(nearly_zero(dist))
                            it->data->state = IS_DEAD;
                    }
                }
                item_list = entitymanager_release_retrieved_item_list(item_list);
                break;
            }

            case EDT_ENEMY: {
                /* delete legacy object */
                enemy_list_t *enemy_list = entitymanager_retrieve_all_objects();
                for(enemy_list_t *it = enemy_list; it != NULL; it = it->next) {
                    if(editor_enemy_name2key(it->data->name) == action.obj_id) {
                        float dist = v2d_magnitude(v2d_subtract(it->data->actor->position, action.obj_position));
                        if(nearly_zero(dist))
                            it->data->state = ES_DEAD;
                    }
                }
                enemy_list = entitymanager_release_retrieved_object_list(enemy_list);
                break;
            }

            case EDT_GROUP: {
                /* can't delete a group directly */
                break;
            }

            case EDT_SSOBJ: {
                /* delete SurgeScript entity */
                editor_remove_entity(action.ssobj_id);
                break;
            }
        }
    }
    else if(action.type == EDA_CHANGESPAWN) {
        /* change spawn point */
        level_set_spawnpoint(action.obj_position);
        spawn_players();
    }
    else if(action.type == EDA_RESTORESPAWN) {
        /* restore spawn point */
        level_set_spawnpoint(action.obj_old_position);
        spawn_players();
    }
    else if(action.type == EDA_CHANGEWATER) {
        /* change waterlevel */
        level_set_waterlevel(action.obj_position.y);
    }
    else if(action.type == EDA_RESTOREWATER) {
        /* restore waterlevel */
        level_set_waterlevel(action.obj_old_position.y);
    }

    /* return a possibly changed action */
    return action;
}

void editor_remove_entity(uint64_t entity_id)
{
    surgescript_object_t* entity_manager = entitymanager_ssobject();
    surgescript_objectmanager_t* manager = surgescript_object_manager(entity_manager);
    surgescript_objecthandle_t entity_handle = entitymanager_find_entity_by_id(entity_manager, entity_id);

    if(surgescript_objectmanager_exists(manager, entity_handle)) {
        surgescript_object_t* entity = surgescript_objectmanager_get(manager, entity_handle);

        bsl_host_anchor_cancel(entity_id);
        surgescript_object_kill(entity);
        entity_info_remove(entity);
    }
    else
        video_showmessage("Can't remove entity %llx", entity_id);
}

void editor_pick_entity(surgescript_object_t* object, surgescript_object_t** best_candidate)
{
    if(entity_info_is_persistent(object) && !surgescript_object_has_tag(object, "detached")) {

        /* find the bounding box of the entity */
        const char* name = surgescript_object_name(object);
        const animation_t* anim = sprite_animation_exists(name, 0) ? sprite_get_animation(name, 0) : sprite_get_animation(NULL, 0);
        const image_t* img = animation_image(anim, 0);
        v2d_t worldpos = scripting_util_world_position(object);
        v2d_t hot_spot = animation_hot_spot(anim);
        float left = worldpos.x - hot_spot.x;
        float top = worldpos.y - hot_spot.y;
        float right = left + image_width(img);
        float bottom = top + image_height(img);

        /* get the world position of the cursor */
        float cx = editor_cursor.world_position.x;
        float cy = editor_cursor.world_position.y;

        /* detected collision between the cursor and the entity? */
        float a[4] = { left, top, right, bottom },
              b[4] = { cx, cy, cx, cy };

        if(bounding_box(a, b)) {
            if(NULL == *best_candidate || scripting_util_object_zindex(object) >= scripting_util_object_zindex(*best_candidate))
                *best_candidate = object;
        }

    }
}

#include "../bor/level_instance_test.inc"

#include "../bor/level_event_test.inc"

#include "../bor/terrain_test.inc"

static editor_action_t bsl_brick_style_action(brick_t* brick,int brightness,const char* texture){
 editor_action_t action={0};action.type=EDA_BRICKSTYLE;action.obj_type=EDT_BRICK;action.brick_uid=brick_placement_id(brick);action.brightness_before=brick_brightness(brick);action.brightness=brightness;action.z_before=action.z_override=brick_z_override(brick);str_cpy(action.texture_before,brick_texture(brick),sizeof action.texture_before);str_cpy(action.texture_id,texture,sizeof action.texture_id);return action;
}
typedef struct bsl_door_nav_t { uint64_t last, next_id, first_id; int direction; v2d_t next_pos, first_pos; } bsl_door_nav_t;
static bool bsl_door_nav_visit(surgescript_object_t* object,void* arg){
 bsl_door_nav_t* n=arg;if(surgescript_object_is_killed(object))return false;
 if(strcmp(surgescript_object_name(object),"BSL Teleporter")||!entity_info_exists(object)||!entity_info_is_persistent(object))return true;
 uint64_t id=entity_info_id(object);if(!id)return true;v2d_t pos=entity_info_spawnpoint(object);
 if(!n->first_id||(n->direction>0?id<n->first_id:id>n->first_id)){n->first_id=id;n->first_pos=pos;}
 if((n->direction>0?id>n->last:id<n->last)&&(!n->next_id||(n->direction>0?id<n->next_id:id>n->next_id))){n->next_id=id;n->next_pos=pos;}
 return true;
}
static void bsl_door_navigate(int direction){
 static uint64_t last=0;bsl_door_nav_t n={0};n.last=last;n.direction=direction;
 surgescript_object_traverse_tree_ex(level_ssobject(),&n,bsl_door_nav_visit);
 if(!n.first_id){last=0;editor_status_display("No teleporters in this level",0,NULL);return;}
 last=n.next_id?n.next_id:n.first_id;editor_camera=n.next_id?n.next_pos:n.first_pos;camera_set_position(editor_camera);
 char msg[128];snprintf(msg,sizeof msg,"Teleporter %016llx",(unsigned long long)last);editor_status_display(msg,0,NULL);
}
static bool bsl_workflow_input(void){
 static const int codes[]={ALLEGRO_KEY_PAD_PLUS,ALLEGRO_KEY_PAD_MINUS,ALLEGRO_KEY_PAD_ASTERISK,ALLEGRO_KEY_PAD_SLASH,ALLEGRO_KEY_HOME,ALLEGRO_KEY_INSERT,ALLEGRO_KEY_1,ALLEGRO_KEY_2,ALLEGRO_KEY_3,ALLEGRO_KEY_4,ALLEGRO_KEY_5,ALLEGRO_KEY_6,ALLEGRO_KEY_7,ALLEGRO_KEY_8,ALLEGRO_KEY_9,ALLEGRO_KEY_0,ALLEGRO_KEY_PGUP,ALLEGRO_KEY_PGDN,ALLEGRO_KEY_END};
 static bool previous[sizeof codes/sizeof codes[0]];bool pressed[sizeof codes/sizeof codes[0]];ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);
 for(size_t i=0;i<sizeof codes/sizeof codes[0];i++){bool held=al_key_down(&keys,codes[i]);pressed[i]=held&&!previous[i];previous[i]=held;}
 if(al_key_down(&keys,ALLEGRO_KEY_LCTRL)||al_key_down(&keys,ALLEGRO_KEY_RCTRL)||al_key_down(&keys,ALLEGRO_KEY_LSHIFT)||al_key_down(&keys,ALLEGRO_KEY_RSHIFT)||al_key_down(&keys,ALLEGRO_KEY_ALT))return false;
 if(pressed[18]){BslLevelProbe level={0};bsl_host_level_probe(&level);if(level.enabled&&level.registered&&*level.id){bsl_editor_launch(NULL,level.id);editor_status_display(bsl_editor_status,0,NULL);}else editor_status_display("This level has no registered local definitions",0,NULL);return true;}
 if(pressed[16]||pressed[17]){bsl_door_navigate(pressed[16]?1:-1);return true;}
 if(pressed[4]){editor_camera=spawn_point;camera_set_position(editor_camera);editor_status_display("Camera moved to level spawn",0,NULL);return true;}
 if(pressed[5]){if(editor_ssobj_count>0){editorpal_config_t config={.type=EDITORPAL_SSOBJ,.ssobj={.name=(const char**)editor_ssobj,.count=editor_ssobj_count}};scenestack_push(storyboard_get_scene(SCENE_EDITORPAL),&config);editor_selected_entity_type=EDT_SSOBJ;editor_next_entity();}else sound_play(SFX_DENY);return true;}
 for(int i=0;i<10;i++)if(pressed[6+i]){if(i<editor_brick_filter_count){editor_brick_refresh_filter(i);if(editor_brick_count>0){editorpal_config_t config={.type=EDITORPAL_BRICK,.brick={.id=(const int*)editor_brick,.count=editor_brick_count}};scenestack_push(storyboard_get_scene(SCENE_EDITORPAL),&config);editor_selected_entity_type=EDT_BRICK;editor_next_entity();}}else editor_status_display("No brick category assigned to this number",0,NULL);return true;}
 if(editor_selected_entity_type==EDT_BRICK&&brick_role_preview(editor_selected_entity_id)==BRR_DECORATION){
  if(pressed[0]||pressed[1]){if(editor_stamp_z<0)editor_stamp_z=(int)(brick_zindex_preview(editor_selected_entity_id)*100.0f+0.5f);editor_stamp_z=clip(editor_stamp_z+(pressed[0]?1:-1),0,100);char msg[96];snprintf(msg,sizeof msg,"Stamp Z: %.2f",editor_stamp_z/100.0);editor_status_display(msg,0,NULL);return true;}
  if(pressed[2]||pressed[3]){editor_stamp_brightness=clip(editor_stamp_brightness+(pressed[2]?1:-1),0,15);char msg[96];snprintf(msg,sizeof msg,"Stamp brightness: %d / 15",editor_stamp_brightness);editor_status_display(msg,0,NULL);return true;}
 }
 return false;
}
static bool bsl_brick_style_input(void){
 static bool previous[3];ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);
 int codes[3]={ALLEGRO_KEY_T,ALLEGRO_KEY_OPENBRACE,ALLEGRO_KEY_CLOSEBRACE};bool pressed[3];
 for(int i=0;i<3;i++){bool held=al_key_down(&keys,codes[i]);pressed[i]=held&&!previous[i];previous[i]=held;}
 if(editor_selected_entity_type!=EDT_BRICK||al_key_down(&keys,ALLEGRO_KEY_LCTRL)||al_key_down(&keys,ALLEGRO_KEY_RCTRL))return false;
 if(!pressed[0]&&!pressed[1]&&!pressed[2])return false;
 brick_t* selected=NULL;iterator_t* it=brickmanager_retrieve_all_bricks(brick_manager);
 while(iterator_has_next(it)){brick_t* brick=iterator_next(it);if(!editor_brick_matches_filter(brick))continue;v2d_t p=brick_position(brick),size=brick_size(brick);if(editor_cursor.world_position.x>=p.x&&editor_cursor.world_position.x<p.x+size.x&&editor_cursor.world_position.y>=p.y&&editor_cursor.world_position.y<p.y+size.y&&(!selected||brick_zindex(brick)>=brick_zindex(selected)))selected=brick;}
 iterator_destroy(it);
 brickrole_t role=selected?brick_role_preview(brick_id(selected)):brick_role_preview(editor_selected_entity_id);
 if(pressed[0]&&role==BRR_COLLISION){
  if(!brick_texture_count()){editor_status_display("No terrain textures are registered for this level",0,NULL);return true;}
  editor_texture_target=selected?brick_placement_id(selected):0;editor_texture_pending=true;
  editorpal_config_t config={.type=EDITORPAL_TEXTURE};scenestack_push(storyboard_get_scene(SCENE_EDITORPAL),&config);return true;
 }
 if((pressed[1]||pressed[2])&&role==BRR_DECORATION){int brightness=clip((selected?brick_brightness(selected):editor_stamp_brightness)+(pressed[2]?1:-1),0,15);if(selected){editor_action_t action=bsl_brick_style_action(selected,brightness,brick_texture(selected));action=editor_action_commit(action);editor_action_register(action);}else editor_stamp_brightness=brightness;char message[96];snprintf(message,sizeof message,"Decoration brightness %d / 15",brightness);editor_status_display(message,0,NULL);return true;}
 return false;
}

/* Terrain editor adapter. Signed: President. */
void bsl_cell_editor_open(void){editor_enable();}
bool bsl_cell_editor_active(void){return editor_is_enabled();}

/* Owned connected-test diagnostic; no input or terrain mutation. Signed: President. */
void bsl_cell_native_brick_probe(void){
 int all=0,active=0;brick_t* first=NULL;iterator_t* it=brickmanager_retrieve_all_bricks(brick_manager);
 while(iterator_has_next(it)){iterator_next(it);all++;}iterator_destroy(it);
 it=brickmanager_retrieve_active_bricks(brick_manager);while(iterator_has_next(it)){brick_t* b=iterator_next(it);if(!first)first=b;active++;}iterator_destroy(it);
 v2d_t camera=camera_get_position();
 fprintf(stderr,"CELL NATIVE BRICKS all=%d active=%d camera=%.1f,%.1f\n",all,active,camera.x,camera.y);
 if(first){v2d_t p=brick_position(first);const image_t* image=brick_image(first);unsigned char r,g,b,a;image_lock((image_t*)image,"r");color_unmap(image_getpixel(image,0,0),&r,&g,&b,&a);image_unlock((image_t*)image);
 fprintf(stderr,"CELL NATIVE FIRST position=%.1f,%.1f size=%d,%d brightness=%d rgba=%u,%u,%u,%u role=%d visible=%d\n",p.x,p.y,image_width(image),image_height(image),brick_brightness(first),r,g,b,a,brick_role_preview(brick_id(first)),brick_role_visible(brick_role_preview(brick_id(first)),false));}
}

#include "../editor_shell/level_adapter.inc"
