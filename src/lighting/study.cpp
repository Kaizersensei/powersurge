#include "study.h"
#include "surface.hpp"
#include "../authoring/world_records.hpp"
#include "../authoring/hub.h"
#include "../authoring/project.h"
#include "../labs/lab.h"
#include "imgui_internal.h"
extern "C" {
void bsl_host_authoring_pause(bool);
#include "../foundation/foundation_host.h"
}
#include "imgui.h"
#include "../ui/widgets.hpp"
#include <array>
#include <memory>
namespace {
std::unique_ptr<ps::lighting::Surface> surface;std::array<image_t*,4> cards{};
ImVec2 combo_point{},entry_point{};std::string test_previous;
bool entry_ready=false;
ps::authoring::SurfaceStudyDefinition definition;ps::lighting::Settings settings;
std::string level_id,error;bool panel_open=true,orbit=false,active=false,displayed=false,input_barrier=false;
void release(){ps::lighting::reset_shader();for(auto*& p:cards){if(p)image_destroy(p);p=nullptr;}surface.reset();}
void load(){
 release();try{surface=std::make_unique<ps::lighting::Surface>();surface->load(ps::authoring::render_snapshot(),definition.library,definition.material,definition.light,definition.profile);settings=surface->defaults;for(auto*& p:cards){p=image_create_ex(256,256,IC_DEFAULT);if(!p)throw std::runtime_error("Cannot allocate study card");}error.clear();}
 catch(const std::exception& e){release();error=e.what();}
}
}
extern "C" bool ps_lighting_study_captures(){return displayed||input_barrier;}
extern "C" bool ps_lighting_study_visible(){return displayed&&!ps_hub_open();}
extern "C" void ps_lighting_study_detach(){release();level_id.clear();error.clear();if(displayed&&!ps_hub_open())bsl_host_authoring_pause(false);active=displayed=input_barrier=false;panel_open=true;orbit=false;}
extern "C" void ps_lighting_study_frame(bool editing){
 displayed=false;entry_ready=false;
 auto* level=ps::authoring::world_snapshot().levels.by_scene(bsl_engine_level_file());auto& registry=ps::authoring::render_snapshot();auto id=level?level->id:std::string{};auto found=registry.studies.find(id);
 if(found==registry.studies.end()){if(active)ps_lighting_study_detach();return;}
 if(level_id!=id){level_id=id;definition=found->second;active=true;panel_open=true;load();}
 if(input_barrier&&!ImGui::IsKeyDown(ImGuiKey_F9)&&!ImGui::GetIO().KeyCtrl)input_barrier=false;
 if(!ps_hub_open()&&!ImGui::GetIO().WantTextInput&&ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F9,false)){panel_open=!panel_open;input_barrier=true;}
 if(!panel_open||editing||ps_hub_open()||ps_project_ui_open())return;
 displayed=true;
 auto& io=ImGui::GetIO();float scale=std::min(1.f,std::min(io.DisplaySize.x/1450.f,io.DisplaySize.y/970.f));scale=std::max(.35f,scale);
 ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.03f,io.DisplaySize.y*.08f),ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x*.94f,io.DisplaySize.y*.82f),ImGuiCond_FirstUseEver);
 if(ImGui::Begin("Terrain surface lighting study",&panel_open)){
  ImGui::TextWrapped("Unoccluded directional preview | Ctrl+F9 hide/show | F10 database | F12 level editor. Controls are disposable preview copies; save definitions in the database and start a fresh test to reload them.");
  ImGui::TextWrapped("The calendar, point/spot placement, shadows, height relief and gameplay-light queries are not evaluated in this study. The course below remains unlit.");
  if(!error.empty()){ImGui::TextWrapped("Lighting unavailable: %s",error.c_str());if(ImGui::Button("Retry saved definition"))load();}
   auto& materials=registry.libraries.at(definition.library).materials;std::string label=definition.material;for(auto& m:materials)if(m["id"]==definition.material)label=m["name"].get<std::string>();bool combo=ImGui::BeginCombo("Surface material",label.c_str());combo_point=ImVec2((ImGui::GetItemRectMin().x+ImGui::GetItemRectMax().x)/2,(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)/2);if(combo){for(auto& m:materials){auto sh=m.value("shader",ps::authoring::Json{});if(!sh.is_string()||registry.shaders.at(sh.get<std::string>()).program!="builtin:terrain-lit"||!m.contains("lighting")||!m["lighting"].value("receive",false))continue;auto id=m["id"].get<std::string>();ImGui::PushID(id.c_str());if(ImGui::Selectable(m["name"].get<std::string>().c_str(),id==definition.material)){definition.material=id;load();}if(id!=definition.material&&!entry_ready){entry_point=ImVec2((ImGui::GetItemRectMin().x+ImGui::GetItemRectMax().x)/2,(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)/2);entry_ready=true;}ImGui::PopID();}ImGui::EndCombo();}else{std::vector<std::string> ids;int at=0;for(auto& m:materials){auto sh=m.value("shader",ps::authoring::Json{});if(sh.is_string()&&registry.shaders.at(sh.get<std::string>()).program=="builtin:terrain-lit"&&m.contains("lighting")&&m["lighting"].value("receive",false)){auto id=m["id"].get<std::string>();if(id==definition.material)at=int(ids.size());ids.push_back(id);}}if(ps::ui::wheel_selection(at,int(ids.size()))){definition.material=ids[at];load();}}
  if(surface){

   float side=std::max(48.f,std::min(256.f*scale,(ImGui::GetContentRegionAvail().x-3*ImGui::GetStyle().ItemSpacing.x)/4));
   if(ImGui::BeginTable("comparisons",4)){
    const char* labels[]={"Unlit albedo","Flat normal lighting","Normal-map lighting","World normal (+Y down)"};for(int i=0;i<4;++i){ImGui::TableNextColumn();ImGui::TextWrapped("%s",labels[i]);ImGui::Image((ImTextureID)(intptr_t)IMAGE2BITMAP(cards[i]),ImVec2(side,side));}ImGui::EndTable();
   }

   ImGui::TextWrapped("OpenGL normal convention: +X right, +Y up, +Z toward viewer. Light angle is toward the source: clockwise from world right. UV rotation also rotates normals; negative scale mirrors them.");
   ImGui::Checkbox("Orbit light (presentation time)",&orbit);ImGui::SameLine();if(ImGui::Button("Reset preview")){settings=surface->defaults;orbit=false;}
   ImGui::SliderFloat("Source angle",&settings.angle,0,359.9f,"%.1f deg");ImGui::SliderFloat("Elevation",&settings.elevation,0,90,"%.1f deg");ImGui::SliderFloat("Light energy",&settings.energy,0,3);ImGui::SliderFloat("Ambient energy",&settings.ambient_energy,0,1);
   ImGui::SliderFloat("Normal strength",&settings.strength,0,3);ImGui::SliderFloat("Roughness",&settings.roughness,0,1);ImGui::SliderFloat("Specular",&settings.specular,0,1);ImGui::SliderFloat("UV rotation",&settings.rotation,-180,180,"%.1f deg");
   bool mirror=settings.scale_x<0;if(ImGui::Checkbox("Mirror X",&mirror))settings.scale_x=-settings.scale_x;ImGui::SameLine();mirror=settings.scale_y<0;if(ImGui::Checkbox("Mirror Y",&mirror))settings.scale_y=-settings.scale_y;ImGui::SameLine();ImGui::Checkbox("DirectX green convention",&settings.directx);
   if(orbit)settings.angle=std::fmod(settings.angle+io.DeltaTime*30,360.f);
  }
 }
 ImGui::End();
}
extern "C" void ps_lighting_study_render(){
 if(!ps_lighting_study_visible()||!surface||!error.empty())return;
 try{for(int i=0;i<4;++i){auto v=settings;v.mode=i==0?0:i==3?2:1;if(i==1)v.strength=0;surface->draw(cards[i],v);}}
 catch(const std::exception& e){error=e.what();}
}
extern "C" int ps_lighting_study_check(char* error,unsigned capacity){return ps::lighting::native_check(error,capacity);}

