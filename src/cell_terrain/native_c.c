/* C-only native enum boundary. Signed: President. */
#include "../physics/obstacle.h"
#include "../physics/obstaclemap.h"
#include "../cell_terrain/c_api.h"
obstacle_t* bsl_cell_obstacle(collisionmask_t* mask,int x,int y,int route) {
 return obstacle_create(mask,(point2d_t){x,y},(obstaclelayer_t)route,0);
}

bool bsl_cell_point(const obstaclemap_t* map,int x,int y,int route){return obstaclemap_solid_exists(map,x,y,(obstaclelayer_t)route);}
