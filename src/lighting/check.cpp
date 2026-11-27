// Native GPU readback against an independent CPU reference. Signed: President.
#include "surface.hpp"
extern "C" {
#include "../core/shader.h"
}
#include <allegro5/allegro_opengl.h>
#include <cstdio>
#include <cstring>
namespace ps::lighting {
namespace {
using V=std::array<double,3>;
V unit(V a){double n=std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);for(auto& v:a)v/=n;return a;}
double linear(double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);}
int srgb(double x){x=std::clamp(x,0.,1.);return int(std::lround(255*(x<=.0031308?12.92*x:1.055*std::pow(x,1/2.4)-.055)));}
std::array<int,3> reference(const Surface& surface,const Settings& p,int x,int y){
 double u=(x+.5)/67-.5,v=(y+.5)/61-.5,r=p.rotation*3.141592653589793/180,c=std::cos(r),s=std::sin(r);
 double qx=(c*u+s*v)*p.scale_x+.5+p.offset_x,qy=(-s*u+c*v)*p.scale_y+.5+p.offset_y;qx-=std::floor(qx);qy-=std::floor(qy);
 auto sample=[&](const cell::MaterialTexture& t){return t.pixels[int(qy*t.h)*t.w+int(qx*t.w)];};auto b=sample(surface.colors());auto n=sample(surface.normals());
 std::array<int,3> base{b.r,b.g,b.b};if(!p.mode)return base;
 double nx=(n.r/255.*2-1)*(p.scale_x<0?-1:1)*p.strength,ny=-(n.g/255.*2-1)*(p.directx?-1:1)*(p.scale_y<0?-1:1)*p.strength;
 auto normal=p.strength==0?V{0,0,1}:unit({c*nx-s*ny,s*nx+c*ny,n.b/255.*2-1});
 if(p.mode==2)return {int(std::lround((normal[0]*.5+.5)*255)),int(std::lround((normal[1]*.5+.5)*255)),int(std::lround((normal[2]*.5+.5)*255))};
 double angle=p.angle*3.141592653589793/180,elevation=p.elevation*3.141592653589793/180;V l{std::cos(angle)*std::cos(elevation),std::sin(angle)*std::cos(elevation),std::sin(elevation)};auto h=unit({l[0],l[1],l[2]+1});
 auto dot=[&](V a){return std::max(0.,normal[0]*a[0]+normal[1]*a[1]+normal[2]*a[2]);};double d=dot(l),highlight=d>=.00001?std::pow(dot(h),std::pow(2.,1+7*(1-p.roughness)))*p.specular:0;
 std::array<int,3> result{};for(int i=0;i<3;++i)result[i]=srgb(linear(base[i]/255.)*(linear(p.ambient[i])*p.ambient_energy+linear(p.color[i])*p.energy*d+p.emission)+linear(p.color[i])*p.energy*highlight);return result;
}
}
int native_check(char* error,unsigned capacity){
 int checks=0;image_t* output=nullptr;
 try {
  auto check=[&](bool okay,const char* why){if(!okay)throw std::runtime_error(why);++checks;};
  const auto* old_managed=shader_get_active();auto* old_actual=al_get_current_shader();auto* old_target=al_get_target_bitmap();auto* old_image=image_drawing_target();auto transform=*al_get_current_transform(),projection=*al_get_current_projection_transform();int blend,src,dst;al_get_blender(&blend,&src,&dst);int clip[4];al_get_clipping_rectangle(clip,clip+1,clip+2,clip+3);
  char diagnostic[1024]="";check(!shader_try_create("__ps_invalid_study","this is not GLSL",diagnostic,sizeof diagnostic)&&*diagnostic,"Invalid optional shader did not fail safely");check(!shader_exists("__ps_invalid_study")&&shader_get_active()==old_managed&&al_get_current_shader()==old_actual,"Rejected shader changed registry or active shader");
  output=image_create_ex(67,61,IC_DEFAULT);check(output!=nullptr,"Cannot allocate GPU check output");
  auto& registry=authoring::render_snapshot();check(registry.studies.count("level:lighting-study")!=0,"Study is not activated by level metadata");
  auto run=[&](const char* material){
   Surface surface;surface.load(registry,"material-library:lighting-study",material,"light:sun","lighting:outdoor");check(surface.colors().w==96&&surface.colors().h==80&&surface.normals().w==128,"NPOT and independent map dimensions lost");
   std::vector<Settings> cases;auto v=surface.defaults;v.specular=0;v.ambient_energy=0;v.color={1,1,1};v.energy=1;v.mode=0;cases.push_back(v);v.mode=1;cases.push_back(v);v.strength=0;cases.push_back(v);v.strength=1;
   for(float angle:{0.f,90.f,180.f,270.f}){v.angle=angle;v.elevation=25;cases.push_back(v);}v.directx=true;cases.push_back(v);v.directx=false;v.rotation=90;cases.push_back(v);v.rotation=-90;cases.push_back(v);v.rotation=0;v.scale_x=-1;cases.push_back(v);v.scale_y=-1;cases.push_back(v);v.rotation=90;cases.push_back(v);v.scale_x=2;v.scale_y=.5;v.offset_x=.13;v.offset_y=-.07;cases.push_back(v);
   v=surface.defaults;v.mode=2;cases.push_back(v);v.directx=true;v.rotation=90;v.scale_y=-1;cases.push_back(v);
   v=surface.defaults;v.energy=0;cases.push_back(v);v.ambient_energy=0;cases.push_back(v);v.emission=.5;cases.push_back(v);
   v=surface.defaults;v.color={.4,.8,.9};v.ambient={.8,.3,.6};v.roughness=0;cases.push_back(v);v.roughness=1;cases.push_back(v);
   for(size_t i=0;i<cases.size();++i){surface.draw(output,cases[i]);auto* pixels=al_lock_bitmap(IMAGE2BITMAP(output),ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_READONLY);check(pixels!=nullptr,"GPU output readback failed");bool okay=true;std::string detail;
    for(int y:{7,21,43,57})for(int x:{9,25,41,55}){auto expected=reference(surface,cases[i],x,y);auto* p=static_cast<const unsigned char*>(pixels->data)+y*pixels->pitch+x*4;for(int k=0;k<3;++k)if(std::abs(int(p[k])-expected[k])>3){okay=false;detail=std::string(material)+" case "+std::to_string(i)+" at "+std::to_string(x)+","+std::to_string(y)+" channel "+std::to_string(k)+" GPU "+std::to_string(p[k])+" expected "+std::to_string(expected[k]);}}
    al_unlock_bitmap(IMAGE2BITMAP(output));check(okay,detail.c_str());
   }
  };
  run("material:study-axes");run("material:study-flat");run("material:study-rounded");
  check(shader_get_active()==old_managed&&al_get_current_shader()==old_actual,"Surface draw changed active shader");check(al_get_target_bitmap()==old_target&&image_drawing_target()==old_image,"Surface draw changed target ownership");
  check(!std::memcmp(&transform,al_get_current_transform(),sizeof transform)&&!std::memcmp(&projection,al_get_current_projection_transform(),sizeof projection),"Surface draw changed transforms");int b,s,d;al_get_blender(&b,&s,&d);check(b==blend&&s==src&&d==dst,"Surface draw changed blend state");int now[4];al_get_clipping_rectangle(now,now+1,now+2,now+3);check(!std::memcmp(now,clip,sizeof clip),"Surface draw changed target clipping");
  {Surface held_surface;held_surface.load(registry,"material-library:lighting-study","material:study-flat","light:sun","lighting:outdoor");bool held=al_is_bitmap_drawing_held();al_use_shader(nullptr);al_hold_bitmap_drawing(true);held_surface.draw(output,held_surface.defaults);bool restored=al_is_bitmap_drawing_held()&&al_get_current_shader()==nullptr&&shader_get_active()==old_managed;al_hold_bitmap_drawing(held);al_use_shader(old_actual);check(restored,"Held drawing or distinct actual/managed shader state was lost");}
  check(!shader_try_create("__ps_surface_lit_v1","invalid",diagnostic,sizeof diagnostic),"Optional shader replaced a registered program");
  auto bad=registry;bad.shaders.at("shader:terrain-lit").program="trusted-source";bool refused=false;try{Surface unsupported;unsupported.load(bad,"material-library:lighting-study","material:study-flat","light:sun","lighting:outdoor");}catch(...){refused=true;}check(refused,"Trusted shader source executed in builtin study");
  reset_shader();image_destroy(output);output=nullptr;
  // Exercise fresh owned images against the same managed program after sampler cleanup.
  {Surface again;again.load(registry,"material-library:lighting-study","material:study-rounded","light:sun","lighting:outdoor");auto* target=image_create_ex(32,32,IC_DEFAULT);check(target!=nullptr,"Reentry allocation failed");again.draw(target,again.defaults);image_destroy(target);check(true,"Reentry draw failed");}
  return checks;
 }catch(const std::exception& e){reset_shader();if(output)image_destroy(output);if(error&&capacity)snprintf(error,capacity,"%s",e.what());return -1;}
}
}
