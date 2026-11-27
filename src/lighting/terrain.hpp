// Cached visual material data; field coverage and physics remain separate. Signed: President.
#pragma once
#include "surface.hpp"
#include "../cell_terrain/terrain.hpp"
namespace ps::lighting {
struct MaterialSample {
 cell::MaterialPixel color{0,0,0,0};std::array<double,3> normal{0,0,1};double roughness=.8,specular=.25,emission=0,receive=0;
};
struct TerrainSlot {cell::Bevel bevel;Settings values;double cosine=1,sine=0;bool receive=false;std::shared_ptr<cell::MaterialTexture> normal;};
class TerrainMaterials {
 std::vector<TerrainSlot> slots;
public:
 Settings light;std::string diagnostics;
 static std::shared_ptr<TerrainMaterials> compile(const authoring::RenderRegistry&,const std::string& level,const std::string& path,const std::vector<cell::Material>&,std::string& diagnostic,bool force_bevel=false);
 cell::Bevel bevel(size_t index)const{return slots.at(index).bevel;}
 MaterialSample sample(size_t material,int x,int y,const cell::MaterialTexture&)const;
};
MaterialSample mix_materials(MaterialSample,MaterialSample,double weight);
std::pair<cell::MaterialPixel,cell::MaterialPixel> pack_material(const MaterialSample&);
void terrain_reset_shader();
class TerrainPass {
 struct Impl;std::unique_ptr<Impl> impl;
public:
 explicit TerrainPass(const Settings&);~TerrainPass();
 void draw(image_t* color,image_t* normal,image_t* properties,int x,int y,image_t* bevel=nullptr);
};
}
