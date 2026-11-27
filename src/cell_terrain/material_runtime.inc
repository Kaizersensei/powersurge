// Fresh, recoverable material image decoding and transactional publication. Signed: President.
struct PreparedMaterials {std::vector<cell::Material> records;std::vector<Texture> images;std::shared_ptr<ps::lighting::TerrainMaterials> lighting;std::string diagnostic;};
void validate_material_references(const cell::Document& doc,const std::vector<cell::Material>& records){
 for(const auto& shape:doc.shapes)if(std::none_of(records.begin(),records.end(),[&](const auto& m){return m.id==shape.material;}))throw std::runtime_error("Material required by terrain/history: "+shape.material);
}
void ensure_material_images(const std::vector<CellMaterial>& records,std::vector<Texture>& images){
 std::set<int> used,shapes;for(auto& block:field.blocks)for(auto id:block.second.cells)if(id)shapes.insert(id);for(auto id:shapes){auto* shape=field.shape(id);if(shape){auto m=std::find_if(records.begin(),records.end(),[&](const auto& m){return m.id==shape->material;});if(m==records.end())throw std::runtime_error("Missing material: "+shape->material);used.insert(int(m-records.begin()));}}
 std::vector<std::pair<int,Texture>> staged;size_t pixels=0;
 for(int n:used){if(images[n].w>0)pixels+=images[n].pixels.size();else{auto image=cell::load_material_images({records[n]},[](const std::string& path){return asset_exists(path.c_str())?al_fopen(asset_path(path.c_str()),"rb"):nullptr;});pixels+=image[0].pixels.size();staged.emplace_back(n,std::move(image[0]));}if(pixels>8*1024*1024)throw std::runtime_error("Active terrain textures exceed8M pixels; palette entries are loaded only when used");}
 for(size_t i=0;i<images.size();++i)if(!used.count(int(i)))images[i]=Texture{};
 for(auto& entry:staged)images[entry.first]=std::move(entry.second);
}
PreparedMaterials prepare_materials(const std::string& source){
 PreparedMaterials result;result.records=cell::decode_materials(source);
 std::set<std::string> ids;for(auto& m:result.records)ids.insert(m.id);for(auto& library:ps::authoring::render_snapshot().libraries)if(library.second.palette)for(auto& m:cell::decode_materials(library.second.legacy))if(ids.insert(m.id).second)result.records.push_back(std::move(m));
 validate_material_references(field.doc,result.records);
 for(const auto* stack:{&undo_stack,&redo_stack})for(const auto& doc:*stack)validate_material_references(doc,result.records);
 result.images.resize(result.records.size());ensure_material_images(result.records,result.images);
 result.lighting=ps::lighting::TerrainMaterials::compile(ps::authoring::render_snapshot(),terrain_level,material_path,result.records,result.diagnostic,std::any_of(field.doc.shapes.begin(),field.doc.shapes.end(),[](const auto& s){return s.bevel&&s.bevel->enabled;}));
 return result;
}
void install_materials(const std::string& source){
 auto proposed=prepare_materials(source);
 // Masks/obstacles stay shared unchanged; only candidate images get replaced.
 auto next=resources;for(auto& r:next)r.image=r.normal=r.properties=r.bevel=nullptr;
 std::set<cell::Key> changed;for(const auto& block:field.blocks)changed.insert(block.first);for(const auto& r:resources)changed.insert(r.key);
 auto previous_dirty=visual_dirty;
 cell_materials.swap(proposed.records);textures.swap(proposed.images);terrain_lighting.swap(proposed.lighting);resources.swap(next);visual_dirty.swap(changed);
 try{rebuild(false,false);}
 catch(...){
  for(auto& r:resources)release_images(r);
  resources.swap(next);cell_materials.swap(proposed.records);textures.swap(proposed.images);terrain_lighting.swap(proposed.lighting);visual_dirty.swap(previous_dirty);
  for(const auto& shape:field.doc.shapes)shape_materials[shape.id]=material_index(shape.material);
  throw;
 }
 bsl_shell_terrain_thumbs_clear();
 for(auto& r:next)release_images(r);
 lighting_diagnostic=std::move(proposed.diagnostic);lighting_failed=false;
}
// Definition assignment publishes candidate visual resources before committing history.
void edit_surface(const std::function<void()>& change){
 commit();
 auto original=field.doc,original_before=before,original_published=published;bool original_dirty=dirty;
 try{
  start_edit();change();cell::validate(field.doc);
  auto next_undo=undo_stack;next_undo.push_back(original);if(next_undo.size()>64)next_undo.erase(next_undo.begin());auto next_published=field.doc;
  ps::authoring::Json rows=ps::authoring::Json::array();for(auto& m:cell_materials){auto ref=[](const std::string& v){return v.empty()?ps::authoring::Json(nullptr):ps::authoring::Json(v);};ps::authoring::Json feedback;for(auto& f:m.feedback)feedback[f.first]=ref(f.second);rows.push_back({{"id",m.id},{"name",m.name},{"texture",m.texture},{"tint",m.tint},{"shader",ref(m.shader)},{"physical_profile",ref(m.physical_profile)},{"acoustic_profile",ref(m.acoustic_profile)},{"feedback",feedback},{"reactions",m.reactions}});}
  install_materials(ps::authoring::Json{{"version",1},{"materials",rows}}.dump());
  undo_stack.swap(next_undo);redo_stack.clear();published=std::move(next_published);dirty=true;drag=false;message="Surface settings applied to the selected shape";
 }catch(...){field.doc=std::move(original);before=std::move(original_before);published=std::move(original_published);dirty=original_dirty;drag=false;for(auto& shape:field.doc.shapes)shape_materials[shape.id]=material_index(shape.material);throw;}
}
void assign_material(int index){
 if(index<0||index>=int(cell_materials.size()))throw std::runtime_error("Unknown material selection");if(chosen().material==cell_materials[index].id)return;edit_surface([&]{chosen().material=cell_materials[index].id;});
}
cell::Bevel selected_bevel(){return chosen().bevel.value_or(terrain_lighting?terrain_lighting->bevel(material_index(chosen().material)):cell::Bevel{});}
bool reload_materials(){
 try{commit();auto registered=ps::authoring::render_snapshot().legacy_materials(material_path);install_materials(registered.empty()?read(asset_path(material_path.c_str())):registered);message="Material textures reloaded from the session registry; record edits require a fresh test. Geometry and physics unchanged";return true;}
 catch(const std::exception& e){message=std::string("Material reload refused: ")+e.what();return false;}
}
