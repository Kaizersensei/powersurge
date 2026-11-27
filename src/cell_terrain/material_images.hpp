// Shared CPU color decoding/compatibility checks for source saves and runtime. Signed: President.
#pragma once
#include "materials.hpp"
#include <allegro5/allegro.h>
#include <functional>
namespace cell {
struct MaterialPixel {uint8_t r,g,b,a;};
struct MaterialTexture {int w=0,h=0;std::vector<MaterialPixel> pixels;};
using OpenMaterialImage=std::function<ALLEGRO_FILE*(const std::string&)>;
std::vector<MaterialTexture> load_material_images(const std::vector<Material>&,const OpenMaterialImage&);
}
