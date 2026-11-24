// Shared presentation controls; runtime state stays in the foundation/terrain providers. Signed: President.
#include "widgets.hpp"
#include "imgui_internal.h"
#include "../editor_shell/shell.h"
#include "../labs/lab.h"
extern "C" {
#include "../core/asset.h"
#include "../core/input.h"
#include "../core/video.h"
}
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Command {const char* id;const char* label;ImGuiKeyChord keys;const char* binding;const char* tip;ImGuiKey second=ImGuiKey_None,third=ImGuiKey_None;};
const Command commands[]={
 {"save","Save all level edits",ImGuiMod_Ctrl|ImGuiKey_S,"Ctrl+S","Save terrain and placements; failures keep the draft."},
 {"reload","Reload level",ImGuiMod_Ctrl|ImGuiKey_R,"Ctrl+R","Unsaved edits require a decision before reload."},
 {"terrain.reload_materials","Reload materials",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_R,"Ctrl+Shift+R","Rescan the material registry without restarting."},
 {"undo","Undo",ImGuiMod_Ctrl|ImGuiKey_Z,"Ctrl+Z","Undo in the active terrain or placement document."},
 {"redo","Redo",ImGuiMod_Ctrl|ImGuiKey_Y,"Ctrl+Y","Redo in the active document."},
 {"resume","Resume play",ImGuiKey_F12,"F12","Resume the current disposable playtest."},
 {"tools","Terrain / Objects",ImGuiKey_F6,"F6","The visible tool row offers the same selection."},
 {"help","Context help",ImGuiKey_F1,"F1","Bindings and quick tips come from these command definitions."},
 {"log","Diagnostic log",ImGuiKey_F8,"F8","Toggle the bottom diagnostic log."},
 {"delete","Delete selection",ImGuiKey_Delete,"Delete","Delete only the current tool's target category."},
 {"events","Event palette",ImGuiMod_Ctrl|ImGuiKey_1,"Ctrl+1","Common helper objects and event anchors."},
 {"inspect","Select / Inspect",ImGuiMod_Ctrl|ImGuiKey_2,"Ctrl+2","Select and drag placed entities; inspect supported overrides."},
 {"palette","Object palette",ImGuiKey_Insert,"Insert","Choose an entry, or drag it to the canvas."},
 {"home","Go to spawn",ImGuiKey_Home,"Home","Move the editor camera to the spawn."},
 {"door.next","Next teleporter",ImGuiKey_PageUp,"Page Up","Focus the next registered door."},
 {"door.previous","Previous teleporter",ImGuiKey_PageDown,"Page Down","Focus the previous registered door."},
 {"dialogue.confirm","Confirm choice",ImGuiKey_Enter,"Enter / H / gamepad A","Confirm the selected outcome once.",ImGuiKey_H,ImGuiKey_GamepadFaceDown},
 {"dialogue.cancel","Cancel conversation",ImGuiKey_Escape,"Esc / J / gamepad B","Cancel remaining steps; committed effects are retained.",ImGuiKey_J,ImGuiKey_GamepadFaceRight},
 {"dialogue.up","Previous choice",ImGuiKey_UpArrow,"Up / W / gamepad Up","Navigate choices, scrolling when required.",ImGuiKey_W,ImGuiKey_GamepadDpadUp},
 {"dialogue.down","Next choice",ImGuiKey_DownArrow,"Down / S / gamepad Down","Navigate choices, scrolling when required.",ImGuiKey_S,ImGuiKey_GamepadDpadDown}
};
const Command* find(const char* id){for(auto& c:commands)if(!strcmp(c.id,id))return &c;return nullptr;}
float number(ALLEGRO_CONFIG* c,const char* section,const char* key,float fallback){
 const char* v=c?al_get_config_value(c,section,key):nullptr;if(!v)return fallback;
 char* end=nullptr;float n=strtof(v,&end);if(end==v||!std::isfinite(n))return fallback;
 while(*end==' '||*end=='\t'||*end=='\r'||*end=='\n')++end;
 return !*end?n:fallback;
}
struct Skin {
 ImVec4 text{.12f,.07f,.025f,1},header{.55f,.33f,.10f,.22f},hovered{.55f,.33f,.1f,.4f},active{.55f,.33f,.1f,.6f},child{0,0,0,0},scrollbar{.2f,.12f,.04f,.12f},grab{.4f,.23f,.07f,.6f},meter_text{1,.95686f,.79216f,1};
 float frame_x=12,frame_y=5,item_x=10,item_y=6;
 float font=32,margin=32,padding=28,message=172,information=132,choices_w=600,choices_h=430,row=44,opening=.18f,closing=.28f,patch=0.5f;int l=32,t=64,r=32,b=64;float meter_scale=1,meter_x=32,meter_y=32,offset=17;} skin;
