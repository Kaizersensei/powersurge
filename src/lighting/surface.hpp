// Opt-in, unoccluded terrain-surface lighting. No world/collision authority. Signed: President.
#pragma once
#include "../authoring/render_records.hpp"
#include "../cell_terrain/material_images.hpp"
extern "C" {
#include "../core/image.h"
}
#include <memory>
namespace ps::lighting {
struct Settings {
 float angle=315,elevation=45,energy=1,ambient_energy=.3f;
 float strength=1,roughness=.8f,specular=.25f,emission=0;
 float rotation=0,scale_x=1,scale_y=1,offset_x=0,offset_y=0;
 bool directx=false;int mode=1; // 0 unlit,1 lit,2 transformed normal
 std::array<float,3> color{1,.94f,.82f},ambient{.78f,.84f,1};
};
class Surface {
 image_t* color_=nullptr;image_t* normal_=nullptr;
 cell::MaterialTexture color_pixels_,normal_pixels_;
public:
 Surface()=default;~Surface();Surface(const Surface&)=delete;Surface& operator=(const Surface&)=delete;
 Settings defaults;
 void load(const authoring::RenderRegistry&,const std::string& library,const std::string& material,const std::string& light,const std::string& profile);
 void draw(image_t* output,const Settings&)const;
 const cell::MaterialTexture& colors()const{return color_pixels_;}
 const cell::MaterialTexture& normals()const{return normal_pixels_;}
};
void reset_shader();
int native_check(char* error,unsigned capacity);
}
