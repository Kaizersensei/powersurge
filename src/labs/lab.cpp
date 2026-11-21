// Fresh-process laboratories; no build or scripting pipeline. Signed: President.
#include "lab.h"
extern "C" {
#include "../core/engine.h"
}
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
struct Profile {const char* id;const char* name;bool automatic;};
const Profile profiles[]={{"terrain","Terrain / editor laboratory",false},{"terrain-journey","Connected terrain journey",true}};
struct Run {
 bool active=false,keep=false,automatic=false,completed=false;int code=0,frames=0,frame_limit=0;
 std::string id,result,error;fs::path root,folder,original,exe;std::string preferences;
 std::chrono::steady_clock::time_point started;
#ifdef _WIN32
 HANDLE lease=INVALID_HANDLE_VALUE;
#else
 int lease=-1;
#endif
} run;
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
 for(char** p=_environ;p&&*p;++p){std::string s=*p;auto n=s.substr(0,s.find('='));if(n.rfind("BSL_",0)==0||n.rfind("BOR_",0)==0)names.push_back(n);}
#else
 for(char** p=environ;p&&*p;++p){std::string s=*p;auto n=s.substr(0,s.find('='));if(n.rfind("BSL_",0)==0||n.rfind("BOR_",0)==0)names.push_back(n);}
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
extern "C" bool ps_lab_automatic(){return run.active&&run.automatic;}
extern "C" const char* ps_lab_preferences(){return run.active?run.preferences.c_str():nullptr;}
extern "C" int ps_lab_exit_code(){return run.code;}
extern "C" int ps_lab_prepare(int* argc,char*** argv){
 try{
  run.exe=executable();std::string selected;bool list=false,check=false;
  arguments.push_back((*argv)[0]);
  for(int i=1;i<*argc;++i){std::string a=(*argv)[i];
   if(a=="--lab"){if(++i==*argc)throw std::runtime_error("--lab requires an identity");selected=(*argv)[i];}
   else if(a=="--lab-list")list=true;
   else if(a=="--lab-storage-check")check=true;
   else if(a=="--keep-lab")run.keep=true;
   else if(a=="--lab-frames"){if(++i==*argc)throw std::runtime_error("--lab-frames requires a count");run.frame_limit=std::stoi((*argv)[i]);if(run.frame_limit<1||run.frame_limit>36000)throw std::runtime_error("Lab frame limit must be1..36000");}
   else arguments.push_back(a);
  }
  // A preview package declares its entry explicitly; external projects retain their own startup.
  auto preview=run.exe.parent_path()/"powersurge-preview.ini";
  if(selected.empty()&&!list&&!check&&arguments.size()==1&&fs::is_regular_file(preview)){
   std::istringstream config(read(preview,1024));std::string line;
   while(std::getline(config,line))if(line.rfind("default_lab=",0)==0){selected=line.substr(12);if(!selected.empty()&&selected.back()=='\r')selected.pop_back();}
   if(selected.empty())throw std::runtime_error("Preview configuration has no default_lab");
  }
  if(list){for(auto& p:profiles)std::printf("%s : %s [%s]\n",p.id,p.name,p.automatic?"automatic":"interactive");return -1;}
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
  run.id=selected;run.automatic=profile->automatic;run.active=true;run.preferences=(run.folder/"user/editor-ui.ini").u8string();run.started=std::chrono::steady_clock::now();
  if(run.automatic)env("BSL_CELL_LEVEL_JOURNEY",(run.folder/"result.json").u8string());
  fs::current_path(run.folder/"content");arguments.insert(arguments.end(),{"--game-folder",(run.folder/"content").u8string(),"--quest","quests/cell-levels.qst","--windowed","--resolution","1"});
  for(auto& a:arguments)pointers.push_back(a.data());pointers.push_back(nullptr);*argc=int(arguments.size());*argv=pointers.data();
  std::printf("PowerSurge lab: %s; temporary run %s\n",run.id.c_str(),run.folder.u8string().c_str());return 1;
 }catch(const std::exception& e){run.error=e.what();run.code=1;std::fprintf(stderr,"PowerSurge lab: %s\n",e.what());try{if(!run.folder.empty())remove_run();}catch(...){}return -1;}
}
extern "C" void ps_lab_tick(){
 if(!run.active||run.completed)return;++run.frames;
 try{
  if(run.automatic&&fs::exists(run.folder/"result.json")){
   run.result=read(run.folder/"result.json");run.code=run.result.find("\"passed\":true")!=std::string::npos?0:1;run.completed=true;engine_quit();
  }else if(std::chrono::steady_clock::now()-run.started>std::chrono::seconds(60)&&run.automatic){run.error="Native scenario exceeded60 seconds";run.code=1;run.completed=true;engine_quit();}
  else if(run.frame_limit&&run.frames>=run.frame_limit){run.code=run.automatic?1:0;run.completed=true;engine_quit();}
 }catch(const std::exception& e){run.error=e.what();run.code=1;run.completed=true;engine_quit();}
}
extern "C" int ps_lab_finish(){
 if(!run.active)return 0;if(run.automatic&&!run.completed)run.code=2;
 try{report();}catch(const std::exception& e){std::fprintf(stderr,"Lab report: %s\n",e.what());run.code=1;}
 try{remove_run();}catch(const std::exception& e){std::fprintf(stderr,"Lab cleanup: %s\n",e.what());run.code=1;}
 run.active=false;return run.code;
}
extern "C" bool ps_lab_spawn(const char* id){
 try{
  if(std::none_of(std::begin(profiles),std::end(profiles),[&](auto& p){return std::string(p.id)==id;}))return false;
#ifdef _WIN32
  auto exe=executable();std::wstring wid;for(const char* c=id;*c;++c)wid+=wchar_t(*c);
  std::wstring cmd=L"\""+exe.wstring()+L"\" --lab "+wid;STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
  if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP,nullptr,exe.parent_path().c_str(),&si,&pi))return false;
  CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return true;
#else
  std::string exe=executable().string();char* args[]={exe.data(),const_cast<char*>("--lab"),const_cast<char*>(id),nullptr};pid_t pid;return posix_spawn(&pid,exe.c_str(),nullptr,nullptr,args,environ)==0;
#endif
 }catch(...){return false;}
}