bool choice_visible=false,frame_available=false,game_skin=true,dialogue_open=false;float mp_fixture=.625f;
bool ready=false;ImFont* game_font=nullptr;ALLEGRO_BITMAP* parchment=nullptr;input_t* pad=nullptr;
std::deque<PsUiCommand> pending;
std::vector<std::pair<ImGuiKey,bool>> native_keys;
std::string capture_path;
struct MouseIntent {ImVec2 point;bool button=false,down=false;float wheel=0;};
std::vector<MouseIntent> native_mouse;std::vector<std::pair<int,ImVec2>> choice_points;
ImVec2 list_point{};float choice_scroll=0;char choice_bounds[256]="";
uint64_t active_owner=0,active_epoch=0;int active_count=0;
bool input_armed=false,needs_scroll=true;double opened_at=0,last_clock=0;float reveal=0;
int last_selection=-1;uint64_t last_meter_owner=0;
struct MenuItem {std::string definition,instance;int quantity;};
struct MenuRegion {PsUiMenuRegion kind;int index;ImVec2 lo,hi;};
struct MenuPoint {PsUiMenuAction action;int index;ImVec2 point;};
uint64_t menu_owner=0,menu_epoch=0;bool menu_open=false,menu_pointer_armed=false;int menu_page=0,menu_selected=0;
std::vector<MenuItem> menu_items;std::vector<MenuRegion> menu_regions;std::vector<MenuPoint> menu_points;
std::deque<PsUiMenuCommand> menu_pending;
struct Pixel {float r,g,b,a;};
Pixel pixel(ALLEGRO_BITMAP* b,int x,int y){Pixel p;al_unmap_rgba_f(al_get_pixel(b,x,y),&p.r,&p.g,&p.b,&p.a);return p;}
float diff(Pixel a,Pixel b){return std::abs(a.r-b.r)+std::abs(a.g-b.g)+std::abs(a.b-b.b)+std::abs(a.a-b.a);}
struct ReadPixels {
 ALLEGRO_BITMAP* image;explicit ReadPixels(ALLEGRO_BITMAP* b):image(b){if(!al_lock_bitmap(b,ALLEGRO_PIXEL_FORMAT_ANY,ALLEGRO_LOCK_READONLY))throw std::runtime_error("Cannot read UI raster");}
 ~ReadPixels(){al_unlock_bitmap(image);}
};
struct Meter {
 int w=0,h=0;ALLEGRO_BITMAP* output=nullptr;std::vector<Pixel> full,empty;std::vector<float> order;double previous=-1;
 void clear(){if(output)al_destroy_bitmap(output);output=nullptr;full.clear();empty.clear();order.clear();w=h=0;previous=-1;}
 bool load(const char* directory){
  std::array<ALLEGRO_BITMAP*,26> frames{};
  struct BitmapFlags {int previous=al_get_new_bitmap_flags();BitmapFlags(){al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);}~BitmapFlags(){al_set_new_bitmap_flags(previous);}} bitmap_flags;
  for(int i=0;i<26;i++){char name[512];snprintf(name,sizeof name,"%s%03d.png",directory,i);frames[i]=al_load_bitmap(asset_path(name));if(!frames[i]){for(auto* p:frames)if(p)al_destroy_bitmap(p);return false;}}
  w=al_get_bitmap_width(frames[0]);h=al_get_bitmap_height(frames[0]);bool valid=w>0&&h>0&&w<=512&&h<=512;
  for(auto* f:frames)valid&=al_get_bitmap_width(f)==w&&al_get_bitmap_height(f)==h;
  if(!valid){for(auto* p:frames)al_destroy_bitmap(p);return false;}
  full.resize(w*h);empty.resize(w*h);order.resize(w*h);
  // Recover the authored fill direction from all reference frames. Interpolate pixel
  // thresholds spatially instead of selecting an animation frame at runtime.
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){
   int n=y*w+x;full[n]=pixel(frames[0],x,y);empty[n]=pixel(frames[25],x,y);
   float d=diff(full[n],empty[n]),remaining=0;
   if(d>.02f){for(int i=1;i<25;i++)remaining+=std::clamp(diff(pixel(frames[i],x,y),empty[n])/d,0.f,1.f);order[n]=1-(remaining+.5f)/25;}
   else order[n]=-1;
  }
  auto original=order;
  for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(original[y*w+x]>=0){
   float sum=0,weights=0;
   for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){int xx=x+dx,yy=y+dy;if(xx<0||yy<0||xx>=w||yy>=h||original[yy*w+xx]<0)continue;float weight=1.f/(1+dx*dx+dy*dy);sum+=original[yy*w+xx]*weight;weights+=weight;}
   order[y*w+x]=sum/weights;
  }
  for(auto* p:frames)al_destroy_bitmap(p);
  int flags=al_get_new_bitmap_flags();al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP);output=al_create_bitmap(w,h);al_set_new_bitmap_flags(flags);
  return output!=nullptr;
 }
 void update(double fraction){
  fraction=std::clamp(fraction,0.,1.);if(previous==fraction||!output)return;previous=fraction;
  auto* lock=al_lock_bitmap(output,ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,ALLEGRO_LOCK_WRITEONLY);if(!lock)throw std::runtime_error("Cannot update owned meter texture");
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){
   int n=y*w+x;float amount=order[n]<0?1:std::clamp(float((fraction-order[n])/.04+.5),0.f,1.f);if(fraction==0)amount=0;if(fraction==1)amount=1;
   auto a=empty[n],b=full[n];auto* p=(unsigned char*)lock->data+y*lock->pitch+4*x;
   p[0]=(unsigned char)std::lround(255*(a.r+(b.r-a.r)*amount));p[1]=(unsigned char)std::lround(255*(a.g+(b.g-a.g)*amount));p[2]=(unsigned char)std::lround(255*(a.b+(b.b-a.b)*amount));p[3]=(unsigned char)std::lround(255*(a.a+(b.a-a.a)*amount));
  }al_unlock_bitmap(output);
 }
} hp,mp;
struct Theme {
 ImGuiStyle previous;Theme(float s):previous(ImGui::GetStyle()){
  auto& st=ImGui::GetStyle();st.WindowPadding=ImVec2(skin.padding*s,std::max(skin.padding,skin.t*skin.patch)*s);st.FramePadding=ImVec2(skin.frame_x*s,skin.frame_y*s);st.ItemSpacing=ImVec2(skin.item_x*s,skin.item_y*s);st.WindowBorderSize=0;
  st.Colors[ImGuiCol_ChildBg]=skin.child;st.Colors[ImGuiCol_ScrollbarBg]=skin.scrollbar;st.Colors[ImGuiCol_ScrollbarGrab]=skin.grab;
  st.Colors[ImGuiCol_Text]=skin.text;st.Colors[ImGuiCol_Header]=skin.header;st.Colors[ImGuiCol_HeaderHovered]=skin.hovered;st.Colors[ImGuiCol_HeaderActive]=skin.active;st.Colors[ImGuiCol_Button]=skin.header;st.Colors[ImGuiCol_ButtonHovered]=skin.hovered;st.Colors[ImGuiCol_ButtonActive]=skin.active;
  ImGui::PushFont(game_font,skin.font*s/std::max(.01f,st.FontScaleMain));
 }~Theme(){ImGui::PopFont();ImGui::GetStyle()=previous;}
};
void patch(ImDrawList* draw,ImVec2 a,ImVec2 b,float s){
 float w=(float)al_get_bitmap_width(parchment),h=(float)al_get_bitmap_height(parchment);float l=skin.l*skin.patch*s,r=skin.r*skin.patch*s,t=skin.t*skin.patch*s,bt=skin.b*skin.patch*s;
 float xs[]={a.x,a.x+l,b.x-r,b.x},ys[]={a.y,a.y+t,b.y-bt,b.y};float us[]={0,skin.l/w,1-skin.r/w,1},vs[]={0,skin.t/h,1-skin.b/h,1};
 for(int y=0;y<3;y++)for(int x=0;x<3;x++)if(xs[x+1]>xs[x]&&ys[y+1]>ys[y])draw->AddImage((ImTextureID)(intptr_t)parchment,ImVec2(xs[x],ys[y]),ImVec2(xs[x+1],ys[y+1]),ImVec2(us[x],vs[y]),ImVec2(us[x+1],vs[y+1]));
}
bool panel(const char* id,ImVec2 pos,ImVec2 size,float s,ImGuiWindowFlags extra=0){
 ImGui::SetNextWindowPos(pos);ImGui::SetNextWindowSize(size);
 bool show=ImGui::Begin(id,nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoNavInputs|extra);
 auto* draw=ImGui::GetWindowDrawList();patch(draw,pos,ImVec2(pos.x+size.x,pos.y+size.y),s);
 ImGui::PushClipRect(ImVec2(pos.x+skin.l*skin.patch*s,pos.y+skin.t*skin.patch*s),ImVec2(pos.x+size.x-skin.r*skin.patch*s,std::max(pos.y+skin.t*skin.patch*s,pos.y+size.y-skin.b*skin.patch*s)),true);return show;
}
void end_panel(){ImGui::PopClipRect();ImGui::End();}
double normalized(double v,double lo,double hi){return std::isfinite(v)&&std::isfinite(lo)&&std::isfinite(hi)&&hi>lo?std::clamp((v-lo)/(hi-lo),0.,1.):-1;}
}
namespace ps::ui {
void native_key(ImGuiKey key,bool down){if(ps_lab_automatic())native_keys.push_back({key,down});}
bool selected_visible(){return choice_visible;}
const char* native_choice_bounds(){return choice_bounds;}
float native_scroll(){return choice_scroll;}
bool native_click_choice(int index,bool down){if(!ps_lab_automatic())return false;for(auto p:choice_points)if(p.first==index){native_mouse.push_back({p.second,true,down,0});return true;}return false;}
void native_wheel(float delta){if(ps_lab_automatic())native_mouse.push_back({list_point,false,false,delta});}
void native_capture(const char* path){if(ps_lab_automatic()&&path)capture_path=path;}
void rendered(){
 if(capture_path.empty())return;
 auto* previous=al_get_new_file_interface();al_set_standard_file_interface();
 bool saved=al_save_bitmap(capture_path.c_str(),al_get_backbuffer(al_get_current_display()));al_set_new_file_interface(previous);capture_path.clear();if(!saved)bsl_shell_note("ui","Could not save native UI review capture.",2);
}
float appearance_number(ALLEGRO_CONFIG* c,const char* key,float fallback){return number(c,"editor",key,fallback);}
const char* shortcut(const char* command){auto* c=find(command);return c?c->binding:nullptr;}
bool pressed(const char* command){auto* c=find(command);if(!c)return false;auto& io=ImGui::GetIO();int mods=c->keys&ImGuiMod_Mask_;return (io.KeyMods&ImGuiMod_Mask_)==mods&&(ImGui::IsKeyPressed(ImGuiKey(c->keys&~ImGuiMod_Mask_),false)||(c->second!=ImGuiKey_None&&ImGui::IsKeyPressed(c->second,false))||(c->third!=ImGuiKey_None&&ImGui::IsKeyPressed(c->third,false)));}
void command_help(){
 static char search[96]="";ImGui::InputTextWithHint("##command-search","Find a command or binding",search,sizeof search);
 for(auto& c:commands){std::string text=std::string(c.label)+" "+c.binding+" "+c.tip;auto query=std::string(search);std::transform(text.begin(),text.end(),text.begin(),[](unsigned char x){return char(std::tolower(x));});std::transform(query.begin(),query.end(),query.begin(),[](unsigned char x){return char(std::tolower(x));});if(!query.empty()&&text.find(query)==std::string::npos)continue;ImGui::Separator();ImGui::Text("%s  [%s]",c.label,c.binding);ImGui::TextWrapped("%s",c.tip);}
}
int instance_field(const BslShellField& f,double& value){
 ImGui::PushID(f.index);ImGui::Separator();ImGui::TextWrapped("%s",f.name);value=f.value;int result=0;
 if(f.boolean){bool v=value!=0;if(ImGui::Checkbox("Value",&v)){value=v;result=1;}}
 else {ImGui::SetNextItemWidth(-1);if(ImGui::InputDouble("##value",&value,f.step,10*f.step,"%.5g")&&std::isfinite(value)){value=std::clamp(value,f.minimum,f.maximum);result=1;}}
 ImGui::TextDisabled("%s",f.overridden?"Instance override":"Template value");if(f.live_available)ImGui::TextWrapped("Effective: %.5g%s",f.effective,std::abs(f.effective-f.value)>1e-9?" (runtime override)":"");
 if(f.overridden&&ImGui::SmallButton("Reset to template"))result=2;
 ImGui::PopID();return result;
}
}
extern "C" void ps_ui_frame(bool active){frame_available=active;}
extern "C" bool ps_ui_conversation_visible(){return ps_ui_enabled()&&reveal>0;}
extern "C" bool ps_ui_enabled(){return ready&&game_skin&&std::getenv("PS_UI_PILOT")!=nullptr;}
extern "C" void ps_ui_init(){
 if(ready)return;
 skin=Skin{};game_skin=true;mp_fixture=.625f;auto* cfg=al_load_config_file(asset_path("ui/widgets.ini"));if(!cfg){bsl_shell_note("ui","Shared game skin missing; legacy presentation retained.",1);return;}
 auto value=[&](const char* key,float fallback,float lo,float hi){return std::clamp(number(cfg,"game",key,fallback),lo,hi);};
 skin.frame_x=value("frame_padding_x",12,0,64);skin.frame_y=value("frame_padding_y",5,0,64);skin.item_x=value("item_spacing_x",10,0,64);skin.item_y=value("item_spacing_y",6,0,64);
 for(auto entry:{std::pair<const char*,ImVec4*>("text",&skin.text),{"header",&skin.header},{"header_hovered",&skin.hovered},{"header_active",&skin.active},{"child",&skin.child},{"scrollbar",&skin.scrollbar},{"scrollbar_grab",&skin.grab},{"meter_text",&skin.meter_text}}){
  const char* text=al_get_config_value(cfg,"colors",entry.first);float a,b,c,d;
  if(text&&sscanf(text,"%f,%f,%f,%f",&a,&b,&c,&d)==4&&std::isfinite(a)&&std::isfinite(b)&&std::isfinite(c)&&std::isfinite(d))*entry.second=ImVec4(std::clamp(a,0.f,1.f),std::clamp(b,0.f,1.f),std::clamp(c,0.f,1.f),std::clamp(d,0.f,1.f));
 }
 skin.font=value("font_size",32,18,64);skin.margin=value("margin",32,4,128);skin.padding=value("padding",28,4,64);skin.message=value("message_height",172,100,320);skin.information=value("information_height",132,80,300);skin.choices_w=value("choices_width",600,300,900);skin.choices_h=value("choices_height",430,120,650);skin.row=value("row_height",44,28,90);skin.opening=value("open_seconds",.18f,0,2);skin.closing=value("close_seconds",.28f,0,2);skin.patch=value("patch_scale",.5f,.1f,1);
 skin.l=int(value("patch_left",32,0,256));skin.t=int(value("patch_top",64,0,256));skin.r=int(value("patch_right",32,0,256));skin.b=int(value("patch_bottom",64,0,256));
 skin.meter_scale=std::round(std::clamp(number(cfg,"meter","scale",1),1.f,4.f));skin.meter_x=number(cfg,"meter","x",32);skin.meter_y=number(cfg,"meter","y",32);skin.offset=std::clamp(number(cfg,"meter","mp_offset",17),0.f,128.f);
 const char* font=al_get_config_value(cfg,"game","font");const char* scroll=al_get_config_value(cfg,"game","scroll");const char* hp_dir=al_get_config_value(cfg,"meter","hp");const char* mp_dir=al_get_config_value(cfg,"meter","mp");
 // The same asset file interface is used by both portable and development builds.
 if(font){auto* file=al_fopen(asset_path(font),"rb");if(file){auto n=al_fsize(file);if(n>0&&n<4*1024*1024){void* bytes=IM_ALLOC(size_t(n));if(al_fread(file,bytes,size_t(n))==size_t(n))game_font=ImGui::GetIO().Fonts->AddFontFromMemoryTTF(bytes,int(n),skin.font);else IM_FREE(bytes);}al_fclose(file);}}
 if(scroll)parchment=al_load_bitmap(asset_path(scroll));
 ready=game_font&&parchment&&hp_dir&&mp_dir&&hp.load(hp_dir)&&mp.load(mp_dir);
 al_destroy_config(cfg);if(!ready){hp.clear();mp.clear();if(parchment)al_destroy_bitmap(parchment);parchment=nullptr;bsl_shell_note("ui","Shared game skin has missing/invalid assets; legacy presentation retained.",2);return;}
 pad=input_create_user("powersurge_ui_pad");ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;
}
extern "C" void ps_ui_detach(){ps_ui_menu_clear();capture_path.clear();native_keys.clear();native_mouse.clear();choice_points.clear();frame_available=false;dialogue_open=false;pending.clear();active_owner=active_epoch=0;active_count=0;input_armed=false;needs_scroll=true;last_selection=-1;reveal=0;last_meter_owner=0;}
extern "C" void ps_ui_shutdown(){ps_ui_detach();if(pad)input_destroy(pad);pad=nullptr;hp.clear();mp.clear();if(parchment)al_destroy_bitmap(parchment);parchment=nullptr;ready=false;game_font=nullptr;}
extern "C" void ps_ui_input(){
 if(!pad)return;
 auto& io=ImGui::GetIO();bool available=input_is_joystick_enabled();if(available)io.BackendFlags|=ImGuiBackendFlags_HasGamepad;else io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;
 const std::pair<ImGuiKey,inputbutton_t> keys[]={{ImGuiKey_GamepadDpadUp,IB_UP},{ImGuiKey_GamepadDpadDown,IB_DOWN},{ImGuiKey_GamepadDpadLeft,IB_LEFT},{ImGuiKey_GamepadDpadRight,IB_RIGHT},{ImGuiKey_GamepadFaceDown,IB_FIRE1},{ImGuiKey_GamepadFaceRight,IB_FIRE2}};
 for(auto k:keys)io.AddKeyEvent(k.first,available&&input_button_down(pad,k.second));
 for(auto key:native_keys)io.AddKeyEvent(key.first,key.second);
 native_keys.clear();
 for(auto m:native_mouse){io.AddMousePosEvent(m.point.x,m.point.y);if(m.button)io.AddMouseButtonEvent(0,m.down);if(m.wheel)io.AddMouseWheelEvent(0,m.wheel);}native_mouse.clear();
}
extern "C" bool ps_ui_request(uint64_t owner,uint64_t epoch,PsUiAction action,int index){
 if(!dialogue_open||!owner||owner!=active_owner||epoch!=active_epoch||action<PS_UI_SELECT||action>PS_UI_CANCEL||pending.size()>=8)return false;
 if(action!=PS_UI_CANCEL&&(index<0||index>=active_count))return false;
 for(auto& c:pending)if(c.owner==owner&&c.epoch==epoch&&c.action==action)return false;
 pending.push_back({owner,epoch,action,index});return true;
}
extern "C" float ps_ui_preview_mp(){return mp_fixture;}
extern "C" void ps_ui_navigate(uint64_t owner,uint64_t epoch,int selected,int count,int delta,bool confirm,bool cancel){
 if(count<1||selected<0||selected>=count)return;
 int next=(selected+delta%count+count)%count;
 if(cancel){ps_ui_request(owner,epoch,PS_UI_CANCEL,next);return;}
 if(next!=selected)ps_ui_request(owner,epoch,PS_UI_SELECT,next);
 if(confirm)ps_ui_request(owner,epoch,PS_UI_CONFIRM,next);
}
extern "C" bool ps_ui_command(uint64_t owner,uint64_t epoch,PsUiCommand* out){
 while(!pending.empty()){auto c=pending.front();pending.pop_front();if(c.owner==owner&&c.epoch==epoch){if(out)*out=c;return true;}}return false;
}
extern "C" void ps_ui_conversation(const PsUiConversation* m){
 if(!frame_available||!ps_ui_enabled()||!m||!m->owner||m->count<1||!m->choices)return;
 if(dialogue_open&&!m->open){pending.clear();input_armed=false;ImGui::SetWindowFocus(nullptr);}
 dialogue_open=m->open;
 if(active_owner!=m->owner||active_epoch!=m->epoch){pending.clear();active_owner=m->owner;active_epoch=m->epoch;active_count=m->count;opened_at=m->clock;last_clock=m->clock;input_armed=false;needs_scroll=true;last_selection=-1;reveal=0;}
 float dt=float(std::clamp(m->clock-last_clock,0.,.1));last_clock=m->clock;float duration=m->open?skin.opening:skin.closing;reveal=m->reduced_motion||duration<=0?(m->open?1.f:0.f):std::clamp(reveal+(m->open?dt:-dt)/duration,0.f,1.f);if(!m->open&&reveal<=0)return;
 auto& io=ImGui::GetIO();float s=std::min(io.DisplaySize.x/1920,io.DisplaySize.y/1080);Theme theme(s);
 bool held=false;for(auto key:{ImGuiKey_Enter,ImGuiKey_Escape,ImGuiKey_H,ImGuiKey_J,ImGuiKey_W,ImGuiKey_S,ImGuiKey_UpArrow,ImGuiKey_DownArrow,ImGuiKey_GamepadDpadUp,ImGuiKey_GamepadDpadDown,ImGuiKey_GamepadFaceDown,ImGuiKey_GamepadFaceRight})held|=ImGui::IsKeyDown(key);
 if(!held&&!ImGui::IsMouseDown(0))input_armed=true;
 int delta=0,clicked=-1;bool accept=false,cancel=false;
 if(m->open&&input_armed){
  delta=int(ps::ui::pressed("dialogue.down"))-int(ps::ui::pressed("dialogue.up"));
  if(delta)needs_scroll=true;
  accept=ps::ui::pressed("dialogue.confirm");
  cancel=ps::ui::pressed("dialogue.cancel");
 }
 float margin=skin.margin*s,w=io.DisplaySize.x-2*margin,choices=std::min(skin.choices_w*s,w*.42f),gap=skin.padding*s,left=w-choices-gap;
 float mh=skin.message*s,ih=skin.information*s,ch=skin.choices_h*s;float y=io.DisplaySize.y-margin-std::max(ch,mh+ih+gap);
 auto animated=[&](float h){return std::max(float((skin.t+skin.b)*skin.patch*s),h*(1-(1-reveal)*(1-reveal)));};
 ImGui::PushID((void*)(uintptr_t)m->owner);ImGui::PushID((int)m->epoch);ImGui::BeginDisabled(!m->open);
 panel("Message",ImVec2(margin,y),ImVec2(left,animated(mh)),s);ImGui::TextWrapped("%s",m->message?m->message:"");end_panel();
 panel("Choice information",ImVec2(margin,y+mh+gap),ImVec2(left,animated(ih)),s);ImGui::TextWrapped("%s",m->information&&*m->information?m->information:"Choose an option.");end_panel();
 panel("Choices",ImVec2(margin+left+gap,y),ImVec2(choices,animated(ch)),s);
 ImGui::Text("Choices %d / %d",m->selected+1,m->count);ImGui::Separator();
 float footer=ImGui::GetTextLineHeightWithSpacing()*2+ImGui::GetFrameHeightWithSpacing();ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::BeginChild("choice-list",ImVec2(0,std::max(1.f,ImGui::GetContentRegionAvail().y-footer)),ImGuiChildFlags_None,ImGuiWindowFlags_NoNavInputs);
 choice_points.clear();auto* list=ImGui::GetCurrentWindow();list_point=ImVec2((list->ClipRect.Min.x+list->ClipRect.Max.x)*.5f,(list->ClipRect.Min.y+list->ClipRect.Max.y)*.5f);choice_scroll=ImGui::GetScrollY();
 if(ImGui::IsWindowHovered()&&io.MouseWheel!=0)needs_scroll=false;
 for(int i=0;i<m->count;i++){
  ImGui::PushID(i);bool selected=m->selected==i;
  float available=ImGui::GetContentRegionAvail().x;auto pos=ImGui::GetCursorScreenPos();float text_height=ImGui::CalcTextSize(m->choices[i],nullptr,false,std::max(1.f,available-16*s)).y;
  if(ImGui::Selectable("##choice",selected,ImGuiSelectableFlags_AllowDoubleClick|ImGuiSelectableFlags_NoPadWithHalfSpacing,ImVec2(0,std::max(skin.row*s,text_height+8*s)))&&input_armed){clicked=i;accept=selected||ImGui::IsMouseDoubleClicked(0);}
  ImGui::GetWindowDrawList()->AddText(nullptr,0,ImVec2(pos.x+8*s,pos.y+4*s),ImGui::GetColorU32(ImGuiCol_Text),m->choices[i],nullptr,std::max(1.f,available-16*s));
  auto item_lo=ImGui::GetItemRectMin(),item_hi=ImGui::GetItemRectMax();auto item_clip=ImGui::GetCurrentWindow()->ClipRect;
  if(item_lo.y>=item_clip.Min.y&&item_hi.y<=item_clip.Max.y)choice_points.push_back({i,ImVec2((item_lo.x+item_hi.x)*.5f,(item_lo.y+item_hi.y)*.5f)});
  if(selected){
   auto clip=ImGui::GetCurrentWindow()->ClipRect;auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
   choice_visible=lo.y>=clip.Min.y-1&&hi.y<=clip.Max.y+1;
   snprintf(choice_bounds,sizeof choice_bounds,"Choice clipping: row %.3f..%.3f, clip %.3f..%.3f, scroll %.3f/%.3f, content %.3f",lo.y,hi.y,clip.Min.y,clip.Max.y,ImGui::GetScrollY(),ImGui::GetScrollMaxY(),ImGui::GetCurrentWindow()->ContentSize.y);
   if(needs_scroll||last_selection!=m->selected||reveal<1){
    float correction=hi.y>clip.Max.y?hi.y-clip.Max.y:lo.y<clip.Min.y?lo.y-clip.Min.y:0;
    ImGui::SetScrollY(std::max(0.f,ImGui::GetScrollY()+correction));needs_scroll=!choice_visible;
   }
  }ImGui::PopID();
 }
 last_selection=m->selected;ImGui::EndChild();ImGui::PopStyleVar();
 if(ImGui::Button(find("dialogue.confirm")->label))accept=true;ImGui::SameLine();if(ImGui::Button(find("dialogue.cancel")->label))cancel=true;
 ImGui::TextWrapped("%s: choose   %s: back",ps::ui::shortcut("dialogue.confirm"),ps::ui::shortcut("dialogue.cancel"));end_panel();
 ImGui::EndDisabled();ImGui::PopID();ImGui::PopID();
 if(m->open&&input_armed){if(clicked>=0)delta=clicked-m->selected;ps_ui_navigate(m->owner,m->epoch,m->selected,m->count,delta,accept,cancel);}
}
extern "C" void ps_ui_meter(uint64_t owner,double value,double minimum,double maximum,double fixture_mp){
 if(!frame_available||!ps_ui_enabled()||!owner)return;
 auto& io=ImGui::GetIO();float s=std::min(io.DisplaySize.x/1920,io.DisplaySize.y/1080);Theme theme(s);
 auto fraction=normalized(value,minimum,maximum);hp.update(fraction<0?0:fraction);mp.update(normalized(fixture_mp,0,1));last_meter_owner=owner;
 float ratio=skin.meter_scale;ImVec2 a(std::round(skin.meter_x*s),std::round(skin.meter_y*s));auto* draw=ImGui::GetForegroundDrawList();
 auto image=[&](Meter& meter,float offset){ImVec2 p(a.x+std::round(offset*ratio),a.y);draw->AddImage((ImTextureID)(intptr_t)meter.output,p,ImVec2(p.x+meter.w*ratio,p.y+meter.h*ratio));};image(hp,0);image(mp,skin.offset);
 char text[192];if(fraction<0)snprintf(text,sizeof text,"HP unavailable\nMP preview fixture %.1f%%",fixture_mp*100);else snprintf(text,sizeof text,"HP %.3g / %.3g\nMP preview fixture %.1f%%",value,maximum,fixture_mp*100);
 draw->AddText(ImVec2(a.x+(hp.w+std::round(skin.offset))*ratio+12*s,a.y+std::max(0.f,(hp.h*ratio-ImGui::CalcTextSize(text).y)*.5f)),ImGui::ColorConvertFloat4ToU32(skin.meter_text),text);
}
extern "C" void ps_ui_probe(PsUiProbe* out){if(out)*out={active_owner,active_epoch,active_count,last_selection,ready,dialogue_open,ps_ui_enabled(),hp.previous,mp.previous,hp.w,hp.h};}
extern "C" void ps_ui_tools(){
 if(!ready||!std::getenv("PS_UI_PILOT")||ps_ui_menu_active())return;
 auto& io=ImGui::GetIO();ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x-440,48),ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize(ImVec2(420,0),ImGuiCond_FirstUseEver);
 ImGui::Begin("Shared UI pilot");ImGui::TextWrapped("T: conversation | Shift+T: 64 choices | I: inventory | F12: inspector | F1: editor help");
 ImGui::BeginDisabled(dialogue_open);if(ImGui::Checkbox("Use game skin",&game_skin))pending.clear();ImGui::EndDisabled();
 ImGui::TextWrapped("Disable the game skin for legacy comparison.");ImGui::TextUnformatted("MP preview fixture");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##mp-fixture",&mp_fixture,0,1,"%.3f");ImGui::TextWrapped("HP is live. MP is a preview value until a runtime MP resource is registered. Keyboard, mouse and mapped gamepad share choice actions.");ImGui::End();
}
extern "C" int ps_ui_check(char* error,unsigned capacity){
 int checks=0;try{
  auto check=[&](bool ok,const char* why){if(!ok)throw std::runtime_error(why);checks++;};
  check(ready,"Game skin assets were not loaded");check(ps::ui::appearance_number(nullptr,"font_size",24)==24,"Null appearance config did not default");
  auto* c=al_create_config();for(const char* text:{"","nan","inf","24garbage"}){al_set_config_value(c,"editor","font_size",text);check(ps::ui::appearance_number(c,"font_size",24)==24,"Malformed appearance value was accepted");}al_set_config_value(c,"editor","font_size","32");check(ps::ui::appearance_number(c,"font_size",24)==32,"Valid appearance value lost");al_destroy_config(c);
  check(normalized(0,0,100)==0&&normalized(100,0,100)==1,"Meter endpoints differ");check(normalized(1,1,1)<0&&normalized(NAN,0,100)<0,"Unavailable meter range not identified");
  for(auto* m:{&hp,&mp}){check(m->w>0&&m->h>0&&m->output,"Meter source missing");check(!(al_get_bitmap_flags(m->output)&(ALLEGRO_MIN_LINEAR|ALLEGRO_MAG_LINEAR)),"Meter still uses linear sampling");check(skin.meter_scale==1,"Meter default is not native pixel scale");
   for(int endpoint:{0,1}){m->update(endpoint);ReadPixels pixels(m->output);float error=0;for(int y=0;y<m->h;y++)for(int x=0;x<m->w;x++)error+=diff(pixel(m->output,x,y),endpoint?m->full[y*m->w+x]:m->empty[y*m->w+x]);check(error<.02f,"Meter endpoint differs from authored raster");}
   m->update(.501);std::vector<Pixel> before;{ReadPixels pixels(m->output);for(int y=0;y<m->h;y++)for(int x=0;x<m->w;x++)before.push_back(pixel(m->output,x,y));}m->update(.509);ReadPixels pixels(m->output);float delta=0;for(int y=0;y<m->h;y++)for(int x=0;x<m->w;x++)delta+=diff(before[y*m->w+x],pixel(m->output,x,y));check(delta>0,"Meter still quantized to 26 frames");
  }
  dialogue_open=true;active_owner=83;active_epoch=7;active_count=64;pending.clear();check(!ps_ui_request(84,7,PS_UI_CONFIRM,0),"Cross-owner UI command accepted");check(!ps_ui_request(83,8,PS_UI_CONFIRM,0),"Stale UI epoch accepted");check(!ps_ui_request(83,7,PS_UI_SELECT,64),"Invalid choice accepted");check(ps_ui_request(83,7,PS_UI_SELECT,63),"Scrolling choice unavailable");check(!ps_ui_request(83,7,PS_UI_SELECT,63),"Duplicate action accepted");PsUiCommand out{};check(ps_ui_command(83,7,&out)&&out.index==63,"Choice routing lost identity");check(ps_ui_request(83,7,PS_UI_CONFIRM,63),"Confirm unavailable");check(!ps_ui_command(83,8,&out),"Obsolete conversation command escaped");ps_ui_navigate(83,7,0,64,1,true,false);check(ps_ui_command(83,7,&out)&&out.action==PS_UI_SELECT&&out.index==1,"Simultaneous navigation lost selection");check(ps_ui_command(83,7,&out)&&out.action==PS_UI_CONFIRM&&out.index==1,"Simultaneous confirm used the previous choice");ps_ui_navigate(83,7,1,64,1,true,true);check(ps_ui_command(83,7,&out)&&out.action==PS_UI_CANCEL&&!ps_ui_command(83,7,nullptr),"Cancel failed to suppress confirm");ps_ui_detach();check(!ps_ui_request(83,7,PS_UI_CONFIRM,0),"Closed conversation accepted a command");check(!ps_ui_command(83,7,&out),"Detached conversation retained commands");
  ps_ui_frame(true);const PsUiMenuItem items[]={{"item:a","",2}};PsUiMenuModel menu{83,4,true,1,0,1,items};ps_ui_menu_begin(&menu);check(ps_ui_menu_request(PS_MENU_SELECT,0),"Menu selection request unavailable");PsUiMenuCommand menu_command{};check(!ps_ui_menu_command(84,4,&menu_command),"Menu request escaped to another owner");check(ps_ui_menu_request(PS_MENU_ACTIVATE,0),"Menu activation request unavailable");check(!ps_ui_menu_command(83,5,&menu_command),"Menu request escaped its revision");check(!ps_ui_menu_request(PS_MENU_SELECT,1)&&!ps_ui_menu_request(PS_MENU_CATEGORY,10),"Invalid menu target accepted");ps_ui_menu_clear();check(!ps_ui_menu_request(PS_MENU_ACTIVATE,0),"Closed menu accepted an action");
  ps_ui_frame(false);const char* labels[]={"Ignored"};PsUiConversation ignored{99,1,"Ignored",labels,1,0,"",0,true,false};ps_ui_conversation(&ignored);ps_ui_meter(99,50,0,100,.5);check(active_owner==0&&last_meter_owner==0,"Widget submitted without an active frame");frame_available=true;
  for(auto& command:commands)check(ps::ui::shortcut(command.id)!=nullptr,"Command help is missing a binding");
  return checks;
 }catch(const std::exception& e){if(error&&capacity)snprintf(error,capacity,"%s",e.what());return -1;}
}

