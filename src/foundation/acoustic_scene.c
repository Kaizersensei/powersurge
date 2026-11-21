// Rasterizes all solid authored/bricklike collision masks, independently of ROI. Signed: Atlas.
#include "spatial_audio.h"
#include "../physics/obstacle.h"
#include <math.h>
#include <stdint.h>
extern void bsl_level_acoustic_visit(void (*visit)(const obstacle_t*,void*),void* data);
extern void bsl_acoustic_terrain_begin(void);
static void raster(const obstacle_t* o,void* data){
 (void)data;point2d_t p=obstacle_get_position(o);int w=obstacle_get_width(o),h=obstacle_get_height(o);
 for(int y=(int)floor(p.y/16.0);y<=(int)floor((p.y+h-1)/16.0);++y)for(int x=(int)floor(p.x/16.0);x<=(int)floor((p.x+w-1)/16.0);++x){
  /* Conservative coverage catches thin walls, masks and slopes without counting brick seams. */
  for(int column=x*16;column<x*16+16;++column)if(obstacle_got_collision(o,column,y*16,column,y*16+15)){bsl_acoustic_cell(x,y,1);break;}
 }
}
void bsl_acoustic_scan_terrain(void){bsl_acoustic_terrain_begin();bsl_level_acoustic_visit(raster,0);}
