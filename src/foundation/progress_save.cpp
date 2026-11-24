// Original bounded selected-progress codec and atomic native storage. Signed: Atlas.
#include "progress_save.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace bsl {namespace {
constexpr std::size_t limit=512*1024,entries=4096;
void need(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
bool identity(const std::string& s){return !s.empty()&&s.size()<=256&&s.find('\0')==std::string::npos;}
std::uint32_t checksum(const std::string& s){std::uint32_t c=~0u;for(unsigned char ch:s){c^=ch;for(int i=0;i<8;i++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
struct Writer {
 std::string data;
 void number(std::uint64_t n){for(int i=0;i<8;i++){data.push_back(static_cast<char>(n&255));n>>=8;}need(data.size()<=limit,"progress save too large");}
 void integer(std::int64_t n){number(static_cast<std::uint64_t>(n));}
 void count(std::size_t n){need(n<=entries,"progress collection too large");number(n);}
 void text(const std::string& s){need(s.size()<=256&&s.find('\0')==std::string::npos,"invalid progress string");number(s.size());data+=s;need(data.size()<=limit,"progress save too large");}
 void real(double n){std::uint64_t bits;static_assert(sizeof bits==sizeof n,"64-bit double required");std::memcpy(&bits,&n,8);number(bits);}
};
struct Reader {
 const std::string& data;std::size_t at=0;
 std::uint64_t number(){need(at<=data.size()&&data.size()-at>=8,"truncated progress save");std::uint64_t n=0;for(int i=0;i<8;i++)n|=std::uint64_t(static_cast<unsigned char>(data[at++]))<<(8*i);return n;}
 std::int64_t integer(){auto n=number();return n<=INT64_MAX?static_cast<std::int64_t>(n):-1-static_cast<std::int64_t>(~n);}
 int small(){auto n=integer();need(n>=INT32_MIN&&n<=INT32_MAX,"progress integer out of range");return static_cast<int>(n);}
 std::size_t count(){auto n=number();need(n<=entries&&n<=data.size()-at,"invalid progress collection");return static_cast<std::size_t>(n);}
 std::string text(){auto n=number();need(n<=256&&n<=data.size()-at,"invalid progress string length");auto s=data.substr(at,static_cast<std::size_t>(n));at+=static_cast<std::size_t>(n);need(s.find('\0')==std::string::npos,"invalid progress string");return s;}
 double real(){auto n=number();double value;std::memcpy(&value,&n,8);return value;}
};
template<class M,class K,class V>void put(M& m,K key,V value){need(m.emplace(std::move(key),std::move(value)).second,"duplicate progress key");}
std::filesystem::path suffix(std::filesystem::path p,const std::string& s){p+=s;return p;}
std::string bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open progress save.");auto n=f.tellg();need(n>=0&&n<=static_cast<std::streamoff>(limit+32),"progress file exceeds limit");std::string s(static_cast<std::size_t>(n),'\0');f.seekg(0);if(!f.read(s.data(),n))throw std::runtime_error("Cannot read complete progress save.");return s;}
void replace(const std::filesystem::path& a,const std::filesystem::path& b){
#ifdef _WIN32
 if(!MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace progress save.");
#else
 if(std::rename(a.c_str(),b.c_str()))throw std::runtime_error("Cannot replace progress save.");
#endif
}
void write_bytes(const std::filesystem::path& p,const std::string& s){
#ifdef _WIN32
 HANDLE f=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create progress file.");DWORD n=0;bool ok=WriteFile(f,s.data(),static_cast<DWORD>(s.size()),&n,nullptr)&&n==s.size();if(ok)ok=FlushFileBuffers(f)!=0;bool closed=CloseHandle(f)!=0;if(!ok||!closed)throw std::runtime_error("Cannot flush progress file.");
#else
 std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(s.data(),s.size());f.flush();if(!f)throw std::runtime_error("Cannot write progress file.");f.close();if(!f)throw std::runtime_error("Cannot close progress file.");
#endif
}
struct Lock {
#ifdef _WIN32
 HANDLE handle=INVALID_HANDLE_VALUE;
 explicit Lock(const std::filesystem::path& target){auto p=suffix(target,".lock");handle=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Progress save is locked/unavailable; files unchanged.");}
 ~Lock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
#else
 int handle=-1;
 explicit Lock(const std::filesystem::path& target){auto p=suffix(target,".lock");handle=open(p.c_str(),O_RDWR|O_CREAT,0600);if(handle<0)throw std::runtime_error("Progress lock unavailable.");if(flock(handle,LOCK_EX|LOCK_NB)){close(handle);handle=-1;throw std::runtime_error("Progress save is locked.");}}
 ~Lock(){if(handle>=0){flock(handle,LOCK_UN);close(handle);}}
#endif
 Lock(const Lock&)=delete;Lock& operator=(const Lock&)=delete;
};
}
ProgressSaveStore::ProgressSaveStore(const Catalog& c,GameplayProfile p,StatCatalog s,LearningCatalog l,ProgressSavePolicy policy,std::vector<CheckpointDef> points):catalog_(c),profile_(std::move(p)),stats_(std::move(s)),learning_(std::move(l)),policy_(std::move(policy)),checkpoints_(std::move(points)){
 need(identity(policy_.revision),"invalid progress revision");std::set<std::string> seen;
 for(const auto& id:policy_.globals)need(seen.insert(id).second&&(catalog_.flag(id)||catalog_.variable(id)),"unknown/duplicate saved global");
}
const CheckpointDef& ProgressSaveStore::destination(const std::string& id) const {
 for(const auto& d:checkpoints_)if(d.id==id&&d.kind==CheckpointKind::permanent)return d;
 throw std::invalid_argument("Save destination is not a registered permanent checkpoint.");
}
RewindProgress ProgressSaveStore::defaults() const {
 RewindProgress p;p.mode=profile_.mode;p.save=profile_.save;p.player=profile_.player;p.selected=profile_.initial;
 PlayerRoster roster(profile_);CharacterStats stats(stats_);
 for(const auto& c:profile_.characters){auto id=roster.character(c.id)->id;stats.character(id,c.id);p.stats[id]=stats.capture(id);p.health[c.id]=c.health;}
 CharacterLearning learning(stats,learning_);for(const auto& c:profile_.characters)learning.refresh(roster.character(c.id)->id);p.learning=learning.capture();
 Inventory inv(catalog_);unsigned n=0;for(const auto& item:profile_.items)need(inv.execute({"initial/"+std::to_string(n++),profile_.player,Op::grant,item.definition,item.instance,"",item.quantity}).code==Code::ok,"invalid save default inventory");
 p.inventory=inv.capture();p.inventory.receipts.clear();Events events(catalog_,inv);p.events=events.capture();return p;
}
RewindProgress ProgressSaveStore::expand(const ProgressDocument& d) const {
 need(d.revision==policy_.revision,"Incompatible progress revision; file preserved.");destination(d.checkpoint);const auto& v=d.values;
 need(v.mode==profile_.mode&&v.save==profile_.save&&v.player==profile_.player,"Progress mode/save/player mismatch.");
 need(v.inventory.receipts.empty()&&v.events.runs.empty(),"Runtime journals are not durable progress.");
 need(std::isfinite(v.calendar)&&v.calendar>=0&&v.calendar<=1e12&&(policy_.calendar||v.calendar==0),"Invalid selected calendar value.");
 auto p=defaults();p.selected=v.selected;p.calendar=v.calendar;
 for(const auto& hp:v.health){need(p.health.count(hp.first),"Unknown saved character HP.");p.health.at(hp.first)=hp.second;}
 for(const auto& c:v.stats){auto i=p.stats.find(c.first);need(i!=p.stats.end(),"Unknown saved character stats.");for(const auto& value:c.second){need(i->second.count(value.first),"Unknown saved character stat.");i->second.at(value.first)=value.second;}}
 PlayerRoster roster(profile_);need(roster.restore_progress(p.selected,p.health)==Code::ok,"Invalid saved character selection/HP.");CharacterStats stats(stats_);
 for(const auto& c:profile_.characters){auto id=roster.character(c.id)->id;stats.character(id,c.id);need(stats.restore(id,p.stats.at(id))==Code::ok,"Invalid saved stat values.");}
 CharacterLearning learning(stats,learning_);for(const auto& c:profile_.characters)learning.refresh(roster.character(c.id)->id);p.learning=learning.capture();
 for(const auto& c:v.learning.revoked){need(p.learning.revoked.count(c.first),"Unknown revoked character.");for(const auto& ability:c.second){p.learning.revoked.at(c.first).insert(ability);p.learning.earned.at(c.first).erase(ability);}}
 for(const auto& c:v.learning.earned){need(p.learning.earned.count(c.first),"Unknown earned character.");for(const auto& ability:c.second){need(!p.learning.revoked.at(c.first).count(ability),"Ability is both earned and revoked.");p.learning.earned.at(c.first).insert(ability);}}
 for(const auto& r:v.learning.receipts){const auto* source=learning.source(r.second.source);need(source&&source->model.empty(),"Transient defeat receipt is not durable progress.");}
 p.learning.receipts=v.learning.receipts;need(learning.restore(p.learning)==Code::ok,"Invalid saved learning state.");
 need(v.inventory.catalog_signature==catalog_.signature(),"Inventory definition revision mismatch.");
 for(const auto& bag:v.inventory.bags)need(bag.first==profile_.player,"Unknown saved bag owner.");
 Inventory inv(catalog_);inv.restore(v.inventory);p.inventory=v.inventory;
 need(v.events.catalog_signature==catalog_.signature(),"Event definition revision mismatch.");p.events.values.clear();
 for(const auto& value:v.events.values){need(std::find(policy_.globals.begin(),policy_.globals.end(),value.first.first)!=policy_.globals.end(),"Unselected saved global.");need(value.first.second.empty()||value.first.second==profile_.player,"Unknown saved global owner.");p.events.values.insert(value);}
 Events events(catalog_,inv);events.restore(p.events);return p;
}
ProgressDocument ProgressSaveStore::select(const CheckpointRecord& r) const {
 const auto& def=destination(r.id);need(r.kind==CheckpointKind::permanent&&r.level==def.level,"Only permanent activation can be saved.");
 ProgressDocument d;d.revision=policy_.revision;d.checkpoint=r.id;d.values=r.progress;auto& p=d.values;auto base=defaults();
 for(auto i=p.health.begin();i!=p.health.end();)if(base.health.count(i->first)&&base.health.at(i->first)==i->second)i=p.health.erase(i);else ++i;
 for(auto c=p.stats.begin();c!=p.stats.end();){auto initial=base.stats.find(c->first);for(auto i=c->second.begin();i!=c->second.end();)if(initial!=base.stats.end()&&initial->second.count(i->first)&&initial->second.at(i->first).units==i->second.units&&initial->second.at(i->first).xp==i->second.xp)i=c->second.erase(i);else ++i;if(c->second.empty())c=p.stats.erase(c);else ++c;}
 for(auto i=p.learning.earned.begin();i!=p.learning.earned.end();)if(i->second.empty())i=p.learning.earned.erase(i);else ++i;
 for(auto i=p.learning.revoked.begin();i!=p.learning.revoked.end();)if(i->second.empty())i=p.learning.revoked.erase(i);else ++i;
 for(auto i=p.learning.receipts.begin();i!=p.learning.receipts.end();){auto source=std::find_if(learning_.sources.begin(),learning_.sources.end(),[&](const auto& s){return s.id==i->second.source;});if(source!=learning_.sources.end()&&!source->model.empty())i=p.learning.receipts.erase(i);else ++i;}
 p.inventory.receipts.clear();p.events.runs.clear();for(auto i=p.events.values.begin();i!=p.events.values.end();)if(std::find(policy_.globals.begin(),policy_.globals.end(),i->first.first)==policy_.globals.end())i=p.events.values.erase(i);else ++i;
 if(!policy_.calendar)p.calendar=0;
 expand(d);return d;
}
CheckpointRecord ProgressSaveStore::reconstruct(const ProgressDocument& d,double x,double y) const {
 const auto& def=destination(d.checkpoint);need(std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=1e7&&std::abs(y)<=1e7,"Invalid authored resume point.");CheckpointRecord r;r.id=def.id;r.level=def.level;r.kind=CheckpointKind::permanent;r.priority=def.priority;r.x=x;r.y=y;r.progress=expand(d);return r;
}
std::string ProgressSaveStore::encode(const ProgressDocument& d) const {
 expand(d);const bool bounded=std::any_of(d.values.inventory.bags.begin(),d.values.inventory.bags.end(),[](const auto& b){return b.second.slot_limit!=0;});const bool revoked=std::any_of(d.values.learning.revoked.begin(),d.values.learning.revoked.end(),[](const auto& c){return !c.second.empty();});const bool slots=bounded||revoked;Writer w;w.text(d.revision);w.text(d.checkpoint);const auto& p=d.values;for(const auto* s:{&p.mode,&p.save,&p.player,&p.selected})w.text(*s);w.real(p.calendar);
 w.count(p.health.size());for(const auto& c:p.health){w.text(c.first);w.integer(c.second);}
 w.count(p.stats.size());for(const auto& c:p.stats){w.text(c.first);w.count(c.second.size());for(const auto& v:c.second){w.text(v.first);w.integer(v.second.units);w.number(v.second.xp);}}
 w.count(p.learning.earned.size());for(const auto& c:p.learning.earned){w.text(c.first);w.count(c.second.size());for(const auto& a:c.second)w.text(a);}
 w.count(p.learning.receipts.size());for(const auto& r:p.learning.receipts){w.text(r.first);w.text(r.second.character);w.text(r.second.source);}
 if(revoked){w.count(p.learning.revoked.size());for(const auto& c:p.learning.revoked){w.text(c.first);w.count(c.second.size());for(const auto& a:c.second)w.text(a);}}
 w.count(p.inventory.bags.size());for(const auto& b:p.inventory.bags){w.text(b.first);if(slots)w.integer(b.second.slot_limit);w.count(b.second.stacks.size());for(const auto& s:b.second.stacks){w.text(s.first);w.integer(s.second);}for(const auto* map:{&b.second.unique,&b.second.equipped}){w.count(map->size());for(const auto& x:*map){w.text(x.first);w.text(x.second);}}w.count(b.second.assigned.size());for(const auto& x:b.second.assigned){w.text(x.first);w.text(x.second.definition);w.text(x.second.instance);}}
 w.count(p.inventory.issued_instances.size());for(const auto& s:p.inventory.issued_instances)w.text(s);
 w.count(p.events.values.size());for(const auto& v:p.events.values){w.text(v.first.first);w.text(v.first.second);w.integer(v.second);}
 Writer header;header.data="BSLPROG\n";header.number(revoked?3:bounded?2:1);header.number(w.data.size());header.number(checksum(w.data));return header.data+w.data;
}
ProgressDocument ProgressSaveStore::decode(const std::string& s) const {
 need(s.size()>=32&&s.size()<=limit+32&&s.compare(0,8,"BSLPROG\n")==0,"Invalid progress header.");Reader r{s};r.at=8;const auto version=r.number();need(version==1||version==2||version==3,"Unsupported progress version; file preserved.");need(r.number()==s.size()-32,"Invalid progress length.");auto sum=r.number();need(sum==checksum(s.substr(32)),"Progress checksum mismatch.");
 ProgressDocument d;d.revision=r.text();d.checkpoint=r.text();auto& p=d.values;p.mode=r.text();p.save=r.text();p.player=r.text();p.selected=r.text();p.calendar=r.real();
 auto n=r.count();for(std::size_t i=0;i<n;i++){auto key=r.text();auto hp=r.small();put(p.health,key,hp);}
 n=r.count();for(std::size_t i=0;i<n;i++){auto c=r.text();StatValues values;auto count=r.count();for(std::size_t j=0;j<count;j++){auto key=r.text();StatValue v;v.units=r.integer();v.xp=r.number();put(values,key,v);}put(p.stats,c,std::move(values));}
 n=r.count();for(std::size_t i=0;i<n;i++){auto c=r.text();std::set<std::string> abilities;auto count=r.count();for(std::size_t j=0;j<count;j++)need(abilities.insert(r.text()).second,"Duplicate saved ability.");put(p.learning.earned,c,std::move(abilities));}
 n=r.count();for(std::size_t i=0;i<n;i++){auto key=r.text();LearningReceipt value;value.character=r.text();value.source=r.text();put(p.learning.receipts,key,value);}
 if(version>=3){n=r.count();for(std::size_t i=0;i<n;i++){auto c=r.text();std::set<std::string> abilities;auto count=r.count();for(std::size_t j=0;j<count;j++)need(abilities.insert(r.text()).second,"Duplicate revoked ability.");put(p.learning.revoked,c,std::move(abilities));}}
 p.inventory.catalog_signature=catalog_.signature();n=r.count();for(std::size_t i=0;i<n;i++){auto key=r.text();Bag b;if(version>=2)b.slot_limit=r.small();auto count=r.count();for(std::size_t j=0;j<count;j++){auto id=r.text();auto quantity=r.small();put(b.stacks,id,quantity);}for(auto* map:{&b.unique,&b.equipped}){count=r.count();for(std::size_t j=0;j<count;j++){auto a=r.text();auto b=r.text();put(*map,a,b);}}count=r.count();for(std::size_t j=0;j<count;j++){auto id=r.text();Selection value;value.definition=r.text();value.instance=r.text();put(b.assigned,id,value);}put(p.inventory.bags,key,std::move(b));}
 n=r.count();for(std::size_t i=0;i<n;i++)need(p.inventory.issued_instances.insert(r.text()).second,"Duplicate issued item.");p.events.catalog_signature=catalog_.signature();n=r.count();for(std::size_t i=0;i<n;i++){auto a=r.text();auto b=r.text();auto value=r.small();put(p.events.values,std::make_pair(a,b),value);}
 need(r.at==s.size(),"Trailing progress bytes.");expand(d);return d;
}
ProgressDocument ProgressSaveStore::read(const std::filesystem::path& p) const {return decode(bytes(p));}
void ProgressSaveStore::write(const std::filesystem::path& p,const ProgressDocument& d) const {
 auto value=encode(d);if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path());Lock lock(p);
 std::optional<std::string> previous;if(std::filesystem::exists(p)){previous=bytes(p);decode(*previous);}
 auto verify=[&](const std::filesystem::path& stage,const std::string& wanted){write_bytes(stage,wanted);auto written=bytes(stage);need(written==wanted,"Temporary progress slot failed read-back verification; active save unchanged.");decode(written);};
 const auto temp=suffix(p,".pending");verify(temp,value);
 if(previous){const auto backup=suffix(p,".previous.pending");verify(backup,*previous);replace(backup,suffix(p,".previous"));}
 replace(temp,p);
}
void ProgressSaveStore::recover(const std::filesystem::path& p) const {
 Lock lock(p);auto previous=bytes(suffix(p,".previous"));decode(previous);
 if(std::filesystem::exists(p)){auto current=bytes(p);if(current.size()>=16&&current.compare(0,8,"BSLPROG\n")==0){Reader r{current};r.at=8;need(r.number()<=3,"Newer primary preserved; recovery refused.");}if(current==previous)return;auto original=suffix(p,".recovery-original");unsigned n=0;while(std::filesystem::exists(original)){need(++n<=1024,"Recovery copy limit reached.");original=suffix(p,".recovery-original."+std::to_string(n));}write_bytes(original,current);}
 auto temp=suffix(p,".pending");write_bytes(temp,previous);auto written=bytes(temp);need(written==previous,"Recovery temporary slot failed read-back; active save unchanged.");decode(written);replace(temp,p);
}
}