namespace {
ImVec2 menu_display(float x,float y){
 auto zero=video_convert_window_to_screen(v2d_new(0,0)),unit=video_convert_window_to_screen(v2d_new(1,1));
 return ImVec2((x-zero.x)/(unit.x-zero.x),(y-zero.y)/(unit.y-zero.y));
}
void menu_point(PsUiMenuAction action,int index){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();menu_points.push_back({action,index,ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f)});}
bool menu_button(const char* label,PsUiMenuAction action,int index=0){bool clicked=ImGui::Button(label);menu_point(action,index);if(clicked)ps_ui_menu_request(action,index);return clicked;}
}
extern "C" bool ps_ui_menu_active(){return menu_open;}
extern "C" void ps_ui_menu_clear(){
 bool owned=menu_open;menu_open=menu_pointer_armed=false;menu_owner=menu_epoch=0;menu_items.clear();menu_regions.clear();menu_points.clear();menu_pending.clear();
 if(owned&&frame_available&&ImGui::GetCurrentContext()){ImGui::ClearActiveID();ImGui::SetWindowFocus(nullptr);}
}
extern "C" void ps_ui_menu_begin(const PsUiMenuModel* m){
 if(!ready||!frame_available||!m||!m->owner||!m->open){ps_ui_menu_clear();return;}
 if(menu_owner!=m->owner||menu_epoch!=m->epoch){menu_pending.clear();menu_pointer_armed=false;ImGui::ClearActiveID();}
 menu_owner=m->owner;menu_epoch=m->epoch;menu_open=true;menu_page=m->page;menu_selected=m->selected;menu_items.clear();menu_regions.clear();menu_points.clear();
 for(int i=0;i<m->count;i++)menu_items.push_back({m->items[i].definition,m->items[i].instance,m->items[i].quantity});
}
extern "C" void ps_ui_menu_region(PsUiMenuRegion kind,int index,float x,float y,float w,float h){
 if(!menu_open||!frame_available||w<=0||h<=0)return;
 if((kind==PS_MENU_SLOT||kind==PS_MENU_CATEGORIES)&&(menu_page!=1&&menu_page!=2))return;
 menu_regions.push_back({kind,index,menu_display(x,y),menu_display(x+w,y+h)});
}
extern "C" bool ps_ui_menu_request(PsUiMenuAction action,int index){
 if(!menu_open||!menu_owner||action<PS_MENU_SELECT||action>PS_MENU_RESUME||menu_pending.size()>=8)return false;
 if(action==PS_MENU_CATEGORY&&index!=-1&&index!=1)return false;
 bool item=action==PS_MENU_SELECT||action==PS_MENU_ACTIVATE||action==PS_MENU_ASSIGN;
 if(item&&(index<0||size_t(index)>=menu_items.size()))return false;
 for(auto& c:menu_pending)if(c.action==action)return false;
 PsUiMenuCommand c{};c.owner=menu_owner;c.epoch=menu_epoch;c.action=action;c.index=index;
 if(item){snprintf(c.definition,sizeof c.definition,"%s",menu_items[index].definition.c_str());snprintf(c.instance,sizeof c.instance,"%s",menu_items[index].instance.c_str());}
 menu_pending.push_back(c);return true;
}
extern "C" bool ps_ui_menu_command(uint64_t owner,uint64_t epoch,PsUiMenuCommand* out){
 while(!menu_pending.empty()){auto c=menu_pending.front();menu_pending.pop_front();if(c.owner==owner&&c.epoch==epoch){if(out)*out=c;return true;}}return false;
}
extern "C" void ps_ui_menu_end(){
 if(!menu_open||!ready||!frame_available)return;
 auto& io=ImGui::GetIO();float scale=std::min(io.DisplaySize.x/1920,io.DisplaySize.y/1080);Theme theme(scale);
 ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(io.DisplaySize);
 ImGui::Begin("Menu pointer surface",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNavInputs|ImGuiWindowFlags_NoBringToFrontOnFocus);
 ImGui::PushID(std::to_string(menu_owner).c_str());ImGui::PushID(std::to_string(menu_epoch).c_str());
 if(!ImGui::IsMouseDown(0))menu_pointer_armed=true;ImGui::BeginDisabled(!menu_pointer_armed);
 for(const auto& r:menu_regions){
  ImGui::PushID(int(r.kind));ImGui::PushID(r.index);
  if(r.kind==PS_MENU_SLOT||r.kind==PS_MENU_LINK){
   ImGui::SetCursorScreenPos(r.lo);bool click=ImGui::InvisibleButton("##target",ImVec2(r.hi.x-r.lo.x,r.hi.y-r.lo.y),ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_PressedOnClickRelease|ImGuiButtonFlags_PressedOnDoubleClick);
   menu_point(r.kind==PS_MENU_SLOT?PS_MENU_SELECT:PS_MENU_PAGE,r.index);
   if(ImGui::IsItemHovered()){ImGui::GetWindowDrawList()->AddRect(r.lo,r.hi,ImGui::GetColorU32(skin.active));ImGui::SetTooltip("%s",r.kind==PS_MENU_SLOT?"Click: select. Double-click: use/equip. Use the buttons below for actions.":"Open this section");}
   if(click){if(r.kind==PS_MENU_LINK)ps_ui_menu_request(PS_MENU_PAGE,r.index);else {ps_ui_menu_request(PS_MENU_SELECT,r.index);if(ImGui::IsMouseDoubleClicked(0))ps_ui_menu_request(PS_MENU_ACTIVATE,r.index);}}
  }else if(r.kind==PS_MENU_CATEGORIES){
   ImGui::SetCursorScreenPos(r.lo);menu_button("<",PS_MENU_CATEGORY,-1);
   ImGui::SetCursorScreenPos(ImVec2(r.hi.x-ImGui::GetFrameHeight(),r.lo.y));menu_button(">",PS_MENU_CATEGORY,1);
  }else if(r.kind==PS_MENU_CONTROLS){
   ImGui::SetCursorScreenPos(ImVec2(r.lo.x,std::max(r.lo.y,r.hi.y-ImGui::GetFrameHeightWithSpacing())));
   if(menu_page==1||menu_page==2){menu_button(menu_page==2?"Assign":"Use / Equip",PS_MENU_ACTIVATE,menu_selected);ImGui::SameLine();menu_button("Assign subweapon",PS_MENU_ASSIGN,menu_selected);ImGui::SameLine();}
   menu_button("Back",PS_MENU_BACK);ImGui::SameLine();menu_button("Resume",PS_MENU_RESUME);
  }ImGui::PopID();ImGui::PopID();
 }
 ImGui::EndDisabled();ImGui::PopID();ImGui::PopID();
 // Cursor positioning above submits actual hit targets; close the layout explicitly.
 ImGui::SetCursorPos(ImVec2(0,0));ImGui::Dummy(ImVec2(1,1));ImGui::End();
}
namespace ps::ui {
bool native_menu_click(PsUiMenuAction action,int index,bool down){if(!ps_lab_automatic())return false;for(auto p:menu_points)if(p.action==action&&p.index==index){native_mouse.push_back({p.point,true,down,0});return true;}return false;}
int native_menu_item(const char* id,int* quantity){for(size_t i=0;i<menu_items.size();i++)if(menu_items[i].definition==id){if(quantity)*quantity=menu_items[i].quantity;return int(i);}return -1;}
int native_menu_selected(){return menu_selected;}
uint64_t native_menu_epoch(){return menu_epoch;}
}
