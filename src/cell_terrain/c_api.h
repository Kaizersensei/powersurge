/* Narrow opaque C facade: upstream C enum forward declarations are not C++ headers. Signed: President. */
#ifndef BSL_CELL_C_API_H
#define BSL_CELL_C_API_H
#include "../core/image.h"
#ifdef __cplusplus
extern "C" {
#endif
struct font_t;
struct collisionmask_t;
struct obstacle_t;
struct obstaclemap_t;
void bsl_cell_editor_open(void);
void bsl_cell_native_brick_probe(void);
bool bsl_cell_editor_active(void);
struct font_t* font_create(const char*);
void font_destroy(struct font_t*);
v2d_t font_get_textsize(const struct font_t*);
void font_set_text(struct font_t*,const char*,...);
void font_set_position(struct font_t*,v2d_t);
void font_set_width(struct font_t*,int);
void font_render(struct font_t*,v2d_t);
struct collisionmask_t* collisionmask_create(const image_t*,int,int,int,int,int);
struct collisionmask_t* collisionmask_destroy(struct collisionmask_t*);
struct obstacle_t* obstacle_destroy(struct obstacle_t*);
void obstaclemap_add(struct obstaclemap_t*,const struct obstacle_t*);
void obstaclemap_clear(struct obstaclemap_t*);
const struct obstaclemap_t* level_obstaclemap(void);
struct obstaclemap_t* obstaclemap_create(void);
struct obstaclemap_t* obstaclemap_destroy(struct obstaclemap_t*);
void obstaclemap_build(struct obstaclemap_t*);
bool bsl_cell_point(const struct obstaclemap_t*,int,int,int);
struct obstacle_t* bsl_cell_obstacle(struct collisionmask_t*,int,int,int);
#ifdef __cplusplus
}
#endif
#endif
