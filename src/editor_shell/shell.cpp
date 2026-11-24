#include "../labs/lab.h"

// Shared native editor UI. Existing terrain/level providers own all writable content. Signed: President.
#include "shell.h"
#include "../ui/widgets.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_allegro5.h"
extern "C" {
#include "../core/asset.h"
#include "../core/video.h"
#include "../core/image.h"
#include "../scenes/level.h"
#include "../cell_terrain/native.h"
#include "../util/v2d.h"
}
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
struct Command {std::string id;double value;uint64_t target;BslShellDrop drop{};};
uint64_t command_target=0;BslShellDrop command_drop{};bool command_has_drop=false;
struct Message {std::string category,text;int severity,count=1;double time;};
struct Region {float x,y,w,h;bool hit(float a,float b)const{return a>=x&&b>=y&&a<x+w&&b<y+h;}};
std::deque<Command> commands;std::deque<Message> log;
std::map<std::string,std::string> statuses,baselines;
std::vector<Region> regions;
bool test_injecting=false;
bool frame_open=false;
bool ready=false,visible=false,scene_level=false,editing=false,was_editing=false,blocked=false,keyboard=false,handled=false;
bool drawer=true,user_hidden=false,log_open=false,details=false,help=false,modal=false,focus=true,release_block=false,wait_release=false;
bool save_and_resume=false,save_and_reload=false;
int last_severity=0;
bool game_ui_capture=false;
bool final_capture=false;
int previous_tool=-1;
int owner[8]={0},tool=0,tab=0,palette_kind=0,severity=0;
float actual_drawer_width=0;
float width=0,height=0,top=0,bottom=0,progress=0,scale=1,slide=.18f,base_size=24,drawer_fraction=.25f,log_fraction=.25f;
bool reduced=false;
char search[128]="",log_search[128]="";
uint64_t previous_selection=0;int previous_shape=0;
std::string settings_path,last_note="Ready",intent,lab_intent;
bool lab_modal=false,save_and_lab=false;
BslShellState state{};
bool view_keys=false,view_middle=false,view_dragging=false,view_sample=false;
float view_start_x=0,view_start_y=0,view_last_x=0,view_last_y=0;
bool palette_active=false,palette_released=false;
BslShellDrop palette_pending{};
ImVec2 test_palette_row{};bool test_palette_visible=false;
bool view_canvas(float x,float y);


