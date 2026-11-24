// Isolated service compatibility review. No runtime fixture override. Signed: Mech.
#include "../foundation/event_skills.hpp"
#include "../foundation/progress_save.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
using namespace bsl;
namespace fs = std::filesystem;
struct Checks {
 int count=0;
 void require(bool value,const char* label) {
  if(!value)throw std::runtime_error(label);
  ++count;
 }
 template<class F> void refuses(F fn,const char* label) {
  bool refused=false;
  try { fn(); } catch(const std::exception&) { refused=true; }
  require(refused,label);
 }
};
std::string read_bytes(const fs::path& p) {
 std::ifstream in(p,std::ios::binary);
 if(!in)throw std::runtime_error("compatibility file read failed");
 return std::string(std::istreambuf_iterator<char>(in),{});
}
void put_bytes(const fs::path& p,const std::string& bytes) {
 std::ofstream out(p,std::ios::binary|std::ios::trunc);
 out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();
 if(!out)throw std::runtime_error("compatibility fixture write failed");
}
unsigned version(const std::string& bytes) {
 if(bytes.size()<32)throw std::runtime_error("compatibility header missing");
 unsigned n=static_cast<unsigned char>(bytes[8]);
 for(unsigned i=9;i<16;i++)if(bytes[i])throw std::runtime_error("unexpected version width");
 return n;
}
// Explicit old definition snapshot: independent of compiled current catalog headers.
LearningCatalog old_learning() {
 LearningCatalog l;
 AbilityDef cut;cut.id="ability:cut";cut.name="Cut";cut.animation="attack5";
 cut.tag="ability.cut";cut.conditions.push_back({"stat:xp",2});l.abilities.push_back(cut);
 l.profiles.push_back({"character:a",{cut.id},{}});
 l.profiles.push_back({"character:b",{cut.id},{}});
 return l;
}
LearningCatalog new_learning() {
 auto l=old_learning();AbilityDef jump;jump.id="ability:double_jump";jump.name="Jump";
 jump.tag="traversal.double_jump";jump.capability=jump.tag;
 jump.conditions.push_back({"stat:xp",2});l.abilities.push_back(jump);
 for(auto& p:l.profiles)p.abilities.push_back(jump.id);
 AbilityDef other=jump;other.id="ability:unassigned";other.name="Unassigned";
 other.tag="traversal.other";other.capability=other.tag;l.abilities.push_back(other);
 return l;
}
}

