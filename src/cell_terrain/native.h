/* Embedded terrain lab boundary. Signed: President. */
#ifndef BSL_CELL_NATIVE_H
#define BSL_CELL_NATIVE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
struct obstaclemap_t;
void bsl_cell_begin(const char* level);
void bsl_cell_end(void);
void bsl_cell_obstacles(struct obstaclemap_t* map);
void bsl_cell_overlay(float camera_x,float camera_y);
void bsl_cell_render(float camera_x,float camera_y,bool editing);
bool bsl_cell_tool(void);
bool bsl_cell_painting(void);
int bsl_cell_world_height(void);
void bsl_cell_input(float x,float y,int grid,float screen_x,float screen_y);
bool bsl_cell_leave(void);
bool bsl_cell_cancel_line(void);
#ifdef __cplusplus
}
#endif
#endif
