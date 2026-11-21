/* Native derived terrain presentation. Geometry remains authoritative. Signed: Atlas. */
#define SRF_SIZE 256
#define SRF_PIXELS (SRF_SIZE*SRF_SIZE)
#define SRF_MAX_CHUNKS 256
#define SRF_MAX_PIXELS 2000000
#include <limits.h>
#include <stdint.h>
typedef struct {char id[65],path[257];int width,height;unsigned char* rgba;image_t* preview;} srf_texture_t;
typedef struct {int route,x,y;unsigned char occupied[SRF_PIXELS],distance[SRF_PIXELS];signed char nx[SRF_PIXELS],ny[SRF_PIXELS];} srf_chunk_t;
typedef struct {int chunk,index;} srf_node_t;
static srf_texture_t* srf_textures;
static size_t srf_texture_capacity;
static int srf_texture_count,srf_base,srf_edge,srf_width=8;
static size_t srf_texture_pixels;
static double srf_edge_mix=.25,srf_light_x=-.6,srf_light_y=-.8,srf_light_strength=.3;
static brick_t** srf_bricks;static int srf_count,srf_capacity;
static srf_chunk_t** srf_chunks;static int srf_chunk_count;
static uint64_t srf_revision=1,srf_geometry_revision,srf_geometry_requested=1;
static bool srf_enabled,srf_failed;
static int srf_rebuilds,srf_regenerated;
static int srf_floor(int n){return n>=0?n/SRF_SIZE:(n-(SRF_SIZE-1))/SRF_SIZE;}
static int srf_index(int x,int y){int lx=x-srf_floor(x)*SRF_SIZE,ly=y-srf_floor(y)*SRF_SIZE;return ly*SRF_SIZE+lx;}
static int srf_find(int route,int x,int y,bool create){
 for(int i=0;i<srf_chunk_count;i++)if(srf_chunks[i]->route==route&&srf_chunks[i]->x==x&&srf_chunks[i]->y==y)return i;
 if(!create||srf_chunk_count==SRF_MAX_CHUNKS)return -1;
 srf_chunks=reallocx(srf_chunks,(srf_chunk_count+1)*sizeof(*srf_chunks));
 srf_chunk_t* chunk=calloc(1,sizeof(*chunk));if(!chunk)fatal_error("Terrain surface allocation failed");chunk->route=route;chunk->x=x;chunk->y=y;srf_chunks[srf_chunk_count]=chunk;return srf_chunk_count++;
}
static srf_chunk_t* srf_cell(int route,int x,int y){int at=srf_find(route,srf_floor(x),srf_floor(y),false);return at<0?NULL:srf_chunks[at];}
static bool srf_occupied(int route,int x,int y){srf_chunk_t* c=srf_cell(route,x,y);return c&&c->occupied[srf_index(x,y)];}
static int srf_texture_index(const char* id){if(!id||!*id)return srf_base;for(int i=0;i<srf_texture_count;i++)if(!strcmp(id,srf_textures[i].id))return i;return -1;}
static bool srf_eligible(const brick_t* b){return b&&b->brick_ref->role==BRR_COLLISION&&b->brick_ref->mask&&!brick_has_movement_path(b)&&brick_is_alive(b);}
static void srf_dirty(void){srf_revision++;if(!srf_revision)srf_revision=1;}
static void srf_geometry_dirty(void){srf_dirty();srf_geometry_requested++;if(!srf_geometry_requested)srf_geometry_requested=1;}
static void srf_drop_chunks(void){for(int i=0;i<srf_chunk_count;i++)free(srf_chunks[i]);free(srf_chunks);srf_chunks=NULL;srf_chunk_count=0;}
static void srf_clear(void){
 for(int i=0;i<srf_texture_count;i++){free(srf_textures[i].rgba);if(srf_textures[i].preview)image_unload(srf_textures[i].preview);}
 free(srf_textures);srf_textures=NULL;srf_texture_capacity=0;srf_texture_count=0;srf_texture_pixels=0;srf_enabled=false;srf_failed=false;srf_base=srf_edge=0;srf_drop_chunks();srf_geometry_dirty();
}
static void srf_register(brick_t* b){
 b->surface_image=NULL;b->surface_revision=0;b->brightness=15;b->texture_id[0]=0;
 if(b->brick_ref->role!=BRR_COLLISION)return;
 if(srf_count==srf_capacity){srf_capacity=srf_capacity?srf_capacity*2:128;srf_bricks=reallocx(srf_bricks,srf_capacity*sizeof(*srf_bricks));}
 srf_bricks[srf_count++]=b;srf_geometry_dirty();
}
static void srf_unregister(brick_t* b){
 if(b->surface_image)image_destroy(b->surface_image);
 for(int i=0;i<srf_count;i++)if(srf_bricks[i]==b){srf_bricks[i]=srf_bricks[--srf_count];srf_geometry_dirty();break;}
 if(!srf_count){free(srf_bricks);srf_bricks=NULL;srf_capacity=0;srf_drop_chunks();}
}
static const collisionmask_t* srf_presentation_mask(const brick_t* b){return b->brick_ref->surface_mask?b->brick_ref->surface_mask:b->brick_ref->mask;}
static bool srf_mask(const brick_t* b,int x,int y){const collisionmask_t* mask=srf_presentation_mask(b);int w=collisionmask_width(mask),h=collisionmask_height(mask);if(b->flip&BRF_HFLIP)x=w-1-x;if(b->flip&BRF_VFLIP)y=h-1-y;return collisionmask_pixel_test(mask,x,y);}
static bool srf_rebuild(void){
 srf_drop_chunks();srf_failed=false;int covered=0;
 for(int b=0;b<srf_count;b++)if(srf_eligible(srf_bricks[b])){
  brick_t* brick=srf_bricks[b];int w=collisionmask_width(brick->brick_ref->mask),h=collisionmask_height(brick->brick_ref->mask);
  for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(srf_mask(brick,x,y)){
   int wx=brick->x+x,wy=brick->y+y,at=srf_find(brick->layer,srf_floor(wx),srf_floor(wy),true);
   if(at<0)goto saturated;
   srf_chunk_t* c=srf_chunks[at];int index=srf_index(wx,wy);
   if(!c->occupied[index]){if(++covered>SRF_MAX_PIXELS)goto saturated;c->occupied[index]=1;}
  }
 }
 srf_node_t* queue=mallocx((covered?covered:1)*sizeof(*queue));int head=0,tail=0;
 for(int c=0;c<srf_chunk_count;c++)for(int at=0;at<SRF_PIXELS;at++)if(srf_chunks[c]->occupied[at]){
  srf_chunk_t* cell=srf_chunks[c];int route=cell->route,x=cell->x*SRF_SIZE+at%SRF_SIZE,y=cell->y*SRF_SIZE+at/SRF_SIZE;
  if(srf_occupied(route,x-1,y)&&srf_occupied(route,x+1,y)&&srf_occupied(route,x,y-1)&&srf_occupied(route,x,y+1))continue;
  double gx=0,gy=0;
  for(int d=-3;d<=3;d++){gx+=(int)srf_occupied(route,x+3,y+d)-(int)srf_occupied(route,x-3,y+d);gy+=(int)srf_occupied(route,x+d,y+3)-(int)srf_occupied(route,x+d,y-3);}
  if(gx==0&&gy==0){gx=(int)srf_occupied(route,x+1,y)-(int)srf_occupied(route,x-1,y);gy=(int)srf_occupied(route,x,y+1)-(int)srf_occupied(route,x,y-1);}
  double length=hypot(gx,gy);cell->nx[at]=length?round(-127*gx/length):0;cell->ny[at]=length?round(-127*gy/length):-127;cell->distance[at]=1;queue[tail++]=(srf_node_t){c,at};
 }
 while(head<tail){
  srf_node_t node=queue[head++];srf_chunk_t* cell=srf_chunks[node.chunk];int index=node.index,distance=cell->distance[index];
  if(distance>=srf_width)continue;
  int x=cell->x*SRF_SIZE+index%SRF_SIZE,y=cell->y*SRF_SIZE+index/SRF_SIZE;
  const int dx[4]={-1,1,0,0},dy[4]={0,0,-1,1};
  for(int d=0;d<4;d++){int wx=x+dx[d],wy=y+dy[d],next=srf_find(cell->route,srf_floor(wx),srf_floor(wy),false);if(next<0)continue;srf_chunk_t* other=srf_chunks[next];int at=srf_index(wx,wy);if(other->occupied[at]&&!other->distance[at]){other->distance[at]=distance+1;other->nx[at]=cell->nx[index];other->ny[at]=cell->ny[index];queue[tail++]=(srf_node_t){next,at};}}
 }
 free(queue);srf_geometry_revision=srf_geometry_requested;srf_rebuilds++;return true;
 saturated:
 srf_failed=true;srf_geometry_revision=srf_geometry_requested;logfile_message("Terrain surface budget saturated; physics retained, displaying collision previews");return false;
}
static void srf_sample(int index,int x,int y,unsigned char rgb[3]){srf_texture_t* t=srf_textures+index;x=((x%t->width)+t->width)%t->width;y=((y%t->height)+t->height)%t->height;int at=(y*t->width+x)*4;for(int c=0;c<3;c++)rgb[c]=t->rgba[at+c];}
static image_t* srf_image(brick_t* b){
 if(!srf_enabled||!srf_eligible(b))return NULL;
 if(srf_geometry_revision!=srf_geometry_requested&&!srf_rebuild())return NULL;
 if(srf_failed)return NULL;
 if(b->surface_image&&b->surface_revision==srf_revision)return b->surface_image;
 int w=collisionmask_width(b->brick_ref->mask),h=collisionmask_height(b->brick_ref->mask),texture=srf_texture_index(b->texture_id);if(texture<0)texture=srf_base;
 if(w>512||h>512){logfile_message("Terrain surface piece exceeds512px; collision preview retained");return NULL;}
 if(b->surface_image)image_destroy(b->surface_image);
 bool held=al_is_bitmap_drawing_held();if(held)image_hold_drawing(false);image_t* previous=image_drawing_target();b->surface_image=image_create(w,h);image_set_drawing_target(b->surface_image);image_clear(color_rgba(0,0,0,0));image_lock(b->surface_image,"w");
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  /* A WRITEONLY lock need not preserve the earlier clear. Initialize holes too. */
  if(!srf_mask(b,x,y)){image_putpixel(x,y,color_rgba(0,0,0,0));continue;}
  int wx=b->x+x,wy=b->y+y;unsigned char colour[3],edge[3];srf_sample(texture,wx,wy,colour);srf_sample(srf_edge,wx,wy,edge);srf_chunk_t* cell=srf_cell(b->layer,wx,wy);int at=srf_index(wx,wy),distance=cell?cell->distance[at]:0;
  double band=distance?1.0-(double)(distance-1)/srf_width:0,light=cell?((double)cell->nx[at]*srf_light_x+cell->ny[at]*srf_light_y)/127:0;
  for(int c=0;c<3;c++){double value=(colour[c]*(1-srf_edge_mix*band)+edge[c]*srf_edge_mix*band)*(1+srf_light_strength*light*band);colour[c]=(unsigned char)clip(round(value),0,255);}
  image_putpixel(x,y,color_rgb(colour[0],colour[1],colour[2]));
 }
 image_unlock(b->surface_image);image_set_drawing_target(previous);if(held)image_hold_drawing(true);b->surface_revision=srf_revision;srf_regenerated++;return b->surface_image;
}
static bool srf_render(brick_t* b,v2d_t camera,bool editor){
 if(!srf_enabled||!srf_eligible(b))return false;
 image_t* image=srf_image(b);if(!image)return false;
 if(brick_role_visible(BRR_DECORATION,editor)){v2d_t top=v2d_subtract(camera,v2d_multiply(video_get_screen_size(),.5f));image_draw(image,b->x-(int)top.x,b->y-(int)top.y,IF_NONE);}
 return true;
}
static int srf_config_statement(const parsetree_statement_t* statement){
 const char* key=nanoparser_get_identifier(statement);const parsetree_parameter_t* p=nanoparser_get_parameter_list(statement);const char* values[4];for(int i=0;i<4;i++){const parsetree_parameter_t* value=nanoparser_get_nth_parameter(p,i+1);values[i]=value?nanoparser_get_string(value):"";}
 if(!strcmp(key,"texture")){
  if((size_t)srf_texture_count==srf_texture_capacity){
   size_t next=srf_texture_capacity?srf_texture_capacity*2:32;
   if(next<=srf_texture_capacity||next>(size_t)INT_MAX-1||next>SIZE_MAX/sizeof(*srf_textures))fatal_error("Terrain texture catalog exceeds addressable storage");
   srf_textures=reallocx(srf_textures,next*sizeof(*srf_textures));
   memset(srf_textures+srf_texture_capacity,0,(next-srf_texture_capacity)*sizeof(*srf_textures));srf_texture_capacity=next;
  }
  srf_texture_t* t=srf_textures+srf_texture_count;
  str_cpy(t->id,values[0],sizeof t->id);str_cpy(t->path,values[1],sizeof t->path);if(!*t->id||!strcmp(t->id,"default")||srf_texture_index(t->id)>=0)fatal_error("Invalid/duplicate terrain texture ID");
  image_t* image=image_load(t->path);t->preview=image;t->width=image_width(image);t->height=image_height(image);if(t->width<1||t->height<1||t->width>1024||t->height>1024)fatal_error("Terrain texture exceeds1024px");
  size_t pixels=(size_t)t->width*t->height;if(pixels>SIZE_MAX-srf_texture_pixels)fatal_error("Terrain texture accounting overflow");srf_texture_pixels+=pixels;
  t->rgba=mallocx(t->width*t->height*4);image_lock(image,"r");for(int y=0;y<t->height;y++)for(int x=0;x<t->width;x++){int at=(y*t->width+x)*4;color_t colour=image_getpixel(image,x,y);if(color_is_transparent(colour))fatal_error("Terrain fill textures must be opaque");color_unmap(colour,t->rgba+at,t->rgba+at+1,t->rgba+at+2,t->rgba+at+3);}image_unlock(image);srf_texture_count++;
 }else if(!strcmp(key,"base")){srf_base=srf_texture_index(values[0]);if(srf_base<0)fatal_error("Unknown base terrain texture");}
 else if(!strcmp(key,"edge")){srf_edge=srf_texture_index(values[0]);srf_width=atoi(values[1]);srf_edge_mix=atof(values[2]);if(srf_edge<0||srf_width<1||srf_width>16||!isfinite(srf_edge_mix)||srf_edge_mix<0||srf_edge_mix>1)fatal_error("Invalid terrain edge profile");}
 else if(!strcmp(key,"light")){srf_light_x=atof(values[0]);srf_light_y=atof(values[1]);srf_light_strength=atof(values[2]);double length=hypot(srf_light_x,srf_light_y);if(!isfinite(length)||length<.001||!isfinite(srf_light_strength)||srf_light_strength<0||srf_light_strength>1)fatal_error("Invalid terrain light");srf_light_x/=length;srf_light_y/=length;}
 else fatal_error("Unknown terrain surface profile command: %s",key);
 return 0;
}
static void srf_load(const char* file){srf_clear();parsetree_program_t* program=nanoparser_construct_tree(asset_path(file));nanoparser_traverse_program(program,srf_config_statement);nanoparser_deconstruct_tree(program);if(!srf_texture_count)fatal_error("No terrain textures registered");srf_enabled=true;srf_dirty();}
int brick_brightness(const brick_t* brick){return brick?brick->brightness:15;}
void brick_set_brightness(brick_t* brick,int value){if(brick)brick->brightness=clip(value,0,15);}
const char* brick_texture(const brick_t* brick){return brick?brick->texture_id:"";}
bool brick_set_texture(brick_t* brick,const char* id){if(!brick||!id||(*id&&srf_texture_index(id)<0)||strlen(id)>=sizeof brick->texture_id)return false;if(strcmp(brick->texture_id,id)){str_cpy(brick->texture_id,id,sizeof brick->texture_id);brick->surface_revision=0;}return true;}
bool brick_surface_set_light(double x,double y){double length=hypot(x,y);if(!isfinite(length)||length<.001)return false;srf_light_x=x/length;srf_light_y=y/length;srf_dirty();return true;}
bool brick_has_surface(const brick_t* brick){return srf_enabled&&!srf_failed&&srf_eligible(brick);}
int brick_palette_order(int id){return brick_exists(id)?brickdata[id]->palette_order:id;}
const char* brick_shape_group(int id){return brick_exists(id)?brickdata[id]->shape_group:"";}
/* Catalog index0 means inherited level default, not an explicit texture. */
int brick_texture_count(void){return srf_enabled?srf_texture_count+1:0;}
const char* brick_texture_id(int index){return index>0&&index<=srf_texture_count?srf_textures[index-1].id:"";}
const char* brick_texture_label(int index){return index==0?"Level default":brick_texture_id(index);}
const image_t* brick_texture_preview(int index){int at=index==0?srf_base:index-1;return srf_enabled&&at>=0&&at<srf_texture_count?srf_textures[at].preview:NULL;}
