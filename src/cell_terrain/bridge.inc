/* Read-only edit/play safety guard; no second actor controller. Signed: President. */
#include "../cell_terrain/native.h"
bool bsl_cell_actors_clear(void) {
 obstaclemap_t* map=obstaclemap_create();bsl_cell_obstacles(map);obstaclemap_build(map);
 bool clear=true;
 for(int i=0;i<64&&clear;i++)if(slots[i]&&slots[i]->placed&&slots[i]->a.health>0)
  clear=body_clear(slots[i],map,slots[i]->a.x,slots[i]->a.y);
 obstaclemap_destroy(map);return clear;
}
