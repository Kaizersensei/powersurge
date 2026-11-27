// Owned source/test lifecycle; main thread only. Signed: President.
namespace {
using ContentVersion=std::map<std::string,std::string>;
struct OwnedTest {
 std::optional<Run> source,next;int transition=0;unsigned reloads=0;uint64_t id=0;
 bool return_editor=false,resume_ui=false;std::string project,entry,test_entry,message,last_report,version;ContentVersion files;
} owned_test;
constexpr int source_edit_action=4;
int owned_finish(int code);
bool owned_check=false;uint64_t owned_check_epoch=0;
uint64_t owned_serial=0;
void owned_check_tick();
bool owned_pending(){return owned_test.transition!=0;}
bool owned_busy(){return owned_pending()||owned_test.source.has_value();}
Run take_run(Run& from){Run value=std::move(from);
#ifdef _WIN32
 from.lease=INVALID_HANDLE_VALUE;
#else
 from.lease=-1;
#endif
 from.active=false;return value;
}
ContentVersion content_version(const fs::path& root){
 if(link(root)||!fs::is_directory(root))throw std::runtime_error("Snapshot source is missing or linked");
 ContentVersion files;uint64_t total=0;
 for(auto& e:fs::recursive_directory_iterator(root)){
  if(link(e.path()))throw std::runtime_error("Linked snapshot input refused");if(!e.is_regular_file())continue;
  auto rel=e.path().lexically_relative(root).generic_u8string();if(rel=="logfile.txt")continue;
  if(e.path().extension()==".pending"||e.path().filename()==".authoring-write.lock")throw std::runtime_error("Finish the source save/recovery before testing");
  auto size=e.file_size();total+=size;if(files.size()>=20000||size>536870912ULL||total>2147483648ULL)throw std::runtime_error("Snapshot exceeds file/size limits");
  std::ifstream in(e.path(),std::ios::binary);if(!in)throw std::runtime_error("Cannot read snapshot input: "+rel);uint64_t hash=14695981039346656037ULL,count=0;char buffer[65536];while(in){in.read(buffer,sizeof buffer);auto n=in.gcount();count+=uint64_t(n);for(std::streamsize i=0;i<n;++i){hash^=static_cast<unsigned char>(buffer[i]);hash*=1099511628211ULL;}}
  if(!in.eof()||count!=size)throw std::runtime_error("Snapshot input changed while reading: "+rel);files.emplace(rel,std::to_string(size)+":"+std::to_string(hash));
 }return files;
}
std::string version_id(const ContentVersion& files){uint64_t h=14695981039346656037ULL;for(auto& file:files)for(unsigned char c:file.first+":"+file.second+"\n"){h^=c;h*=1099511628211ULL;}std::ostringstream out;out<<std::hex<<h;return out.str();}
void validate_source(const fs::path& root,const std::string& entry){
 auto relative=fs::u8path(entry);if(relative.empty()||relative.is_absolute()||relative.lexically_normal()!=relative||entry.find('\\')!=entry.npos||*relative.begin()==".."||!fs::is_regular_file(root/relative))throw std::runtime_error("Saved test level is unavailable");
 if(fs::exists(root/"database/.authoring-create.json")||fs::exists(root/"database/.authoring-create.json.create.pending"))throw std::runtime_error("Resolve interrupted level creation before testing");
 auto checked=root;for(auto& part:relative){checked/=part;if(link(checked))throw std::runtime_error("Linked level path refused");}
 if(!fs::is_regular_file(root/"lab-content.marker"))throw std::runtime_error("This first test controller requires an owned lab project");
 ps::authoring::compile(ps::authoring::read_records([&](const std::string& p){return ps::authoring::read_file(root/"database"/p);}));
}
bool owns_lease(const Run& owner){
#ifdef _WIN32
 return owner.lease!=INVALID_HANDLE_VALUE;
#else
 return owner.lease>=0;
#endif
}
void discard_prepared(Run& candidate){
 if(candidate.folder.empty())return;if(!owns_lease(candidate)||!owned(candidate.folder,candidate.root)){release_lease_for(candidate);throw std::runtime_error("Unknown prepared workspace retained");}
 delete_payload(candidate.folder);release_lease_for(candidate);delete_metadata(candidate.folder);candidate.folder.clear();
}
Run prepare_test(const fs::path& source,const std::string& entry,ContentVersion& version){
 validate_source(source,entry);auto before=content_version(source);Run candidate;candidate.root=storage();candidate.original=run.original;candidate.exe=run.exe;candidate.snapshot_source=source.u8string();candidate.id="database";
 try{
  std::random_device random;for(int i=0;i<10;++i){candidate.folder=candidate.root/("run-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(random()));if(fs::create_directory(candidate.folder))break;if(i==9)throw std::runtime_error("Cannot allocate test workspace");}
  if(!take_lease_for(candidate,candidate.folder))throw std::runtime_error("Cannot lease prepared test workspace");write(candidate.folder/".owner",std::string(run_magic)+candidate.folder.filename().u8string()+"\n");
  for(auto& file:before){auto target=candidate.folder/"content"/fs::u8path(file.first);fs::create_directories(target.parent_path());fs::copy_file(source/fs::u8path(file.first),target);}
  if(before!=content_version(source)||before!=content_version(candidate.folder/"content"))throw std::runtime_error("Source changed during snapshot; live session preserved");
  validate_source(candidate.folder/"content",entry);fs::create_directory(candidate.folder/"user");version=std::move(before);return candidate;
 }catch(...){if(owns_lease(candidate)&&!candidate.folder.empty()&&owned(candidate.folder,candidate.root)){try{discard_prepared(candidate);}catch(...){release_lease_for(candidate);}}else release_lease_for(candidate);throw;}
}
const Profile& profile_for(const std::string& id){auto p=std::find_if(std::begin(profiles),std::end(profiles),[&](const Profile& p){return id==p.id;});if(p==std::end(profiles))throw std::runtime_error("Unknown owned source profile");return *p;}
void finish_test(const char* outcome){run.result=ps::authoring::Json{{"passed",true},{"test_session",owned_test.id},{"outcome",outcome}}.dump();run.completed=true;run.code=0;finish_current();owned_test.last_report=last_report_path;}

bool owned_apply(commandline_t* cmd){
 const int action=owned_test.transition;owned_test.transition=0;
 try{
  if(!bsl_host_begin_lab_run())throw std::runtime_error("Gameplay bindings survived teardown; test refused");
  if(action==PS_TEST_LAUNCH)owned_test.source=take_run(run);else if(action!=source_edit_action)finish_test(action==PS_TEST_RELOAD?"reloaded":"stopped");
  if(action==source_edit_action){configure_run(profile_for(run.id),owned_test.entry);owned_test.resume_ui=true;owned_test.message="Opened saved source level for editing. Files and source storage identity are unchanged.";}
  else if(action==PS_TEST_STOP){run=take_run(*owned_test.source);owned_test.source.reset();configure_run(profile_for(run.id),owned_test.entry);owned_test.resume_ui=true;owned_test.message="Test stopped. Source level restarted; test progress was not imported.";}
  else {run=take_run(*owned_test.next);owned_test.next.reset();configure_run(profile_for("database"),owned_test.test_entry);owned_test.message=action==PS_TEST_RELOAD?"Saved source reloaded into a fresh test.":"Owned test running; source workspace remains parked.";}
  *cmd=commandline_parse(int(arguments.size()),pointers.data());if(action!=PS_TEST_STOP&&action!=source_edit_action)run.embedded=true;std::printf("PowerSurge session: %s; temporary run %s\n",run.id.c_str(),run.folder.u8string().c_str());return true;
 }catch(const std::exception& e){
  owned_test.message=std::string("Test transition failed: ")+e.what();bsl_shell_note("test",owned_test.message.c_str(),2);
  if(owned_test.next){try{discard_prepared(*owned_test.next);}catch(...){}owned_test.next.reset();}
  // Keep source files/drafts if a post-teardown host failure cannot be recovered.
  if(owned_test.source){owned_test.source->keep=true;try{write(owned_test.source->folder/".keep","Test host failure: source retained\n");}catch(...){}}
  run.error=owned_test.message;run.completed=true;run.result=ps::authoring::Json{{"passed",false},{"transition",action}}.dump();run.code=1;run.keep=true;if(!run.folder.empty()){run.active=true;try{write(run.folder/".keep","Test transition failed; workspace retained\n");}catch(...){}}finish_current();owned_finish(1);run.code=1;return false;
 }
}
void owned_tick(){if(owned_test.resume_ui&&!bsl_host_boot_busy()&&level_time()>=.05f){if(bsl_shell_present()&&std::string(bsl_engine_level_file())==owned_test.entry){owned_test.resume_ui=false;ps_hub_resume_authoring(owned_test.return_editor);}}}
int owned_finish(int code){
 if(owned_test.next){try{discard_prepared(*owned_test.next);}catch(...){release_lease_for(*owned_test.next);code=1;}owned_test.next.reset();}
 if(owned_test.source){Run ended=take_run(run);run=take_run(*owned_test.source);owned_test.source.reset();int source_code=finish_current();run=std::move(ended);if(source_code)code=source_code;}
 owned_test.transition=0;return code;
}
}
extern "C" void ps_lab_test_info(PsLabTestInfo* info){if(!info)return;*info={};info->active=owned_test.source.has_value();info->pending=owned_pending();info->source_retained=owned_test.source&&owned_test.source->keep;info->session=owned_busy()?owned_test.id:0;info->generation=generation;info->reloads=owned_test.reloads;
 snprintf(info->source,sizeof info->source,"%s",owned_test.project.c_str());snprintf(info->level,sizeof info->level,"%s",owned_test.entry.c_str());snprintf(info->message,sizeof info->message,"%s",owned_test.message.c_str());snprintf(info->report,sizeof info->report,"%s",owned_test.last_report.c_str());snprintf(info->snapshot,sizeof info->snapshot,"%s",owned_test.version.c_str());}
extern "C" bool ps_lab_retain_project(const char* project,const char* reason){try{if(!project)return false;auto path=fs::canonical(fs::u8path(project));Run* owner=nullptr;if(run.active&&fs::equivalent(path,run.folder/"content"))owner=&run;else if(owned_test.source&&fs::equivalent(path,owned_test.source->folder/"content"))owner=&*owned_test.source;if(!owner)return false;if(owner->project)return owns_lease(*owner);owner->keep=true;write(owner->folder/".keep",std::string(reason?reason:"Authored project retained")+"\n");return true;}catch(const std::exception& e){bsl_shell_note("recovery",e.what(),2);return false;}}
static int owned_request(const PsLabTestRequest* request,PsLabTestReply* output,const char* sequence){
 PsLabTestReply local{};auto& reply=output?*output:local;reply={};auto fail=[&](int status,const std::string& why){reply.status=status;snprintf(reply.message,sizeof reply.message,"%s",why.c_str());owned_test.message=why;bsl_shell_note("test",why.c_str(),status==PS_TEST_FAILED?2:1);return status;};
 if(!request||request->version!=1||request->action<PS_TEST_LAUNCH||request->action>PS_TEST_STOP)return fail(PS_TEST_INVALID,"Unsupported test request");
 if(!run.active||(!owned_check&&!project_check&&!render_check&&run.automatic))return fail(PS_TEST_UNAVAILABLE,"Owned tests require an interactive lab workspace");
 if(owned_pending()||project_pending()||!pending_lab.empty()||request->generation!=generation)return fail(PS_TEST_INVALID,"Test request is pending or belongs to an older run");
 const bool launch=request->action==PS_TEST_LAUNCH;if((launch&&(owned_test.source||request->session))||(!launch&&(!owned_test.source||request->session!=owned_test.id)))return fail(PS_TEST_INVALID,"Test session identity no longer matches");
 try{
  size_t length=0;if(request->project)while(length<=8192&&request->project[length])++length;if(!request->project||!length||length>8192)return fail(PS_TEST_INVALID,"Invalid test project path");auto root=fs::canonical(fs::u8path(request->project));auto expected=launch?run.folder/"content":owned_test.source->folder/"content";if(!fs::equivalent(root,expected))return fail(PS_TEST_INVALID,"Test project is not owned by this session");
  BslShellState view{};bsl_shell_snapshot(&view);if(launch&&(view.terrain_dirty||view.placements_dirty))return fail(PS_TEST_DIRTY,"Save source terrain and placements before testing");if(request->action!=PS_TEST_STOP&&ps_hub_has_drafts())return fail(PS_TEST_DIRTY,"Save or discard database drafts before testing saved content");
  std::string entry=launch?bsl_engine_level_file():owned_test.entry;if(entry.empty()||entry.find("presentation/")!=entry.npos)return fail(PS_TEST_UNAVAILABLE,"Open a saved gameplay level before testing");
  std::string target=launch?entry:owned_test.test_entry;
  if(sequence){if(!launch)return fail(PS_TEST_INVALID,"Sequence target applies only to launch");size_t n=0;while(n<=128&&sequence[n])++n;if(!n||n>128)return fail(PS_TEST_INVALID,"Invalid sequence identity");auto records=ps::authoring::read_records([&](const std::string& p){return ps::authoring::read_file(root/"database"/p);});auto world=ps::authoring::compile_world(records);auto* definition=world.sequence(sequence);if(!definition)return fail(PS_TEST_UNAVAILABLE,"Sequence is not registered in the saved source");target=definition->file;}
  if(!request->discard){std::string why=launch?"Launch replaces gameplay with disposable test progress. Stop restarts the saved source level.":"Replace this test level and discard test progress?";if(!launch){bool changed=view.terrain_dirty||view.placements_dirty||content_version(run.folder/"content")!=owned_test.files;if(changed)why+=" The test has level/content edits; saved files will be retained separately, never merged.";}return fail(PS_TEST_CONFIRM,why);}
  ContentVersion version;if(request->action!=PS_TEST_STOP){auto prepared=prepare_test(root,target,version);owned_test.next=take_run(prepared);}else {if(owned_test.source->project?!owns_lease(*owned_test.source)||ps::project::load(owned_test.source->folder).json["id"]!=owned_test.source->project->json["id"]:!owned(owned_test.source->folder,owned_test.source->root))throw std::runtime_error("Source ownership changed; Stop refused");validate_source(root,entry);}
  if(!launch&&content_version(run.folder/"content")!=owned_test.files){run.keep=true;write(run.folder/".keep","Saved test-only content edits retained; not merged into source\n");}
  if(launch){owned_test.id=++owned_serial;owned_test.reloads=0;owned_test.return_editor=view.editing;owned_test.project=root.u8string();owned_test.entry=entry;owned_test.test_entry=target;owned_test.last_report.clear();}else if(request->action==PS_TEST_RELOAD)owned_test.reloads++;
  if(request->action!=PS_TEST_STOP){owned_test.files=std::move(version);owned_test.version=version_id(owned_test.files);}owned_test.transition=request->action;owned_test.message="Test transition prepared; current source and progress remain isolated.";reply.status=PS_TEST_ACCEPTED;snprintf(reply.message,sizeof reply.message,"%s",owned_test.message.c_str());ps_hub_suspend_for_transition();engine_restart(nullptr);return reply.status;
 }catch(const std::exception& e){if(owned_test.next){try{discard_prepared(*owned_test.next);}catch(...){}owned_test.next.reset();}return fail(PS_TEST_FAILED,std::string("Test preparation failed; current session preserved. ")+e.what());}
}

extern "C" int ps_lab_test_request(const PsLabTestRequest* r,PsLabTestReply* p){return owned_request(r,p,nullptr);}
extern "C" int ps_lab_test_sequence(const PsLabTestRequest* r,const char* sequence,PsLabTestReply* p){if(!sequence){if(p){*p={};p->status=PS_TEST_INVALID;snprintf(p->message,sizeof p->message,"Sequence identity required");}return PS_TEST_INVALID;}return owned_request(r,p,sequence);}

extern "C" int ps_lab_edit_level(int version,unsigned long long expected,const char* project,const char* id,PsLabTestReply* output){
 PsLabTestReply local{};auto& reply=output?*output:local;reply={};auto fail=[&](int code,const char* why){reply.status=code;snprintf(reply.message,sizeof reply.message,"%s",why);return code;};
 if(version!=1||expected!=generation||!project||!id)return fail(PS_TEST_INVALID,"Invalid or stale source-level request");if(!run.active||(!owned_check&&!project_check&&!render_check&&run.automatic)||owned_busy()||project_pending()||!pending_lab.empty())return fail(PS_TEST_UNAVAILABLE,"Stop the owned test before opening a source level");
 try{size_t n=0;while(n<=8192&&project[n])++n;if(!n||n>8192)return fail(PS_TEST_INVALID,"Invalid source root");n=0;while(n<=128&&id[n])++n;if(!n||n>128)return fail(PS_TEST_INVALID,"Invalid level identity");if(link(run.folder/"content"))return fail(PS_TEST_FAILED,"Linked source directory refused");auto root=fs::canonical(fs::u8path(project));if(!fs::equivalent(root,run.folder/"content"))return fail(PS_TEST_INVALID,"Source-level request belongs to another project");BslShellState view{};bsl_shell_snapshot(&view);if(view.terrain_dirty||view.placements_dirty||ps_hub_has_drafts())return fail(PS_TEST_DIRTY,"Save terrain, placements and database drafts before changing levels");auto records=ps::authoring::read_records([&](const std::string& p){return ps::authoring::read_file(root/"database"/p);});auto world=ps::authoring::compile_world(records);auto* level=world.levels.by_id(id);if(!level||!level->editable)return fail(PS_TEST_UNAVAILABLE,"Level is not registered or not editable");validate_source(root,level->scene);if(!level->terrain.document.empty())for(auto& file:{level->terrain.document,level->terrain.materials}){auto at=root;for(auto& part:fs::u8path(file)){at/=part;if(link(at))return fail(PS_TEST_FAILED,"Linked terrain attachment refused");}if(!fs::is_regular_file(at))return fail(PS_TEST_UNAVAILABLE,"Level terrain attachment is missing");}if(!ps_lab_retain_project(project,"Authoring level opened; source retained"))return fail(PS_TEST_FAILED,"Cannot retain source before opening level");owned_test.entry=level->scene;owned_test.return_editor=true;owned_test.transition=source_edit_action;owned_test.message="Source-level transition prepared.";reply.status=PS_TEST_ACCEPTED;snprintf(reply.message,sizeof reply.message,"%s",owned_test.message.c_str());ps_hub_suspend_for_transition();engine_restart(nullptr);return reply.status;
 }catch(const std::exception& e){return fail(PS_TEST_FAILED,e.what());}
}
