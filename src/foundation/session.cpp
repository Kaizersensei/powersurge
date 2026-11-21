// Original bounded session-data codec/storage. Signed: Atlas.
#include "session.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#endif

namespace bsl { namespace {
constexpr std::size_t max_bytes=8*1024*1024, max_entries=32768;
void require(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
bool id(const std::string& s){return !s.empty()&&s.size()<=256&&s.find('\0')==std::string::npos;}
std::uint64_t decimal(const std::string& s){
 require(!s.empty()&&s.size()<=16&&(s.size()==1||s[0]!='0'),"invalid saved counter");std::uint64_t n=0;
 for(unsigned char c:s){require(c>='0'&&c<='9',"invalid saved counter");n=n*10+c-'0';}
 require(n<=9007199254740991ULL,"saved counter exceeds exact range");return n;
}
std::uint32_t crc(const std::string& bytes){std::uint32_t c=~0u;for(unsigned char b:bytes){c^=b;for(int k=0;k<8;++k)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
struct Writer {
 std::string bytes;
 void u64(std::uint64_t n){for(int i=0;i<8;++i){bytes.push_back(static_cast<char>(n&255));n>>=8;}}
 void integer(int n){u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(n)));}
 void count(std::size_t n){require(n<=max_entries,"save collection too large");u64(n);}
 void text(const std::string& s){require(s.size()<=2*1024*1024,"save string too large");u64(s.size());bytes+=s;require(bytes.size()<=max_bytes,"save exceeds size limit");}
 void real(double n){static_assert(sizeof(double)==8,"64-bit double required");std::uint64_t bits;std::memcpy(&bits,&n,8);u64(bits);}
};
struct Reader {
 const std::string& bytes;std::size_t at=0;
 std::uint64_t u64(){require(bytes.size()-at>=8,"truncated save");std::uint64_t n=0;for(int i=0;i<8;++i)n|=std::uint64_t(static_cast<unsigned char>(bytes[at++]))<<(i*8);return n;}
 int integer(){auto n=u64();if(n<=std::uint64_t(INT32_MAX))return static_cast<int>(n);require(n>=std::uint64_t(INT64_MAX)+1&&~n<=std::uint64_t(INT32_MAX),"invalid saved integer");return -1-static_cast<int>(~n);}
 std::size_t count(){auto n=u64();require(n<=max_entries&&n<=bytes.size()-at,"save collection exceeds bounds");return static_cast<std::size_t>(n);}
 std::string text(){auto n=u64();require(n<=2*1024*1024&&n<=bytes.size()-at,"save string exceeds bounds");std::string s=bytes.substr(at,static_cast<std::size_t>(n));at+=static_cast<std::size_t>(n);return s;}
 double real(){auto n=u64();double value;std::memcpy(&value,&n,8);return value;}
};
template<class Map,class K,class V>void insert(Map& m,K key,V value){require(m.emplace(std::move(key),std::move(value)).second,"duplicate saved key");}
std::string file_bytes(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot read lab data file.");
 auto size=in.tellg();require(size>=0&&size<=static_cast<std::streamoff>(max_bytes+32),"save file exceeds bounds");
 std::string bytes(static_cast<std::size_t>(size),'\0');in.seekg(0);if(!in.read(bytes.data(),size))throw std::runtime_error("Cannot read complete lab data file.");return bytes;
}
void replace(const std::filesystem::path& from,const std::filesystem::path& to){
#ifdef _WIN32
 if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace lab data file.");
#else
 if(std::rename(from.c_str(),to.c_str())!=0)throw std::runtime_error("Cannot replace lab data file.");
#endif
}
void durable_write(const std::filesystem::path& path,const std::string& bytes){
#ifdef _WIN32
 HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create lab data temporary file.");
 DWORD written=0;bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size();if(ok)ok=FlushFileBuffers(file)!=0;
 bool closed=CloseHandle(file)!=0;if(!ok||!closed)throw std::runtime_error("Cannot flush lab data file.");
#else
 std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(bytes.data(),bytes.size());file.flush();if(!file)throw std::runtime_error("Cannot write lab data file.");file.close();if(!file)throw std::runtime_error("Cannot close lab data file.");
#endif
}
std::filesystem::path suffixed(std::filesystem::path path,const char* suffix){path+=suffix;return path;}
struct Lock {
 std::filesystem::path path;
 explicit Lock(const std::filesystem::path& target):path(suffixed(target,".lock")){
  if(!std::filesystem::create_directory(path))throw std::runtime_error("Lab data slot is locked; no changes written.");
 }
 ~Lock(){std::error_code error;std::filesystem::remove(path,error);}
};
}
void SessionDataStore::validate(const Catalog& catalog,const SessionData& s){
 require(id(s.mode)&&id(s.level)&&id(s.content)&&s.participants.size()>=1&&s.participants.size()<=4,"invalid session-data identity");
 require(s.request_id<=9007199254740991ULL&&s.conversation_id<=9007199254740991ULL&&std::isfinite(s.calendar_seconds)&&s.calendar_seconds>=0&&s.calendar_seconds<=1e12,"invalid session-data clocks/counters");
 std::set<std::string> players;
 for(const auto& p:s.participants)require((p.id=="player/1"||p.id=="player/2"||p.id=="player/3"||p.id=="player/4")&&id(p.character)&&p.health>0&&players.insert(p.id).second,"invalid saved participant");
 Inventory inv(catalog);inv.restore(s.inventory);Events events(catalog,inv);events.restore(s.events);
 require(s.inventory.receipts.size()<=max_entries&&s.inventory.issued_instances.size()<=max_entries,"inventory journal exceeds save limit");
 for(const auto& p:s.inventory.bags)require(players.count(p.first),"unknown inventory participant");
 for(const auto& value:s.events.values)require(value.first.second.empty()||players.count(value.first.second),"unknown event-value participant");
 for(const auto& entry:s.events.runs){const auto& r=entry.second;require(players.count(r.context.owner)&&players.count(r.context.player),"unknown event participant");require(!r.context.travel,"travel-bound events cannot be encoded by legacy lab format");require(r.context.character.empty(),"character-bound events require compact progress saves, not the legacy lab format");
  if(r.id.compare(0,9,"dialogue.")==0)require(decimal(r.id.substr(9))<=s.conversation_id,"saved dialogue counter precedes journal");
 }
 for(const auto& entry:s.inventory.receipts){const auto& r=entry.second.request;require(r.context.empty(),"transaction metadata requires compact progress saves, not legacy lab data");
  if(r.transaction.compare(0,3,"ui/")==0)require(decimal(r.transaction.substr(3))<=s.request_id,"saved request counter precedes journal");
  if(r.transaction.compare(0,7,"action/")==0)require(decimal(r.transaction.substr(7))<=s.request_id,"saved action counter precedes journal");
  if(r.transaction.compare(0,6,"event/")!=0)continue;
  auto slash=r.transaction.find('/',6);require(slash!=std::string::npos,"invalid event receipt");auto run=s.events.runs.find(r.transaction.substr(6,slash-6));require(run!=s.events.runs.end(),"event receipt has no journal");
  const auto& event=run->second;auto index=decimal(r.transaction.substr(slash+1));const auto* def=catalog.event(event.definition);require(index<event.next&&index<def->commands.size(),"event receipt is ahead of cursor");const auto& c=def->commands[static_cast<std::size_t>(index)];
  require((c.op==EventOp::grant_item||c.op==EventOp::remove_item)&&r.op==(c.op==EventOp::grant_item?Op::grant:Op::remove)&&r.owner==event.context.player&&r.definition==c.target&&r.quantity==c.value&&r.slot.empty()&&r.instance.empty(),"event receipt disagrees with command");
 }
}
void SessionDataStore::restore_services(const Catalog& c,const SessionData& s,Inventory& inventory,Events& events){
 validate(c,s);require(!inventory.busy_&&inventory.state_.catalog_signature==c.signature()&&events.catalog_.signature()==c.signature()&& &events.inventory_==&inventory,"live service bundle mismatch");
 Inventory next_inventory(c,inventory.enabled_);next_inventory.restore(s.inventory);Events next_events(c,next_inventory);next_events.restore(s.events);
 auto& a=inventory.state_;auto& b=next_inventory.state_;a.catalog_signature.swap(b.catalog_signature);a.bags.swap(b.bags);a.receipts.swap(b.receipts);a.issued_instances.swap(b.issued_instances);
 events.values_.swap(next_events.values_);events.runs_.swap(next_events.runs_);
}
std::string SessionDataStore::encode(const Catalog& c,const SessionData& s){
 validate(c,s);const bool bounded=std::any_of(s.inventory.bags.begin(),s.inventory.bags.end(),[](const auto& b){return b.second.slot_limit!=0;});Writer w;w.text(s.mode);w.text(s.level);w.text(s.content);w.u64(s.request_id);w.u64(s.conversation_id);w.real(s.calendar_seconds);
 w.count(s.participants.size());for(const auto& p:s.participants){w.text(p.id);w.text(p.character);w.integer(p.health);}
 w.text(s.inventory.catalog_signature);w.count(s.inventory.bags.size());
 for(const auto& entry:s.inventory.bags){w.text(entry.first);const auto& b=entry.second;if(bounded)w.integer(b.slot_limit);
  w.count(b.stacks.size());for(const auto& x:b.stacks){w.text(x.first);w.integer(x.second);}
  for(const auto* map:{&b.unique,&b.equipped}){w.count(map->size());for(const auto& x:*map){w.text(x.first);w.text(x.second);}}
  w.count(b.assigned.size());for(const auto& x:b.assigned){w.text(x.first);w.text(x.second.definition);w.text(x.second.instance);}
 }
 w.count(s.inventory.receipts.size());for(const auto& x:s.inventory.receipts){const auto& r=x.second.request;w.text(r.transaction);w.text(r.owner);w.integer(static_cast<int>(r.op));w.text(r.definition);w.text(r.instance);w.text(r.slot);w.integer(r.quantity);}
 w.count(s.inventory.issued_instances.size());for(const auto& x:s.inventory.issued_instances)w.text(x);
 w.text(s.events.catalog_signature);w.count(s.events.values.size());for(const auto& x:s.events.values){w.text(x.first.first);w.text(x.first.second);w.integer(x.second);}
 w.count(s.events.runs.size());for(const auto& x:s.events.runs){const auto& r=x.second;w.text(r.id);w.text(r.definition);w.text(r.context.owner);w.text(r.context.player);w.integer(static_cast<int>(r.status));w.u64(r.next);w.integer(r.wait_ticks);w.integer(static_cast<int>(r.error));}
 require(w.bytes.size()<=max_bytes,"save exceeds size limit");Writer header;header.bytes="BSLDATA\n";header.u64(bounded?2:1);header.u64(w.bytes.size());header.u64(crc(w.bytes));return header.bytes+w.bytes;
}
SessionData SessionDataStore::decode(const Catalog& c,const std::string& bytes){
 require(bytes.size()>=32&&bytes.size()<=max_bytes+32&&bytes.compare(0,8,"BSLDATA\n")==0,"invalid lab-data header");Reader r{bytes};r.at=8;const auto version=r.u64();require(version==1||version==2,"unsupported lab-data version; file left intact");require(r.u64()==bytes.size()-32,"invalid lab-data length");auto checksum=r.u64();require(checksum==crc(bytes.substr(32)),"lab-data checksum mismatch");
 SessionData s;s.mode=r.text();s.level=r.text();s.content=r.text();s.request_id=r.u64();s.conversation_id=r.u64();s.calendar_seconds=r.real();
 auto n=r.count();require(n<=4,"too many participants");for(std::size_t i=0;i<n;++i){ParticipantData p;p.id=r.text();p.character=r.text();p.health=r.integer();s.participants.push_back(std::move(p));}
 s.inventory.catalog_signature=r.text();n=r.count();require(n<=4,"too many inventory owners");for(std::size_t i=0;i<n;++i){auto owner=r.text();Bag b;if(version>=2)b.slot_limit=r.integer();
  auto count=r.count();for(std::size_t k=0;k<count;++k){auto key=r.text();int value=r.integer();insert(b.stacks,key,value);}
  for(auto* map:{&b.unique,&b.equipped}){count=r.count();for(std::size_t k=0;k<count;++k){auto key=r.text();auto value=r.text();insert(*map,key,value);}}
  count=r.count();for(std::size_t k=0;k<count;++k){auto key=r.text();Selection selected;selected.definition=r.text();selected.instance=r.text();insert(b.assigned,key,selected);}insert(s.inventory.bags,owner,std::move(b));
 }
 n=r.count();for(std::size_t i=0;i<n;++i){Request q;q.transaction=r.text();q.owner=r.text();q.op=static_cast<Op>(r.integer());q.definition=r.text();q.instance=r.text();q.slot=r.text();q.quantity=r.integer();insert(s.inventory.receipts,std::make_pair(q.owner,q.transaction),Receipt{q});}
 n=r.count();for(std::size_t i=0;i<n;++i)require(s.inventory.issued_instances.insert(r.text()).second,"duplicate issued item");
 s.events.catalog_signature=r.text();n=r.count();for(std::size_t i=0;i<n;++i){auto key=r.text();auto player=r.text();int value=r.integer();insert(s.events.values,std::make_pair(key,player),value);}
 n=r.count();require(n<=4096,"too many event runs");for(std::size_t i=0;i<n;++i){EventRun event;event.id=r.text();event.definition=r.text();event.context.owner=r.text();event.context.player=r.text();event.status=static_cast<EventStatus>(r.integer());auto cursor=r.u64();require(cursor<=256,"event cursor exceeds bounds");event.next=static_cast<std::size_t>(cursor);event.wait_ticks=r.integer();event.error=static_cast<Code>(r.integer());insert(s.events.runs,event.id,event);}
 require(r.at==bytes.size(),"trailing lab-data bytes");validate(c,s);return s;
}
SessionData SessionDataStore::read(const Catalog& c,const std::filesystem::path& path){return decode(c,file_bytes(path));}
void SessionDataStore::write(const Catalog& c,const std::filesystem::path& path,const SessionData& s){
 auto bytes=encode(c,s);std::filesystem::create_directories(path.parent_path());Lock lock(path);
 const std::filesystem::path temp=suffixed(path,".pending"),backup=suffixed(path,".previous"),backup_temp=suffixed(path,".previous.pending");
 if(std::filesystem::exists(path)){auto previous=file_bytes(path);decode(c,previous);durable_write(backup_temp,previous);replace(backup_temp,backup);}
 durable_write(temp,bytes);replace(temp,path);
}
void SessionDataStore::recover(const Catalog& c,const std::filesystem::path& path){
 Lock lock(path);const std::filesystem::path backup=suffixed(path,".previous"),temp=suffixed(path,".pending");
 auto bytes=file_bytes(backup);decode(c,bytes);
 // Never overwrite an unsupported newer primary during recovery.
 if(std::filesystem::exists(path)){auto current=file_bytes(path);if(current.size()>=16&&current.compare(0,8,"BSLDATA\n")==0){Reader r{current};r.at=8;require(r.u64()<=2,"newer primary preserved; recovery refused");}
  if(current==bytes)return;
  auto original=suffixed(path,".recovery-original");unsigned revision=0;
  while(std::filesystem::exists(original)){require(++revision<=1024,"recovery-original limit reached; files preserved");original=suffixed(path,(".recovery-original."+std::to_string(revision)).c_str());}
  durable_write(original,current);
 }
 durable_write(temp,bytes);replace(temp,path);
}
}
