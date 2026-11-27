// Full imported-fill validation and bounded runtime residency. Signed: President.
int catalog_checks(char* error,unsigned capacity){
 int checks=0;auto original=field.doc,old_before=before;auto old_undo=undo_stack,old_redo=redo_stack;int old_selected=selected;bool old_dirty=dirty;std::filesystem::path held,original_file;
 auto restore=[&]{if(!held.empty()&&std::filesystem::exists(held))std::filesystem::rename(held,original_file);field.doc=original;field.rebuild();selected=old_selected;before=old_before;drag=false;rebuild();undo_stack=old_undo;redo_stack=old_redo;dirty=old_dirty;};
 try{
  auto check=[&](bool b,const char* why){if(!b)throw std::runtime_error(why);++checks;};
  auto inventory=ps::authoring::parse(read(asset_path("textures/material_types/catalog.json")));check(inventory["entries"].size()==1107,"Imported source count changed");int fills=0,alpha=0,unsupported=0;
  auto used=[&]{return std::count_if(textures.begin(),textures.end(),[](const auto& t){return t.w>0;});};auto resident=used();check(resident<=3&&cell_materials.size()==806,"Palette eagerly loaded textures or lost shared entries");
  for(auto& item:inventory["entries"]){auto file=item["path"].get<std::string>();check(asset_exists(file.c_str()),"Imported file missing from package");if(item["status"]=="opaque_fill"){
    auto found=std::find_if(cell_materials.begin(),cell_materials.end(),[&](const auto& m){return m.texture==file;});check(found!=cell_materials.end(),"Compatible fill missing from palette");auto image=cell::load_material_images({*found},[](const std::string& file){return al_fopen(asset_path(file.c_str()),"rb");});check(image[0].w==item["width"]&&image[0].h==item["height"],"Native decoder disagrees with imported image dimensions");++fills;
   }else if(item["status"]=="transparent"){check(file.rfind("inspection/material_types/non_opaque/",0)==0,"Alpha image not separated for inspection");++alpha;}else{check(file.rfind("inspection/material_types/unsupported/",0)==0,"Unsupported image not separated for inspection");++unsupported;}}
  check(fills==803&&alpha==300&&unsupported==4,"Compatibility counts differ");
  int index=int(cell_materials.size())-1;check(textures[index].w==0,"Unused material unexpectedly resident");PsTerrainMaterialProbe probe{};ps_terrain_material_probe(cell_materials[index].id.c_str(),-1,-1,&probe);check(probe.present&&used()==resident,"Unused material probe failed or changed active residency");
  // Missing palette content must refuse assignment without a partial draft or C exception.
  auto geometry=cell::encode(field.doc);auto old_masks=bake_stats.masks,hash=resource_hash();original_file=std::filesystem::u8path(asset_path(cell_materials[index].texture.c_str()));held=original_file;held+=".native-held";check(!std::filesystem::exists(held),"Unexpected held fixture");std::filesystem::rename(original_file,held);
  bool refused=false;try{assign_material(index);}catch(...){refused=true;}check(refused&&!drag&&dirty==old_dirty&&cell::encode(field.doc)==geometry&&resource_hash()==hash,"Failed assignment mutated active terrain or draft");std::filesystem::rename(held,original_file);held.clear();bsl_shell_terrain_tool(true);check(!drag,"Failed assignment retriggered an unguarded commit");
  assign_material(index);check(chosen().material==cell_materials[index].id&&used()<=3&&textures[index].w>0,"Palette material did not become a real active fill");check(bake_stats.masks==old_masks,"Material selection rebuilt collision");history(true);check(cell::encode(field.doc)==geometry,"Material selection undo failed");
  for(int n=0;n<140;++n)check(bsl_shell_material_image(n)!=nullptr,"Visible thumbnail could not be generated");check(std::count_if(shell_thumbs.begin(),shell_thumbs.end(),[](auto* p){return p!=nullptr;})<=128&&used()<=3,"Thumbnail browsing retained unbounded source textures");
  restore();return checks;
 }catch(const std::exception& e){try{restore();}catch(...){}if(error&&capacity)snprintf(error,capacity,"%s",e.what());return -1;}
}
