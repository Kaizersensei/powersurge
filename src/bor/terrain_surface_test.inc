/* Opt-in surface service checks; no source-level saves. Signed: Atlas. */
#include "../core/engine.h"
static bool srf_test_rgb_equal(color_t colour,const unsigned char expected[3]){unsigned char r,g,b,a;color_unmap(colour,&r,&g,&b,&a);return r==expected[0]&&g==expected[1]&&b==expected[2]&&a==255;}
static int srf_test_brightness(image_t* image,int x,int y){unsigned char r,g,b,a;image_lock(image,"r");color_unmap(image_getpixel(image,x,y),&r,&g,&b,&a);image_unlock(image);return r+g+b;}
void brick_surface_test_tick(void){
 const char* requested=getenv("BSL_SURFACE_TEST");static int frames;static bool finished;
 if(!requested||finished||++frames<60)return;
 finished=true;
 int checks=0,failures=0;
#define SURFACE_CHECK(expr,msg) do{bool ok=(expr);checks++;if(!ok)failures++;logfile_message("SURFACE %s: %s",ok?"PASS":"FAIL",msg);}while(0)
 if(!strcmp(requested,"capacity")){
  int count=srf_texture_count;char last_id[65]={0};if(count)snprintf(last_id,sizeof last_id,"%s",srf_textures[count-1].id);
  SURFACE_CHECK(srf_enabled&&count>64&&srf_texture_capacity>=(size_t)count,"dynamic catalog grows beyond64 entries");
  SURFACE_CHECK(srf_texture_pixels>8000000,"catalog exceeds the former aggregate pixel cap");
  SURFACE_CHECK(count>0&&srf_texture_index(last_id)==count-1&&brick_texture_preview(count)!=NULL,"last registered texture and preview remain addressable");
  int full=brick_resolve_definition("shape:full_square");brick_t* probe=level_create_brick(full,v2d_new(4096,640),BRL_DEFAULT,BRF_NOFLIP);
  bool chosen=brick_set_texture(probe,last_id);image_t* rendered=srf_image(probe);unsigned char expected[3]={0};if(count)srf_sample(count-1,4112,656,expected);
  bool matches=false;if(rendered){image_lock(rendered,"r");matches=srf_test_rgb_equal(image_getpixel(rendered,16,16),expected);image_unlock(rendered);}
  SURFACE_CHECK(chosen&&matches,"a texture beyond index64 renders on actual terrain");
  srf_load("materials/collision-lab.surfaces");SURFACE_CHECK(srf_texture_count==count&&srf_texture_index(last_id)==count-1&&brick_texture_preview(count)!=NULL,"clear and reload rebuild the dynamic registry coherently");
  rendered=srf_image(probe);SURFACE_CHECK(rendered&&strcmp(brick_texture(probe),last_id)==0,"placed texture identity survives catalog recreation");
  const char* path=getenv("BSL_SURFACE_REPORT");FILE* report=path?fopen(path,"w"):NULL;
  if(report){fprintf(report,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"textures\":%d,\"capacity\":%zu,\"pixels\":%zu}\n",failures?"false":"true",checks,failures,srf_texture_count,srf_texture_capacity,srf_texture_pixels);fclose(report);}
  engine_quit();return;
 }
 SURFACE_CHECK(srf_enabled&&srf_texture_count>=2,"live surface profile loads registered texture folder");
 int full=brick_resolve_definition("shape:full_square"),cloud_id=brick_resolve_definition("shape:full_square_cloud");
 SURFACE_CHECK(full==0&&cloud_id==1&&brick_type_preview(cloud_id)==BRK_CLOUD,"first full square has its adjacent cloud counterpart");
 SURFACE_CHECK(brick_palette_order(full)==0&&brick_palette_order(cloud_id)==1&&*brick_shape_group(2),"palette ordering and larger-group labels are explicit metadata");
 brick_t* a=level_create_brick(full,v2d_new(4096,640),BRL_DEFAULT,BRF_NOFLIP);const obstacle_t* physical=brick_obstacle(a);
 image_t* image=srf_image(a);srf_chunk_t* cell=srf_cell(BRL_DEFAULT,4127,656);int index=srf_index(4127,656);
 SURFACE_CHECK(image&&cell&&cell->distance[index]==1,"newly placed collider receives live texture and an exposed edge");
 int regeneration=srf_regenerated;srf_image(a);SURFACE_CHECK(srf_regenerated==regeneration,"unchanged frame reuses its surface cache");
 brick_t* b=level_create_brick(full,v2d_new(4128,640),BRL_DEFAULT,BRF_NOFLIP);srf_image(a);cell=srf_cell(BRL_DEFAULT,4127,656);
 SURFACE_CHECK(cell&&cell->distance[index]==0,"adjacent placement removes internal edge without save/reload");
 brick_kill(b);srf_image(a);cell=srf_cell(BRL_DEFAULT,4127,656);
 SURFACE_CHECK(!brick_is_alive(b)&&cell&&cell->distance[index]==1,"editor-style kill refreshes neighbor edge before manager cleanup");
 unsigned char sample[3];int rebuilds=srf_rebuilds;
 SURFACE_CHECK(brick_set_texture(a,"rock"),"texture selector can bind registered material to one placement");
 image=srf_image(a);srf_sample(srf_texture_index("rock"),4112,656,sample);image_lock(image,"r");bool rock=srf_test_rgb_equal(image_getpixel(image,16,16),sample);image_unlock(image);
 SURFACE_CHECK(rock&&srf_rebuilds==rebuilds,"texture choice updates pixels without rebuilding geometry");
 uint64_t revision=srf_revision;SURFACE_CHECK(!brick_set_texture(a,"missing")&&!strcmp(brick_texture(a),"rock")&&srf_revision==revision,"missing texture leaves current choice intact");
 brick_surface_set_light(-1,0);int lit=srf_test_brightness(srf_image(a),0,16);brick_surface_set_light(1,0);int shadow=srf_test_brightness(srf_image(a),0,16);
 SURFACE_CHECK(lit>shadow&&srf_rebuilds==rebuilds,"automatic borders react to light direction without geometry work");
 revision=srf_revision;SURFACE_CHECK(!brick_surface_set_light(NAN,0)&&srf_revision==revision,"invalid light preserves live surface state");
 SURFACE_CHECK(brick_obstacle(a)==physical&&brick_is_alive(a),"material and light edits leave physical obstacle identity intact");
 brick_t* cloud=level_create_brick(cloud_id,v2d_new(4096,704),BRL_DEFAULT,BRF_NOFLIP);image=srf_image(cloud);
 SURFACE_CHECK(brick_obstacle(cloud)&&image&&image_width(image)==32&&image_height(image)==32&&srf_mask(cloud,16,31),"cloud presentation retains the complete authored32px square independently from solver cloudification");
 int grass=brick_resolve_definition("decoration:snakegrass"),before=srf_count;brick_t* decoration=level_create_brick(grass,v2d_new(4096,640),BRL_DEFAULT,BRF_NOFLIP);
 SURFACE_CHECK(!brick_obstacle(decoration)&&srf_count==before&&brick_brightness(decoration)==15,"decoration remains collision-free at neutral brightness");
 brick_set_brightness(decoration,0);SURFACE_CHECK(brick_brightness(decoration)==0,"darkest decoration level is zero");brick_set_brightness(decoration,15);SURFACE_CHECK(brick_brightness(decoration)==15,"neutral decoration level is fifteen");
 SURFACE_CHECK(!srf_image(decoration),"decorative stamps never enter the auto-terrain renderer");
 before=srf_count;surgescript_object_t* platform=level_create_object("BOR Moving Ledge",v2d_new(4320,640));SURFACE_CHECK(platform&&srf_count==before,"moving platform remains a separate entity and is not auto-textured");
 brick_t* remote=level_create_brick(full,v2d_new(12000,640),BRL_GREEN,BRF_HFLIP);srf_image(remote);SURFACE_CHECK(srf_occupied(BRL_GREEN,12016,656)&&!srf_occupied(BRL_DEFAULT,12016,656),"off-screen surface geometry participates on its own terrain route");
 // Find an asymmetric current preset and verify presentation mirrors native mask.
 brick_t* partial=level_create_brick(2,v2d_new(4400,640),BRL_DEFAULT,BRF_HFLIP);image=srf_image(partial);bool matches=image!=NULL;
 if(image){image_lock(image,"r");for(int y=0;y<32;y++)for(int x=0;x<32;x++){bool solid=srf_mask(partial,x,y);bool drawn=!color_is_transparent(image_getpixel(image,x,y));if(solid!=drawn){if(matches){unsigned char r,g,b,a;color_unmap(image_getpixel(image,x,y),&r,&g,&b,&a);logfile_message("SURFACE mismatch x%d y%d solid%d drawn%d rgba%d,%d,%d,%d size%d,%d",x,y,solid,drawn,r,g,b,a,image_width(image),image_height(image));}matches=false;}}image_unlock(image);}
 SURFACE_CHECK(matches,"flipped partial shapes retain exact visual coverage");
 const char* path=getenv("BSL_SURFACE_REPORT");FILE* report=path?fopen(path,"w"):NULL;
 if(report){fprintf(report,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"rebuilds\":%d,\"surface_images\":%d,\"chunks\":%d,\"textures\":%d}\n",failures?"false":"true",checks,failures,srf_rebuilds,srf_regenerated,srf_chunk_count,srf_texture_count);fclose(report);}
 engine_quit();
#undef SURFACE_CHECK
}
