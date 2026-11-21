// Fresh, recoverable material image decoding and transactional publication. Signed: President.
struct PreparedMaterials {std::vector<cell::Material> records;std::vector<Texture> images;};
void validate_material_references(const cell::Document& doc,const std::vector<cell::Material>& records){
 for(const auto& shape:doc.shapes)if(std::none_of(records.begin(),records.end(),[&](const auto& m){return m.id==shape.material;}))throw std::runtime_error("Material required by terrain/history: "+shape.material);
}
PreparedMaterials prepare_materials(const std::string& source){
 PreparedMaterials result;result.records=cell::decode_materials(source);
 validate_material_references(field.doc,result.records);
 for(const auto* stack:{&undo_stack,&redo_stack})for(const auto& doc:*stack)validate_material_references(doc,result.records);
 size_t pixels=0;
 for(const auto& m:result.records){
  if(!asset_exists(m.texture.c_str()))throw std::runtime_error("Missing texture: "+m.texture);
  std::unique_ptr<ALLEGRO_FILE,decltype(&al_fclose)> file(al_fopen(asset_path(m.texture.c_str()),"rb"),al_fclose);
  if(!file)throw std::runtime_error("Cannot open texture: "+m.texture);
  unsigned char header[24];const unsigned char png[]={137,80,78,71,13,10,26,10};
  if(al_fsize(file.get())>16*1024*1024||al_fread(file.get(),header,sizeof header)!=sizeof header||std::memcmp(header,png,8)||std::memcmp(header+12,"IHDR",4))throw std::runtime_error("Invalid or oversized PNG: "+m.texture);
  auto number=[&](int at){uint32_t n=0;for(int i=0;i<4;i++)n=(n<<8)|header[at+i];return n;};
  uint32_t width=number(16),height=number(20);
  if(!width||!height||width>1024||height>1024||(pixels+=size_t(width)*height)>8*1024*1024)throw std::runtime_error("Material texture budget exceeded");
  if(!al_fseek(file.get(),0,ALLEGRO_SEEK_SET))throw std::runtime_error("Cannot rewind texture");
  std::unique_ptr<ALLEGRO_BITMAP,decltype(&al_destroy_bitmap)> bitmap(al_load_bitmap_flags_f(file.get(),".png",ALLEGRO_MEMORY_BITMAP),al_destroy_bitmap);
  if(!bitmap||al_get_bitmap_width(bitmap.get())!=static_cast<int>(width)||al_get_bitmap_height(bitmap.get())!=static_cast<int>(height))throw std::runtime_error("Cannot decode texture: "+m.texture);
  Texture t;t.w=width;t.h=height;t.pixels.resize(size_t(width)*height);
  auto* lock=al_lock_bitmap(bitmap.get(),ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_READONLY);
  if(!lock)throw std::runtime_error("Cannot read texture pixels");
  bool opaque=true;
  for(int y=0;y<t.h;y++){const auto* row=static_cast<const uint8_t*>(lock->data)+y*lock->pitch;
   for(int x=0;x<t.w;x++){const auto* p=row+x*4;opaque=opaque&&p[3]==255;t.pixels[y*t.w+x]={static_cast<uint8_t>(p[0]*m.tint[0]/255),static_cast<uint8_t>(p[1]*m.tint[1]/255),static_cast<uint8_t>(p[2]*m.tint[2]/255),255};}
  }
  al_unlock_bitmap(bitmap.get());
  if(!opaque)throw std::runtime_error("Material texture requires opaque pixels: "+m.texture);
  result.images.push_back(std::move(t));
 }
 return result;
}
void install_materials(const std::string& source){
 auto proposed=prepare_materials(source);
 // Masks/obstacles stay shared unchanged; only candidate images get replaced.
 auto next=resources;for(auto& r:next)r.image=nullptr;
 std::set<cell::Key> changed;for(const auto& block:field.blocks)changed.insert(block.first);for(const auto& r:resources)changed.insert(r.key);
 auto previous_dirty=visual_dirty;
 cell_materials.swap(proposed.records);textures.swap(proposed.images);resources.swap(next);visual_dirty.swap(changed);
 try{rebuild(false,false);}
 catch(...){
  for(auto& r:resources)if(r.image)image_destroy(r.image);
  resources.swap(next);cell_materials.swap(proposed.records);textures.swap(proposed.images);visual_dirty.swap(previous_dirty);
  for(const auto& shape:field.doc.shapes)shape_materials[shape.id]=material_index(shape.material);
  throw;
 }
 bsl_shell_terrain_thumbs_clear();
 for(auto& r:next)if(r.image)image_destroy(r.image);
}
bool reload_materials(){
 try{commit();install_materials(read(asset_path(material_path.c_str())));message="Material registry/textures reloaded; geometry and physics unchanged";return true;}
 catch(const std::exception& e){message=std::string("Material reload refused: ")+e.what();return false;}
}
