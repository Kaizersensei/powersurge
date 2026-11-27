/* Shared native editor shell; commands delegate to existing owners. Signed: President. */
#ifndef BSL_EDITOR_SHELL_H
#define BSL_EDITOR_SHELL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
union ALLEGRO_EVENT;
typedef struct BslShellState {
 bool editing,terrain,terrain_dirty,placements_dirty,readonly,presentation;
 int ready_kind,ready_index;
 int tool,grid,paint_action,brightness,selected_shape,preset,mode,blend_cells,material,materials;
 bool blend,bevel_override,bevel_enabled;
 int bevel_profile,bevel_contact;float bevel_width,bevel_depth,bevel_smoothness;
 float curve,x,y,selected_x,selected_y,zoom;
 uint64_t selection;
 char level[512],name[256],message[512],definition[257];
} BslShellState;
typedef struct BslShellDrop {int kind,index;float x,y;} BslShellDrop;
void bsl_shell_request_drop(int kind,int index,float x,float y);
bool bsl_shell_drop_payload(BslShellDrop*);
typedef struct BslShellField {int index;bool boolean,overridden,live_available;double value,effective,minimum,maximum,step;char name[96];} BslShellField;
void bsl_cell_editor_open(void);
void bsl_shell_event(union ALLEGRO_EVENT*);
void bsl_shell_tick(bool level_scene);
void bsl_shell_render(void);
void bsl_shell_detach(void);
uint64_t bsl_shell_command_target(void);
void bsl_shell_shutdown(void);
bool bsl_shell_present(void);
bool bsl_shell_block_editor(void);
bool bsl_shell_navigation_keyboard(void);
bool bsl_shell_view_fixture(void);
bool bsl_shell_keyboard(void);
void bsl_shell_note(const char* category,const char* message,int severity);
void bsl_shell_status(const char* category,const char* text);
void bsl_shell_submit(const char* command,double value);
bool bsl_shell_pop(char* command,int capacity,double* value);
void bsl_shell_snapshot(BslShellState*);
bool bsl_shell_level_update(void);
bool bsl_shell_physics_body(const char* placement);
bool bsl_shell_native_fixture(int phase);
void bsl_shell_level_begin(void);
void bsl_shell_level_end(void);
void bsl_shell_enter(void);
int bsl_shell_palette_count(int kind);
const char* bsl_shell_palette_name(int kind,int index);
void* bsl_shell_palette_image(int kind,int index);
bool bsl_shell_field(int index,BslShellField*);
bool bsl_shell_level_preflight(void);
void bsl_shell_terrain_thumbs_collect(void);
bool bsl_shell_terrain_preview_fixture(void);
void bsl_shell_terrain_snapshot(BslShellState*);
void bsl_shell_terrain_tool(bool enabled);
void bsl_shell_terrain_command(const char*,double);
bool bsl_shell_terrain_preflight(void);
bool bsl_shell_terrain_save(void);
bool bsl_shell_terrain_pick(double x,double y);
bool bsl_shell_file_capture(const char* path);
bool bsl_shell_file_unchanged(const char* path);
unsigned bsl_shell_debug_get(void);
void bsl_shell_debug_set(unsigned value);
void bsl_shell_settings_save(void);
#ifdef __cplusplus
}
#endif
#endif
