
static int shell_paint_action=0;
// UI-neutral access to the existing terrain owner/history. Signed: President.
extern "C" void bsl_shell_terrain_snapshot(BslShellState* out){
 out->paint_action=shell_paint_action;out->terrain=active;out->terrain_dirty=dirty;
 if(!active)return;
 const auto& s=chosen();out->selected_shape=selected;out->preset=s.preset=="round";
 out->mode=mode;out->curve=curve;out->blend=s.blend;out->blend_cells=s.blend_cells;
 out->material=material_index(s.material);out->materials=static_cast<int>(cell_materials.size());
 auto bevel=selected_bevel();out->bevel_override=s.bevel.has_value();out->bevel_enabled=bevel.enabled;out->bevel_profile=bevel.profile=="round"?1:bevel.profile=="concave"?2:0;out->bevel_contact=bevel.contact=="continuous"?1:bevel.contact=="soft_seam"?2:0;out->bevel_width=bevel.width_m;out->bevel_depth=bevel.depth_m;out->bevel_smoothness=bevel.smoothness;

 snprintf(out->message,sizeof out->message,"%s",message.c_str());
}
extern "C" void bsl_shell_terrain_tool(bool enabled){
 if(drag)try{commit();}catch(const std::exception& e){bsl_shell_note("terrain",e.what(),2);return;}tool=enabled&&active;pending=false;preview.valid=false;
}
extern "C" void bsl_shell_terrain_command(const char* id,double value){
 if(!active)return;
 struct RefreshPreview {~RefreshPreview(){if(pending&&preview.valid)update_preview(preview.stroke.u,preview.stroke.v);}} refresh;
 try{
  if(!strcmp(id,"brush")){brush=std::clamp((int)value,8,128);return;}
  if(!strcmp(id,"action")){commit();pending=false;preview.valid=false;shell_paint_action=std::clamp((int)value,0,2);return;}
  if(!strcmp(id,"finish")){commit();return;}
  if(!strcmp(id,"line")){pending=false;preview.valid=false;return;}
  if(!strcmp(id,"material")){assign_material(static_cast<int>(value));return;}
  if(!strncmp(id,"bevel.",6)){if(!std::isfinite(value))throw std::runtime_error("Invalid bevel value");auto b=selected_bevel();
   edit_surface([&]{if(!strcmp(id,"bevel.inherit")){chosen().bevel.reset();return;}
    if(!strcmp(id,"bevel.enabled"))b.enabled=value!=0;else if(!strcmp(id,"bevel.profile"))b.profile=value==1?"round":value==2?"concave":"linear";
    else if(!strcmp(id,"bevel.contact"))b.contact=value==1?"continuous":value==2?"soft_seam":"separate";
    else if(!strcmp(id,"bevel.width"))b.width_m=value;else if(!strcmp(id,"bevel.depth"))b.depth_m=value;else if(!strcmp(id,"bevel.smoothness"))b.smoothness=value;else throw std::runtime_error("Unknown bevel setting");chosen().bevel=b;});return;
  }
  if(!strcmp(id,"curve")){curve=std::clamp(value,0.,1.);return;}
  if(!strcmp(id,"mode_set")){mode=std::clamp(static_cast<int>(value),0,2);return;}
  if(!strcmp(id,"select")){selected=static_cast<int>(value);return;}
  editor_command(id);
 }catch(const std::exception& e){message=e.what();bsl_shell_note("terrain",e.what(),2);}
}
extern "C" bool bsl_shell_terrain_preflight(){
 if(!active||!dirty)return true;
 try{commit();validate_materials(field.doc);if(read(path)!=loaded)throw std::runtime_error("Terrain changed externally; save refused");return true;}
 catch(const std::exception& e){bsl_shell_note("save",e.what(),2);return false;}
}
extern "C" bool bsl_shell_terrain_save(){if(!active||!dirty)return true;return save();}
extern "C" bool bsl_shell_terrain_pick(double x,double y){
 if(!active)return false;
 const auto* s=field.sample(x-origin,y-origin);if(!s)return false;selected=s->id;return true;
}
extern "C" const char* bsl_shell_material_name(int index){return active&&index>=0&&index<(int)cell_materials.size()?cell_materials[index].name.c_str():"";}
namespace {std::vector<image_t*> shell_thumbs,shell_retired_thumbs;}
extern "C" void bsl_shell_terrain_thumbs_clear(){shell_retired_thumbs.insert(shell_retired_thumbs.end(),shell_thumbs.begin(),shell_thumbs.end());shell_thumbs.clear();}
extern "C" void* bsl_shell_material_image(int index){
 if(!active||index<0||index>=(int)textures.size())return nullptr;
 if(shell_thumbs.size()!=textures.size()){bsl_shell_terrain_thumbs_clear();shell_thumbs.resize(textures.size(),nullptr);}
 if(!shell_thumbs[index]){
  if(std::count_if(shell_thumbs.begin(),shell_thumbs.end(),[](auto* p){return p!=nullptr;})>=128){bsl_shell_terrain_thumbs_clear();shell_thumbs.resize(textures.size(),nullptr);}
  try{
   Texture temporary;const Texture* source=&textures[index];if(source->w==0){auto image=cell::load_material_images({cell_materials[index]},[](const std::string& path){return asset_exists(path.c_str())?al_fopen(asset_path(path.c_str()),"rb"):nullptr;});temporary=std::move(image.front());source=&temporary;}
   const auto& t=*source;auto* p=image_create_ex(64,64,IC_DEFAULT);if(!p)return nullptr;
   auto* lock=al_lock_bitmap(IMAGE2BITMAP(p),ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_WRITEONLY);if(!lock){image_destroy(p);return nullptr;}
   double factor=64./std::max(t.w,t.h);int w=std::max(1,int(t.w*factor)),h=std::max(1,int(t.h*factor)),ox=(64-w)/2,oy=(64-h)/2;
   for(int y=0;y<64;++y){auto* row=(Pixel*)((char*)lock->data+y*lock->pitch);for(int x=0;x<64;++x)row[x]=(x>=ox&&y>=oy&&x<ox+w&&y<oy+h)?t.pixels[std::min(t.h-1,(y-oy)*t.h/h)*t.w+std::min(t.w-1,(x-ox)*t.w/w)]:Pixel{0,0,0,0};}
   al_unlock_bitmap(IMAGE2BITMAP(p));shell_thumbs[index]=p;
  }catch(const std::exception& e){bsl_shell_note("material",e.what(),1);return nullptr;}

 }return IMAGE2BITMAP(shell_thumbs[index]);
}

