// Fresh-process laboratories; no build or scripting pipeline. Signed: President.
#include "lab.h"
#include "../ui/widgets.hpp"
extern "C" int ps_ui_compat_check(const char*,char*,unsigned);
extern "C" int ps_ui_pilot_open(const char*);
extern "C" int ps_ui_pilot_inventory();
extern "C" int ps_ui_pilot_mix_confirm(int);
extern "C" {
#include "../core/engine.h"
#include "../core/commandline.h"
#include "../core/video.h"
#include "../core/image.h"
#include "../core/scene.h"
#include "../core/storyboard.h"
#include "../scripting/scripting.h"
#include "../entities/actor.h"
#include "../foundation/foundation_host.h"
#include "../editor_shell/shell.h"
}
#include <allegro5/allegro_image.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <spawn.h>
extern char** environ;
#endif
namespace {
namespace fs=std::filesystem;
const char* storage_magic="PowerSurge lab storage v1\n";
const char* run_magic="PowerSurge native lab run v1\n";
struct Profile {const char* id;const char* name;bool automatic;bool frontend=false;};
const Profile profiles[]={{"ui-pilot","Shared UI / conversation / meter pilot",false},{"ui-pilot-check","Shared UI and save compatibility checks",true},{"frontend","Intro / title / main menu",false,true},{"terrain","Terrain / editor laboratory",false},{"physics","Physics objects laboratory",false},{"display-check","Fullscreen monitor and window restoration checks",true},{"presentation-menu-check","Menu artwork and navigation checks",true},{"editor-contract","Editor placement / save checks",true},{"terrain-journey","Connected terrain journey",true}};
struct Run {
 bool active=false,keep=false,automatic=false,completed=false,frontend=false,embedded=false;int code=0,frames=0,frame_limit=0;
 std::string id,result,error;fs::path root,folder,original,exe;std::string preferences;
 std::chrono::steady_clock::time_point started;
#ifdef _WIN32
 HANDLE lease=INVALID_HANDLE_VALUE;
#else
 int lease=-1;
#endif
} run;
std::string pending_lab;unsigned long long generation=0;
bool switch_check=false;
void ui_check_tick();
void switch_check_tick();void menu_check_tick();void display_check_tick();
std::vector<std::string> arguments;std::vector<char*> pointers;
std::string read(const fs::path& p,size_t limit=262144){
 std::error_code ec;auto n=fs::file_size(p,ec);if(ec||n>limit)throw std::runtime_error("Missing/oversized lab file: "+p.u8string());
 std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
void write(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);f<<text;f.close();if(!f)throw std::runtime_error("Cannot write lab file: "+p.u8string());}
bool link(const fs::path& p){
#ifdef _WIN32
 auto a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT);
#else
 return fs::is_symlink(fs::symlink_status(p));
#endif
}
std::string quoted(const std::string& s){std::string o="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=c;}else if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else if(c=='\t')o+="\\t";else if(c>=32)o+=c;}return o+'"';}
fs::path executable(){
#ifdef _WIN32
 wchar_t p[32768];auto n=GetModuleFileNameW(nullptr,p,32768);if(!n||n>=32768)throw std::runtime_error("Cannot locate engine executable");return fs::path(p);
#else
 return fs::read_symlink("/proc/self/exe");
#endif
}
void env(const char* name,const std::string& value){
#ifdef _WIN32
 if(_putenv_s(name,value.c_str()))throw std::runtime_error("Cannot set lab environment");
#else
 setenv(name,value.c_str(),1);
#endif
}
void clear_old_configuration(){
 std::vector<std::string> names;
#ifdef _WIN32
 for(char** p=_environ;p&&*p;++p){std::string s=*p;auto n=s.substr(0,s.find('='));if(n.rfind("BSL_",0)==0||n.rfind("BOR_",0)==0||n.rfind("PS_UI_",0)==0)names.push_back(n);}
#else
 for(char** p=environ;p&&*p;++p){std::string s=*p;auto n=s.substr(0,s.find('='));if(n.rfind("BSL_",0)==0||n.rfind("BOR_",0)==0||n.rfind("PS_UI_",0)==0)names.push_back(n);}
#endif
 for(auto& n:names){
#ifdef _WIN32
  _putenv_s(n.c_str(),"");
#else
  unsetenv(n.c_str());
#endif
 }
}
fs::path storage(){
 auto root=fs::temp_directory_path()/"PowerSurge-labs-v1";
 if(link(root))throw std::runtime_error("Lab storage cannot be a link");
 if(fs::exists(root)){if(link(root/".owner")||read(root/".owner",128)!=storage_magic)throw std::runtime_error("Unrecognized lab storage; preserved");}
 else {fs::create_directory(root);write(root/".owner",storage_magic);}
 return root;
}
bool owned(const fs::path& p,const fs::path& root){
 if(p.parent_path()!=root||p.filename().u8string().rfind("run-",0)!=0||link(p)||!fs::is_directory(p))return false;
 try {if(link(p/".owner")||read(p/".owner",256)!=std::string(run_magic)+p.filename().u8string()+"\n")return false;
  for(auto& e:fs::recursive_directory_iterator(p))if(link(e.path()))return false;
 }catch(...){return false;}return true;
}
bool take_lease(const fs::path& p){
#ifdef _WIN32
 auto h=CreateFileW((p/".lease").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE)return false;run.lease=h;
#else
 int f=open((p/".lease").c_str(),O_RDWR|O_CREAT,0600);if(f<0)return false;if(flock(f,LOCK_EX|LOCK_NB)){close(f);return false;}run.lease=f;
#endif
 return true;
}
void release_lease(){
#ifdef _WIN32
 if(run.lease!=INVALID_HANDLE_VALUE){CloseHandle(run.lease);run.lease=INVALID_HANDLE_VALUE;}
#else
 if(run.lease>=0){close(run.lease);run.lease=-1;}
#endif
}
void delete_payload(const fs::path& p){
 // Keep the ownership record and live lease until all large payloads are gone.
 for(auto& e:fs::directory_iterator(p))if(e.path().filename()!=".owner"&&e.path().filename()!=".lease")fs::remove_all(e.path());
}
void delete_metadata(const fs::path& p){
 std::error_code ec;fs::remove(p/".lease",ec);if(ec)throw std::runtime_error("Cannot remove lab lease metadata");
 fs::remove(p/".owner",ec);if(ec)throw std::runtime_error("Cannot remove lab ownership metadata");
 fs::remove(p,ec);if(ec&&fs::exists(p))throw std::runtime_error("Cannot remove empty lab directory");
}
unsigned clean_stale(const fs::path& root){
 unsigned count=0;
 for(auto& e:fs::directory_iterator(root)){
  auto p=e.path();if(!owned(p,root)||fs::exists(p/".keep"))continue;
  auto previous=run.lease;
  if(!take_lease(p))continue;
  bool recognized=owned(p,root);
  try{if(recognized)delete_payload(p);}catch(...){release_lease();run.lease=previous;throw;}
  release_lease();run.lease=previous;
  if(recognized){delete_metadata(p);++count;}
 }
 return count;
}
void remove_run(){
 if(run.folder.empty()){release_lease();return;}
 if(!owned(run.folder,run.root))throw std::runtime_error("Lab cleanup refused an unrecognized or linked directory");
 fs::current_path(run.original);if(run.keep){write(run.folder/".keep","Explicitly retained by --keep-lab\n");release_lease();return;}
 delete_payload(run.folder);release_lease();delete_metadata(run.folder);
}
void copy_content(const fs::path& from,const fs::path& to){
 fs::create_directories(to);
 for(auto& e:fs::recursive_directory_iterator(from)){
  if(link(e.path()))throw std::runtime_error("Lab fixture links are unsupported");auto dest=to/e.path().lexically_relative(from);
  if(e.is_directory())fs::create_directory(dest);else if(e.is_regular_file())fs::copy_file(e.path(),dest);
 }
}
void report(){
 auto dir=run.exe.parent_path()/"lab-reports";
 if(link(dir))throw std::runtime_error("Report folder cannot be a link");
 std::error_code ec;fs::create_directories(dir,ec);if(ec){dir=run.root/"reports";fs::create_directories(dir);}
 auto name="report-"+run.folder.filename().u8string().substr(4);auto file=dir/(name+".json");
 std::ostringstream text;text<<"{\n  \"owner\":\"PowerSurge lab v1\",\n  \"lab\":"<<quoted(run.id)<<",\n  \"status\":"<<quoted(run.completed?(run.code?"failed":"passed"):(run.automatic?"cancelled":"completed"))<<",\n  \"exit_code\":"<<run.code<<",\n  \"error\":"<<quoted(run.error)<<",\n  \"retained\":"<<(run.keep?quoted(run.folder.u8string()):"null")<<",\n  \"native_result\":"<<(run.result.empty()?"null":run.result)<<"\n}\n";
 auto temp=file;temp+=".tmp";write(temp,text.str());fs::rename(temp,file);
 auto log=run.folder/"content/logfile.txt";if(fs::is_regular_file(log)){std::ifstream in(log,std::ios::binary);in.seekg(0,std::ios::end);auto n=in.tellg();if(n>std::streamoff(131072))in.seekg(n-std::streamoff(131072));else in.seekg(0);write(dir/(name+".log"),{std::istreambuf_iterator<char>(in),{}});}
 std::vector<fs::path> files;
 for(auto& e:fs::directory_iterator(dir))if(e.is_regular_file()&&!link(e.path())&&e.path().extension()==".json"&&e.path().filename().u8string().rfind("report-",0)==0){try{if(read(e.path(),524288).find("\"owner\":\"PowerSurge lab v1\"")!=std::string::npos)files.push_back(e.path());}catch(...){}}
 std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return fs::last_write_time(a)>fs::last_write_time(b);});
 for(size_t i=10;i<files.size();++i){auto logpath=files[i];logpath.replace_extension(".log");if(!link(logpath))fs::remove(logpath);fs::remove(files[i]);}
 std::printf("PowerSurge lab report: %s\n",file.u8string().c_str());
}
int storage_check(){
 auto root=storage();clean_stale(root);int checks=0;
 auto p=root/("run-storage-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(p);write(p/".owner",std::string(run_magic)+p.filename().u8string()+"\n");
 if(!owned(p,root))throw std::runtime_error("Owned fixture not recognized");++checks;
 if(!take_lease(p))throw std::runtime_error("Cannot lease fixture");clean_stale(root);if(!fs::exists(p))throw std::runtime_error("Active run removed");++checks;release_lease();
 auto unknown=root/("unknown-"+p.filename().u8string());fs::create_directory(unknown);write(unknown/"sentinel","preserve");
 if(clean_stale(root)!=1||fs::exists(p))throw std::runtime_error("Stale fixture not removed");++checks;
 if(read(unknown/"sentinel")!="preserve")throw std::runtime_error("Unknown folder changed");++checks;
 fs::remove(unknown/"sentinel");fs::remove(unknown);std::printf("PowerSurge storage checks: %d passed\n",checks);return 0;
}
}
extern "C" bool ps_lab_active(){return run.active;}
extern "C" bool ps_lab_automatic(){return run.active&&(switch_check||(run.automatic&&!run.completed));}
extern "C" const char* ps_lab_preferences(){return run.active?run.preferences.c_str():nullptr;}
extern "C" int ps_lab_exit_code(){return run.code;}
extern "C" int ps_lab_prepare(int* argc,char*** argv){
 try{
  arguments.clear();pointers.clear();
  run.exe=executable();std::string selected;bool list=false,check=false;
  arguments.push_back((*argv)[0]);
  for(int i=1;i<*argc;++i){std::string a=(*argv)[i];
   if(a=="--lab"){if(++i==*argc)throw std::runtime_error("--lab requires an identity");selected=(*argv)[i];}
   else if(a=="--lab-list")list=true;
   else if(a=="--lab-storage-check")check=true;
   else if(a=="--lab-switch-check")switch_check=true;
   else if(a=="--keep-lab")run.keep=true;
   else if(a=="--lab-frames"){if(++i==*argc)throw std::runtime_error("--lab-frames requires a count");run.frame_limit=std::stoi((*argv)[i]);if(run.frame_limit<1||run.frame_limit>36000)throw std::runtime_error("Lab frame limit must be1..36000");}
   else arguments.push_back(a);
  }
  // A preview package declares its entry explicitly; external projects retain their own startup.
  auto preview=run.exe.parent_path()/"powersurge-preview.ini";
  if(selected.empty()&&!list&&!check&&arguments.size()==1&&fs::is_regular_file(preview)){
   std::istringstream config(read(preview,1024));std::string line;
   while(std::getline(config,line))if(line.rfind("startup=",0)==0||line.rfind("default_lab=",0)==0){selected=line.substr(line.find('=')+1);if(!selected.empty()&&selected.back()=='\r')selected.pop_back();}
   if(selected.empty())throw std::runtime_error("Preview configuration has no startup");
  }
  if(list){for(auto& p:profiles)if(!p.frontend)std::printf("%s : %s [%s]\n",p.id,p.name,p.automatic?"automatic":"interactive");return -1;}
  if(check){run.code=storage_check();return -1;}
  if(selected.empty()){
   if(run.keep||run.frame_limit)throw std::runtime_error("Lab options require --lab");
   auto old=fs::temp_directory_path()/"PowerSurge-labs-v1";
   if(fs::exists(old)){try{clean_stale(storage());}catch(const std::exception& e){std::fprintf(stderr,"Lab maintenance skipped: %s\n",e.what());}}
   return 0;
  }
  auto profile=std::find_if(std::begin(profiles),std::end(profiles),[&](auto& p){return p.id==selected;});
  if(profile==std::end(profiles))throw std::runtime_error("Unknown lab; use --lab-list");
  if(arguments.size()!=1)throw std::runtime_error("Lab owns launch configuration; use --lab, --lab-frames and --keep-lab only");
  run.root=storage();clean_stale(run.root);run.original=fs::current_path();
  auto seed=run.exe.parent_path()/"labs/content";
  if(!fs::exists(seed/"lab-content.marker"))throw std::runtime_error("Lab fixture pack is missing beside the executable");
  std::random_device random;
  for(int i=0;i<10;++i){run.folder=run.root/("run-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(random()));if(fs::create_directory(run.folder))break;if(i==9)throw std::runtime_error("Cannot create unique lab run");}
  if(!take_lease(run.folder))throw std::runtime_error("Cannot lock lab run");
  write(run.folder/".owner",std::string(run_magic)+run.folder.filename().u8string()+"\n");
  if(run.keep)write(run.folder/".keep","Explicit retention requested\n");
  copy_content(seed,run.folder/"content");fs::create_directory(run.folder/"user");
  clear_old_configuration();env("OPENSURGE_USER_PATH",(run.folder/"user").u8string());env("BSL_PROGRESS_PATH",(run.folder/"progress.bsp").u8string());env("BOR_SESSION_DATA_PATH",(run.folder/"session.bsl").u8string());
  // Compatibility adapter to existing native service switches. Owned by this lab process only.
  const char* services[]={"GAMEPLAY","PROGRESSION","LEARNING","CHECKPOINT","PROGRESS_SAVE","PROGRESS_BOOT","PICKUPS","LOOT","EQUIPMENT","TELEPORT","CALENDAR"};
  for(auto service:services)env((std::string("BSL_")+service).c_str(),"1");
  env("BSL_COLLIDER_DEBUG","0");
  run.id=selected;run.frontend=profile->frontend;run.automatic=profile->automatic;run.active=true;run.preferences=(run.folder/"user/editor-ui.ini").u8string();run.started=std::chrono::steady_clock::now();++generation;
  if(selected=="ui-pilot"||selected=="ui-pilot-check")env("PS_UI_PILOT","1");
  if(selected=="ui-pilot-check")fs::remove(run.folder/"content/editor-ui.ini");
  if(run.automatic&&selected!="ui-pilot-check"&&selected!="presentation-menu-check"&&selected!="display-check")env(selected=="editor-contract"?"BSL_SHELL_CHECK":"BSL_CELL_LEVEL_JOURNEY",(run.folder/"result.json").u8string());
  fs::current_path(run.folder/"content");arguments.insert(arguments.end(),{"--game-folder",(run.folder/"content").u8string()});if(run.frontend)arguments.insert(arguments.end(),{"--quest","quests/initial.qst"});else if(selected=="presentation-menu-check"||selected=="display-check")arguments.insert(arguments.end(),{"--level","levels/presentation/mainmenu.lev"});else if(selected=="physics")arguments.insert(arguments.end(),{"--level","levels/physics-lab.lev"});else arguments.insert(arguments.end(),{"--quest","quests/cell-levels.qst"});arguments.insert(arguments.end(),{"--windowed","--resolution","1"});
  for(auto& a:arguments)pointers.push_back(a.data());pointers.push_back(nullptr);*argc=int(arguments.size());*argv=pointers.data();
  std::printf("PowerSurge session: %s; temporary run %s\n",run.id.c_str(),run.folder.u8string().c_str());return 1;
 }catch(const std::exception& e){run.error=e.what();run.code=1;std::fprintf(stderr,"PowerSurge lab: %s\n",e.what());try{if(!run.folder.empty())remove_run();}catch(...){}return -1;}
}
extern "C" void ps_lab_tick(){
 if(run.active&&switch_check)switch_check_tick();
 if(!run.active||run.completed)return;++run.frames;
 if(run.id=="ui-pilot-check"){ui_check_tick();return;}
 if(run.id=="presentation-menu-check"){menu_check_tick();return;}
 if(run.id=="display-check"){display_check_tick();return;}
 try{
  if(run.automatic&&fs::exists(run.folder/"result.json")){
   run.result=read(run.folder/"result.json");run.code=run.result.find("\"passed\":true")!=std::string::npos?0:1;run.completed=true;if(!run.embedded)engine_quit();else bsl_shell_note("lab",run.code?"Automatic laboratory failed; see its report.":"Automatic laboratory passed; current level remains open.",run.code?2:0);
  }else if(std::chrono::steady_clock::now()-run.started>std::chrono::seconds(60)&&run.automatic){run.error="Native scenario exceeded60 seconds";run.code=1;run.completed=true;if(!run.embedded)engine_quit();else bsl_shell_note("lab",run.code?"Automatic laboratory failed; see its report.":"Automatic laboratory passed; current level remains open.",run.code?2:0);}
  else if(run.frame_limit&&run.frames>=run.frame_limit){run.code=run.automatic?1:0;run.completed=true;if(!run.embedded)engine_quit();else bsl_shell_note("lab",run.code?"Automatic laboratory failed; see its report.":"Automatic laboratory passed; current level remains open.",run.code?2:0);}
 }catch(const std::exception& e){run.error=e.what();run.code=1;run.completed=true;if(!run.embedded)engine_quit();else bsl_shell_note("lab",run.code?"Automatic laboratory failed; see its report.":"Automatic laboratory passed; current level remains open.",run.code?2:0);}
}
extern "C" int ps_lab_finish(){
 if(!run.active)return 0;if(run.automatic&&!run.completed)run.code=2;
 try{report();}catch(const std::exception& e){std::fprintf(stderr,"Lab report: %s\n",e.what());run.code=1;}
 try{remove_run();}catch(const std::exception& e){release_lease();std::fprintf(stderr,"Lab cleanup: %s\n",e.what());run.code=1;}
 run.active=false;return run.code;
}
extern "C" unsigned long long ps_lab_generation(){return generation;}
extern "C" bool ps_lab_spawn(const char* id){
 if(!id||!pending_lab.empty())return false;
 if(std::none_of(std::begin(profiles),std::end(profiles),[&](auto& p){return !p.frontend&&std::string(p.id)==id;}))return false;
 pending_lab=id;engine_restart(nullptr);return true;
}
extern "C" bool ps_lab_apply_switch(commandline_t* cmd){
 if(pending_lab.empty())return true;
 auto next=std::move(pending_lab);pending_lab.clear();
 auto exe=run.exe.u8string();ps_lab_finish();
 if(!bsl_host_begin_lab_run()){run.code=1;std::fprintf(stderr,"Previous gameplay bindings survived teardown; laboratory refused.\n");return false;}
 run=Run{};
 char option[]="--lab";char* av[]={exe.data(),option,next.data()};char** args=av;int argc=3;
 if(ps_lab_prepare(&argc,&args)<0)return false;
 *cmd=commandline_parse(argc,args);run.embedded=true;return true;
}

extern "C" void ps_lab_mark_saved(){
 if(!run.active||run.automatic)return;
 try{
  auto seed=run.exe.parent_path()/"labs/content";
  for(const auto* name:{"levels","terrain"}){
   auto folder=run.folder/"content"/name;if(!fs::exists(folder))continue;
   for(auto& e:fs::recursive_directory_iterator(folder))if(e.is_regular_file()&&!link(e.path())){
    auto rel=e.path().lexically_relative(run.folder/"content");auto original=seed/rel;
    if(!fs::exists(original)||read(e.path(),8*1024*1024)!=read(original,8*1024*1024)){
     run.keep=true;write(run.folder/".keep","Explicitly saved authoring changes\n");
     std::string message="Saved test workspace retained at "+run.folder.u8string();bsl_shell_note("workspace",message.c_str(),0);return;
    }
   }
  }
 }catch(const std::exception& e){run.keep=true;bsl_shell_note("workspace",e.what(),2);}
}
namespace {
void switch_check_tick(){
 static int phase=0,checks=0;static unsigned long long previous=0;
 static auto begun=std::chrono::steady_clock::now();
 auto check=[&](bool ok,const char* error){if(!ok)throw std::runtime_error(error);++checks;};
 try{
  if(std::chrono::steady_clock::now()-begun>std::chrono::seconds(150))throw std::runtime_error("Same-instance check timed out");
  if(run.frames<45||bsl_host_boot_busy()||!pending_lab.empty())return;
  std::string scene=bsl_engine_level_file();
  if(phase==0){
   if(scene!="levels/presentation/title.lev"&&scene!="levels/presentation/mainmenu.lev")return;
   check(run.frontend,"Default session is not the restored frontend");check(generation>0,"Session generation missing");
   previous=generation;phase++;check(ps_lab_spawn("ui-pilot-check"),"UI pilot switch refused");
  }else if(phase==1&&run.id=="ui-pilot-check"&&run.completed){
   check(run.code==0,"UI pilot contract failed");check(std::getenv("PS_UI_PILOT")!=nullptr,"UI pilot did not own its opt-in flag");previous=generation;phase++;check(ps_lab_spawn("terrain"),"Terrain switch refused");
  }else if(phase==2&&run.id=="terrain"){
   check(std::getenv("PS_UI_PILOT")==nullptr,"UI pilot configuration escaped to ordinary gameplay");
   check(generation>previous,"Terrain did not use a new run context");check(scene=="levels/cell-terrain.lev","Wrong terrain entry");
   BslGameplayProbe game{};bsl_host_gameplay_probe(&game);check(game.actor&&game.health==100&&game.items==3,"Terrain session not reconstructed");
   previous=generation;phase++;check(ps_lab_spawn("physics"),"Physics switch refused");
  }else if(phase==3&&run.id=="physics"){
   check(generation>previous,"Physics reused prior run context");check(scene=="levels/physics-lab.lev","Wrong physics entry");
   for(const char* id:{"b5010001","b5010002","b5010003","b5010004","b5010005"})check(bsl_shell_physics_body(id),"Physics factory did not reconstruct");
   previous=generation;phase++;check(ps_lab_spawn("editor-contract"),"Editor check switch refused");
  }else if(phase==4&&run.id=="editor-contract"&&run.completed){
   check(generation>previous,"Editor check reused prior context");check(run.code==0,"Editor contract failed");
   previous=generation;phase++;check(ps_lab_spawn("terrain-journey"),"Journey switch refused");
  }else if(phase==5&&run.id=="terrain-journey"&&run.completed){
   check(generation>previous,"Journey reused prior context");check(run.code==0,"First integrated journey failed");
   previous=generation;phase++;check(ps_lab_spawn("terrain-journey"),"Repeated journey switch refused");
  }else if(phase==6&&run.id=="terrain-journey"&&run.completed){
   check(generation>previous,"Repeated journey reused prior context");check(run.code==0,"Repeated journey failed");
   auto native=run.result;run.result="{\"passed\":true,\"session_switches\":6,\"checks\":"+std::to_string(checks)+",\"last_journey\":"+native+"}";
   switch_check=false;phase++;std::printf("Same-instance checks passed: %d; six transitions, including UI teardown and repeated automatic journey.\n",checks);engine_quit();
  }
 }catch(const std::exception& e){run.code=1;run.completed=true;run.error=e.what();switch_check=false;std::fprintf(stderr,"Same-instance check failed: %s\n",e.what());engine_quit();}
}
}

namespace {
struct MenuVisuals {int sprites=0,visible=0;surgescript_object_t* menu=nullptr;};
bool menu_visit(surgescript_object_t* o,void* data){
 auto& p=*static_cast<MenuVisuals*>(data);std::string name=surgescript_object_name(o);
 if(name=="BSL C2 Main Menu")p.menu=o;
 if(name=="BSL Presentation Sprite")p.sprites++;
 if(name=="Actor"){
  auto* manager=surgescript_object_manager(o);auto parent=surgescript_object_parent(o);
  if(surgescript_objectmanager_exists(manager,parent)&&std::string(surgescript_object_name(surgescript_objectmanager_get(manager,parent)))=="BSL Presentation Sprite"){
   auto* actor=scripting_actor_ptr(o);if(actor&&actor->visible&&actor->alpha>0.01f)p.visible++;
  }
 }
 return true;
}
MenuVisuals menu_probe(){MenuVisuals p;auto* level=scripting_util_surgeengine_component(surgescript_vm(),"Level");surgescript_object_traverse_tree_ex(level,&p,menu_visit);return p;}
void menu_capture(const char* name){
 auto* shot=video_take_snapshot();if(!shot)return;auto* previous=al_get_new_file_interface();al_set_standard_file_interface();
 al_save_bitmap((run.folder/name).u8string().c_str(),IMAGE2BITMAP(shot));al_set_new_file_interface(previous);image_destroy(shot);
}
void menu_check_tick(){
 static unsigned long long epoch=0;static int checks=0,before=0,after=0;
 if(epoch!=generation){epoch=generation;checks=before=after=0;}
 auto check=[&](bool ok,const char* why){if(!ok)throw std::runtime_error(why);checks++;};
 try{
  if(std::chrono::steady_clock::now()-run.started>std::chrono::seconds(60))throw std::runtime_error("Menu navigation check timed out");
  if(run.frames==15){auto p=menu_probe();before=p.visible;menu_capture("menu-before.png");check(p.menu!=nullptr,"Main menu is missing");check(p.sprites==5&&p.visible==5,"Menu artwork missing before navigation");}
  if(run.frames==25||run.frames==135||run.frames==165){
   auto p=menu_probe();check(p.menu&&surgescript_object_has_function(p.menu,"moveSelection"),"Menu navigation action missing");
   auto* direction=surgescript_var_set_number(surgescript_var_create(),run.frames==165?-1:1);const surgescript_var_t* args[]={direction};auto* result=surgescript_var_create();
   surgescript_object_call_function(p.menu,"moveSelection",args,1,result);int selected=int(surgescript_var_get_number(result));surgescript_var_destroy(result);surgescript_var_destroy(direction);
   check(selected==(run.frames==135?2:1),"Wrong menu selection after navigation");
  }
  if(run.frames==120||run.frames==210){auto p=menu_probe();after=p.visible;menu_capture("menu-after.png");check(p.sprites==5&&p.visible==5,"Menu artwork disappeared after navigation/garbage collection");}
  if(run.frames==210){auto p=menu_probe();check(p.menu!=nullptr,"Menu disappeared before activation");surgescript_object_call_function(p.menu,"activate",nullptr,0,nullptr);}
  if(run.frames==255){
   check(scenestack_top()==storyboard_get_scene(SCENE_STAGESELECT),"Level Select menu action did not open its scene");menu_capture("level-select.png");
   run.result="{\"passed\":true,\"checks\":"+std::to_string(checks)+",\"artwork_before\":"+std::to_string(before)+",\"artwork_after\":"+std::to_string(after)+"}";run.completed=true;run.code=0;
   std::printf("Menu navigation checks passed: %d; artwork retained through GC and Level Select activation.\n",checks);if(!run.embedded)engine_quit();else bsl_shell_note("lab","Menu navigation checks passed; Level Select remains open.",0);
  }
 }catch(const std::exception& e){
  menu_capture("menu-after.png");run.error=e.what();run.code=1;run.completed=true;
  run.result="{\"passed\":false,\"checks\":"+std::to_string(checks)+",\"artwork_before\":"+std::to_string(before)+",\"artwork_after\":"+std::to_string(after)+"}";
  std::fprintf(stderr,"Menu navigation check failed: %s\n",e.what());if(!run.embedded)engine_quit();else bsl_shell_note("lab",e.what(),2);
 }
}
}

namespace {
void display_check_tick(){
 static unsigned long long epoch=0;static int phase=0;
 if(epoch!=generation){epoch=generation;phase=0;}
 if(run.frames<30||run.frames%10)return;
 char error[256]="";int monitors=0,checks=0;
 int test_phase=std::chrono::steady_clock::now()-run.started>std::chrono::seconds(60)?-1:phase++;
 int status=video_fullscreen_check_step(test_phase,&monitors,&checks,error,sizeof error);
 if(status==0&&std::chrono::steady_clock::now()-run.started<std::chrono::seconds(60))return;
 bool passed=status==1;
 if(passed){auto menu=menu_probe();passed=menu.menu&&menu.visible==5;if(!passed)snprintf(error,sizeof error,"Menu artwork lost during fullscreen toggles");else checks++;}
 run.result="{\"passed\":"+std::string(passed?"true":"false")+",\"monitors\":"+std::to_string(monitors)+",\"checks\":"+std::to_string(checks)+"}";
 run.code=passed?0:1;run.completed=true;if(!passed)run.error=*error?error:"Display checks timed out";
 std::printf("Fullscreen monitor checks %s: %d monitors, %d assertions.\n",passed?"passed":"FAILED",monitors,checks);
 if(!run.embedded)engine_quit();else bsl_shell_note("lab",passed?"Monitor checks passed; window restored.":run.error.c_str(),passed?0:2);
}
}

namespace {
void ui_capture(const char* name){
 ps::ui::native_capture((run.folder/name).u8string().c_str());
}
void ui_check_tick(){
 static unsigned long long epoch=0;static int checks=0,phase=0;static PsUiProbe ui{};static float scroll_before=0;static int inventory_index=-1,inventory_quantity=0,health_before=0;
 if(epoch!=generation){epoch=generation;checks=phase=0;}
 if(run.frames<40||run.frames%15)return;
 auto check=[&](bool okay,const char* why){if(!okay)throw std::runtime_error(why);checks++;};
 try{
  char error[512]="";if(std::chrono::steady_clock::now()-run.started>std::chrono::seconds(60))throw std::runtime_error("UI scenario timed out");
  if(phase==0){int n=ps_ui_check(error,sizeof error);check(n>0,error);checks+=n;int compat=ps_ui_compat_check(run.folder.u8string().c_str(),error,sizeof error);check(compat>0,error);checks+=compat;check(ps_ui_pilot_open("dialogue:choice_list")==0,"Could not open the authoritative conversation controller");phase++;}
  else if(phase==1){ps_ui_probe(&ui);check(ui.ready&&ui.open&&ui.choices==64,"Real long conversation did not reach shared widgets");check(ui.hp_fraction>=0&&ui.hp_fraction<=1,"Live HP range invalid");check(ui.width==48&&ui.height==64,"C2 meter dimensions changed");ps::ui::native_key(ImGuiKey_GamepadDpadUp,true);phase++;}
  else if(phase==2){ui_capture("ui-long-choices.png");ps_ui_probe(&ui);check(ui.selected==63,"Gamepad navigation did not wrap and scroll to the final choice");check(ps::ui::selected_visible(),ps::ui::native_choice_bounds());ps::ui::native_key(ImGuiKey_GamepadDpadUp,false);BslConversationProbe conversation{};bsl_host_conversation_probe(&conversation);check(conversation.open,"Conversation closed before confirm");ps::ui::native_key(ImGuiKey_Enter,true);phase++;}
  else if(phase==3){ps::ui::native_key(ImGuiKey_Enter,false);BslConversationProbe c{};bsl_host_conversation_probe(&c);check(!c.open,"Confirmed choice did not close authoritative conversation");check(c.choice==63,"Wrong choice delivered to controller");check(!ps_ui_request(ui.owner+1,ui.epoch,PS_UI_CONFIRM,63),"Cross-owner late confirmation accepted");check(ps_ui_pilot_open("dialogue:training_choice")==0,"Could not reopen conversation");phase++;}
  else if(phase==4){ps_ui_probe(&ui);check(ui.open,"Second conversation not presented");ps::ui::native_key(ImGuiKey_GamepadFaceRight,true);phase++;}
  else if(phase==5){ps::ui::native_key(ImGuiKey_GamepadFaceRight,false);BslConversationProbe c{};bsl_host_conversation_probe(&c);check(!c.open,"Cancellation did not release authoritative conversation");ui_capture("ui-live-meter.png");phase++;}
  else if(phase==6){check(!bsl_host_captures_commands(),"Conversation left gameplay input captured after close");check(fs::is_regular_file(run.folder/"ui-long-choices.png")&&fs::is_regular_file(run.folder/"ui-live-meter.png"),"Rendered UI captures missing");check(ps_ui_pilot_open("dialogue:choice_list")==0,"Could not open mouse conversation");phase++;}
  else if(phase==7){scroll_before=ps::ui::native_scroll();ps::ui::native_wheel(-1);phase++;}
  else if(phase==8){check(ps::ui::native_scroll()>scroll_before,"Mouse wheel did not scroll the in-game list");check(ImGui::GetIO().MouseDrawCursor,"In-game UI cursor is hidden");ps::ui::native_wheel(100);phase++;}
  else if(phase==9){check(ps::ui::native_click_choice(1,true),"Mouse selection target is not visible");phase++;}
  else if(phase==10){check(ps::ui::native_click_choice(1,false),"Mouse release target missing");phase++;}
  else if(phase==11){ps_ui_probe(&ui);check(ui.selected==1,"Mouse click did not select the in-game choice");check(ps::ui::native_click_choice(1,true),"Mouse confirm target missing");phase++;}
  else if(phase==12){check(ps::ui::native_click_choice(1,false),"Mouse confirm release target missing");phase++;}
  else if(phase==13){BslConversationProbe c{};bsl_host_conversation_probe(&c);check(!c.open&&c.choice==1,"Second mouse click did not confirm the in-game choice");check(!bsl_host_captures_commands(),"Mouse conversation retained gameplay input capture");check(ps_ui_pilot_inventory()==0,"Could not open the real inventory controller");BslGameplayProbe player{};bsl_host_gameplay_probe(&player);health_before=player.health;phase++;}
  else if(phase==14){inventory_index=ps::ui::native_menu_item("item:strawberry",&inventory_quantity);if(inventory_index<0||!ps::ui::native_menu_click(PS_MENU_SELECT,inventory_index,true))return;check(true,"Inventory mouse target ready");phase++;}
  else if(phase==15){check(ps::ui::native_menu_click(PS_MENU_SELECT,inventory_index,false),"Inventory slot release missing");phase++;}
  else if(phase==16){check(ps::ui::native_menu_selected()==inventory_index,"Inventory click did not select its own slot");check(ps::ui::native_menu_click(PS_MENU_ACTIVATE,inventory_index,true),"Inventory action button missing");phase++;}
  else if(phase==17){check(ps_ui_pilot_mix_confirm(0)==0,"Could not arm owned mixed input check");check(ps::ui::native_menu_click(PS_MENU_ACTIVATE,inventory_index,false),"Inventory action release missing");phase++;}
  else if(phase==18){check(ps_ui_pilot_mix_confirm(1)==1,"Mixed mouse and legacy keyboard input was not exercised");int quantity=0;check(ps::ui::native_menu_item("item:strawberry",&quantity)>=0&&quantity==inventory_quantity-1,"Mouse item action did not consume exactly one owned item");BslGameplayProbe player{};bsl_host_gameplay_probe(&player);check(player.health>health_before,"Mouse item action did not reach the real health effect");ui_capture("ui-inventory-mouse.png");check(ps::ui::native_menu_click(PS_MENU_RESUME,0,true),"Inventory Resume button missing");phase++;}
  else if(phase==19){check(ps::ui::native_menu_click(PS_MENU_RESUME,0,false),"Inventory Resume release missing");phase++;}
  else if(phase==20){check(!ps_ui_menu_active()&&!bsl_host_captures_commands(),"Inventory mouse close retained pause/input ownership");check(!ps_ui_menu_request(PS_MENU_ACTIVATE,inventory_index),"Closed inventory accepted a repeated action");run.result="{\"passed\":true,\"checks\":"+std::to_string(checks)+",\"save_versions\":[1,2,3],\"choice_count\":64}";run.code=0;run.completed=true;if(!run.embedded)engine_quit();else bsl_shell_note("lab","Shared UI and compatibility checks passed.",0);}
 }catch(const std::exception& e){run.error=e.what();run.code=1;run.completed=true;run.result="{\"passed\":false,\"checks\":"+std::to_string(checks)+"}";std::fprintf(stderr,"Shared UI checks: %s\n",e.what());if(!run.embedded)engine_quit();else bsl_shell_note("lab",e.what(),2);}
}
}
