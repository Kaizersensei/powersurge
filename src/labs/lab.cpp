// Fresh-process laboratories; no build or scripting pipeline. Signed: President.
#include "lab.h"
#include "../lighting/study.h"
#include "../environment/projection.hpp"
#include "../environment/sky.h"
#include "../authoring/world_records.hpp"
#include "../authoring/project.h"
#include "../authoring/project_store.hpp"
#include "../authoring/render_records.hpp"
#include "../cell_terrain/native.h"
extern "C" int ps_render_record_check(const char*,const char*,char*,unsigned);
extern "C" int ps_hub_native_render_edit(int);
#include "../authoring/hub.h"
#include "../authoring/documents.hpp"
#include "../authoring/dialogue.hpp"
#include "../authoring/event_sequence.hpp"
#include <optional>
#include <map>
#include <cstring>
#include <cstdint>
#include "../foundation/calendar_records.hpp"
namespace bsl {const Catalog& database_snapshot();}
extern "C" bool ps_hub_native_select(const char*);
extern "C" int ps_hub_native_sky_ready();
extern "C" bool ps_hub_native_project_is(const char*);
extern "C" int ps_hub_native_navigation_check(char*,unsigned);
#include "../ui/widgets.hpp"
extern "C" int ps_database_document_check(const char*,const char*,char*,unsigned);
extern "C" int ps_database_runtime_probe(char*,unsigned);
extern "C" void ps_hub_native_close();
extern "C" int ps_ui_compat_check(const char*,char*,unsigned);
extern "C" int ps_ui_pilot_open(const char*);
extern "C" int ps_ui_pilot_inventory();
extern "C" int ps_ui_pilot_mix_confirm(int);
extern "C" {
#include "../core/engine.h"
#include "../core/asset.h"
#include "../core/quest.h"
#include "../scenes/quest.h"
#include "../scenes/level.h"
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
const Profile profiles[]={{"sky","Sky / calendar and clouds course",false},{"sky-check","Sky profiles / GPU projection",true},{"environment-check","Shared environment / clocks and weather",true},{"line-check","Terrain lines / input recovery",true},{"bevel","Contour bevel / contact course",false},{"bevel-check","Contour bevel / material picker checks",true},{"material-catalog-check","Imported texture catalogue / residency",true},{"terrain-lighting","Terrain normal-map lighting course",false},{"terrain-lighting-check","Real terrain lighting / geometry isolation",true},{"lighting-study","Normal-map directional lighting study",false},{"lighting-check","GPU surface lighting / normal conventions",true},{"render-record-check","Rendering records / albedo binding",true},{"project-session-check","Persistent source / isolated test journey",true},{"test-session-check","Owned test session lifecycle",true},{"database","Database authoring workspace",false},{"database-check","Database documents / runtime round trip",true},{"database-check-play","Database round-trip continuation",true},{"ui-pilot","Shared UI / conversation / meter pilot",false},{"ui-pilot-check","Shared UI and save compatibility checks",true},{"frontend","Intro / title / main menu",false,true},{"terrain","Terrain / editor laboratory",false},{"physics","Physics objects laboratory",false},{"display-check","Fullscreen monitor and window restoration checks",true},{"presentation-menu-check","Menu artwork and navigation checks",true},{"editor-contract","Editor placement / save checks",true},{"terrain-journey","Connected terrain journey",true}};
struct Run {
 bool active=false,keep=false,automatic=false,completed=false,frontend=false,embedded=false;int code=0,frames=0,frame_limit=0;
 std::string id,result,error,snapshot_source;fs::path root,folder,original,exe;std::string preferences;
 std::optional<ps::project::Manifest> project;
 std::chrono::steady_clock::time_point started;
#ifdef _WIN32
 HANDLE lease=INVALID_HANDLE_VALUE;
#else
 int lease=-1;
#endif
} run;
std::string pending_lab;fs::path pending_seed,seed_override,seed_cleanup;std::string pending_entry,entry_override;unsigned long long generation=0;
bool render_check=false;void render_check_tick();void render_check_begin();
bool project_pending();bool switch_check=false,project_check=false;void project_check_tick();int project_check_finish(int);
void database_check_tick();
void ui_check_tick();
void switch_check_tick();void menu_check_tick();void display_check_tick();
std::string last_report_path;
int finish_current();
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
bool take_lease_for(Run& owner,const fs::path& p){
#ifdef _WIN32
 auto h=CreateFileW((p/".lease").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE)return false;owner.lease=h;
#else
 int f=open((p/".lease").c_str(),O_RDWR|O_CREAT,0600);if(f<0)return false;if(flock(f,LOCK_EX|LOCK_NB)){close(f);return false;}owner.lease=f;
#endif
 return true;
}
bool take_lease(const fs::path& p){return take_lease_for(run,p);}
void release_lease_for(Run& owner){
#ifdef _WIN32
 if(owner.lease!=INVALID_HANDLE_VALUE){CloseHandle(owner.lease);owner.lease=INVALID_HANDLE_VALUE;}
#else
 if(owner.lease>=0){close(owner.lease);owner.lease=-1;}
#endif
}
void release_lease(){release_lease_for(run);}
void delete_payload(const fs::path& p){
 // Keep the ownership record and live lease until all large payloads are gone.
 for(auto& e:fs::directory_iterator(p))if(e.path().filename()!=".owner"&&e.path().filename()!=".lease")fs::remove_all(e.path());
}
void delete_metadata(const fs::path& p){
 std::error_code ec;fs::remove(p/".lease",ec);if(ec)throw std::runtime_error("Cannot remove lab lease metadata");
 fs::remove(p/".owner",ec);if(ec)throw std::runtime_error("Cannot remove lab ownership metadata");
 fs::remove(p,ec);if(ec&&fs::exists(p))throw std::runtime_error("Cannot remove empty lab directory");
}
bool has_authoring_recovery(const fs::path& p){
 auto cache=p/"user/database-drafts";
 try{if(fs::exists(p/"content/database/.authoring-create.json")||fs::exists(p/"content/database/.authoring-create.json.create.pending")||link(p/"content/database/.authoring-create.json")||link(p/"content/database/.authoring-create.json.create.pending"))return true;if(!fs::exists(cache))return false;auto marker=ps::authoring::parse(read(cache/"owner.json",65536));if(marker.value("format",0)!=1||fs::weakly_canonical(fs::u8path(marker.value("root",std::string{})))!=fs::weakly_canonical(p/"content"))return true;
  for(auto& e:fs::directory_iterator(cache))if(e.is_regular_file()&&e.path().filename().u8string().find(".draft.json")!=std::string::npos)return true;
 }catch(...){return true;}return false;
}
unsigned clean_stale(const fs::path& root){
 unsigned count=0;
 for(auto& e:fs::directory_iterator(root)){
  auto p=e.path();if(!owned(p,root)||fs::exists(p/".keep")||has_authoring_recovery(p))continue;
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
 if(run.project){fs::current_path(run.original);release_lease();return;}
 if(run.folder.empty()){release_lease();return;}
 if(!owned(run.folder,run.root))throw std::runtime_error("Lab cleanup refused an unrecognized or linked directory");
 if(has_authoring_recovery(run.folder))run.keep=true;
 fs::current_path(run.original);if(run.keep){write(run.folder/".keep","Retained authoring/recovery workspace or explicit --keep-lab\n");release_lease();return;}
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
 last_report_path=file.u8string();std::printf("PowerSurge lab report: %s\n",file.u8string().c_str());
}
void configure_run(const Profile& profile,std::string entry){
 const std::string identity=profile.id;arguments.clear();pointers.clear();arguments.push_back(run.exe.u8string());run.frames=run.code=0;run.completed=false;run.result.clear();run.error.clear();
  auto local=run.project?run.folder/".powersurge":run.folder;fs::create_directories(local/"user");
  clear_old_configuration();env("OPENSURGE_USER_PATH",(local/"user").u8string());env("BSL_PROGRESS_PATH",(local/"progress.bsp").u8string());env("BOR_SESSION_DATA_PATH",(local/"session.bsl").u8string());
  // Compatibility adapter to existing native service switches. Owned by this lab process only.
  const char* services[]={"GAMEPLAY","PROGRESSION","LEARNING","CHECKPOINT","PROGRESS_SAVE","PROGRESS_BOOT","PICKUPS","LOOT","EQUIPMENT","TELEPORT","CALENDAR"};
  for(auto service:services)env((std::string("BSL_")+service).c_str(),"1");
  if(run.project)env("BSL_PROGRESS_BOOT","0"); // Authored source entry wins over gameplay resume.
  env("BSL_COLLIDER_DEBUG","0");
  run.id=profile.id;run.frontend=profile.frontend&&entry.empty();run.automatic=profile.automatic;run.active=true;run.preferences=(local/"user/editor-ui.ini").u8string();run.started=std::chrono::steady_clock::now();++generation;
  if(identity=="ui-pilot"||identity=="ui-pilot-check"||identity=="database-check"||identity=="database-check-play")env("PS_UI_PILOT","1");
  if(identity=="ui-pilot-check")fs::remove(run.folder/"content/editor-ui.ini");
  if(identity=="environment-check")env("BSL_ENVIRONMENT_TEST","1");
  if(run.automatic&&identity!="sky-check"&&identity!="environment-check"&&identity!="line-check"&&identity!="bevel-check"&&identity!="material-catalog-check"&&identity!="terrain-lighting-check"&&identity!="lighting-check"&&identity!="render-record-check"&&identity!="project-session-check"&&identity!="test-session-check"&&identity!="database-check"&&identity!="database-check-play"&&identity!="ui-pilot-check"&&identity!="presentation-menu-check"&&identity!="display-check")env(identity=="editor-contract"?"BSL_SHELL_CHECK":"BSL_CELL_LEVEL_JOURNEY",(run.folder/"result.json").u8string());
  fs::current_path(run.folder/"content");arguments.insert(arguments.end(),{"--game-folder",(run.folder/"content").u8string()});if(!entry.empty()){arguments.insert(arguments.end(),{fs::path(entry).extension()==".qst"?"--quest":"--level",entry});}else if(run.frontend)arguments.insert(arguments.end(),{"--quest","quests/initial.qst"});else if(identity=="presentation-menu-check"||identity=="display-check")arguments.insert(arguments.end(),{"--level","levels/presentation/mainmenu.lev"});else if(identity=="sky"||identity=="sky-check")arguments.insert(arguments.end(),{"--level","levels/sky-study.lev"});else if(identity=="line-check"||identity=="bevel"||identity=="bevel-check")arguments.insert(arguments.end(),{"--level","levels/bevel-demo.lev"});else if(identity=="terrain-lighting"||identity=="terrain-lighting-check")arguments.insert(arguments.end(),{"--level","levels/terrain-lighting.lev"});else if(identity=="lighting-study"||identity=="lighting-check")arguments.insert(arguments.end(),{"--level","levels/lighting-study.lev"});else if(identity=="physics")arguments.insert(arguments.end(),{"--level","levels/physics-lab.lev"});else arguments.insert(arguments.end(),{"--quest","quests/cell-levels.qst"});arguments.insert(arguments.end(),{"--windowed","--resolution","1"});
  for(auto& a:arguments)pointers.push_back(a.data());pointers.push_back(nullptr);
}
int storage_check(){
 auto root=storage();clean_stale(root);int checks=0;
 auto p=root/("run-storage-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(p);write(p/".owner",std::string(run_magic)+p.filename().u8string()+"\n");
 if(!owned(p,root))throw std::runtime_error("Owned fixture not recognized");++checks;
 if(!take_lease(p))throw std::runtime_error("Cannot lease fixture");clean_stale(root);if(!fs::exists(p))throw std::runtime_error("Active run removed");++checks;release_lease();
 auto unknown=root/("unknown-"+p.filename().u8string());fs::create_directory(unknown);write(unknown/"sentinel","preserve");
 if(clean_stale(root)!=1||fs::exists(p))throw std::runtime_error("Stale fixture not removed");++checks;
 if(read(unknown/"sentinel")!="preserve")throw std::runtime_error("Unknown folder changed");++checks;
 fs::remove(unknown/"sentinel");fs::remove(unknown);
 auto recovery=root/("run-recovery-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(recovery);write(recovery/".owner",std::string(run_magic)+recovery.filename().u8string()+"\n");fs::create_directories(recovery/"user/database-drafts");write(recovery/"user/database-drafts/owner.json","{\"format\":99}");
 auto previous=run;run.folder=recovery;run.root=root;run.original=fs::current_path();run.keep=false;
 if(!take_lease(recovery))throw std::runtime_error("Cannot lease recovery fixture");remove_run();
 if(!run.keep||!fs::exists(recovery/"user/database-drafts/owner.json"))throw std::runtime_error("Active cleanup deleted unknown recovery");++checks;
 if(read(recovery/"user/database-drafts/owner.json")!="{\"format\":99}")throw std::runtime_error("Unknown recovery changed during retention");++checks;
 if(!owned(recovery,root)||!take_lease(recovery))throw std::runtime_error("Recovery fixture cleanup refused");delete_payload(recovery);release_lease();delete_metadata(recovery);run=previous;
 std::printf("PowerSurge storage checks: %d passed\n",checks);return 0;
}
}
#include "owned_test.inc"
#include "owned_check.inc"
#include "project_session.inc"
#include "project_storage_check.inc"
#include "project_check.inc"
#include "render_record_check.inc"
extern "C" bool ps_lab_active(){return run.active;}
extern "C" bool ps_lab_automatic(){return run.active&&(render_check||project_check||owned_check||switch_check||(run.automatic&&!run.completed));}
extern "C" const char* ps_lab_snapshot_source(){return run.active?run.snapshot_source.c_str():"";}
extern "C" const char* ps_lab_preferences(){return run.active?run.preferences.c_str():nullptr;}
extern "C" int ps_lab_exit_code(){return run.code;}
extern "C" int ps_lab_prepare(int* argc,char*** argv){
 try{
  arguments.clear();pointers.clear();
  run.exe=executable();std::string selected,project_path;bool list=false,check=false,project_storage=false;
  arguments.push_back((*argv)[0]);
  for(int i=1;i<*argc;++i){std::string a=(*argv)[i];
   if(a=="--lab"){if(++i==*argc)throw std::runtime_error("--lab requires an identity");selected=(*argv)[i];}
   else if(a=="--project"){if(++i==*argc)throw std::runtime_error("--project requires a project directory or manifest");project_path=(*argv)[i];}
   else if(a=="--project-storage-check")project_storage=true;
   else if(a=="--lab-list")list=true;
   else if(a=="--lab-storage-check")check=true;
   else if(a=="--lab-switch-check")switch_check=true;
   else if(a=="--keep-lab")run.keep=true;
   else if(a=="--lab-frames"){if(++i==*argc)throw std::runtime_error("--lab-frames requires a count");run.frame_limit=std::stoi((*argv)[i]);if(run.frame_limit<1||run.frame_limit>36000)throw std::runtime_error("Lab frame limit must be1..36000");}
   else arguments.push_back(a);
  }
  // A preview package declares its entry explicitly; external projects retain their own startup.
  auto preview=run.exe.parent_path()/"powersurge-preview.ini";
  if(project_path.empty()&&!project_storage&&selected.empty()&&!list&&!check&&arguments.size()==1&&fs::is_regular_file(preview)){
   std::istringstream config(read(preview,1024));std::string line;
   while(std::getline(config,line))if(line.rfind("startup=",0)==0||line.rfind("default_lab=",0)==0){selected=line.substr(line.find('=')+1);if(!selected.empty()&&selected.back()=='\r')selected.pop_back();}
   if(selected.empty())throw std::runtime_error("Preview configuration has no startup");
  }
  if(project_storage){if(!selected.empty()||!project_path.empty()||arguments.size()!=1)throw std::runtime_error("Project storage check accepts no other launch options");run.code=project_storage_check();return -1;}
  if(!project_path.empty()){if(!selected.empty()||list||check||arguments.size()!=1||run.keep||run.frame_limit)throw std::runtime_error("Project launch accepts only --project");project_start(fs::u8path(project_path));*argc=int(arguments.size());*argv=pointers.data();return 1;}
  if(list){for(auto& p:profiles)if(!p.frontend&&std::string(p.id)!="database-check-play")std::printf("%s : %s [%s]\n",p.id,p.name,p.automatic?"automatic":"interactive");return -1;}
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
  auto seed=seed_override.empty()?run.exe.parent_path()/"labs/content":seed_override;
  if(!fs::exists(seed/"lab-content.marker"))throw std::runtime_error("Lab fixture pack is missing beside the executable");
  std::random_device random;
  for(int i=0;i<10;++i){run.folder=run.root/("run-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(random()));if(fs::create_directory(run.folder))break;if(i==9)throw std::runtime_error("Cannot create unique lab run");}
  if(!take_lease(run.folder))throw std::runtime_error("Cannot lock lab run");
  write(run.folder/".owner",std::string(run_magic)+run.folder.filename().u8string()+"\n");
  if(run.keep)write(run.folder/".keep","Explicit retention requested\n");
  run.snapshot_source=seed_override.empty()?std::string{}:fs::canonical(seed).u8string();
  copy_content(seed,run.folder/"content");seed_override.clear();fs::create_directory(run.folder/"user");
  if(selected=="render-record-check")render_check_begin();
  if(selected=="project-session-check")project_check_begin();
  if(selected=="test-session-check"){owned_check=true;++owned_check_epoch;}
  configure_run(*profile,std::move(entry_override));entry_override.clear();*argc=int(arguments.size());*argv=pointers.data();
  std::printf("PowerSurge session: %s; temporary run %s\n",run.id.c_str(),run.folder.u8string().c_str());return 1;
 }catch(const std::exception& e){run.error=e.what();run.code=1;std::fprintf(stderr,"PowerSurge lab: %s\n",e.what());try{if(!run.folder.empty())remove_run();}catch(...){}return -1;}
}
extern "C" void ps_lab_tick(){
 project_tick();owned_tick();if(render_check){render_check_tick();return;}if(project_check){project_check_tick();return;}if(owned_check){owned_check_tick();return;}
 if(run.active&&switch_check)switch_check_tick();
 if(!run.active||run.completed)return;++run.frames;
 if(run.id=="sky-check"){
  if(run.frames==30){char error[1024]="";int checks=ps_sky_check(run.folder.u8string().c_str(),error,sizeof error);run.code=checks<0?1:0;run.error=error;run.result=ps::authoring::Json{{"passed",checks>0},{"checks",checks},{"actual_gpu_readback",true},{"registered_sky",true},{"horizon_ratio",.01}}.dump();std::printf("Sky checks: %d; %s\n",checks,error);if(run.code)run.completed=true;}
  if(run.frames==36)ps::ui::native_capture((run.folder/"sky-course.png").u8string().c_str());
  if(run.frames>36&&!run.completed&&fs::exists(run.folder/"sky-course.png")&&!ps_hub_open()){auto root=(run.folder/"content").u8string();PsHubRequest request{1,root.c_str(),"sky:study","levels/sky-study.lev","",""};PsHubReply reply{};if(ps_hub_request(&request,&reply)!=PS_HUB_OPENED){run.code=1;run.error=reply.message;run.completed=true;}}
  if(ps_hub_open()&&!run.completed){int ready=ps_hub_native_sky_ready();if(ready<0){run.code=1;run.error="Sky definition form reported an ImGui error";run.completed=true;}else if(ready>0&&!fs::exists(run.folder/"sky-definitions.png")){ps::ui::native_capture((run.folder/"sky-definitions.png").u8string().c_str());auto result=ps::authoring::parse(run.result);result["sky_form_rendered"]=true;run.result=result.dump();}}
  if(fs::exists(run.folder/"sky-definitions.png"))run.completed=true;
  if(std::chrono::steady_clock::now()-run.started>std::chrono::seconds(60)){run.code=1;run.error="Sky native capture deadline exceeded";run.completed=true;}

  if(run.completed){if(!run.embedded)engine_quit();else bsl_shell_note("test",run.code?run.error.c_str():"Registered sky and GPU projection checks passed",run.code?2:0);}return;
 }
 if(run.id=="environment-check"){
  int phase=-1;if(run.frames==30)phase=0;if(run.frames==38)phase=1;if(run.frames==50)phase=2;if(run.frames==60)phase=3;
  if(phase>=0){char error[1024]="";int checks=ps::environment::native_check(phase,error,sizeof error);run.code=checks<0?1:0;run.error=error;run.result=ps::authoring::Json{{"passed",checks>0&&phase==3},{"checks",checks},{"shared_clocks",checks>0},{"camera_independent",true},{"new_visuals",false}}.dump();if(run.code||phase==3)run.completed=true;std::printf("Environment checks phase %d: %d; %s\n",phase,checks,error);}
  if(run.completed){bsl_host_calendar_test_policy(0);if(!run.embedded)engine_quit();else bsl_shell_note("test",run.code?run.error.c_str():"Shared environment, weather and clock checks passed",run.code?2:0);}return;
 }
 if(run.id=="line-check"){
  if(run.frames==30){char error[1024]="";int checks=ps_terrain_line_check(error,sizeof error);run.code=checks<0?1:0;run.error=error;run.result=ps::authoring::Json{{"passed",checks>0},{"checks",checks},{"large_course",checks>0},{"input_recovery",checks>0},{"synthetic_fixture",true}}.dump();std::printf("Terrain line checks: %d; %s\n",checks,error);run.completed=true;}
  if(run.completed){if(!run.embedded)engine_quit();else bsl_shell_note("test",run.code?run.error.c_str():"Terrain line and input-recovery checks passed",run.code?2:0);}return;
 }
 if(run.id=="bevel-check"){
  if(run.frames==30){char error[1024]="";int checks=ps_terrain_bevel_check(error,sizeof error);run.code=checks<0?1:0;run.error=error;run.result=ps::authoring::Json{{"passed",checks>0},{"checks",checks},{"collision_unchanged",checks>0},{"contour_bevel_gpu",checks>0}}.dump();std::printf("Bevel checks: %d; %s\n",checks,error);if(run.code)run.completed=true;else ps::ui::native_capture((run.folder/"bevel-course.png").u8string().c_str());}
  if(run.frames>=35&&run.frames<=62&&(run.frames-35)%3==0){int phase=(run.frames-35)/3;if(ps_editor_bevel_ui_check(phase)<0){run.code=1;run.error="Bevel material-picker UI failed phase "+std::to_string(phase);run.completed=true;}else{auto result=ps::authoring::parse(run.result);result["ui_checks"]=phase+1;run.result=result.dump();}if(phase==7)ps::ui::native_capture((run.folder/"material-picker.png").u8string().c_str());}
  if(run.frames==66)run.completed=true;if(run.completed){if(!run.embedded)engine_quit();else bsl_shell_note("test",run.code?run.error.c_str():"Bevel and material-picker checks passed",run.code?2:0);}return;
 }
 if(run.id=="material-catalog-check"){
  if(run.frames==30){char error[1024]="";int checks=ps_material_catalog_check(error,sizeof error);run.code=checks<0?1:0;run.error=error;run.result=ps::authoring::Json{{"passed",checks>0},{"checks",checks},{"source_images",1107},{"native_opaque_fills",803},{"inspection_alpha",300},{"inspection_unsupported",4}}.dump();std::printf("Material catalogue checks: %d; %s\n",checks,error);if(run.code)run.completed=true;else{bsl_cell_editor_open();}}
  if(run.frames==35)bsl_shell_submit("tool",1);if(run.frames==50)ps::ui::native_capture((run.folder/"material-import.png").u8string().c_str());
  if(run.frames==60)run.completed=true;if(run.completed){if(!run.embedded)engine_quit();else bsl_shell_note("test",run.code?run.error.c_str():"Imported texture catalogue checks passed",run.code?2:0);}return;
 }
 if(run.id=="terrain-lighting-check"){
  if(run.frames==30){char error[1024]="";int checks=ps_terrain_lighting_check(error,sizeof error);run.code=checks<0?1:0;run.error=error;run.result=ps::authoring::Json{{"passed",checks>0},{"checks",checks},{"actual_terrain_gpu",true},{"collision_unchanged",checks>0}}.dump();if(run.code)run.completed=true;else ps::ui::native_capture((run.folder/"terrain-lighting.png").u8string().c_str());std::printf("Terrain lighting checks: %d; %s\n",checks,error);}
  if(run.frames==38)run.completed=true;
  if(run.completed){if(!run.embedded)engine_quit();else bsl_shell_note("test",run.code?run.error.c_str():"Real terrain lighting checks passed",run.code?2:0);}return;
 }
 if(run.id=="lighting-check"){
  static unsigned long long epoch=0;static int count=0;if(epoch!=generation){epoch=generation;count=0;}
  if(run.frames>=12&&run.frames<=48&&run.frames%4==0){if(ps_lighting_study_ui_check((run.frames-12)/4)<0){run.error="Lighting study input/material selection failed at phase "+std::to_string((run.frames-12)/4);run.code=1;run.completed=true;}else ++count;}
  if(run.frames==56&&!run.completed){char error[1024]="";int result=ps_lighting_study_check(error,sizeof error);if(result<0){run.code=1;run.error=error;run.completed=true;}else count+=result;}
  if(run.frames==8&&!run.completed)ps::ui::native_capture((run.folder/"lighting-rounded.png").u8string().c_str());
  if(run.frames==60&&!run.completed)ps::ui::native_capture((run.folder/"lighting-study.png").u8string().c_str());
  if(run.frames==66)run.completed=true;
  if(run.completed){run.result=ps::authoring::Json{{"passed",run.code==0},{"checks",count},{"actual_gpu_readback",true},{"real_material_selector",true},{"world_occlusion",false}}.dump();std::printf("Lighting GPU / UI checks: %d; %s\n",count,run.error.c_str());if(!run.embedded)engine_quit();else bsl_shell_note("test",run.code?run.error.c_str():"Lighting GPU and material-selection checks passed",run.code?2:0);}return;
 }

 if(run.id=="database-check"||run.id=="database-check-play"){database_check_tick();return;}
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
namespace {
int finish_current(){
 if(!run.active)return 0;if(run.project){remove_run();run.active=false;return run.code;}if(run.automatic&&!run.completed)run.code=2;
 if(has_authoring_recovery(run.folder))run.keep=true;
 try{report();}catch(const std::exception& e){std::fprintf(stderr,"Lab report: %s\n",e.what());run.code=1;}
 try{remove_run();}catch(const std::exception& e){release_lease();std::fprintf(stderr,"Lab cleanup: %s\n",e.what());run.code=1;}
 run.active=false;return run.code;
}
}
extern "C" int ps_lab_finish(){int code=finish_current();return project_check_finish(owned_finish(code));}
extern "C" unsigned long long ps_lab_generation(){return generation;}
extern "C" bool ps_lab_spawn(const char* id){
 if(!id||!pending_lab.empty()||owned_busy()||project_pending()){bsl_shell_note("test","Stop the owned test before selecting another laboratory.",1);return false;}
 if(std::none_of(std::begin(profiles),std::end(profiles),[&](auto& p){return !p.frontend&&std::string(p.id)==id;}))return false;
 if(ps_hub_has_drafts()){bsl_shell_note("project","Save or discard database drafts before replacing the source with a laboratory.",1);return false;}
 pending_lab=id;ps_hub_suspend_for_transition();engine_restart(nullptr);return true;
}
extern "C" bool ps_lab_apply_switch(commandline_t* cmd){
 if(project_next)return project_apply(cmd);
 if(owned_pending())return owned_apply(cmd);
 if(pending_lab.empty())return true;
 auto next=std::move(pending_lab);pending_lab.clear();
 auto exe=run.exe.u8string();auto source=std::move(pending_seed);pending_seed.clear();auto entry=std::move(pending_entry);pending_entry.clear();if(!ps_hub_release_project()){bsl_shell_note("project","Unsaved drafts prevented laboratory replacement.",2);return false;}ps_lab_finish();
 if(!bsl_host_begin_lab_run()){run.code=1;std::fprintf(stderr,"Previous gameplay bindings survived teardown; laboratory refused.\n");return false;}
 run=Run{};seed_override=std::move(source);entry_override=std::move(entry);
 char option[]="--lab";char* av[]={exe.data(),option,next.data()};char** args=av;int argc=3;
 if(ps_lab_prepare(&argc,&args)<0)return false;
 if(!seed_cleanup.empty()){
  auto old=std::move(seed_cleanup);seed_cleanup.clear();auto current=run.lease;bool acquired=false;
  try{if(has_authoring_recovery(old))throw std::runtime_error("Authoring recovery retained in snapshot source");if(!owned(old,run.root))throw std::runtime_error("Snapshot source cleanup ownership failed");if(!take_lease(old))throw std::runtime_error("Snapshot source still active");acquired=true;delete_payload(old);release_lease();acquired=false;delete_metadata(old);}
  catch(const std::exception& e){if(acquired)release_lease();std::fprintf(stderr,"Snapshot source retained: %s\n",e.what());}
  run.lease=current;
 }
 *cmd=commandline_parse(argc,args);run.embedded=true;return true;
}

extern "C" void ps_lab_retain_authoring(const char* reason){
 if(!run.active||run.project)return;run.keep=true;write(run.folder/".keep",std::string(reason?reason:"Authoring workspace")+"\n");
}
extern "C" bool ps_lab_test_project(const char* folder){
 try{
  if(!run.automatic){PsLabTestRequest request{1,PS_TEST_LAUNCH,0,generation,folder,true};return ps_lab_test_request(&request,nullptr)==PS_TEST_ACCEPTED;}
  if(!run.active||!folder||!pending_lab.empty())return false;auto source=fs::canonical(fs::u8path(folder));
  if(link(source)||!fs::is_regular_file(source/"database/project.json")||!fs::is_regular_file(source/"lab-content.marker"))return false;
  std::string entry=bsl_engine_level_file();if(entry.empty()||entry.find("presentation/")!=std::string::npos)entry="levels/cell-terrain.lev";
  if(!fs::is_regular_file(source/entry))return false;
  if(source==fs::canonical(run.folder/"content")){if(run.automatic&&!run.keep)seed_cleanup=run.folder;ps_lab_retain_authoring("Authoring source retained for snapshot test");}
  pending_entry=entry;pending_seed=source;if(ps_lab_spawn(run.automatic?"database-check-play":"database"))return true;pending_seed.clear();pending_entry.clear();return false;
 }catch(const std::exception& e){bsl_shell_note("database",e.what(),2);return false;}
}
extern "C" void ps_lab_mark_saved(){
 if(!run.active||run.automatic)return;
 try{
  if(run.project){project_saved();return;}
  if(owned_test.source){if(content_version(run.folder/"content")!=owned_test.files){run.keep=true;write(run.folder/".keep","Explicitly saved test-only content retained; not merged into source\n");bsl_shell_note("workspace","Saved test-only content retained separately; Stop does not merge it into the source.",0);}return;}

  auto seed=seed_override.empty()?run.exe.parent_path()/"labs/content":seed_override;
  for(const auto* name:{"levels","terrain","database"}){
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
  if((run.frames<45&&!run.completed)||bsl_host_boot_busy()||!pending_lab.empty())return;
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
   previous=generation;phase++;check(ps_lab_spawn("test-session-check"),"Owned lifecycle switch refused");
  }else if(phase==5&&run.id=="test-session-check"&&run.completed){
   check(generation>previous,"Owned lifecycle reused prior context");check(run.code==0&&!owned_busy(),"Owned lifecycle did not release its source/test session");previous=generation;phase++;check(ps_lab_spawn("terrain-journey"),"Journey switch refused");
  }else if(phase==6&&run.id=="terrain-journey"&&run.completed){
   check(generation>previous,"Journey reused prior context");check(run.code==0,"First integrated journey failed");
   previous=generation;phase++;check(ps_lab_spawn("terrain-journey"),"Repeated journey switch refused");
  }else if(phase==7&&run.id=="terrain-journey"&&run.completed){
   check(generation>previous,"Repeated journey reused prior context");check(run.code==0,"Repeated journey failed");
   auto native=run.result;run.result="{\"passed\":true,\"session_switches\":7,\"checks\":"+std::to_string(checks)+",\"last_journey\":"+native+"}";
   switch_check=false;phase++;std::printf("Same-instance checks passed: %d; seven laboratory transitions, including owned-test return and repeated automatic journey.\n",checks);engine_quit();
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

namespace {
void database_check_tick(){
 static int phase=0,checks=0,health=0,quantity=0;static bool embedded=false,keep_evidence=false;static double calendar_before=0;
 auto check=[&](bool okay,const char* why){if(!okay)throw std::runtime_error(why);checks++;};
 if(run.frames<45||run.frames%15||bsl_host_boot_busy())return;
 try{
  if(std::chrono::steady_clock::now()-run.started>std::chrono::seconds(60))throw std::runtime_error("Database round trip timed out");
  if(run.id=="database-check"&&phase==0){
   checks=0;embedded=run.embedded;keep_evidence=run.keep;char error[512]="";int n=ps_database_document_check((run.folder/"content").u8string().c_str(),run.folder.u8string().c_str(),error,sizeof error);check(n>0,error);checks+=n;ps_hub_toggle();check(ps_hub_native_select("level:cell-lab"),"Level authoring surface did not select its record");phase++;
  }else if(run.id=="database-check"&&phase==1){
   check(ps_hub_open(),"Database surface did not open");ps::ui::native_capture((run.folder/"database-hub.png").u8string().c_str());phase=101;
  }else if(run.id=="database-check"&&phase==101){if(!fs::is_regular_file(run.folder/"database-hub.png"))return;char error[512]="";int n=ps_hub_native_navigation_check(error,sizeof error);check(n>0,error);checks+=n;check(ps_hub_native_select("switch:door_open"),"Switch surface did not select its record");phase=102;
  }else if(run.id=="database-check"&&phase==102){ps::ui::native_capture((run.folder/"database-switch.png").u8string().c_str());phase=2;
  }else if(run.id=="database-check"&&phase==2){
   if(!fs::is_regular_file(run.folder/"database-switch.png"))return;
   using namespace ps::authoring;Workspace draft(run.folder/"content",run.folder/"user/runtime-edit");auto item=draft.open("item:strawberry"),effect=draft.open("effect:strawberry_heal");auto j=parse(item->source);j["name"]="Native authored supply";draft.edit(*item,j.dump(2));draft.save(*item);j=parse(effect->source);j["amount"]=17;draft.edit(*effect,j.dump(2));draft.save(*effect);
   auto field=draft.open("field:wind_lab");j=parse(field->source);j["name"]="Authored left wind";j["strength"]=240.5;j["direction"]=-1;draft.edit(*field,j.dump(2));draft.save(*field);auto* active_field=bsl::database_snapshot().field("field:wind_lab");check(active_field&&active_field->strength==120&&active_field->direction==1,"Field edit changed active runtime snapshot");
   auto dialogue=draft.open("dialogue:training_choice");j=parse(dialogue->source);j["text"]="Native-authored message.\nChoose the clock experiment.";auto rows=dialogue_rows(j);size_t at=0;for(;at<rows.size();++at)if(rows[at].outcome=="event:calendar_hour")break;check(at<rows.size(),"Fixture lacks calendar outcome");dialogue_move(j,at,0);rows=dialogue_rows(j);rows[0].label="Advance authored hour";rows[0].information="Authored branch advances only the calendar.";dialogue_rows(j,rows);draft.edit(*dialogue,j.dump(2));draft.save(*dialogue);
   auto flag=draft.open("switch:door_open"),counter=draft.open("variable:training_choices");j=parse(flag->source);j["name"]="Authored annex access";j["initial"]=true;draft.edit(*flag,j.dump(2));draft.save(*flag);j=parse(counter->source);j["name"]="Authored choice count";j["initial"]=7;j["minimum"]=-10;j["maximum"]=2000000;draft.edit(*counter,j.dump(2));draft.save(*counter);auto event=draft.open("event:training_reward");j=parse(event->source);j["name"]="Authored conditional reward";auto commands=event_rows(j);commands[2]["value"]=3;event_rows(j,commands);event_insert(j,0,{{"op","wait_ticks"},{"ticks",3}});event_insert(j,3,{{"op","if_switch"},{"target","switch:door_open"},{"equals",true},{"skip",1}});draft.edit(*event,j.dump(2));draft.save(*event);check(bsl::database_snapshot().event("event:training_reward")->commands.size()==3,"Edited event changed active sequence snapshot");check(!bsl_host_progress_flag("switch:door_open")&&bsl_host_progress_variable("variable:training_choices")==0,"Authoring edit mutated live scoped state");check(fs::is_regular_file(run.folder/"database-switch.png"),"Switch form render capture missing");
   check(bsl::database_snapshot().dialogue("dialogue:training_choice")->text!="Native-authored message.\nChoose the clock experiment.","Saved dialogue replaced an active snapshot");
   char name[256]="";check(ps_database_runtime_probe(name,sizeof name)==5,"Saved definitions changed an active session snapshot");check(std::string(name)!="Native authored supply","Active inventory changed without a new test");check(fs::is_regular_file(run.folder/"database-hub.png"),"Database render capture missing");ps_hub_native_close();phase++;run.result="{\"passed\":true,\"phase\":\"saved\",\"checks\":"+std::to_string(checks)+"}";run.completed=true;check(ps_lab_test_project((run.folder/"content").u8string().c_str()),"Staged same-instance test was refused");
  }else if(run.id=="database-check-play"&&phase==3){
   char name[256]="";check(ps_database_runtime_probe(name,sizeof name)==17&&std::string(name)=="Native authored supply","Saved item/effect did not reach new runtime snapshot");check(bsl_host_progress_flag("switch:door_open")&&bsl_host_progress_variable("variable:training_choices")==7,"Saved defaults did not reach new runtime state");auto* definition=bsl::database_snapshot().variable("variable:training_choices");check(definition&&definition->minimum==-10&&definition->maximum==2000000,"Authored widened bounds not loaded");check(ps_ui_pilot_inventory()==0,"Cannot open actual runtime inventory");BslGameplayProbe game{};bsl_host_gameplay_probe(&game);health=game.health;auto* field=bsl::database_snapshot().field("field:wind_lab");check(field&&field->strength==240.5&&field->direction==-1,"Saved field did not reach fresh runtime snapshot");check(game.player[0],"Runtime field participant missing");bsl::WindHelper helper("database/field-test",*field);check(helper.enter(game.player)==std::vector<bsl::LoopChange>{bsl::LoopChange::start},"Runtime field did not start");check(helper.enter(game.player).empty(),"Runtime field duplicate entry restarted loop");auto requests=helper.requests();check(requests.size()==1&&requests[0].participant==game.player&&requests[0].acceleration_x==-240.5,"Authored field request did not reach runtime service");check(helper.clear()==std::vector<bsl::LoopChange>{bsl::LoopChange::stop}&&helper.requests().empty(),"Runtime field test retained occupants");phase++;
  }else if(run.id=="database-check-play"&&phase==4){
   int i=ps::ui::native_menu_item("item:strawberry",&quantity);if(i<0)return;check(quantity>0,"Runtime item is missing");check(bsl_host_use_item("item:strawberry")==0,"Authored recovery item cannot be consumed");BslGameplayProbe game{};bsl_host_gameplay_probe(&game);check(game.health==health+17,"Runtime did not apply authored17HP recovery");phase++;
  }else if(run.id=="database-check-play"&&phase==5){
   int left=0;check(ps::ui::native_menu_item("item:strawberry",&left)>=0&&left==quantity-1,"Runtime did not consume exactly one authored item");check(ps_ui_menu_request(PS_MENU_RESUME,0),"Runtime Resume intention refused");phase++;
  }else if(run.id=="database-check-play"&&phase==6){
   check(!ps_ui_menu_active()&&!bsl_host_captures_commands(),"Database test retained pause/input ownership");check(ps_ui_pilot_open("dialogue:training_choice")==0,"Cannot open authored conversation");phase++;
  }else if(run.id=="database-check-play"&&phase==7){
   auto* d=bsl::database_snapshot().dialogue("dialogue:training_choice");check(d&&d->text=="Native-authored message.\nChoose the clock experiment.","Authored message missing from runtime");check(d->choices[0]=="Advance authored hour"&&d->choice_help[0]=="Authored branch advances only the calendar."&&d->outcomes[0]=="event:calendar_hour","Choice metadata/order lost in runtime");PsUiProbe ui{};ps_ui_probe(&ui);check(ui.open&&ui.choices==8&&ui.selected==0,"Authored dialogue did not reach game widgets");BslCalendarProbe clock{};bsl_host_calendar_probe(&clock);check(clock.enabled,"Calendar outcome service missing");calendar_before=clock.elapsed;ps::ui::native_capture((run.folder/"database-dialogue.png").u8string().c_str());if(keep_evidence)ps_lab_retain_authoring("Explicit native rendered evidence");check(ps_ui_request(ui.owner,ui.epoch,PS_UI_CONFIRM,0),"Authored choice intent refused");phase++;
  }else if(run.id=="database-check-play"&&phase==8){
   BslConversationProbe talk{};bsl_host_conversation_probe(&talk);check(!talk.open&&talk.choice==0,"Authored choice was not delivered");BslCalendarProbe clock{};bsl_host_calendar_probe(&clock);auto rules=compiled_calendar();check(rules.has_value(),"Calendar definition missing");double skipped=3600.0*rules->denominator/rules->numerator;check(clock.elapsed-calendar_before>=skipped&&clock.elapsed-calendar_before<skipped+10,"Authored outcome did not advance one configured game hour");check(ps_ui_pilot_open("dialogue:training_choice")==0,"Cannot reopen for cancellation");phase++;
  }else if(run.id=="database-check-play"&&phase==9){
   PsUiProbe ui{};ps_ui_probe(&ui);BslCalendarProbe clock{};bsl_host_calendar_probe(&clock);calendar_before=clock.elapsed;check(ui.open&&ps_ui_request(ui.owner,ui.epoch,PS_UI_CANCEL,0),"Cancellation intent refused");phase++;
  }else if(run.id=="database-check-play"&&phase==10){
   BslConversationProbe talk{};bsl_host_conversation_probe(&talk);BslCalendarProbe clock{};bsl_host_calendar_probe(&clock);check(!talk.open&&!bsl_host_captures_commands(),"Cancellation retained dialogue input ownership");check(clock.elapsed-calendar_before<10,"Cancellation incorrectly dispatched an outcome");check(ps_ui_pilot_open("dialogue:choice_list")==0,"Cannot open scoped-state event demonstration");phase++;
  }else if(run.id=="database-check-play"&&phase==11){
   PsUiProbe ui{};ps_ui_probe(&ui);check(ui.open&&ui.choices==64,"State demonstration choices missing");check(bsl_host_progress_variable("variable:training_choices")==7&&!bsl_host_progress_flag("switch:training_reward"),"State demonstration did not start from authored defaults");check(ps_ui_request(ui.owner,ui.epoch,PS_UI_SELECT,63),"State event selection refused");phase++;
  }else if(run.id=="database-check-play"&&phase==12){PsUiProbe ui{};ps_ui_probe(&ui);check(ui.selected==63&&ps_ui_request(ui.owner,ui.epoch,PS_UI_CONFIRM,63),"State event confirm refused");phase++;
  }else if(run.id=="database-check-play"&&phase==13){
   check(bsl_host_progress_variable("variable:training_choices")==10&&bsl_host_progress_flag("switch:training_reward"),"Authored event did not apply its wait/condition/increment sequence");check(ps::authoring::parse(ps::authoring::read_file(run.folder/"content/database/records/variable-training_choices.json"))["initial"]==7,"Gameplay event wrote into authoritative definitions");check(!bsl_host_captures_commands(),"State demonstration retained input ownership");run.result="{\"passed\":true,\"checks\":"+std::to_string(checks)+",\"runtime_recovery\":17,\"dialogue_outcome\":\"event:calendar_hour\",\"authored_initial\":7,\"runtime_value\":10,\"field_acceleration\":-240.5,\"field_actor_coupling\":false,\"same_instance_snapshot\":true}";run.completed=true;run.code=0;phase=0;std::printf("Database documents and runtime checks passed: %d\n",checks);if(!embedded)engine_quit();else bsl_shell_note("lab","Database documents and runtime round trip passed.",0);
  }else if(run.id=="database-check-play"&&phase==0)throw std::runtime_error("Continuation requires an initial database-check run");
 }catch(const std::exception& e){run.error=e.what();run.code=1;run.completed=true;run.result="{\"passed\":false,\"checks\":"+std::to_string(checks)+"}";phase=0;std::fprintf(stderr,"Database checks: %s\n",e.what());if(!embedded)engine_quit();else bsl_shell_note("lab",e.what(),2);}
}
}