extern "C" int bsl_shell_paint_action(void){return shell_paint_action;}

extern "C" void bsl_shell_terrain_thumbs_collect(){for(auto* p:shell_retired_thumbs)if(p)image_destroy(p);shell_retired_thumbs.clear();}

extern "C" bool bsl_shell_terrain_preview_fixture(){
 if(!std::getenv("BSL_SHELL_CHECK")||!active)return false;
 auto old_preview=preview;bool old_pending=pending,old_tool=tool;double old_ax=ax,old_ay=ay,old_curve=curve;int old_brush=brush,old_mode=mode;
 auto document=cell::encode(field.doc);auto counters=bake_stats;
 tool=true;pending=true;ax=-96;ay=-64;mode=0;curve=.5;update_preview(96,-64);
 bsl_shell_terrain_command("curve",.8);bsl_shell_terrain_command("mode_set",1);bsl_shell_terrain_command("brush",16);
 bool okay=preview.valid&&!preview.lines.empty()&&preview.stroke.mode==1&&preview.stroke.size==16&&std::abs(preview.stroke.curve-.8)<.001&&cell::encode(field.doc)==document&&bake_stats.uploads==counters.uploads&&bake_stats.masks==counters.masks;
 bool cancelled=bsl_cell_cancel_line();okay=okay&&cancelled&&!pending&&!preview.valid&&!bsl_cell_cancel_line()&&cell::encode(field.doc)==document;
 preview=std::move(old_preview);pending=old_pending;tool=old_tool;ax=old_ax;ay=old_ay;curve=old_curve;brush=old_brush;mode=old_mode;return okay;
}