std::string raw_file(const char* name,bool& okay){
 okay=false;if(!name||!*name)return {};
 ALLEGRO_FILE* f=al_fopen(asset_path(name),"rb");if(!f)return {};
 auto n=al_fsize(f);std::string bytes;
 if(n>=0&&n<=16*1024*1024){bytes.resize(size_t(n));okay=al_fread(f,bytes.data(),bytes.size())==bytes.size();}
 al_fclose(f);return bytes;
}
bool hit(float x,float y){for(const auto& r:regions)if(r.hit(x,y))return true;return false;}
bool view_canvas(float x,float y){return x>=0&&y>=top&&x<width&&y<height-bottom&&!hit(x,y);}
bool contains(const char* value,const char* query){
 std::string a=value,b=query;for(auto& c:a)c=char(std::tolower((unsigned char)c));for(auto& c:b)c=char(std::tolower((unsigned char)c));return a.find(b)!=std::string::npos;
}
void submit(const char* id,double value=0){commands.push_back({id,value,state.selection});}
void note(const char* category,const char* text,int severity){
 if(!text||!*text)return;
 double now=al_get_time();
 if(!log.empty()&&log.back().category==category&&log.back().text==text&&log.back().severity==severity){log.back().count++;log.back().time=now;}
 else {log.push_back({category,std::string(text).substr(0,4096),severity,1,now});while(log.size()>512)log.pop_front();}
 last_note=text;last_severity=severity;
}
float config_number(ALLEGRO_CONFIG* c,const char* key,float fallback){
 return ps::ui::appearance_number(c,key,fallback);
}
void init(){
 auto* display=al_get_current_display();if(!display)return;
 IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();
 io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.ConfigInputTrickleEventQueue=false;
 ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.WindowRounding=3;style.FrameRounding=3;style.WindowBorderSize=1;
 style.Colors[ImGuiCol_WindowBg]=ImVec4(.065f,.09f,.12f,.98f);style.Colors[ImGuiCol_ChildBg]=style.Colors[ImGuiCol_WindowBg];
 style.Colors[ImGuiCol_Button]=ImVec4(.12f,.25f,.30f,1);style.Colors[ImGuiCol_Header]=ImVec4(.16f,.35f,.40f,1);
 auto* cfg=al_load_config_file(asset_path("editor-ui.ini"));
 const char* font=cfg?al_get_config_value(cfg,"editor","font"):nullptr;
 base_size=std::clamp(config_number(cfg,"font_size",24),16.f,36.f);scale=std::clamp(config_number(cfg,"scale",1),.75f,2.f);slide=std::clamp(config_number(cfg,"slide_seconds",.18f),0.f,.5f);
 reduced=cfg&&al_get_config_value(cfg,"editor","reduced_motion")&&!strcmp(al_get_config_value(cfg,"editor","reduced_motion"),"true");
 if(!font)font="fonts/Roboto-Medium.ttf";
 bool font_ok=false;auto data=raw_file(font,font_ok);
 if(font_ok){void* owned=IM_ALLOC(data.size());memcpy(owned,data.data(),data.size());io.Fonts->AddFontFromMemoryTTF(owned,(int)data.size(),base_size);}
 else io.Fonts->AddFontDefault();
 drawer_fraction=std::clamp(config_number(cfg,"drawer_fraction",.25f),.15f,.25f);
 log_fraction=std::clamp(config_number(cfg,"log_fraction",.25f),.1f,1.f/3);
 style.WindowPadding=ImVec2(config_number(cfg,"padding",10),config_number(cfg,"padding",10));
 style.FramePadding=ImVec2(config_number(cfg,"frame_padding_x",8),config_number(cfg,"frame_padding_y",5));
 for(auto entry:{std::pair<const char*,ImGuiCol>("background",ImGuiCol_WindowBg),{"panel",ImGuiCol_FrameBg},{"accent",ImGuiCol_ButtonActive}}){
  const char* text=cfg?al_get_config_value(cfg,"colors",entry.first):nullptr;float r,g,b,a;
  if(text&&sscanf(text,"%f,%f,%f,%f",&r,&g,&b,&a)==4&&std::isfinite(r)&&std::isfinite(g)&&std::isfinite(b)&&std::isfinite(a))style.Colors[entry.second]=ImVec4(std::clamp(r,0.f,1.f),std::clamp(g,0.f,1.f),std::clamp(b,0.f,1.f),std::clamp(a,0.f,1.f));
 }
 if(cfg)al_destroy_config(cfg);else note("editor","Optional editor-ui.ini is absent; using default appearance.",1);
 char user[4096];asset_user_datadir(user,sizeof user);
 uint64_t workspace_key=UINT64_C(14695981039346656037);for(const unsigned char* p=(const unsigned char*)user;*p;p++){workspace_key^=*p;workspace_key*=UINT64_C(1099511628211);}
 const char* local=std::getenv("LOCALAPPDATA");auto directory=std::filesystem::u8path(local?local:user)/"PowerSurge/editor-shell"/std::to_string(workspace_key);
 std::error_code directory_error;if(!ps_lab_active())std::filesystem::create_directories(directory,directory_error);settings_path=(directory/"preferences.ini").u8string();
 if(ps_lab_active())settings_path=ps_lab_preferences();
 auto* previous_files=al_get_new_file_interface();al_set_standard_file_interface();
 auto* prefs=al_load_config_file(settings_path.c_str());al_set_new_file_interface(previous_files);
 if(prefs){scale=std::clamp(config_number(prefs,"scale",scale),.75f,2.f);auto* d=al_get_config_value(prefs,"editor","drawer");if(d)drawer=strcmp(d,"hidden")!=0;user_hidden=!drawer;auto* motion=al_get_config_value(prefs,"editor","reduced_motion");if(motion)reduced=!strcmp(motion,"true");al_destroy_config(prefs);}
 if(std::getenv("BSL_SHELL_CHECK")&&std::getenv("BSL_SHELL_SCALE"))scale=std::clamp(float(atof(std::getenv("BSL_SHELL_SCALE"))),.75f,2.f);
 if(!ImGui_ImplAllegro5_Init(display)){ImGui::DestroyContext();return;}
 ready=true;note("editor","Native editor shell ready.",0);ps_ui_init();
}
bool command_button(const char* label,const char* command,double value=0,bool enabled=true){
 float available=std::max(1.f,ImGui::GetContentRegionAvail().x-2*ImGui::GetStyle().FramePadding.x);
 std::istringstream words(label);std::string word,line,wrapped;
 while(words>>word){auto next=line.empty()?word:line+" "+word;if(!line.empty()&&ImGui::CalcTextSize(next.c_str()).x>available){wrapped+=line+"\\n";line=word;}else line=next;}
 wrapped+=line;auto caption=wrapped+"###command";ImGui::PushID(command);ImGui::BeginDisabled(!enabled);
 bool pressed=ImGui::Button(caption.c_str(),ImVec2(std::min(ImGui::GetContentRegionAvail().x,ImGui::CalcTextSize(wrapped.c_str()).x+2*ImGui::GetStyle().FramePadding.x),0));
 if(pressed)submit(command,value);ImGui::EndDisabled();if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",label);ImGui::PopID();return pressed;
}
void tool_button(const char* label,int value){
 if(state.tool==value)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
 ImGui::BeginDisabled(value==1&&!state.terrain);
 if(ImGui::Button(label)){submit("tool",value);tool=value;if(!user_hidden){drawer=true;tab=value==0?1:0;}if(value==1)palette_kind=2;else if(value==4)palette_kind=1;else palette_kind=0;}
 ImGui::EndDisabled();if(value==1&&!state.terrain&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("This level has no cell terrain attachment. Configure one through level properties.");
 if(state.tool==value)ImGui::PopStyleColor();
}
void want_depart(const char* target){
 if(state.terrain_dirty||state.placements_dirty){intent=target;modal=true;}
 else submit(target);
}
void menu_item(const char* label,const char* shortcut,const char* id,double value=0,bool enabled=true){if(ImGui::MenuItem(label,ps::ui::shortcut(id)?ps::ui::shortcut(id):shortcut,false,enabled))submit(id,value);}
void menu(){
 if(!ImGui::BeginMainMenuBar())return;
 top=ImGui::GetWindowHeight();
 if(ImGui::BeginMenu("File")){
  menu_item("Save all level edits","Ctrl+S","save",0,!state.readonly);
  if(ImGui::MenuItem("Reload level...","Ctrl+R"))want_depart("reload");
  ImGui::Separator();menu_item("Open RPG editor",nullptr,"external");ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Edit")){
  menu_item(state.tool==1?"Undo terrain":"Undo placements","Ctrl+Z","undo");menu_item("Redo","Ctrl+Y","redo");
  menu_item("Delete selected entity","Delete","delete",0,state.selection&&*state.name&&!strstr(state.name,"Decoration /"));
  ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Level")){
  menu_item("Level properties in RPG editor",nullptr,"level");
  menu_item("Go to spawn","Home","home");menu_item("Next teleporter","Page Up","door",1);menu_item("Previous teleporter","Page Down","door",-1);
  ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("View")){
  menu_item("Reset editor zoom (100%)",nullptr,"view.reset");
  if(ImGui::BeginMenu("Editor zoom")){
   for(float zoom:{.2f,.5f,1.f,2.f,5.f,10.f}){char label[32];snprintf(label,sizeof label,"%.0f%%",zoom*100);menu_item(label,nullptr,"view.set",zoom);}
   ImGui::EndMenu();
  }
  ImGui::Separator();
  if(ImGui::MenuItem("Right drawer",nullptr,drawer)){drawer=!drawer;user_hidden=!drawer;}
  ImGui::MenuItem("Diagnostic log","F8",&log_open);ImGui::MenuItem("Live diagnostics",nullptr,&details);
  if(ImGui::BeginMenu("Collision overlays")){
   const char* labels[]={"Actor body","Hurtboxes","Hitboxes","Origins","Ledge sensors","Soft contacts"};
   auto mask=bsl_shell_debug_get();for(int i=0;i<6;i++)if(ImGui::MenuItem(labels[i],nullptr,(mask&(1u<<i))!=0))bsl_shell_debug_set(mask^(1u<<i));
   ImGui::EndMenu();
  }
  if(ImGui::BeginMenu("Interface scale")){for(float n:{.75f,1.f,1.25f,1.5f,2.f}){char text[32];snprintf(text,sizeof text,"%.0f%%",100*n);if(ImGui::MenuItem(text,nullptr,scale==n))scale=n;}ImGui::MenuItem("Reduced motion",nullptr,&reduced);ImGui::EndMenu();}
  ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Test")){
  if(ImGui::MenuItem("Resume gameplay","F12"))submit("resume");
  if(ImGui::BeginMenu("Laboratories")){
   if(ImGui::MenuItem("Shared UI pilot")){lab_intent="ui-pilot";lab_modal=true;}
   if(ImGui::MenuItem("Shared UI / compatibility checks")){lab_intent="ui-pilot-check";lab_modal=true;}
   if(ImGui::MenuItem("Terrain / editor")){lab_intent="terrain";lab_modal=true;}
   if(ImGui::MenuItem("Physics objects")){lab_intent="physics";lab_modal=true;}
   if(ImGui::MenuItem("Connected terrain journey")){lab_intent="terrain-journey";lab_modal=true;}
   if(ImGui::MenuItem("Editor interaction checks")){lab_intent="editor-contract";lab_modal=true;}
   if(ImGui::MenuItem("Fullscreen monitor checks (moves this window)")){lab_intent="display-check";lab_modal=true;}
   if(ImGui::MenuItem("Menu artwork checks")){lab_intent="presentation-menu-check";lab_modal=true;}
   ImGui::EndMenu();
  }ImGui::EndMenu();
 }
 if(ImGui::BeginMenu("Help")){ImGui::MenuItem("Editor controls","F1",&help);ImGui::EndMenu();}
 ImGui::EndMainMenuBar();
}
void toolbar(){
 ImGui::SetNextWindowPos(ImVec2(0,top));ImGui::SetNextWindowSize(ImVec2(width,0));
 ImGui::Begin("##toolbar",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
 if(width>1100*scale){
  tool_button("Select / Inspect",0);ImGui::SameLine();tool_button("Terrain",1);ImGui::SameLine();tool_button("Objects",2);
  ImGui::SameLine();tool_button("Events",3);ImGui::SameLine();tool_button("Decoration",4);
 }else{
  const char* names[]={"Select / Inspect","Terrain","Objects","Events","Decoration"};
  ImGui::SetNextItemWidth(std::min(width-20,480*scale));
  if(ImGui::BeginCombo("##active_tool",names[state.tool])){
   for(int i=0;i<5;i++){ImGui::BeginDisabled(i==1&&!state.terrain);if(ImGui::Selectable(names[i],state.tool==i))submit("tool",i);ImGui::EndDisabled();}
   ImGui::EndCombo();
  }
 }
 if(state.tool==1){
  int action=state.paint_action;const char* actions[]={"Paint","Erase","Line"};ImGui::SetNextItemWidth(std::min(130*scale,width*.3f));if(ImGui::Combo("##action",&action,actions,3))submit("terrain.action",action);
  ImGui::SameLine();int grid=state.grid;ImGui::SetNextItemWidth(std::min(130*scale,width*.3f));
  const char* sizes[]={"8 px","16 px","32 px","64 px","128 px"};int at=0;while(at<4&&(8<<at)<grid)at++;
  if(ImGui::Combo("##brush_grid",&at,sizes,5))submit("grid",8<<at);
  ImGui::SameLine();ImGui::SetNextItemWidth(std::min(150*scale,width*.32f));int mode=state.mode;const char* modes[]={"Linear","Over","Under"};if(ImGui::Combo("##smoothing",&mode,modes,3))submit("terrain.mode_set",mode);
  if(width>1200*scale){ImGui::SameLine();command_button("New shape","terrain.new_shape");}
 }
 if(width>1100*scale)ImGui::TextDisabled("Tool: %s",state.tool==0?"Select / Inspect":state.tool==1?"Terrain (Shift + two clicks: line)":state.tool==2?"Objects":state.tool==3?"Events":"Decoration");
 if(state.ready_kind>=0&&state.ready_index>=0)ImGui::TextWrapped("Ready: %s",bsl_shell_palette_name(state.ready_kind,state.ready_index));
 float total=ImGui::GetWindowPos().y+ImGui::GetWindowHeight();regions.push_back({0,0,width,total});top=total;
 ImGui::End();
}
void palette_choice(int kind,int index){
 submit(kind==0?"palette.entity":kind==1?"palette.brick":"terrain.material",index);
 if(kind==2)submit("tool",1);
 note("placement",kind==2?"Material selected. Drag onto a terrain shape to assign it.":"Ready for placement. Click the level or drag this entry into it.",0);
}
void palette(){
 test_palette_visible=false;
 const char* kinds[]={"Entities","Decoration / bricks","Materials"};
 ImGui::TextUnformatted("Category");ImGui::SetNextItemWidth(-1);if(ImGui::Combo("##category",&palette_kind,kinds,3))search[0]=0;
 ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##search","Search palette",search,sizeof search);
 ImGui::TextWrapped(palette_kind==2?"Select a material; drag onto terrain to assign it.":"Click to prepare placement. Drag an entry into the level to place it.");
 int count=bsl_shell_palette_count(palette_kind);ImGui::TextDisabled("%d entries",count);
 ImGui::BeginChild("entries",ImVec2(0,0),ImGuiChildFlags_None);
 for(int i=0;i<count;i++){
  const char* name=bsl_shell_palette_name(palette_kind,i);if(!contains(name,search))continue;ImGui::PushID(i);
  auto* bitmap=(ALLEGRO_BITMAP*)bsl_shell_palette_image(palette_kind,i);
  float side=44*scale,gap=bitmap?side+ImGui::GetStyle().ItemSpacing.x:0;
  float row_width=ImGui::GetContentRegionAvail().x,text_width=std::max(1.f,row_width-gap);
  ImVec2 size=ImGui::CalcTextSize(name,nullptr,false,text_width);ImVec2 start=ImGui::GetCursorScreenPos();
  bool selected=palette_kind==2?state.material==i:state.ready_kind==palette_kind&&state.ready_index==i;
  if(ImGui::Selectable("##entry",selected,palette_kind==2?ImGuiSelectableFlags_None:ImGuiSelectableFlags_SelectOnClick,ImVec2(0,std::max(size.y,side))))palette_choice(palette_kind,i);
  if(!strcmp(name,"BOR Pole")&&ImGui::IsItemVisible()){test_palette_row=ImVec2(start.x+10,start.y+side*.5f);test_palette_visible=true;}
  if(ImGui::BeginDragDropSource()){
   BslShellDrop payload{palette_kind,i,0,0};palette_pending=payload;palette_active=true;ImGui::SetDragDropPayload("PS_PALETTE",&payload,sizeof payload);
   ImGui::TextUnformatted(name);ImGui::TextUnformatted("Release over the level; Esc cancels");ImGui::EndDragDropSource();
  }
  if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",name);
  auto* draw=ImGui::GetWindowDrawList();if(bitmap)draw->AddImage((ImTextureID)(intptr_t)bitmap,start,ImVec2(start.x+side,start.y+side));
  draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(start.x+gap,start.y),ImGui::GetColorU32(ImGuiCol_Text),name,nullptr,text_width);
  ImGui::PopID();
 }
 ImGui::EndChild();
}
void palette_drag(){
 if(!palette_active)return;
 auto mouse=ImGui::GetIO().MousePos;
 if(!editing||!focus||ImGui::IsKeyPressed(ImGuiKey_Escape,false)){
  palette_active=palette_released=false;ImGui::ClearDragDrop();note("placement","Placement cancelled.",0);return;
 }
 if(palette_released||ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
  auto drop=palette_pending;palette_active=palette_released=false;
  if(view_canvas(mouse.x,mouse.y)&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)&&!state.readonly)
   bsl_shell_request_drop(drop.kind,drop.index,mouse.x,mouse.y);
  else note("placement","Drop cancelled: release over the level canvas.",0);
  ImGui::ClearDragDrop();release_block=true;wait_release=true;
 }
}

