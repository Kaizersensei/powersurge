// Opt-in checks use actual editor transactions and private files; never normal saves. Signed: President.
void native_checks(){
 static bool ran=false;const char* report=std::getenv("BSL_CELL_CHECK");if(!report||ran)return;ran=true;
 auto original=field.doc;std::string original_path=path,original_loaded=loaded;int original_selected=selected;bool passed=true;int checks=0;
 auto check=[&](bool b,const char* why){if(!b)throw std::runtime_error(why);checks++;};
 try{
  check(active&&!resources.empty(),"material geometry not active");
  auto* map=obstaclemap_create();bsl_cell_obstacles(map);obstaclemap_build(map);
  bool same=true;
  for(auto& r:resources){
   image_lock(r.image,"r");
   for(int y=0;y<cell::chunk&&same;y++)for(int x=0;x<cell::chunk&&same;x++){
    const auto* sh=field.sample(r.key.first*cell::chunk+x,r.key.second*cell::chunk+y);
    uint8_t rr,g,b,a;color_unmap(image_getpixel(r.image,x,y),&rr,&g,&b,&a);
    same=(a>0)==(sh!=nullptr);
    if(sh)same=same&&bsl_cell_point(map,origin+r.key.first*cell::chunk+x,origin+r.key.second*cell::chunk+y,sh->route);
   }image_unlock(r.image);
  }
  obstaclemap_destroy(map);check(same,"render/collision coverage mismatch");
  const auto* sampled=field.sample(-160,96);check(sampled&&sampled->id==1&&sampled->blend&&sampled->blend_cells==2,"sampled shape association lost");
  selected=sampled->id;start_edit();stroke(-1536,-320,-1536,-320,false,0);commit();rebuild();
  check(field.sample(-1536,-320)!=nullptr,"signed paint failed");
  map=obstaclemap_create();bsl_cell_obstacles(map);obstaclemap_build(map);
  bool distant_present=bsl_cell_point(map,origin-1536,origin-320,0);obstaclemap_destroy(map);
  check(distant_present,"off-screen terrain collision absent");
  history(true);check(field.sample(-1536,-320)==nullptr,"undo did not remove painted coverage");
  history(false);check(field.sample(-1536,-320)!=nullptr,"redo did not restore coverage");
  std::filesystem::path test=std::filesystem::u8path(report).parent_path()/"save-check.json";
  path=test.u8string();loaded=cell::encode(original);{std::ofstream o(test,std::ios::binary);o<<loaded;}
  std::string prior=loaded;check(save(),"atomic save failed");
  check(read(path+".bak")==prior&&read(path)==cell::encode(field.doc),"save backup/commit mismatch");
  std::string external="{broken external content";{std::ofstream o(test,std::ios::binary);o<<external;}
  check(!save()&&read(path)==external,"external edit overwritten");
  std::string state=cell::encode(field.doc);bool refused=false;try{reload_base();}catch(...){refused=true;}
  check(refused&&cell::encode(field.doc)==state,"bad reload replaced live draft");
  {std::ofstream o(test,std::ios::binary);o<<prior;}loaded=prior;reload_base();
  check(cell::encode(field.doc)==prior,"valid reload did not reconstruct base");
  std::string good=path;path=(test.parent_path()/"missing"/"file.json").u8string();
  check(!save()&&read(good)==prior,"failed save changed base");
  field.doc=original;field.rebuild();rebuild();
  performance_check();
  check(perf.equal,"incremental brush output differs from full rebuild");
  check(perf.fast_uploads<perf.reference_uploads,"brush still uploads the whole terrain");
  check(perf.deferred_masks==0&&perf.fast_masks<=perf.reference_masks,"stroke collision masks were not batched");
  pending=true;ax=-96;ay=-64;mode=1;curve=.5;
  auto snapshot=cell::encode(field.doc);auto counters=bake_stats;auto preview_start=preview.builds;update_preview(352,-64);update_preview(352,-64);
  check(preview.valid&&!preview.lines.empty()&&preview.builds==preview_start+1,"line preview did not cache exact geometry");
  check(cell::encode(field.doc)==snapshot&&bake_stats.uploads==counters.uploads&&bake_stats.masks==counters.masks,"line preview changed terrain or built masks/images");
  bool fits=true;
  for(int width:{426,960,1920}){auto ui=tool_layout(width);for(const auto& r:ui.buttons)if(r.x<ui.panel.x||r.x+r.w>ui.panel.x+ui.panel.w||r.y+r.h>ui.panel.y+ui.panel.h)fits=false;
   for(size_t i=0;i<ui.buttons.size();i++)for(size_t j=i+1;j<ui.buttons.size();j++){auto a=ui.buttons[i],b=ui.buttons[j];if(a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y)fits=false;}}
  check(fits,"tool buttons overlap or overflow at supported logical widths");
  pending=false;auto old_material=chosen().material;editor_command("material_next");
  check(chosen().material!=old_material,"material control did not edit the selected shape");
  history(true);check(chosen().material==old_material,"shape material change did not undo");
  history(false);check(chosen().material!=old_material,"shape material change did not redo");
  auto ui=tool_layout(video_get_screen_size().x);auto box=ui.buttons[2];auto count=field.doc.shapes.size();
  bool caught=tool_pointer(box.x+1,box.y+1,1,true);
  bool retained=tool_pointer(ui.panel.x+ui.panel.w+1,ui.panel.y,1,false);tool_pointer(-1,-1,0,false);
  check(caught&&retained&&!pointer_capture&&field.doc.shapes.size()==count+1,"tool click/drag did not remain captured by the panel");
  pending=false;preview.valid=false;
  field.doc=original;field.rebuild();selected=original_selected;undo_stack.clear();redo_stack.clear();rebuild();
  std::string registry=read(asset_path(material_path.c_str())),tinted=registry;
  auto at=tinted.find("255");if(at==std::string::npos)throw std::runtime_error("Expected material fixture tint");
  tinted.replace(at,3,"160");
  auto pixels_before=resource_hash(),masks_before=bake_stats.masks;auto document_before=cell::encode(field.doc);
  install_materials(tinted);
  check(resource_hash()!=pixels_before,"live material tint did not change rendered pixels");
  check(bake_stats.masks==masks_before&&cell::encode(field.doc)==document_before,"material reload changed collision or authored geometry");
  auto tinted_hash=resource_hash();
  std::string refusal;
  auto refuses=[&](const std::string& text){try{install_materials(text);refusal.clear();return false;}catch(const std::exception& e){refusal=e.what();return true;}};
  check(refuses("{broken registry")&&resource_hash()==tinted_hash,"malformed registry changed live presentation");
  std::string missing=tinted;at=missing.find("material:stone");missing.replace(at,14,"material:removed");
  check(refuses(missing)&&resource_hash()==tinted_hash,"missing material identity was substituted");
  missing=tinted;at=missing.find("textures/stone.png");missing.replace(at,18,"textures/missing-material.png");
  check(refuses(missing)&&resource_hash()==tinted_hash,"missing texture discarded the valid material set");
  std::string alpha_path="textures/check-alpha-"+std::to_string(GetCurrentProcessId())+".png";
  struct AlphaCleanup {std::filesystem::path p;bool owned=false;~AlphaCleanup(){if(owned){std::error_code e;std::filesystem::remove(p,e);}}} cleanup{std::filesystem::u8path(alpha_path)};
  if(std::filesystem::exists(cleanup.p))throw std::runtime_error("Private alpha fixture already exists");
  std::filesystem::copy_file(std::filesystem::u8path(asset_path("images/presentation/transparent.png")),cleanup.p);cleanup.owned=true;
  missing=tinted;at=missing.find("textures/stone.png");missing.replace(at,18,alpha_path);
  check(refuses(missing)&&refusal.find("opaque")!=std::string::npos&&resource_hash()==tinted_hash,"transparent material texture damaged the active set");
  cell::Document historical=original;historical.shapes.front().material="material:history-only";undo_stack.push_back(historical);
  bool history_refused=refuses(registry);undo_stack.clear();
  check(history_refused&&resource_hash()==tinted_hash,"material removal invalidated undo history");
  std::string added=registry;at=added.rfind(']');
  added.insert(at,R"(,{"id":"material:test-added","name":"Added live","texture":"textures/stone.png","tint":[128,255,255],"shader":null,"physical_profile":null,"acoustic_profile":null,"feedback":{"impact":null,"footstep":null,"damage":null,"destruction":null},"reactions":[]})");
  install_materials(added);editor_command("material_prev");
  check(cell_materials.size()==4&&chosen().material=="material:test-added","new runtime material did not reach the selector");
  history(true);
  bool redo_refused=refuses(registry);redo_stack.clear();
  check(redo_refused,"material removal invalidated redo history");
  std::string live_path="textures/check-live-"+std::to_string(GetCurrentProcessId())+".png";
  AlphaCleanup live_cleanup{std::filesystem::u8path(live_path)};if(std::filesystem::exists(live_cleanup.p))throw std::runtime_error("Private live texture fixture already exists");
  auto write_swatch=[&](Pixel color){
   std::unique_ptr<image_t,decltype(&image_destroy)> swatch(image_create_ex(cell::chunk,cell::chunk,IC_DEFAULT),image_destroy);
   if(!swatch)throw std::runtime_error("Private swatch allocation failed");
   std::array<Pixel,cell::chunk*cell::chunk> pixels;pixels.fill(color);upload(swatch.get(),pixels);
   auto* previous=al_get_new_file_interface();al_set_standard_file_interface();bool saved=al_save_bitmap(live_path.c_str(),IMAGE2BITMAP(swatch.get()));al_set_new_file_interface(previous);
   if(!saved)throw std::runtime_error("Private swatch write failed");
   live_cleanup.owned=true;
  };
  std::string fresh=registry;at=fresh.find("textures/stone.png");fresh.replace(at,18,live_path);
  write_swatch({200,20,10,255});install_materials(fresh);auto red_hash=resource_hash();
  write_swatch({10,20,200,255});install_materials(fresh);
  check(resource_hash()!=red_hash,"same-path PNG reload retained stale cached pixels");
  editor_command("reload_materials");
  check(resource_hash()==pixels_before&&read(asset_path(material_path.c_str()))==registry,"reload did not restore pixels or changed the user's registry");
  check(bake_stats.masks==masks_before,"material-only changes rebuilt physical masks");

 }catch(const std::exception& e){passed=false;logfile_message("CELL CHECK FAIL: %s",e.what());}
 field.doc=original;field.rebuild();selected=original_selected;path=original_path;loaded=original_loaded;drag=pending=dirty=false;preview.valid=false;mode=0;curve=.5;undo_stack.clear();redo_stack.clear();rebuild();
 std::ofstream out(std::filesystem::u8path(report));out<<"{\"passed\":"<<(passed?"true":"false")<<",\"checks\":"<<checks<<",\"performance\":{\"reference_uploads\":"<<perf.reference_uploads<<",\"fast_uploads\":"<<perf.fast_uploads<<",\"reference_masks\":"<<perf.reference_masks<<",\"fast_masks\":"<<perf.fast_masks<<",\"masks_during_drag\":"<<perf.deferred_masks<<",\"reference_seconds\":"<<perf.reference_seconds<<",\"fast_seconds\":"<<perf.fast_seconds<<"},\"signed\":\"President\"}";
}