// Exercise the actual combo and hide/show route through queued ImGui input.
extern "C" int ps_lighting_study_ui_check(int phase){
 if(!ps_lab_automatic())return -1;auto& io=ImGui::GetIO();
 auto click=[&](ImVec2 point,bool down){io.AddMousePosEvent(point.x,point.y);io.AddMouseButtonEvent(0,down);};
 auto toggle=[&](bool down){io.AddKeyEvent(ImGuiMod_Ctrl,down);io.AddKeyEvent(ImGuiKey_F9,down);};
 switch(phase){
 case 0:if(!displayed||!surface||!error.empty())return -1;test_previous=definition.material;click(combo_point,true);break;
 case 1:click(combo_point,false);break;
 case 2:if(!entry_ready)return -1;click(entry_point,true);break;
 case 3:click(entry_point,false);break;
 case 4:if(definition.material==test_previous||!surface||!error.empty()||!cards[0]||ImGui::GetCurrentContext()->ErrorCountCurrentFrame)return -1;break;
 case 5:toggle(true);break;
 case 6:if(displayed||!input_barrier)return -1;toggle(false);break;
 case 7:if(displayed||input_barrier)return -1;toggle(true);break;
 case 8:if(!displayed||!input_barrier)return -1;toggle(false);break;
 case 9:if(!displayed||input_barrier||!error.empty())return -1;break;
 default:return -1;
 }return 1;
}