void properties(){
 if(*state.name){
  ImGui::TextWrapped("%s",state.name);ImGui::TextDisabled("Placement %llx",(unsigned long long)state.selection);
  ImGui::TextWrapped("Position: %.0f, %.0f px",state.selected_x,state.selected_y);
  ImGui::TextDisabled("Drag in Select mode to move.");
  BslShellField f{};int fields=0;
  while(bsl_shell_field(fields++,&f)){
   double value=f.value;int action=ps::ui::instance_field(f,value);char command[64];
   if(action){snprintf(command,sizeof command,action==2?"reset.%d":"field.%d",f.index);submit(command,value);}
  }
  if(fields==1)ImGui::TextWrapped("No editable instance fields are registered for this object.");
  if(strstr(state.name,"Decoration /")){int v=state.brightness;if(ImGui::SliderInt("Brightness",&v,0,15))submit("brightness",v);}
  if(!strcmp(state.name,"BSL Event Anchor")){
   ImGui::SeparatorText("Common event");
   if(ImGui::BeginCombo("##event","Choose registered event")){
    if(ImGui::Selectable("Unbound"))submit("event",-1);
    for(int i=0;i<bsl_shell_palette_count(3);i++)if(ImGui::Selectable(bsl_shell_palette_name(3,i)))submit("event",i);
    ImGui::EndCombo();
   }
  }
  command_button("Open RPG editor","external");
  command_button("Edit linked definition","definition",0,!strcmp(state.name,"BSL Teleporter")||!strcmp(state.name,"BSL Event Anchor"));
  if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&strcmp(state.name,"BSL Teleporter")&&strcmp(state.name,"BSL Event Anchor"))ImGui::SetTooltip("No supported definition handoff is registered for this object yet.");
  if(strstr(state.name,"BSL Physics"))ImGui::TextWrapped("Experimental physics lab. Shape/profile editing, attack forces and actor support are not bound.");
 }else if(state.terrain){
  ImGui::TextWrapped("Terrain shape %d",state.selected_shape);
  int at=state.material;
  ImGui::TextUnformatted("Material");ImGui::SetNextItemWidth(-1);
  if(ImGui::BeginCombo("##material",bsl_shell_palette_name(2,at))){for(int i=0;i<state.materials;i++)if(ImGui::Selectable(bsl_shell_palette_name(2,i),i==at))submit("terrain.material",i);ImGui::EndCombo();}
  command_button("New shape","terrain.new_shape");
  ImGui::TextWrapped("Brush shape");command_button(state.preset?"Round":"Square","terrain.preset");
  ImGui::TextWrapped("Blend with adjacent");bool blend=state.blend;if(ImGui::Checkbox("On",&blend))submit("terrain.blend");
  ImGui::TextWrapped("Blend width (cells)");ImGui::SetNextItemWidth(-1);int cells=state.blend_cells;if(ImGui::SliderInt("##blendcells",&cells,0,4)){int delta=cells-state.blend_cells;for(int i=0;i<std::abs(delta);i++)submit(delta>0?"terrain.blend_more":"terrain.blend_less");}
  ImGui::TextWrapped("Curve");ImGui::SetNextItemWidth(-1);float curve=state.curve;if(ImGui::SliderFloat("##curve",&curve,0,1,"%.2f"))submit("terrain.curve",curve);
  ImGui::TextWrapped("Material edits affect every region of this shape. New shape creates an independent association.");
  command_button("Reload materials","terrain.reload_materials");
 }else ImGui::TextWrapped("Select an object in the scene, or choose a tool and a palette entry.");
}
void right_drawer(float dt){
 float target=drawer?1.f:0.f;progress=reduced||slide<=0?target:progress+std::clamp(target-progress,-dt/slide,dt/slide);
 if(std::abs(progress-target)<.002f)progress=target;
 float panel_width=width*drawer_fraction;float ease=1-(1-progress)*(1-progress)*(1-progress);float x=width-panel_width*ease;
 float h=std::max(1.f,height-top-bottom);float handle=22*scale;
 ImGui::SetNextWindowPos(ImVec2(std::min(width-handle,x-handle),top+h*.5f-25*scale));
 ImGui::SetNextWindowSize(ImVec2(handle,50*scale));
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
 ImGui::Begin("##handle",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
 if(ImGui::Button(drawer?">":"<",ImVec2(-1,-1))){drawer=!drawer;user_hidden=!drawer;}
 auto hp=ImGui::GetWindowPos(),hs=ImGui::GetWindowSize();regions.push_back({hp.x,hp.y,hs.x,hs.y});ImGui::End();ImGui::PopStyleVar();
 if(progress==0)return;
 ImGui::SetNextWindowPos(ImVec2(x,top));ImGui::SetNextWindowSize(ImVec2(panel_width,h));
 ImGui::Begin("Inspector",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
 actual_drawer_width=ImGui::GetWindowSize().x;regions.push_back({x,top,actual_drawer_width,h});
 if(ImGui::GetContentRegionAvail().x<ImGui::CalcTextSize("Palette Properties").x+50*scale){const char* tabs[]={"Palette","Properties"};ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##tab",tab==0?"Palette":"Props")){for(int i=0;i<2;i++)if(ImGui::Selectable(tabs[i],tab==i))tab=i;ImGui::EndCombo();}}
 else {if(ImGui::Button("Palette"))tab=0;ImGui::SameLine();if(ImGui::Button("Properties"))tab=1;}
 ImGui::Separator();if(tab==0)palette();else {auto identity=std::to_string(state.selection)+":"+std::to_string(state.selected_shape);ImGui::PushID(identity.c_str());properties();ImGui::PopID();}
 ImGui::End();
}
void status_bar(){
 const float row=ImGui::GetFrameHeightWithSpacing();bottom=row;
 ImGui::SetNextWindowPos(ImVec2(0,height-row));ImGui::SetNextWindowSize(ImVec2(width,row));
 ImGui::Begin("##status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
 if(ImGui::SmallButton(log_open?"Log v":"Log ^"))log_open=!log_open;
 ImGui::SameLine();ImGui::Text("%s%s%s",editing?"EDIT":"PLAY",state.terrain_dirty?" terrain*":"",state.placements_dirty?" placements*":"");
 if(width>1100*scale){ImGui::SameLine();ImGui::Text("| %.2f, %.2f m | grid %d | zoom %.0f%%",state.x,state.y,state.grid,state.zoom*100);}
 ImGui::SameLine();ImGui::TextColored(last_severity==2?ImVec4(1,.45f,.4f,1):ImVec4(.8f,.87f,.9f,1),"%s",last_note.c_str());if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",last_note.c_str());
 ImGui::End();regions.push_back({0,height-row,width,row});
 if(log_open){
  float h=std::min(height/3,std::max(1.f,height*log_fraction));bottom+=h;
  ImGui::SetNextWindowPos(ImVec2(0,height-bottom));ImGui::SetNextWindowSize(ImVec2(width,h));
  ImGui::Begin("Diagnostic log",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
  const char* filters[]={"All","Warnings + errors","Errors"};ImGui::SetNextItemWidth(std::min(230*scale,width*.3f));ImGui::Combo("##severity",&severity,filters,3);
  ImGui::SameLine();ImGui::SetNextItemWidth(std::min(230*scale,width*.3f));ImGui::InputTextWithHint("##logsearch","Category or message",log_search,sizeof log_search);
  ImGui::SameLine();if(ImGui::Button("Copy")){std::string text;for(const auto& m:log)text+=m.category+": "+m.text+"\n";ImGui::SetClipboardText(text.c_str());}
  ImGui::BeginChild("messages");for(const auto& m:log)if(m.severity>=severity&&(contains(m.category.c_str(),log_search)||contains(m.text.c_str(),log_search))){ImGui::TextColored(m.severity==2?ImVec4(1,.45f,.4f,1):m.severity==1?ImVec4(1,.8f,.4f,1):ImVec4(.8f,.87f,.9f,1),"%.1f [%s] x%d",m.time,m.category.c_str(),m.count);ImGui::SameLine();ImGui::TextWrapped("%s",m.text.c_str());}
  ImGui::EndChild();ImGui::End();regions.push_back({0,height-bottom,width,h});
 }
}
void dialogs(){
 if(lab_modal){ImGui::OpenPopup("Replace current level?");lab_modal=false;}
 if(ImGui::BeginPopupModal("Replace current level?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::TextWrapped("Starting this laboratory replaces the current level in this instance and starts a disposable gameplay session.");
  if(state.terrain_dirty||state.placements_dirty){
   ImGui::TextWrapped("The current level has unsaved edits. Saved test workspaces will be retained when changing labs.");
   if(ImGui::Button("Save and start lab")){submit("save");save_and_lab=true;ImGui::CloseCurrentPopup();}
   ImGui::SameLine();
   if(ImGui::Button("Discard edits and start")){submit(("lab."+lab_intent).c_str());ImGui::CloseCurrentPopup();}
  }else if(ImGui::Button("Replace level and start")){submit(("lab."+lab_intent).c_str());ImGui::CloseCurrentPopup();}
  ImGui::SameLine();if(ImGui::Button("Cancel")){lab_intent.clear();save_and_lab=false;ImGui::CloseCurrentPopup();}
  ImGui::EndPopup();
 }

 if(help){ImGui::SetNextWindowSize(ImVec2(std::min(width,700*scale),0),ImGuiCond_Appearing);ImGui::Begin("Editor controls",&help);
  ImGui::TextWrapped("Choose Select / Inspect, Terrain or Objects from the tool row. Select mode never paints. Palette searches placeable runtime objects; Properties edits supported fields.");
  ImGui::TextWrapped("Terrain: drag to paint; right drag erases; Shift + two clicks draws a line. Esc or right click cancels its first point. Middle click samples the active tool; middle drag pans. Ctrl + wheel zooms this editor view (20%%-1000%%); View resets zoom. G changes the grid/brush.");
  ps::ui::command_help();
  auto hp=ImGui::GetWindowPos(),hs=ImGui::GetWindowSize();regions.push_back({hp.x,hp.y,hs.x,hs.y});ImGui::End();
 }
 if(details){ImGui::SetNextWindowSize(ImVec2(std::min(width,760*scale),height*.5f),ImGuiCond_Appearing);ImGui::Begin("Live diagnostics",&details);
  ImGui::TextWrapped("Scene: %s",state.level);ImGui::Text("Tool %d | selected %llx",state.tool,(unsigned long long)state.selection);
  for(const auto& entry:statuses){ImGui::SeparatorText(entry.first.c_str());ImGui::TextWrapped("%s",entry.second.c_str());}auto hp=ImGui::GetWindowPos(),hs=ImGui::GetWindowSize();regions.push_back({hp.x,hp.y,hs.x,hs.y});ImGui::End();
 }
 if(modal){ImGui::OpenPopup("Unsaved level edits");modal=false;}
 if(ImGui::BeginPopupModal("Unsaved level edits",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::TextUnformatted("Terrain and/or placements contain unsaved changes.");
  if(ImGui::Button("Save and continue")){submit("save");save_and_reload=intent=="reload";save_and_resume=intent=="resume";ImGui::CloseCurrentPopup();}
  ImGui::SameLine();if(ImGui::Button("Discard and reload")){submit("reload");ImGui::CloseCurrentPopup();}
  ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
 }
}
void test_tick();
}
extern "C" bool bsl_shell_present(){return ready;}
extern "C" void bsl_shell_status(const char* category,const char* text){if(text)statuses[category]=std::string(text).substr(0,16384);}
extern "C" void bsl_shell_note(const char* category,const char* text,int severity){note(category,text,severity);}
extern "C" void bsl_shell_submit(const char* id,double value){submit(id,value);}
extern "C" bool bsl_shell_pop(char* id,int capacity,double* value){if(commands.empty())return false;auto at=std::find_if(commands.begin(),commands.end(),[](const Command& c){return c.id.rfind("field.",0)==0||c.id.rfind("reset.",0)==0;});if(at==commands.end())at=commands.begin();auto c=*at;commands.erase(at);snprintf(id,capacity,"%s",c.id.c_str());*value=c.value;command_target=c.target;command_drop=c.drop;command_has_drop=c.id=="palette.drop"||c.id=="view.sample";return true;}
extern "C" void bsl_shell_request_drop(int kind,int index,float x,float y){commands.push_back({"palette.drop",double(index),state.selection,{kind,index,x,y}});}
extern "C" bool bsl_shell_drop_payload(BslShellDrop* out){if(!command_has_drop||!out)return false;*out=command_drop;return true;}
extern "C" uint64_t bsl_shell_command_target(){return command_target;}
extern "C" void bsl_shell_detach(){ps_ui_detach();visible=false;commands.clear();previous_selection=0;previous_shape=0;previous_tool=-1;editing=was_editing=false;view_middle=view_dragging=palette_active=palette_released=false;view_keys=false;regions.clear();}
extern "C" bool bsl_shell_keyboard(){if(ps_lab_automatic())return true;if(!ready||!visible)return false;if(std::getenv("BSL_SHELL_CHECK")&&!editing)return true;return editing?(keyboard||handled||blocked):game_ui_capture;}
extern "C" bool bsl_shell_block_editor(){return blocked||handled||view_middle;}
extern "C" bool bsl_shell_navigation_keyboard(){return view_keys;}
extern "C" bool bsl_shell_file_capture(const char* path){bool ok;auto bytes=raw_file(path,ok);if(ok)baselines[path]=std::move(bytes);return ok;}
extern "C" bool bsl_shell_file_unchanged(const char* path){bool ok;auto bytes=raw_file(path,ok);auto found=baselines.find(path);return ok&&found!=baselines.end()&&found->second==bytes;}
extern "C" void bsl_shell_event(ALLEGRO_EVENT* e){
 if(!ready)return;
 if(std::getenv("BSL_SHELL_CHECK")&&!test_injecting&&(e->type>=ALLEGRO_EVENT_JOYSTICK_AXIS&&e->type<=ALLEGRO_EVENT_MOUSE_WARPED))return;
 ImGui_ImplAllegro5_ProcessEvent(e);
 if(editing&&focus){
  const bool popup=ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId);
  if(e->type==ALLEGRO_EVENT_MOUSE_BUTTON_DOWN&&e->mouse.button==3&&!popup){
   view_middle=true;view_dragging=false;view_sample=view_canvas(e->mouse.x,e->mouse.y);
   view_start_x=view_last_x=e->mouse.x;view_start_y=view_last_y=e->mouse.y;
  }
  if(e->type==ALLEGRO_EVENT_MOUSE_AXES&&view_middle){
   float x=e->mouse.x,y=e->mouse.y;
   if(!view_dragging&&std::hypot(x-view_start_x,y-view_start_y)>=6){view_dragging=true;submit("cancel");}
   if(view_dragging){submit("view.pan_x",x-view_last_x);submit("view.pan_y",y-view_last_y);}
   view_last_x=x;view_last_y=y;
  }
  if(e->type==ALLEGRO_EVENT_MOUSE_BUTTON_UP&&e->mouse.button==3&&view_middle){
   if(!view_dragging&&view_sample&&view_canvas(e->mouse.x,e->mouse.y))commands.push_back({"view.sample",0,state.selection,{-1,0,view_start_x,view_start_y}});
   view_middle=view_dragging=false;release_block=true;
  }
  if(e->type==ALLEGRO_EVENT_MOUSE_AXES&&e->mouse.dz&&!popup&&view_canvas(e->mouse.x,e->mouse.y)){
   ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);
   if(al_key_down(&keys,ALLEGRO_KEY_LCTRL)||al_key_down(&keys,ALLEGRO_KEY_RCTRL))submit("view.zoom",e->mouse.dz);
  }
  if(e->type==ALLEGRO_EVENT_MOUSE_BUTTON_UP&&e->mouse.button==1&&palette_active)palette_released=true;
 }

 if(e->type==ALLEGRO_EVENT_DISPLAY_SWITCH_OUT){view_middle=view_dragging=palette_active=palette_released=false;focus=false;wait_release=true;submit("cancel");for(auto& o:owner)o=0;}
 if(e->type==ALLEGRO_EVENT_DISPLAY_SWITCH_IN)focus=true;
 if(e->type==ALLEGRO_EVENT_MOUSE_BUTTON_DOWN&&e->mouse.button<8)owner[e->mouse.button]=visible&&(hit(e->mouse.x,e->mouse.y)||ImGui::GetIO().WantCaptureMouse)?1:2;
 if(e->type==ALLEGRO_EVENT_MOUSE_BUTTON_UP&&e->mouse.button<8){if(owner[e->mouse.button]==1)release_block=true;owner[e->mouse.button]=0;}
}
extern "C" void bsl_shell_settings_save(){
 if(!ready||settings_path.empty())return;
 auto* cfg=al_create_config();char text[32];snprintf(text,sizeof text,"%.3f",scale);al_set_config_value(cfg,"editor","scale",text);al_set_config_value(cfg,"editor","drawer",drawer?"visible":"hidden");
 al_set_config_value(cfg,"editor","reduced_motion",reduced?"true":"false");
 auto temp=std::filesystem::u8path(settings_path+".tmp");auto* previous=al_get_new_file_interface();al_set_standard_file_interface();bool saved=al_save_config_file(temp.u8string().c_str(),cfg);al_set_new_file_interface(previous);
 if(saved){std::error_code error;std::filesystem::rename(temp,std::filesystem::u8path(settings_path),error);if(error){
#ifdef _WIN32
 MoveFileExW(temp.c_str(),std::filesystem::u8path(settings_path).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
#endif
 }}
 al_destroy_config(cfg);
}
extern "C" void bsl_shell_tick(bool is_level){
 if(frame_open){ImGui::EndFrame();frame_open=false;ps_ui_frame(false);}
 scene_level=is_level;if(!scene_level){visible=false;commands.clear();return;}
 if(!ready)init();if(!ready)return;
 bsl_shell_snapshot(&state);editing=state.editing;if(state.presentation&&!editing){visible=false;return;}
 static std::string terrain_notice;
 if(state.terrain&&*state.message&&terrain_notice!=state.message){terrain_notice=state.message;if(terrain_notice.rfind("F12 edits",0)!=0)note("terrain",state.message,terrain_notice.find("refus")!=std::string::npos||terrain_notice.find("failed")!=std::string::npos||terrain_notice.find("overlaps")!=std::string::npos?2:0);}visible=true;handled=false;
 if(editing&&!was_editing){tool=0;tab=1;submit("tool",0);}
 bool reset_focus=!editing&&was_editing;if(reset_focus)bsl_shell_settings_save();was_editing=editing;
 auto& io=ImGui::GetIO();ImGui_ImplAllegro5_SetDisplay(al_get_current_display());ImGui_ImplAllegro5_NewFrame();ps_ui_input();
 ImGui::GetStyle().FontScaleMain=scale;io.MouseDrawCursor=editing||log_open||details||std::getenv("PS_UI_PILOT")||ps_ui_menu_active();
 ImGui::NewFrame();frame_open=true;ps_ui_frame(true);if(reset_focus){ImGui::ClearActiveID();ImGui::SetWindowFocus(nullptr);keyboard=false;}
 width=io.DisplaySize.x;height=io.DisplaySize.y;regions.clear();
 keyboard=io.WantCaptureKeyboard;ALLEGRO_MOUSE_STATE mouse;al_get_mouse_state(&mouse);
 ALLEGRO_KEYBOARD_STATE held_keys;al_get_keyboard_state(&held_keys);bool held_key=false;for(int k=1;k<ALLEGRO_KEY_MAX;k++)held_key|=al_key_down(&held_keys,k);
 if(io.WantTextInput&&held_key)wait_release=true;
 if(wait_release&&mouse.buttons==0&&!held_key){wait_release=false;release_block=true;}
 if(editing){
  if(!user_hidden&&((state.selection&&state.selection!=previous_selection)||state.selected_shape!=previous_shape)){drawer=true;tab=1;}
  previous_selection=state.selection;previous_shape=state.selected_shape;
  if(previous_tool!=state.tool){if(!user_hidden){drawer=true;tab=state.tool==0?1:0;}palette_kind=state.tool==1?2:state.tool==4?1:0;previous_tool=state.tool;}
  menu();toolbar();
 }
 status_bar();if(editing)right_drawer(std::min(.1f,io.DeltaTime));dialogs();
 view_keys=editing&&focus&&!io.WantTextInput&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)&&!help&&!details&&!ImGui::IsAnyItemActive();
 if(editing&&!io.WantTextInput&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)){
  if(!help&&!details&&(ImGui::IsKeyPressed(ImGuiKey_Escape,false)||(view_canvas(io.MousePos.x,io.MousePos.y)&&ImGui::IsMouseClicked(ImGuiMouseButton_Right)))&&bsl_cell_cancel_line()){handled=true;wait_release=true;}
  if(!handled&&ps::ui::pressed("resume")){submit("resume");handled=true;}
  if(!io.KeyCtrl&&ps::ui::pressed("tools")){submit("tool",state.tool==1?2:1);handled=true;}
  const ImGuiKey overlays[]={ImGuiKey_F3,ImGuiKey_F4,ImGuiKey_F5,ImGuiKey_None,ImGuiKey_F2,ImGuiKey_F7};for(int i=0;i<6;i++)if(i!=3&&ImGui::IsKeyPressed(overlays[i],false)){bsl_shell_debug_set(bsl_shell_debug_get()^(1u<<i));handled=true;}
  if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F6,false)){bsl_shell_debug_set(bsl_shell_debug_get()^8u);handled=true;}
  if(ps::ui::pressed("help")){help=!help;handled=true;}
  if(ps::ui::pressed("save")){submit("save");handled=true;}
  if(ps::ui::pressed("terrain.reload_materials")){submit("terrain.reload_materials");handled=true;}else if(ps::ui::pressed("reload")){want_depart("reload");handled=true;}
  if(ps::ui::pressed("undo")){submit("undo");handled=true;}
  if(ps::ui::pressed("redo")){submit("redo");handled=true;}
  if(ps::ui::pressed("delete")){submit("delete");handled=true;}
  if(ps::ui::pressed("events")){submit("tool",3);handled=true;}
  if(ps::ui::pressed("inspect")){submit("tool",0);handled=true;}
  if(ps::ui::pressed("palette")){drawer=true;user_hidden=false;tab=0;submit("tool",2);handled=true;}
  if(ps::ui::pressed("home")){submit("home");handled=true;}
  if(ps::ui::pressed("door.next")){submit("door",1);handled=true;}
  if(ps::ui::pressed("door.previous")){submit("door",-1);handled=true;}
 }
 if(!io.WantTextInput&&ImGui::IsKeyPressed(ImGuiKey_Escape,false)&&(help||details)){help=details=false;handled=true;wait_release=true;}
 if(!io.WantTextInput&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)&&ps::ui::pressed("log")){log_open=!log_open;handled=true;}
 if(editing)palette_drag();
 if(editing&&view_canvas(mouse.x,mouse.y)&&!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)&&!palette_active&&
    (ImGui::IsMouseClicked(ImGuiMouseButton_Left)||ImGui::IsMouseClicked(ImGuiMouseButton_Middle))){
  ImGui::ClearActiveID();ImGui::SetWindowFocus(nullptr);keyboard=false;
 }
 bool world=false,ui=false;for(int o:owner){world|=o==2;ui|=o==1;}
 if(world&&hit(mouse.x,mouse.y)&&(mouse.buttons&3)&&!view_middle){submit("cancel");wait_release=true;}
 blocked=!focus||wait_release||release_block||ui||(!world&&(hit(mouse.x,mouse.y)||io.WantCaptureMouse));
 game_ui_capture=(keyboard&&((!ps_ui_enabled()&&!ps_ui_menu_active())||io.WantTextInput||ImGui::IsAnyItemActive()))||handled||ui||release_block||wait_release||!focus;
 release_block=false;
 if(save_and_lab){if(!state.terrain_dirty&&!state.placements_dirty){submit(("lab."+lab_intent).c_str());save_and_lab=false;}else if(commands.empty()){save_and_lab=false;note("save","Lab was not started: some edits could not be saved.",2);}}
 if(save_and_resume||save_and_reload){if(!state.terrain_dirty&&!state.placements_dirty){submit(save_and_reload?"reload":"resume");save_and_resume=save_and_reload=false;}else if(commands.empty()){save_and_resume=save_and_reload=false;note("save","Save incomplete; remaining edits are retained.",2);}}
 test_tick();
}
extern "C" void bsl_shell_render(){
 if(!ready||!visible||!frame_open)return;
 ps_ui_tools();ImGui::Render();frame_open=false;ps_ui_frame(false);
 ImGui_ImplAllegro5_RenderDrawData(ImGui::GetDrawData());ps::ui::rendered();
 bsl_shell_terrain_thumbs_collect();
 const char* report=std::getenv("BSL_SHELL_SCREENSHOT");static int frame=0;
 if(report&&editing&&(++frame==60||final_capture)){auto* previous=al_get_new_file_interface();al_set_standard_file_interface();al_save_bitmap(report,al_get_backbuffer(al_get_current_display()));al_set_new_file_interface(previous);if(final_capture){std::ofstream marker(std::filesystem::u8path(std::string(report)+".ready"));marker<<"rendered";final_capture=false;}}
}
extern "C" void bsl_shell_shutdown(){if(!ready)return;if(frame_open){ImGui::EndFrame();frame_open=false;ps_ui_frame(false);}ps_ui_shutdown();bsl_shell_settings_save();bsl_shell_terrain_thumbs_collect();ImGui_ImplAllegro5_Shutdown();ImGui::DestroyContext();ready=false;}
namespace {
void test_pointer(int type,float x,float y,int button=0){
 test_injecting=true;ALLEGRO_EVENT event{};event.type=type;event.mouse.display=al_get_current_display();event.mouse.x=(int)x;event.mouse.y=(int)y;event.mouse.button=button;bsl_shell_event(&event);test_injecting=false;
}
void test_view_capture(const char* report,const char* suffix){
 auto* shot=video_take_snapshot();if(!shot)throw std::runtime_error("View capture failed");
 auto* previous=al_get_new_file_interface();al_set_standard_file_interface();
 bool saved=al_save_bitmap((std::string(report)+suffix).c_str(),IMAGE2BITMAP(shot));
 al_set_new_file_interface(previous);image_destroy(shot);if(!saved)throw std::runtime_error("View capture write failed");
}
void test_tick(){
 const char* report=std::getenv("BSL_SHELL_CHECK");if(!report)return;
 static int step=0,frames=0,checks=0,drag_phase=0;static bool done=false;
 static bool passed=true;static std::string error;static int old_material=0;
 static unsigned long long generation=0;
 if(generation!=ps_lab_generation()){generation=ps_lab_generation();step=frames=checks=drag_phase=0;done=false;passed=true;error.clear();old_material=0;}
 if(done)return;++frames;
 {std::ofstream trace(std::filesystem::u8path(std::string(report)+".progress"));trace<<step<<" "<<state.tool<<" "<<state.material<<" "<<state.terrain_dirty<<" "<<state.placements_dirty;}

 try{
  if(ImGui::GetCurrentContext()->ErrorCountCurrentFrame)throw std::runtime_error("ImGui reported a layout/input error");
  auto check=[&](bool value,const char* why){if(!value)throw std::runtime_error(why);checks++;};
  if(step==0&&frames>15){bsl_cell_editor_open();step++;}
  else if(step==1&&editing&&frames>30){
   check(state.tool==0,"F12 entry is not Select / Inspect");
   check(ImGui::GetIO().Fonts!=nullptr,"font atlas missing");
   check(bsl_shell_palette_count(0)>0,"entity palette empty");
   check(bsl_shell_palette_count(2)>0,"material palette empty");
   check(actual_drawer_width<=width/4+.1f&&actual_drawer_width>0,"inspector width exceeds quarter");
   check(ImGui::GetIO().MouseDrawCursor,"editor pointer is not enabled");
   bool any=false;for(const auto& r:regions)if(r.hit(width-20,top+20))any=true;check(any,"drawer missing input region");
   check(bsl_shell_native_fixture(0),"actual placement/move/parameter transaction failed");
   old_material=state.material;submit("tool",1);step++;frames=0;
  }else if(step==2&&frames>8){
   check(state.tool==1,"terrain menu did not activate tool");
   check(bsl_shell_view_fixture(),"editor zoom limits/inverse mapping/anchor/pan changed gameplay camera");
   check(bsl_shell_terrain_preview_fixture(),"toolbar preview did not refresh without painting/uploading");
   submit("view.set",.2);
   submit("terrain.mode_set",2);submit("terrain.curve",.7);submit("grid",16);
   submit("terrain.material",(old_material+1)%state.materials);step++;frames=0;
  }else if(step==3&&frames>8){
   check(std::abs(state.zoom-.2f)<.001,"wide editor zoom not applied");test_view_capture(report,".wide.png");submit("view.set",10);
   check(state.mode==2&&std::abs(state.curve-.7)<.01,"line controls did not reach terrain");
   check(state.grid==16,"brush/grid command failed");check(state.material!=old_material,"material selection failed");
   check(state.terrain_dirty,"authoring edit did not mark terrain dirty");submit("undo");step++;frames=0;
  }else if(step==4&&frames>8){
   check(std::abs(state.zoom-10)<.001,"close editor zoom not applied");test_view_capture(report,".close.png");submit("view.reset");
   check(state.material==old_material,"contextual terrain undo failed");submit("redo");step++;frames=0;
  }else if(step==5&&frames>8){
   check(state.material!=old_material,"contextual terrain redo failed");
   submit("terrain.material",old_material);submit("tool",2);step++;frames=0;
  }else if(step==6&&frames>8){
   check(state.tool==2,"Objects command did not leave terrain");
   int index=-1;for(int i=0;i<bsl_shell_palette_count(0);++i)if(!strcmp(bsl_shell_palette_name(0,i),"BOR Pole")){index=i;break;}
   check(index>=0,"drop fixture palette entry missing");
   if(drag_phase==0){
    check(bsl_shell_native_fixture(6),"cannot record pre-drop count");
    strcpy(search,"BOR Pole");drawer=true;user_hidden=false;tab=0;drag_phase++;frames=0;
   }else if(drag_phase==1&&test_palette_visible){
    test_pointer(ALLEGRO_EVENT_MOUSE_AXES,test_palette_row.x,test_palette_row.y);
    test_pointer(ALLEGRO_EVENT_MOUSE_BUTTON_DOWN,test_palette_row.x,test_palette_row.y,1);drag_phase++;frames=0;
   }else if(drag_phase==2){
    test_pointer(ALLEGRO_EVENT_MOUSE_AXES,width*.35f,height*.4f);drag_phase++;frames=0;
   }else if(drag_phase==3){
    check(palette_active,"real palette drag did not arm");
    test_pointer(ALLEGRO_EVENT_MOUSE_BUTTON_UP,width*.35f,height*.4f,1);drag_phase++;frames=0;
   }else if(drag_phase==4){
    check(!palette_active,"palette release was not consumed");search[0]=0;step++;frames=0;
   }
  }else if(step==7&&frames>8){
   check(bsl_shell_native_fixture(7),"palette release did not create exactly one instance");
   check(state.tool==2&&state.ready_kind==0&&state.ready_index>=0,"palette selection was not armed for placement");
   check(!strcmp(state.name,"BOR Pole"),"palette drop did not place/select an instance");
   check(bsl_shell_native_fixture(5),"drop placement undo/redo failed");submit("tool",0);
   auto before=state.selection;drawer=false;user_hidden=true;
   note("test","Repeated diagnostic",1);note("test","Repeated diagnostic",1);
   check(log.back().count==2,"diagnostic repeats are not coalesced");
   check(log.size()<=512,"diagnostic ring unbounded");
   check(state.selection==before,"hiding drawer changed selection");
   drawer=true;user_hidden=false;tab=0;
   // Save through real protected owners, in the disposable runtime only.
   submit("save");step++;frames=0;
  }else if(step==8&&frames>8){
   check(!state.terrain_dirty&&!state.placements_dirty,"Save all did not clear both documents");
   check(bsl_shell_file_unchanged(state.level),"saved level baseline did not refresh");
   check(bsl_shell_native_fixture(1),"saved placement/parameters failed round trip");
   check(bsl_shell_native_fixture(2),"failed level save changed its original or cleared dirty state");
   check(bsl_shell_native_fixture(3),"external level edit was not refused safely");
   submit("save");
   test_injecting=true;ALLEGRO_EVENT pointer{};pointer.type=ALLEGRO_EVENT_MOUSE_BUTTON_DOWN;pointer.mouse.display=al_get_current_display();pointer.mouse.button=1;pointer.mouse.x=15;pointer.mouse.y=10;bsl_shell_event(&pointer);
   check(owner[1]==1,"UI press was not captured");
   pointer.type=ALLEGRO_EVENT_MOUSE_AXES;pointer.mouse.x=(int)(width*.5f);pointer.mouse.y=(int)(height*.5f);bsl_shell_event(&pointer);
   check(owner[1]==1,"UI drag lost ownership outside its panel");
   pointer.type=ALLEGRO_EVENT_MOUSE_BUTTON_UP;bsl_shell_event(&pointer);check(release_block&&owner[1]==0,"UI release leaked to the canvas");
   ALLEGRO_EVENT focus_event{};focus_event.display.source=al_get_current_display();focus_event.type=ALLEGRO_EVENT_DISPLAY_SWITCH_OUT;bsl_shell_event(&focus_event);check(wait_release&&!focus,"focus loss did not cancel captures");focus_event.type=ALLEGRO_EVENT_DISPLAY_SWITCH_IN;bsl_shell_event(&focus_event);test_injecting=false;

   const unsigned original=bsl_shell_debug_get();bsl_shell_debug_set(original^2u);check(bsl_shell_debug_get()==(original^2u),"independent overlay command failed");bsl_shell_debug_set(original);
   log_open=true;step++;frames=0;
  }else if(step==9&&frames>75){
   check(state.tool==0,"capture changed selected tool");
   submit("resume");step++;frames=0;
  }else if(step==10&&frames>15){
   check(!editing,"resume from editor failed");
   check(std::filesystem::is_regular_file(std::filesystem::u8path(settings_path)),"machine-local UI preferences were not saved");
   if(std::getenv("BSL_PHYSICS_LAB"))check(bsl_shell_native_fixture(4),"physics props did not reconstruct after editor resume");
   bsl_cell_editor_open();step++;frames=0;
  }else if(step==11&&frames>10){
   check(editing&&state.tool==0,"re-entry did not restore neutral selection");
   done=true;
  }
 }catch(const std::exception& e){passed=false;error=e.what();done=true;}
 if(done){
  final_capture=true;
  std::ofstream out(std::filesystem::u8path(report));
  out<<"{\"passed\":"<<(passed?"true":"false")<<",\"checks\":"<<checks<<",\"error\":\""<<error<<"\",\"width\":"<<width<<",\"height\":"<<height<<",\"signed\":\"President\"}\n";
 }
}
}
