#include "documents.hpp"
#include "field_definition.hpp"
#include "render_records.hpp"
#include "world_records.hpp"
#include "level_creation.hpp"
#include "../cell_terrain/materials.hpp"
#include "../cell_terrain/material_images.hpp"
#include "scoped_state.hpp"
#include "event_catalog.hpp"
#include "../foundation/gameplay_records.hpp"
#include "../foundation/learning_records.hpp"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif
namespace ps::authoring {
namespace {
void require(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
bool link(const fs::path& p){
#ifdef _WIN32
 auto a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT);
#else
 return fs::is_symlink(fs::symlink_status(p));
#endif
}
void relative(const std::string& s){fs::path p=fs::u8path(s);require(!s.empty()&&s.size()<256&&!p.is_absolute()&&s.find('\\')==s.npos&&s.find(':')==s.npos,"Unsafe record path");for(auto& part:p)require(part!=".."&&part!="."&&!part.empty(),"Unsafe path component");require(s.rfind("records/",0)==0&&p.extension()==".json","Records must be JSON below records/");}
std::string text(const Json& j,const char* key,const std::string& fallback="",bool optional=false){if(optional&&!j.contains(key))return fallback;require(j.contains(key)&&j[key].is_string(),std::string(key)+": text required");auto s=j[key].get<std::string>();require(s.size()<=65536&&s.find('\0')==s.npos,std::string(key)+": invalid text");return s;}
int integer(const Json& j,const char* key,int fallback=0,bool optional=false){if(optional&&!j.contains(key))return fallback;require(j.contains(key)&&j[key].is_number_integer(),std::string(key)+": integer required");if(j[key].is_number_unsigned()){auto n=j[key].get<uint64_t>();require(n<=INT32_MAX,"Integer out of range");return int(n);}auto n=j[key].get<int64_t>();require(n>=INT32_MIN&&n<=INT32_MAX,"Integer out of range");return int(n);}
std::vector<std::string> strings(const Json& j,const char* key,bool optional=false){if(optional&&!j.contains(key))return {};require(j.contains(key)&&j[key].is_array(),std::string(key)+": list required");std::vector<std::string> v;for(auto& x:j[key]){require(x.is_string(),"Text list entry required");auto value=x.get<std::string>();require(value.size()<=65536&&value.find('\0')==value.npos,"Invalid text list entry");v.push_back(std::move(value));}return v;}
std::string key(const std::string& s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return std::to_string(h);}
struct Lock {
#ifdef _WIN32
 HANDLE h=INVALID_HANDLE_VALUE;explicit Lock(const fs::path& root){h=CreateFileW((root/".authoring-write.lock").c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);require(h!=INVALID_HANDLE_VALUE,"Another editor is saving this project; draft preserved");}~Lock(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
#else
 int h=-1;explicit Lock(const fs::path& root){h=::open((root/".authoring-write.lock").c_str(),O_CREAT|O_RDWR,0600);if(h<0)throw std::runtime_error("Cannot lock project");if(flock(h,LOCK_EX|LOCK_NB)){::close(h);h=-1;throw std::runtime_error("Another editor is saving");}}~Lock(){if(h>=0){flock(h,LOCK_UN);::close(h);}}
#endif
};
}
Json parse(const std::string& bytes){
 require(bytes.size()<=8*1024*1024,"JSON document exceeds 8 MiB");std::vector<std::set<std::string>> keys;
 auto callback=[&](int depth,Json::parse_event_t event,Json& value){require(depth<=32,"JSON nesting limit");if(event==Json::parse_event_t::object_start)keys.emplace_back();else if(event==Json::parse_event_t::object_end)keys.pop_back();else if(event==Json::parse_event_t::key)require(keys.back().insert(value.get<std::string>()).second,"Duplicate JSON key");return true;};
 return Json::parse(bytes,callback);
}
std::string read_file(const fs::path& p){require(!link(p),"Linked document refused: "+p.filename().u8string());std::error_code ec;auto n=fs::file_size(p,ec);require(!ec&&n<=8*1024*1024,"Missing or oversized document: "+p.filename().u8string());std::ifstream f(p,std::ios::binary);require(bool(f),"Cannot open document");std::string s{std::istreambuf_iterator<char>(f),{}};require(s.size()==n,"Document changed while reading");return s;}
void atomic_file(const fs::path& target,const std::string& bytes){
 require(!link(target)&&!link(target.parent_path()),"Linked save target refused");require(bytes.size()<=8*1024*1024,"Save exceeds document limit");static std::atomic<unsigned> serial{0};auto temp=target;temp+="."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial)+".pending";
 bool created=false;try{
#ifdef _WIN32
  HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);require(f!=INVALID_HANDLE_VALUE,"Cannot create temporary save; original intact");created=true;DWORD written=0;bool okay=WriteFile(f,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(f);CloseHandle(f);require(okay,"Temporary save write failed; original intact");
#else
  int f=::open(temp.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);require(f>=0,"Cannot create temporary save");created=true;size_t at=0;while(at<bytes.size()){auto n=::write(f,bytes.data()+at,bytes.size()-at);if(n<=0){::close(f);throw std::runtime_error("Temporary save write failed");}at+=n;}bool okay=fsync(f)==0;::close(f);require(okay,"Temporary save flush failed");
#endif
  require(read_file(temp)==bytes,"Temporary save verification failed; original intact");
#ifdef _WIN32
  require(MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Atomic replacement failed; original intact");
#else
  fs::rename(temp,target);
#endif
 }catch(...){if(created){std::error_code ec;fs::remove(temp,ec);}throw;}
}
std::vector<Json> read_records(const Read& read){
 auto project=parse(read("project.json"));require(integer(project,"format")==1,"Unsupported database format; files preserved");text(project,"project_id");require(project.contains("records")&&project["records"].is_array()&&project["records"].size()<=4096,"Invalid record index");std::set<std::string> ids,paths;std::vector<Json> records;size_t total=0;
 for(auto& entry:project["records"]){auto id=text(entry,"id"),file=text(entry,"file");relative(file);require(ids.insert(id).second&&paths.insert(file).second,"Duplicate registry identity/path");auto raw=read(file);require(raw.size()<=1024*1024,"Record exceeds 1 MiB");total+=raw.size();require(total<=64*1024*1024,"Database exceeds 64 MiB");auto j=parse(raw);require(text(j,"id")==id,"Record identity differs from index: "+id);require(integer(j,"version")==1,"Unsupported record version: "+id);text(j,"name");records.push_back(std::move(j));}
 return records;
}
bsl::Catalog compile(const std::vector<Json>& records){
 std::vector<bsl::ItemDef> items;std::vector<bsl::EffectDef> effects;std::vector<bsl::DialogueDef> dialogues;std::vector<bsl::FieldDef> fields;std::vector<bsl::SwitchDef> flags;std::vector<bsl::VariableDef> vars;std::vector<bsl::EventDef> events;

 compile_world(records);compile_render(records);
 auto learning=compiled_learning_catalog();std::set<std::string> abilities;for(auto& a:learning.abilities)abilities.insert(a.id);
 for(auto& j:records){auto id=text(j,"id"),kind=text(j,"kind");try{
  if(kind=="item"){auto storage=text(j,"storage");require(storage=="stack"||storage=="unique","Unknown storage");items.push_back({id,text(j,"name"),storage=="stack"?bsl::Storage::stack:bsl::Storage::unique,integer(j,"max_stack"),text(j,"slot","",true),text(j,"effect","",true),text(j,"category","misc",true),text(j,"description","",true)});}
  else if(kind=="effect")effects.push_back({id,integer(j,"amount")});
  else if(kind=="dialogue")dialogues.push_back({id,text(j,"text"),strings(j,"choices"),strings(j,"outcomes",true),strings(j,"choice_help",true)});
  else if(kind=="field")fields.push_back(field_definition(j));
  else if(kind=="switch"||kind=="variable"){auto scope=text(j,"scope");require(scope=="session"||scope=="player","Unknown scope");auto s=scope=="session"?bsl::Scope::session:bsl::Scope::player;if(kind=="switch"){require(j.contains("initial")&&j["initial"].is_boolean(),"Boolean initial value required");flags.push_back({id,s,j["initial"].get<bool>()});}else vars.push_back({id,s,integer(j,"initial"),integer(j,"minimum"),integer(j,"maximum")});}
  else if(kind=="event"){require(j.contains("commands")&&j["commands"].is_array(),"Event commands required");bsl::EventDef event{id,{}};for(auto& c:j["commands"]){command_shape(c);event_external_references(c);auto op=text(c,"op");auto* operation=command_spec(op);require(operation,"Unrecognized event command: "+op);auto target=text(c,"target","",true);if(op=="grant_skill"||op=="revoke_skill")require(abilities.count(target),"Missing registered ability: "+target);int value=0;if(op=="set_switch"||op=="if_switch"){const char* field=op=="if_switch"?"equals":"value";require(c.contains(field)&&c[field].is_boolean(),"Boolean event value required");value=c[field].get<bool>()?1:0;}else if(op=="wait_ticks")value=integer(c,"ticks");else if(op=="grant_item"||op=="remove_item"||op=="buy_shop_item"||op=="sell_shop_item")value=integer(c,"quantity");else value=integer(c,"value",0,true);event.commands.push_back({operation->native,target,value,integer(c,"skip",0,true),text(c,"receipt","",true),text(c,"offer","",true),integer(c,"x",0,true),integer(c,"y",0,true)});}events.push_back(std::move(event));}
  else if(kind=="level"||kind=="sequence"||render_kind(kind)){}
  else throw std::runtime_error("Unsupported record kind: "+kind);
 }catch(const std::exception& e){throw std::runtime_error(id+": "+e.what());}}
 bsl::Catalog catalog(std::move(items),std::move(effects),std::move(dialogues),std::move(fields),std::move(flags),std::move(vars),std::move(events));
 // Validate authored starting inventory against the same definitions before saving.
 auto profile=compiled_gameplay_profile();bsl::Inventory inventory(catalog);unsigned index=0;for(auto& item:profile.items)require(inventory.execute({"validate/"+std::to_string(index++),profile.player,bsl::Op::grant,item.definition,item.instance,"",item.quantity}).code==bsl::Code::ok,"Starting inventory conflicts with "+item.definition);
 return catalog;
}
void Workspace::owns(const Document& d)const{auto it=documents_.find(d.id);require(it!=documents_.end()&&it->second.get()==&d,"Document belongs to a different workspace");auto entry=std::find_if(entries_.begin(),entries_.end(),[&](const Entry& e){return e.id==d.id;});require(entry!=entries_.end()&&entry->file==d.file,"Document mapping changed");}
fs::path Workspace::source_path(const std::string& name)const{relative(name);require(!link(root_)&&!link(root_/"database")&&fs::canonical(root_)==root_,"Project/database directory was replaced");auto p=root_/"database";for(auto& part:fs::u8path(name)){p/=part;require(!link(p),"Linked record path refused");}auto resolved=fs::weakly_canonical(p);auto rel=resolved.lexically_relative(root_/"database");require(!rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..","Record escaped project root");return p;}
fs::path Workspace::draft_path(const std::string& id)const{return cache_/(key(id)+".draft.json");}
Workspace::Workspace(fs::path root,fs::path cache):root_(fs::canonical(root)),cache_(fs::absolute(cache)){
 require(fs::is_directory(root_)&&!link(root_/"database")&&!link(cache_),"Invalid project/cache root");fs::create_directories(cache_);auto marker=cache_/"owner.json";Json identity={{"format",1},{"root",root_.u8string()}};
 if(fs::exists(marker))require(parse(read_file(marker))==identity,"Recovery cache belongs to another project; preserved");else atomic_file(marker,identity.dump(2));
 project_baseline_=read_file(root_/"database/project.json");auto project=parse(project_baseline_);auto records=read_records([&](const std::string& p){return p=="project.json"?project_baseline_:read_file(source_path(p));});compile(records);
 size_t i=0;for(auto& e:project["records"]){auto& j=records.at(i++);entries_.push_back({text(e,"id"),text(e,"file"),text(j,"kind"),text(j,"name"),text(j,"kind")=="item"?text(j,"storage"):std::string{}});}
}
std::shared_ptr<Document> Workspace::open(const std::string& id){auto old=documents_.find(id);if(old!=documents_.end())return old->second;auto e=std::find_if(entries_.begin(),entries_.end(),[&](const Entry& e){return e.id==id;});require(e!=entries_.end(),"Unknown record");auto d=std::make_shared<Document>(id,e->file);d->baseline=d->source=read_file(source_path(e->file));d->recovery=fs::exists(draft_path(id));documents_[id]=d;return d;}
void Workspace::edit(Document& d,const std::string& source,const std::string& group){owns(d);require(source.size()<=1024*1024,"Draft exceeds limit");if(source==d.source)return;if(group.empty()||group!=d.merge){d.undo.push_back(d.source);if(d.undo.size()>128)d.undo.erase(d.undo.begin());}d.merge=group;d.redo.clear();d.source=source;d.error.clear();}
void Workspace::finish_edits(){for(auto& p:documents_)p.second->merge.clear();}
bool Workspace::undo(Document& d){owns(d);if(d.undo.empty())return false;d.redo.push_back(d.source);d.source=d.undo.back();d.undo.pop_back();d.merge.clear();d.error.clear();return true;}
bool Workspace::redo(Document& d){owns(d);if(d.redo.empty())return false;d.undo.push_back(d.source);d.source=d.redo.back();d.redo.pop_back();d.merge.clear();d.error.clear();return true;}
std::vector<Json> Workspace::candidate(const Document* replacement,std::map<std::string,std::string>* inputs)const{return read_records([&](const std::string& name){auto raw=read_file(name=="project.json"?root_/"database/project.json":source_path(name));if(name=="project.json")require(raw==project_baseline_,"Database index changed externally; reopen project before saving");if(inputs)(*inputs)[name]=raw;if(replacement&&name==replacement->file)return replacement->source;return raw;});}
void Workspace::validate(Document& d)const{owns(d);
 auto j=parse(d.source),baseline=parse(d.baseline);require(text(j,"id")==d.id&&text(j,"kind")==text(baseline,"kind")&&integer(j,"version")==1,"Stable identity/kind/version cannot change in this surface");
 if(text(j,"kind")=="switch"||text(j,"kind")=="variable")scoped_edit(baseline,j);
 if(text(j,"kind")=="level")require(j.at("scene")==baseline.at("scene")&&j.at("properties").at("terrain")==baseline.at("properties").at("terrain"),"Scene/terrain changes require a coordinated level migration; draft preserved");
 if(text(j,"kind")=="terrain_material_library"){require(j.value("palette",false)==baseline.value("palette",false),"Shared palette membership requires coordinated migration");require(j.at("legacy_path")==baseline.at("legacy_path"),"Changing a material-library attachment requires coordinated migration");auto a=baseline.at("materials"),b=j.at("materials");require(a.size()==b.size(),"Material creation/removal needs a reference migration");for(size_t i=0;i<a.size();++i)require(a[i].at("id")==b[i].at("id"),"Stable material identity/order cannot change in this surface");}
 if(text(j,"kind")=="sequence")require(j.at("file")==baseline.at("file"),"Sequence route changes require a coordinated migration; draft preserved");
 if(text(j,"kind")=="item")require(j.at("storage")==baseline.at("storage")&&j.at("slot")==baseline.at("slot"),"Structural storage/slot changes require a template migration; draft preserved");auto records=candidate(&d);compile(records);
 if(render_kind(text(j,"kind"))){auto registry=compile_render(records);std::set<std::string> assets;for(auto& entry:registry.libraries)for(auto& m:entry.second.materials){assets.insert(m["texture"].get<std::string>());if(m.contains("maps"))for(auto& map:m["maps"])if(map.is_string())assets.insert(map.get<std::string>());}for(auto& shader:registry.shaders)if(!shader.second.source.empty())assets.insert(shader.second.source);
  for(auto& asset:assets){auto at=root_;for(auto& part:fs::u8path(asset)){at/=part;require(!link(at),"Linked rendering asset refused");}require(fs::is_regular_file(at),"Missing rendering asset: "+asset);}
  for(auto& entry:registry.libraries)for(auto color:cell::decode_materials(entry.second.legacy)){
   auto path=root_/fs::u8path(color.texture);auto identity=std::make_pair(fs::file_size(path),fs::last_write_time(path));auto key=path.u8string();auto old=validated_color_assets_.find(key);if(old!=validated_color_assets_.end()&&old->second==identity)continue;
   color.tint={255,255,255};cell::load_material_images({color},[&](const std::string& name){auto* previous=al_get_new_file_interface();al_set_standard_file_interface();auto* file=al_fopen((root_/fs::u8path(name)).u8string().c_str(),"rb");al_set_new_file_interface(previous);return file;});require(identity==std::make_pair(fs::file_size(path),fs::last_write_time(path)),"Rendering asset changed during validation");validated_color_assets_[key]=identity;
  }
 }
}
void Workspace::save(Document& d){
 try{owns(d);source_path(d.file);Lock lock(root_/"database");require(!creation_pending(),"Resolve interrupted level creation before saving records");require(read_file(source_path(d.file))==d.baseline,"External file conflict; save refused, draft preserved");validate(d);if(!d.dirty())return;std::map<std::string,std::string> inputs;compile(candidate(&d,&inputs));
  auto recovery=draft_path(d.id);if(fs::exists(recovery)){auto v=parse(read_file(recovery));require(v.value("format",0)==1&&v.value("root","")==root_.u8string()&&v.value("id","")==d.id,"Unknown recovery data preserved; save refused");}
  auto previous=cache_/(key(d.id)+".previous.json");atomic_file(previous,d.baseline);require(read_file(previous)==d.baseline,"Previous-good backup verification failed");
  for(auto& input:inputs)require(read_file(input.first=="project.json"?root_/"database/project.json":source_path(input.first))==input.second,"Validation input changed during save; draft preserved: "+input.first);require(read_file(source_path(d.file))==d.baseline,"File changed during validation; draft preserved");atomic_file(source_path(d.file),d.source);d.baseline=d.source;d.external=false;d.checkpoint.clear();
  for(auto& e:entries_)if(e.id==d.id)e.name=text(parse(d.source),"name");
  std::error_code cleanup;if(fs::exists(recovery))fs::remove(recovery,cleanup);d.recovery=bool(cleanup);d.error=cleanup?"Saved; old draft recovery could not be removed":"";
 }catch(const std::exception& e){d.error=e.what();throw;}
}
#include "level_creation.inc"
void Workspace::save_all(){unsigned saved=0;for(auto& p:documents_)if(p.second->dirty()){try{save(*p.second);saved++;}catch(const std::exception& e){throw std::runtime_error("Saved "+std::to_string(saved)+" documents; "+p.first+": "+e.what());}}}
void Workspace::poll(){bool index_changed=read_file(root_/"database/project.json")!=project_baseline_;for(auto& p:documents_)p.second->external=index_changed||read_file(source_path(p.second->file))!=p.second->baseline;}
void Workspace::reload(Document& d,bool discard){owns(d);require(discard||!d.dirty(),"Reload would discard edits; choose Discard explicitly");auto bytes=read_file(source_path(d.file));require(text(parse(bytes),"id")==d.id,"Reloaded identity changed; existing draft preserved");d.baseline=d.source=bytes;d.external=false;d.error.clear();d.undo.clear();d.redo.clear();d.merge.clear();d.checkpoint.clear();if(discard){auto file=draft_path(d.id);if(fs::exists(file)){auto j=parse(read_file(file));if(j.value("format",0)==1&&j.value("root","")==root_.u8string()&&j.value("id","")==d.id)fs::remove(file);}d.recovery=fs::exists(file);}}
void Workspace::checkpoint(){for(auto& p:documents_){auto& d=*p.second;if(!d.dirty()||d.source==d.checkpoint)continue;auto file=draft_path(d.id);if(fs::exists(file)){auto old=parse(read_file(file));require(old.value("format",0)==1&&old.value("root","")==root_.u8string()&&old.value("id","")==d.id,"Unknown recovery data preserved; checkpoint refused");}Json saved={{"format",1},{"root",root_.u8string()},{"id",d.id},{"baseline",d.baseline},{"source",d.source}};atomic_file(file,saved.dump());d.checkpoint=d.source;d.recovery=true;}}
void Workspace::recover(Document& d){owns(d);auto j=parse(read_file(draft_path(d.id)));require(integer(j,"format")==1&&text(j,"root")==root_.u8string()&&text(j,"id")==d.id,"Recovery identity/version mismatch");require(j.contains("source")&&j["source"].is_string()&&j.contains("baseline")&&j["baseline"].is_string(),"Invalid recovery payload");auto source=j["source"].get<std::string>(),baseline=j["baseline"].get<std::string>();require(source.size()<=1024*1024&&baseline.size()<=1024*1024,"Recovery record exceeds limit");require(text(parse(baseline),"id")==d.id,"Invalid recovery baseline");edit(d,source);d.external=read_file(source_path(d.file))!=baseline;d.baseline=baseline;d.error=d.external?"Recovered draft conflicts with disk; source was not overwritten":"";}
bool Workspace::dirty()const{for(auto& p:documents_)if(p.second->dirty())return true;return false;}
std::vector<std::string> Workspace::usages(const std::string& id)const{std::vector<std::string> result;auto records=candidate(nullptr);std::string attachment;for(auto& j:records)if(j.value("id","")==id&&j.value("kind","")=="terrain_material_library")attachment=j["legacy_path"].get<std::string>();for(auto& j:records){bool found=false;std::function<void(const Json&)> visit=[&](const Json& v){if(v.is_string())found|=v.get<std::string>()==id||(!attachment.empty()&&render_fold(v.get<std::string>())==render_fold(attachment));else if(v.is_array()||v.is_object())for(auto& x:v)visit(x);};for(auto it=j.begin();it!=j.end();++it)if(it.key()!="id")visit(it.value());if(found)result.push_back(text(j,"id"));}return result;}
}
