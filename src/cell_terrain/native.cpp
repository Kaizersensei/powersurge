#include "../labs/lab.h"
extern "C" int bsl_shell_paint_action(void);
extern "C" void bsl_shell_terrain_thumbs_clear();
#include "../editor_shell/shell.h"
// First attached level-editor adapter. Existing Surge/BOR physics stays authoritative. Signed: President.
#include "terrain.hpp"
#include "native.h"
#include "materials.hpp"
#include "../foundation/foundation_host.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <memory>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <cstring>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
extern "C" {
#include "../core/image.h"
#include "../core/asset.h"
#include "../core/video.h"

#include "../core/logfile.h"
#include "c_api.h"




}
namespace {
constexpr int origin=4096;
struct Resource {std::array<uint8_t,cell::side*cell::side> coverage{};bool coverage_valid=false;image_t* image=nullptr;std::vector<collisionmask_t*> masks;std::vector<obstacle_t*> obstacles;cell::Key key;};
struct Pixel {uint8_t r,g,b,a;};
struct Texture {int w=0,h=0;std::vector<Pixel> pixels;};
using CellMaterial=cell::Material;std::vector<CellMaterial> cell_materials;
cell::Field field;std::vector<Resource> resources;std::vector<Texture> textures;
std::vector<cell::Document> undo_stack,redo_stack;
std::set<cell::Key> visual_dirty,collision_dirty;
double last_visual=0;
struct BakeStats {uint64_t uploads=0,masks=0,blend_calls=0;double seconds=0;} bake_stats;
std::vector<int> shape_materials(65536,-1);
cell::Document before,published;bool active=false,tool=true,dirty=false,drag=false,pending=false;
int selected=1,brush=32,mode=0,visible_chunks=0;double curve=.5,ax=0,ay=0,lastx=0,lasty=0,cursorx=0,cursory=0;
unsigned previous_buttons=0;std::array<bool,ALLEGRO_KEY_MAX> keys{};
std::string loaded,path,message,terrain_level,terrain_scene,material_path; font_t* label=nullptr;
const CellMaterial* material(const std::string& id){for(const auto& m:cell_materials)if(id==m.id)return &m;return nullptr;}
int material_index(const std::string& id){for(size_t i=0;i<std::size(cell_materials);i++)if(id==cell_materials[i].id)return static_cast<int>(i);return -1;}
std::string read(const std::string& file){if(std::filesystem::file_size(std::filesystem::u8path(file))>4*1024*1024)throw std::runtime_error("Terrain file budget exceeded");std::ifstream i(std::filesystem::u8path(file),std::ios::binary);if(!i)throw std::runtime_error("Cannot read terrain document");return std::string(std::istreambuf_iterator<char>(i),{});}
void release(std::vector<Resource>& r){for(auto& c:r){for(auto* p:c.obstacles)obstacle_destroy(p);for(auto* p:c.masks)collisionmask_destroy(p);if(c.image)image_destroy(c.image);}r.clear();}
void validate_materials(const cell::Document& d){cell::validate(d);for(const auto& s:d.shapes)if(!material(s.material))throw std::runtime_error("Missing material: "+s.material);}
Pixel texel(int n,int x,int y){
 if(n<0)return {255,0,255,255};
 const auto& t=textures[n];int px=x-cell::Field::floor_div(x,t.w)*t.w,py=y-cell::Field::floor_div(y,t.h)*t.h;
 return t.pixels[py*t.w+px];
}
Resource* resource(cell::Key key){for(auto& r:resources)if(r.key==key)return &r;return nullptr;}
bool occupied(cell::Key key){auto i=field.blocks.find(key);return i!=field.blocks.end()&&std::any_of(i->second.cells.begin(),i->second.cells.end(),[](int n){return n!=0;});}
void dirty_chunks(const std::set<cell::Key>& changed){
 int width=0;for(const auto& s:field.doc.shapes)if(s.blend)width=std::max(width,s.blend_cells*cell::base);
 int halo=(width+cell::chunk-1)/cell::chunk;
 for(auto key:changed){
  collision_dirty.insert(key);visual_dirty.insert(key);
  for(int y=-halo;y<=halo;y++)for(int x=-halo;x<=halo;x++){cell::Key n{key.first+x,key.second+y};if(field.blocks.count(n)||resource(n))visual_dirty.insert(n);}
 }
}
void upload(image_t* image,const std::array<Pixel,cell::chunk*cell::chunk>& pixels){
 ALLEGRO_LOCKED_REGION* lock=al_lock_bitmap(IMAGE2BITMAP(image),ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_WRITEONLY);
 if(!lock)throw std::runtime_error("Terrain upload lock failed");
 for(int y=0;y<cell::chunk;y++)std::memcpy(static_cast<char*>(lock->data)+y*lock->pitch,pixels.data()+y*cell::chunk,cell::chunk*sizeof(Pixel));
 al_unlock_bitmap(IMAGE2BITMAP(image));
}
void rebuild(bool full=true,bool sync_collision=true){
 double started=al_get_time();validate_materials(field.doc);
 if(full){for(const auto& c:field.blocks){visual_dirty.insert(c.first);collision_dirty.insert(c.first);}for(const auto& r:resources){visual_dirty.insert(r.key);collision_dirty.insert(r.key);}}
 for(const auto& s:field.doc.shapes)shape_materials[s.id]=material_index(s.material);
 bool held=al_is_bitmap_drawing_held();if(held)al_hold_bitmap_drawing(false);
 image_t* scratch=nullptr;
 struct Staged {cell::Key key;std::array<uint8_t,cell::side*cell::side> coverage{};std::vector<collisionmask_t*> masks;std::vector<obstacle_t*> obstacles;};
 std::vector<Staged> staged;
 try{
  if(!visual_dirty.empty()){
   auto blends=field.blends_for(visual_dirty);bake_stats.blend_calls++;
   std::array<Pixel,cell::chunk*cell::chunk> pixels{};
   for(auto key:visual_dirty){
    auto* r=resource(key);
    if(!r&&occupied(key)){Resource n;n.key=key;resources.push_back(n);r=&resources.back();}
    if(!r)continue;
    if(!r->image){r->image=image_create_ex(cell::chunk,cell::chunk,IC_DEFAULT);if(!r->image)throw std::runtime_error("Terrain image allocation failed");}
    auto block=field.blocks.find(key);auto mix=blends.find(key);
    for(int y=0;y<cell::chunk;y++)for(int x=0;x<cell::chunk;x++){
     int index=(y/cell::step)*cell::side+x/cell::step,id=block==field.blocks.end()?0:block->second.cells[index];
     Pixel result{0,0,0,0};
     if(id){int wx=key.first*cell::chunk+x,wy=key.second*cell::chunk+y;result=texel(shape_materials[id],wx,wy);
      if(mix!=blends.end()){auto m=mix->second[index];if(m.other&&m.weight>0){Pixel other=texel(shape_materials[m.other],wx,wy);double w=m.weight;result={static_cast<uint8_t>(result.r*(1-w)+other.r*w+.5),static_cast<uint8_t>(result.g*(1-w)+other.g*w+.5),static_cast<uint8_t>(result.b*(1-w)+other.b*w+.5),255};}}
     }
     pixels[y*cell::chunk+x]=result;
    }
    upload(r->image,pixels);bake_stats.uploads++;
   }
   visual_dirty.clear();last_visual=al_get_time();
  }
  if(sync_collision&&!collision_dirty.empty()){
   for(auto key:collision_dirty){
    auto* r=resource(key);if(!r)continue;
    std::array<uint8_t,cell::side*cell::side> coverage{};int routes=0;
    auto block=field.blocks.find(key);
    if(block!=field.blocks.end())for(size_t i=0;i<coverage.size();i++){const auto* shape=field.shape(block->second.cells[i]);if(shape){coverage[i]=shape->route+1;routes|=1<<shape->route;}}
    if(r->coverage_valid&&coverage==r->coverage)continue;
    staged.push_back({});auto& next=staged.back();next.key=key;next.coverage=coverage;
    if(routes&&!scratch){int flags=al_get_new_bitmap_flags();al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);scratch=image_create(cell::chunk,cell::chunk);al_set_new_bitmap_flags(flags);if(!scratch)throw std::runtime_error("CPU collision buffer allocation failed");}
    for(int route=0;route<3;route++)if(routes&(1<<route)){
     std::array<Pixel,cell::chunk*cell::chunk> pixels{};
     for(int y=0;y<cell::chunk;y++)for(int x=0;x<cell::chunk;x++)if(coverage[(y/cell::step)*cell::side+x/cell::step]==route+1)pixels[y*cell::chunk+x]={255,255,255,255};
     upload(scratch,pixels);image_lock(scratch,"r");
     collisionmask_t* mask=collisionmask_create(scratch,0,0,cell::chunk,cell::chunk,0);image_unlock(scratch);
     if(!mask)throw std::runtime_error("Collision mask allocation failed");
     next.masks.push_back(mask);next.obstacles.push_back(bsl_cell_obstacle(mask,origin+key.first*cell::chunk,origin+key.second*cell::chunk,route));bake_stats.masks++;
    }
   }
   if(!staged.empty()){
    obstaclemap_clear(const_cast<obstaclemap_t*>(level_obstaclemap()));
    for(auto& next:staged){auto* r=resource(next.key);for(auto* p:r->obstacles)obstacle_destroy(p);for(auto* p:r->masks)collisionmask_destroy(p);
     r->obstacles.swap(next.obstacles);r->masks.swap(next.masks);r->coverage=next.coverage;r->coverage_valid=true;
     // Old pointers have already been freed; staged arrays must not own them.
     next.obstacles.clear();next.masks.clear();
    }
   }
   collision_dirty.clear();
   for(auto i=resources.begin();i!=resources.end();)if(i->obstacles.empty()&&!occupied(i->key)){image_destroy(i->image);i=resources.erase(i);}else ++i;
  }
  if(sync_collision)published=field.doc;
  if(scratch)image_destroy(scratch);
  if(held)al_hold_bitmap_drawing(true);
  bake_stats.seconds+=al_get_time()-started;
 }catch(...){
  for(auto& next:staged){for(auto* p:next.obstacles)obstacle_destroy(p);for(auto* p:next.masks)collisionmask_destroy(p);}
  if(scratch)image_destroy(scratch);
  if(held)al_hold_bitmap_drawing(true);
  throw;
 }
}
cell::Shape& chosen(){for(auto& s:field.doc.shapes)if(s.id==selected)return s;throw std::runtime_error("No selected shape");}
void start_edit(){if(!drag){before=field.doc;drag=true;}}
void commit(){if(drag){rebuild(false);if(cell::encode(before)!=cell::encode(field.doc)){undo_stack.push_back(before);if(undo_stack.size()>64)undo_stack.erase(undo_stack.begin());redo_stack.clear();dirty=true;}drag=false;}}
void stroke(double x,double y,double u,double v,bool erase,int kind){
 cell::Stroke s;s.shape=selected;s.erase=erase;s.x=x;s.y=y;s.u=u;s.v=v;s.size=brush;s.mode=kind;s.curve=curve;
 cell::Field next=field;next.doc.strokes.push_back(s);cell::validate(next.doc);auto changed=next.paint(s);if(changed.empty())return;field=std::move(next);dirty_chunks(changed);
}
void new_shape(const std::string& id){
 start_edit();int next=1;for(const auto& s:field.doc.shapes)next=std::max(next,s.id+1);auto s=chosen();s.id=next;s.material=id;field.doc.shapes.push_back(s);selected=next;commit();message="New shape: "+id;
}
void history(bool backwards){
 commit();auto& from=backwards?undo_stack:redo_stack;auto& to=backwards?redo_stack:undo_stack;
 if(!from.empty()){cell::Field next;next.doc=from.back();next.rebuild();to.push_back(field.doc);field=std::move(next);from.pop_back();if(!field.shape(selected))selected=field.doc.shapes.front().id;rebuild();dirty=true;pending=false;}
}
void reload_base(){
 commit();cell::Field next;std::string source=read(path);next.doc=cell::decode(source);validate_materials(next.doc);next.rebuild();
 field=std::move(next);selected=field.doc.shapes.front().id;rebuild();loaded=source;undo_stack.clear();redo_stack.clear();dirty=false;pending=false;message="Reloaded saved base";
}
bool save(){
 try{
  commit();validate_materials(field.doc);std::string data=cell::encode(field.doc);
  if(read(path)!=loaded)throw std::runtime_error("File changed externally; save refused. Keep draft, reload to resolve.");
  auto destination=std::filesystem::u8path(path),temporary=std::filesystem::u8path(path+".tmp"),backup=std::filesystem::u8path(path+".bak");
  {std::ofstream o(temporary,std::ios::binary|std::ios::trunc);o<<data;o.flush();if(!o)throw std::runtime_error("Temporary terrain write failed");}
  if(cell::encode(cell::decode(read(path+".tmp")))!=data)throw std::runtime_error("Temporary terrain verification failed");
  if(read(path)!=loaded)throw std::runtime_error("Concurrent external edit; save refused");
  std::filesystem::copy_file(destination,backup,std::filesystem::copy_options::overwrite_existing);
  if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Atomic terrain replacement failed");
  loaded=data;dirty=false;message="Terrain saved atomically; previous base in .bak";return true;
 }catch(const std::exception& e){message=e.what();return false;}
}
#include "material_runtime.inc"
#include "editor_tools.inc"
void native_checks();
void trace(){
 const char* p=std::getenv("BSL_CELL_TRACE");if(!p)return;static double last=-1;double now=al_get_time();if(last>=0&&now-last<.25)return;last=now;
 std::ofstream out(std::filesystem::u8path(p));out<<"{\"active\":"<<(active?"true":"false")<<",\"chunks\":"<<field.blocks.size()<<",\"visible_chunks\":"<<visible_chunks<<",\"resources\":"<<resources.size()<<",\"shapes\":"<<field.doc.shapes.size()<<",\"strokes\":"<<field.doc.strokes.size()<<",\"selected\":"<<selected<<",\"dirty\":"<<(dirty?"true":"false")<<",\"uploads\":"<<bake_stats.uploads<<",\"mask_builds\":"<<bake_stats.masks<<",\"queued_masks\":"<<collision_dirty.size()<<",\"origin\":[4096,4096],\"step\":2}";
}
#include "native_perf.inc"
#include "native_checks.inc"
#include "level_journey.inc"
}
#include "../physics_lab/contours.inc"
#include "../physics_lab/live.inc"
extern "C" void bsl_cell_end(){lab_end();release(resources);textures.clear();cell_materials.clear();if(label){font_destroy(label);label=nullptr;}active=false;undo_stack.clear();redo_stack.clear();field={};loaded.clear();terrain_level.clear();terrain_scene.clear();material_path.clear();visual_dirty.clear();collision_dirty.clear();last_visual=0;drag=pending=dirty=false;keys.fill(false);previous_buttons=0;preview={};tools_visible=true;pointer_capture=false;bsl_shell_terrain_thumbs_clear();}
extern "C" void bsl_cell_begin(const char* level){
 bsl_cell_end();BslLevelTerrain attachment{};if(!level||!bsl_host_level_terrain(level,&attachment))return;
 terrain_level=attachment.id;terrain_scene=level;material_path=attachment.materials;
 try{
  path=asset_path(attachment.document);loaded=read(path);field.doc=cell::decode(loaded);field.rebuild();if(field.doc.shapes.empty())throw std::runtime_error("Terrain requires a shape");selected=field.doc.shapes.front().id;
  auto proposed=prepare_materials(read(asset_path(material_path.c_str())));cell_materials=std::move(proposed.records);textures=std::move(proposed.images);
  rebuild();active=true;tool=true;label=font_create(ui_font_role);message="F12 edits this level; F6 switches terrain/object tools";trace();
 }catch(const std::exception& e){logfile_message("Cell terrain load failed: %s",e.what());bsl_cell_end();video_showmessage("Cell terrain refused: %s",e.what());}
}
extern "C" void bsl_cell_obstacles(obstaclemap_t* map){if(active)for(const auto& r:resources)for(auto* o:r.obstacles)obstaclemap_add(map,o);}
extern "C" int bsl_cell_world_height(){return active?8192:0;}
extern "C" bool bsl_cell_painting(){return active&&tool;}
extern "C" bool bsl_cell_tool(){return active&&tool;}
extern "C" bool bsl_cell_actors_clear(void);
extern "C" bool bsl_cell_leave(){lab_end();if(!active)return true;commit();pending=false;try{rebuild(false);if(!bsl_cell_actors_clear()){message="Terrain overlaps an actor. Undo/erase the enclosing area before resuming.";return false;}return true;}catch(const std::exception& e){message=e.what();return false;}}
extern "C" bool bsl_cell_cancel_line(){
 if(!active||!pending)return false;
 pending=false;preview.valid=false;message="Line cancelled; terrain unchanged";return true;
}
extern "C" void bsl_cell_input(float px,float py,int grid,float screen_x,float screen_y){
 if(!active||std::getenv("BSL_CELL_UI_CAPTURE"))return;
 try{
  ALLEGRO_KEYBOARD_STATE state;al_get_keyboard_state(&state);ALLEGRO_MOUSE_STATE mouse;al_get_mouse_state(&mouse);
  auto edge=[&](int k){return al_key_down(&state,k)&&!keys[k];};
  bool ctrl=al_key_down(&state,ALLEGRO_KEY_LCTRL)||al_key_down(&state,ALLEGRO_KEY_RCTRL),shift=al_key_down(&state,ALLEGRO_KEY_LSHIFT)||al_key_down(&state,ALLEGRO_KEY_RSHIFT);
  double x=std::clamp(double(px-origin),-double(cell::limit),double(cell::limit)),y=std::clamp(double(py-origin),-double(cell::limit),double(cell::limit));
  cursorx=px-origin;cursory=py-origin;
  int spacing=std::clamp(grid,8,128);brush=spacing; x=std::floor(x/spacing)*spacing+spacing*.5;y=std::floor(y/spacing)*spacing+spacing*.5;
  bool left=mouse.buttons&1,right=(mouse.buttons&2)||(bsl_shell_paint_action()==1&&(mouse.buttons&1));shift=shift||bsl_shell_paint_action()==2;bool press=left&&!(previous_buttons&1);
  /* F1 is owned by the shared Help command. */
  bool over_tools=false;pointer_x=screen_x;pointer_y=screen_y;
  if(edge(ALLEGRO_KEY_ESCAPE)){pending=false;commit();}
  else if(edge(ALLEGRO_KEY_N))new_shape(chosen().material);
  else if(edge(ALLEGRO_KEY_M)){int n=material_index(chosen().material);new_shape(cell_materials[(n+1)%std::size(cell_materials)].id);}
  else if(edge(ALLEGRO_KEY_P))editor_command("preset");
  else if(edge(ALLEGRO_KEY_B))editor_command("blend");
  else if(edge(ALLEGRO_KEY_OPENBRACE)||edge(ALLEGRO_KEY_CLOSEBRACE))editor_command(edge(ALLEGRO_KEY_OPENBRACE)?"blend_less":"blend_more");
  else if(edge(ALLEGRO_KEY_C))mode=(mode+1)%3;
  else if(edge(ALLEGRO_KEY_EQUALS)||edge(ALLEGRO_KEY_MINUS))curve=std::clamp(curve+(edge(ALLEGRO_KEY_EQUALS)?.1:-.1),0.,1.);
  else if(over_tools){}
  else if(right&&pending){bsl_cell_cancel_line();}
  else if(!ctrl&&shift&&press){
   commit();if(!pending){ax=x;ay=y;pending=true;message="Click line endpoint; Esc cancels";}
   else {start_edit();stroke(ax,ay,x,y,false,mode);commit();pending=false;rebuild(false);}
  }
  else if(!ctrl&&!shift&&(left||right)){
   bool first=!drag;if(first||x!=lastx||y!=lasty){start_edit();stroke(first?x:lastx,first?y:lasty,x,y,right,0);lastx=x;lasty=y;
   // Show the new geometry immediately; the same generated mask serves play mode.
   if(al_get_time()-last_visual>=1./30)rebuild(false,false);}
  }
  if(drag&&!left&&!right)commit();
  if(!over_tools)update_preview(x,y);
  else if(pending&&preview.valid)update_preview(preview.stroke.u,preview.stroke.v);
  previous_buttons=mouse.buttons;for(int k=0;k<ALLEGRO_KEY_MAX;k++)keys[k]=al_key_down(&state,k);trace();
 }catch(const std::exception& e){field.doc=published;field.rebuild();drag=false;if(!field.shape(selected))selected=field.doc.shapes.front().id;message=e.what();}
}
extern "C" void bsl_cell_render(float cx,float cy,bool editing){
 level_journey_tick();
 if(!active)return;
 native_checks();tool_capture();editing=editing||bsl_cell_editor_active();
 if(editing&&drag&&!visual_dirty.empty()&&al_get_time()-last_visual>=1./30){
  try{rebuild(false,false);}catch(const std::exception& e){field.doc=published;field.rebuild();drag=false;visual_dirty.clear();collision_dirty.clear();rebuild();message=e.what();}
 }
 visible_chunks=0;
 float dx=cx-video_get_screen_size().x*.5f,dy=cy-video_get_screen_size().y*.5f;
 for(const auto& r:resources){int x=origin+r.key.first*cell::chunk-static_cast<int>(dx),y=origin+r.key.second*cell::chunk-static_cast<int>(dy);if(x+cell::chunk<0||y+cell::chunk<0||x>video_get_screen_size().x||y>video_get_screen_size().y)continue;visible_chunks++;image_draw(r.image,x,y,IF_NONE);}

 static int captures=0;const char* capture=std::getenv("BSL_CELL_SCREENSHOT");
 if(capture&&++captures==90){image_t* shot=video_take_snapshot();if(shot){const ALLEGRO_FILE_INTERFACE* previous=al_get_new_file_interface();al_set_standard_file_interface();al_save_bitmap(capture,IMAGE2BITMAP(shot));al_set_new_file_interface(previous);image_destroy(shot);}}
 if(capture&&captures%30==0)trace();
}


extern "C" void bsl_cell_overlay(float cx,float cy){
 if(!active)return;
 float dx=cx-video_get_screen_size().x*.5f,dy=cy-video_get_screen_size().y*.5f;

  image_line(origin-static_cast<int>(dx),0,origin-static_cast<int>(dx),video_get_screen_size().y,color_rgb(100,100,170));
  image_line(0,origin-static_cast<int>(dy),video_get_screen_size().x,origin-static_cast<int>(dy),color_rgb(100,100,170));
  draw_preview(dx,dy);
  if(tool&&!bsl_shell_block_editor()){int x=origin+static_cast<int>(std::floor(cursorx/brush))*brush-static_cast<int>(dx),y=origin+static_cast<int>(std::floor(cursory/brush))*brush-static_cast<int>(dy);image_rect(x,y,x+brush,y+brush,color_rgb(255,205,90));}
  /* Unified shell owns controls and status. */
}

#include "../editor_shell/terrain_adapter.inc"

extern "C" bool bsl_shell_physics_body(const char* placement){uint64_t id=0;return lab.factory&&bsl_object_factory_lookup(lab.factory,placement,&id);}
