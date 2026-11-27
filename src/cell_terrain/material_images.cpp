// Same validation before publication and playback; no geometry or GPU work. Signed: President.
#include "material_images.hpp"
#include <allegro5/allegro_image.h>
#include <memory>
#include <cstring>
#include <stdexcept>
namespace cell {
std::vector<MaterialTexture> load_material_images(const std::vector<Material>& records,const OpenMaterialImage& open) {
 std::vector<MaterialTexture> images;size_t pixels=0;
 for(const auto& m:records) {
  std::unique_ptr<ALLEGRO_FILE,decltype(&al_fclose)> file(open(m.texture),al_fclose);
  if(!file)throw std::runtime_error("Cannot open texture: "+m.texture);
  unsigned char header[24];const unsigned char png[]={137,80,78,71,13,10,26,10};
  if(al_fsize(file.get())>16*1024*1024||al_fread(file.get(),header,sizeof header)!=sizeof header||std::memcmp(header,png,8)||std::memcmp(header+12,"IHDR",4))throw std::runtime_error("Invalid or oversized PNG: "+m.texture);
  auto number=[&](int at){uint32_t n=0;for(int i=0;i<4;i++)n=(n<<8)|header[at+i];return n;};
  uint32_t width=number(16),height=number(20);
  if(!width||!height||width>1024||height>1024||(pixels+=size_t(width)*height)>8*1024*1024)throw std::runtime_error("Material texture budget exceeded (1024px dimension /8M aggregate pixels)");
  if(!al_fseek(file.get(),0,ALLEGRO_SEEK_SET))throw std::runtime_error("Cannot rewind texture");
  std::unique_ptr<ALLEGRO_BITMAP,decltype(&al_destroy_bitmap)> bitmap(al_load_bitmap_flags_f(file.get(),".png",ALLEGRO_MEMORY_BITMAP),al_destroy_bitmap);
  if(!bitmap||al_get_bitmap_width(bitmap.get())!=static_cast<int>(width)||al_get_bitmap_height(bitmap.get())!=static_cast<int>(height))throw std::runtime_error("Cannot decode texture: "+m.texture);
  MaterialTexture texture;texture.w=width;texture.h=height;texture.pixels.resize(size_t(width)*height);
  auto* lock=al_lock_bitmap(bitmap.get(),ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_READONLY);if(!lock)throw std::runtime_error("Cannot read texture pixels");
  bool opaque=true;
  for(int y=0;y<texture.h;y++){const auto* row=static_cast<const uint8_t*>(lock->data)+y*lock->pitch;for(int x=0;x<texture.w;x++){const auto* p=row+x*4;opaque=opaque&&p[3]==255;texture.pixels[y*texture.w+x]={static_cast<uint8_t>(p[0]*m.tint[0]/255),static_cast<uint8_t>(p[1]*m.tint[1]/255),static_cast<uint8_t>(p[2]*m.tint[2]/255),255};}}
  al_unlock_bitmap(bitmap.get());if(!opaque)throw std::runtime_error("Material color texture requires opaque pixels: "+m.texture);
  images.push_back(std::move(texture));
 }
 return images;
}
}