extern "C" int ps_ui_compat_check(const char* owned_temp_root,char* error,unsigned capacity) {
 if(error&&capacity)error[0]='\0';
 try {
  Checks c;
  c.require(owned_temp_root&&*owned_temp_root,"owned temporary root required");
  fs::path root=fs::u8path(owned_temp_root);
  c.require(root.is_absolute()&&fs::is_directory(root)&&!fs::is_symlink(root),"caller root must be an existing absolute owned directory");
  // Never reuse or clean a prior run. Caller owns cleanup of its lab tree.
  const auto dir=root/"ui-compat";
  c.require(fs::create_directory(dir),"compatibility subdirectory already exists; fresh lab root required");
  const auto slot=dir/"progress.bsp";
  auto previous=slot;previous+=".previous";
  auto pending=slot;pending+=".pending";
  const std::string A="player:test/character:a",B="player:test/character:b";
  const std::string ability="ability:double_jump",cap="traversal.double_jump";
  GameplayProfile profile{"mode:test","save:test","player:test","character:a",
   {{"character:a","A","model:a",100},{"character:b","B","model:b",100}}, {}};
  StatDef xp;xp.id="stat:xp";xp.name="XP";xp.initial=1;xp.minimum=1;xp.maximum=10;
  StatCatalog statdefs;statdefs.stats.push_back(xp);
  statdefs.profiles.push_back({"character:a",{xp.id},{}});
  statdefs.profiles.push_back({"character:b",{xp.id},{}});
  EventDef grant{"event:grant",{{EventOp::grant_skill,ability}}};
  EventDef revoke{"event:revoke",{{EventOp::revoke_skill,ability}}};
  EventDef delayed{"event:delay",{{EventOp::wait_ticks,"",2},{EventOp::grant_skill,ability}}};
  Catalog catalog({},{},{},{},{},{},{grant,revoke,delayed});
  Inventory inv(catalog);Events events(catalog,inv);
  CharacterStats stats(statdefs);stats.character(A,"character:a");stats.character(B,"character:b");
  CharacterLearning skills(stats,new_learning());skills.refresh(A);skills.refresh(B);
  EventSkills port(skills,{{A,profile.player},{B,"player:other"}});events.bind_skills(port);
  EventContext ca{"actor:test",profile.player,A},cb{"actor:test","player:other",B};
  auto run=[&](const char* id,const char* definition,const EventContext& ctx,EventStatus expected) {
   c.require(events.start(id,definition,ctx).code==Code::ok,"event start failed");events.tick();
   c.require(events.run(id)&&events.run(id)->status==expected,"unexpected event completion status");
  };
  c.require(!skills.permits(A,cap)&&!skills.permits(B,cap),"initial assigned capabilities must be locked");
  c.require(skills.permits(A,"traversal.unassigned"),"unassigned capability fallback changed");
  c.require(skills.grant(A,"ability:unassigned")==Code::denied,"unassigned ability must be denied");
  c.require(skills.grant(A,"ability:missing")==Code::denied,"unknown ability must be denied");
  c.require(events.start("missing","event:grant",{"actor:test",profile.player,""}).code==Code::invalid,"missing character must refuse event");
  run("foreign","event:grant",{"actor:test",profile.player,B},EventStatus::failed);
  c.require(!skills.permits(B,cap),"foreign owner changed character ability");
  run("grant-a","event:grant",ca,EventStatus::completed);
  c.require(skills.permits(A,cap)&&!skills.permits(B,cap),"grant must override threshold only for recipient");
  run("revoke-a","event:revoke",ca,EventStatus::completed);
  c.require(stats.grant(A,{{xp.id,false,1}})==Code::ok,"XP threshold update failed");
  skills.refresh(A);c.require(!skills.permits(A,cap),"refresh must preserve explicit revocation");
  c.require(events.start("cancel","event:delay",cb).code==Code::ok,"delayed event start failed");events.tick();
  c.require(events.cancel("cancel"),"delayed event cancellation failed");
  for(int i=0;i<4;i++)events.tick();
  c.require(!skills.permits(B,cap)&&events.run("cancel")->status==EventStatus::cancelled,"cancelled event mutated recipient");
  EventDef bad{"event:bad",{{EventOp::grant_skill,"ability:missing"}}};
  Catalog invalid({},{},{},{},{},{},{bad});Inventory invalid_inv(invalid);Events invalid_events(invalid,invalid_inv);
  bool diagnostic=false;
  try {invalid_events.bind_skills(port);}catch(const std::invalid_argument& e){std::string m=e.what();diagnostic=m.find("event:bad")!=std::string::npos&&m.find("command 1")!=std::string::npos&&m.find("ability:missing")!=std::string::npos;}
  c.require(diagnostic,"bad ability reference must identify event, command and ability");
  c.require(invalid_events.start("bad","event:bad",ca).code==Code::disabled,"failed binding must leave port unbound");

  CheckpointDef checkpoint{"checkpoint:rest","levels:test",1,CheckpointKind::permanent,20,64};
  ProgressSavePolicy policy{"revision:test",{},false};
  ProgressSaveStore current(catalog,profile,statdefs,new_learning(),policy,{checkpoint});
  ProgressSaveStore legacy(catalog,profile,statdefs,old_learning(),policy,{checkpoint});
  auto record=[&](const LearningState& learning) {
   CheckpointRecord r;r.id=checkpoint.id;r.level=checkpoint.level;r.kind=checkpoint.kind;r.priority=checkpoint.priority;
   auto& p=r.progress;p.mode=profile.mode;p.save=profile.save;p.player=profile.player;p.selected=profile.initial;
   p.health={{"character:a",100},{"character:b",100}};
   p.stats={{A,stats.capture(A)},{B,stats.capture(B)}};
   p.learning=learning;p.inventory=inv.capture();p.inventory.receipts.clear();p.events=events.capture();p.events.runs.clear();return r;
  };
  CharacterLearning oldskills(stats,old_learning());oldskills.refresh(A);oldskills.refresh(B);
  auto oldrecord=record(oldskills.capture());
  const auto old1=legacy.select(oldrecord);const auto v1=legacy.encode(old1);
  c.require(version(v1)==1,"unbounded legacy document must encode as v1");
  auto decoded1=current.reconstruct(current.decode(v1),16,32);
  c.require(decoded1.progress.learning.earned.at(A).count(ability)&&!decoded1.progress.learning.earned.at(B).count(ability),"v1 reconstruction must add eligible capability per character");
  c.require(decoded1.x==16&&decoded1.y==32&&decoded1.level==checkpoint.level,"authored resume destination changed");
  oldrecord.progress.inventory.bags[profile.player].slot_limit=8;
  const auto old2=legacy.select(oldrecord);const auto v2=legacy.encode(old2);
  c.require(version(v2)==2,"bounded legacy document must encode as v2");
  auto decoded2=current.reconstruct(current.decode(v2),0,0);
  c.require(decoded2.progress.inventory.bags.at(profile.player).slot_limit==8&&decoded2.progress.learning.earned.at(A).count(ability),"v2 capacity or additive learning lost");
  current.write(slot,current.decode(v2));c.require(read_bytes(slot)==v2,"first legacy-version publication changed bytes");

  auto removed_record=record(skills.capture());removed_record.progress.inventory.bags[profile.player].slot_limit=8;
  CheckpointBook book;book.enter(checkpoint.level);book.record(removed_record);
  c.require(book.candidates().size()==1,"checkpoint capture missing");
  auto snap=book.candidates().front()->progress.learning;
  run("regrant-a","event:grant",ca,EventStatus::completed);
  run("grant-b","event:grant",cb,EventStatus::completed);
  c.require(skills.permits(A,cap)&&skills.permits(B,cap),"independent grants failed");
  c.require(skills.restore(snap)==Code::ok&&!skills.permits(A,cap)&&!skills.permits(B,cap),"checkpoint rewind did not restore both character states");
  const auto removed=current.select(removed_record);const auto v3=current.encode(removed);
  c.require(version(v3)==3,"revoked document must encode as v3");
  current.write(slot,removed);
  c.require(read_bytes(slot)==v3&&read_bytes(previous)==v2,"v3 publication did not preserve previous good slot");
  auto restored=current.reconstruct(current.read(slot),0,0);
  CharacterStats restored_stats(statdefs);restored_stats.character(A,"character:a");restored_stats.character(B,"character:b");
  c.require(restored_stats.restore(A,restored.progress.stats.at(A))==Code::ok&&restored_stats.restore(B,restored.progress.stats.at(B))==Code::ok,"saved stats restore failed");
  CharacterLearning restored_skills(restored_stats,new_learning());restored_skills.refresh(A);restored_skills.refresh(B);
  c.require(restored_skills.restore(restored.progress.learning)==Code::ok,"v3 learning restoration failed");
  restored_skills.refresh(A);c.require(!restored_skills.permits(A,cap)&&restored_skills.capture().revoked.at(A).count(ability),"v3 revocation did not survive refresh");
  port.rebind(restored_skills);run("after-rebind","event:grant",ca,EventStatus::completed);
  c.require(restored_skills.permits(A,cap)&&!skills.permits(A,cap),"event port did not follow replacement service");
  c.require(restored_skills.revoke(A,ability)==Code::ok&&restored_skills.restore(restored.progress.learning)==Code::ok,"regrant/rewind failed on restored service");
  auto contradictory=restored.progress.learning;contradictory.earned.at(A).insert(ability);
  c.require(restored_skills.restore(contradictory)==Code::invalid&&!restored_skills.permits(A,cap),"contradictory restore must refuse without mutation");
  c.require(read_bytes(slot)==v3&&read_bytes(previous)==v2,"reads or service restoration changed disk slots");

  c.require(fs::create_directory(pending),"failed-write fixture could not reserve pending path");
  c.refuses([&]{current.write(slot,removed);},"blocked pending path must refuse publication");
  c.require(read_bytes(slot)==v3&&read_bytes(previous)==v2,"failed publication changed primary/backup");
  c.require(fs::remove(pending),"owned empty pending fixture cleanup failed");
  auto corrupt=v3;corrupt.back()^=1;put_bytes(slot,corrupt);
  c.refuses([&]{current.read(slot);},"corrupt checksum must refuse read");
  c.refuses([&]{current.write(slot,removed);},"corrupt current primary must refuse overwrite");
  c.require(read_bytes(slot)==corrupt&&read_bytes(previous)==v2,"corrupt save refusal changed slots");
  current.recover(slot);auto recovery_original=slot;recovery_original+=".recovery-original";
  c.require(read_bytes(slot)==v2&&read_bytes(previous)==v2&&read_bytes(recovery_original)==corrupt,"explicit recovery must retain corrupt original and previous slot");
  auto future=v3;future[8]=4;put_bytes(slot,future);
  c.refuses([&]{current.read(slot);},"future version must refuse read");
  c.refuses([&]{current.write(slot,removed);},"future version must refuse overwrite");
  c.refuses([&]{current.recover(slot);},"future version must refuse recovery");
  c.require(read_bytes(slot)==future&&read_bytes(previous)==v2&&read_bytes(recovery_original)==corrupt,"future-version refusal changed preserved files");
  return c.count;
 } catch(const std::exception& e) {
  if(error&&capacity)std::snprintf(error,capacity,"%s",e.what());
 } catch(...) {
  if(error&&capacity)std::snprintf(error,capacity,"Unknown UI compatibility check failure");
 }
 return -1;
}
