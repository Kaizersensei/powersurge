#include "../editor_shell/shell.h"
#include "../ui/widgets.h"
#include "../labs/lab.h"
#include "spatial_audio.h"
// Native service host for one disposable loaded-level laboratory. Signed: Atlas.
#include "foundation.hpp"
#include "gameplay.hpp"
#include "gameplay_records.hpp"
#include "progression.hpp"
#include "progression_records.hpp"
#include "learning.hpp"
#include "learning_records.hpp"
#include "checkpoints.hpp"
#include "checkpoint_records.hpp"
#include "progress_save.hpp"
#include "progress_save_records.hpp"
#include "pickups.hpp"
#include "pickup_records.hpp"
#include "loot.hpp"
#include "loot_records.hpp"
#include "equipment.hpp"
#include "equipment_records.hpp"
#include <random>
#include "events.hpp"
#include "event_rewards.hpp"
#include "event_skills.hpp"
#include "quests.hpp"
#include "quest_records.hpp"
#include "commerce.hpp"
#include "commerce_records.hpp"
#include "teleport_records.hpp"
#include "calendar_records.hpp"
#include "weather_records.hpp"
#include "level_records.hpp"
#include "session.hpp"
#include "bor_health_effects.hpp"
#include "foundation_records.hpp"
#include "interaction_records.hpp"
#include "actor_placement_records.hpp"
#include "actor_template_records.hpp"
#include "foundation_host.h"
#include "../presentation/selection_catalog.h"
#include <allegro5/allegro.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iterator>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <map>

namespace {
std::map<uint64_t,BorActor*> bindings;
std::map<uint64_t,std::string> player_slots;
uint64_t session_generation=0;
std::unique_ptr<bsl::PlayerRoster> gameplay_roster;
const bsl::LevelRegistry& level_registry(){static const bsl::LevelRegistry registry(compiled_levels());return registry;}
bool gameplay_enabled(){const char* value=std::getenv("BSL_GAMEPLAY");return value&&std::string(value)=="1";}
bool progression_enabled(){const char* value=std::getenv("BSL_PROGRESSION");return gameplay_enabled()&&value&&std::string(value)=="1";}
bool learning_enabled(){const char* value=std::getenv("BSL_LEARNING");return progression_enabled()&&value&&std::string(value)=="1";}
bool checkpoint_enabled(){const char* value=std::getenv("BSL_CHECKPOINT");return learning_enabled()&&value&&std::string(value)=="1";}
bool progress_save_enabled(){const char* value=std::getenv("BSL_PROGRESS_SAVE");return checkpoint_enabled()&&value&&std::string(value)=="1";}
bool progress_boot_enabled(){const char* value=std::getenv("BSL_PROGRESS_BOOT");return progress_save_enabled()&&value&&std::string(value)=="1";}
bool pickups_enabled(){const char* value=std::getenv("BSL_PICKUPS");return progress_boot_enabled()&&value&&std::string(value)=="1";}
bool loot_enabled(){const char* value=std::getenv("BSL_LOOT");return pickups_enabled()&&value&&std::string(value)=="1";}
bool calendar_enabled(){const char* value=std::getenv("BSL_CALENDAR");return progress_boot_enabled()&&value&&std::string(value)=="1";}
bool equipment_enabled(){const char* value=std::getenv("BSL_EQUIPMENT");return loot_enabled()&&value&&std::string(value)=="1";}
std::uint64_t loot_epoch=0;
std::unique_ptr<bsl::LootDrops> make_loot(const bsl::Catalog& catalog){
 if(loot_epoch==UINT64_MAX)throw std::runtime_error("Loot level epoch exhausted.");
 auto seed=std::random_device{}();return std::make_unique<bsl::LootDrops>(catalog,compiled_loot(),++loot_epoch,seed?seed:1);
}
// Launch choice affects fresh runs only; restored progress owns its selection. Signed: Mech.
std::unique_ptr<bsl::PlayerRoster> fresh_roster(){
 auto next=std::make_unique<bsl::PlayerRoster>(compiled_gameplay_profile());
 const char* requested=std::getenv("BSL_START_CHARACTER");const char* continuing=std::getenv("BSL_PROGRESS_CONTINUE");
 if(requested&&*requested&&!(continuing&&std::string(continuing)=="1")&&next->choose_initial(requested)!=bsl::Code::ok)throw std::invalid_argument("Requested initial character is absent from this mode roster.");
 return next;
}
bsl::PlayerRoster& roster(){if(!gameplay_roster)gameplay_roster=fresh_roster();return *gameplay_roster;}
std::string owner_id(uint64_t id){
 if(gameplay_enabled()&&gameplay_roster&&gameplay_roster->bound(id))return gameplay_roster->profile().player;
 auto found=player_slots.find(id);return found==player_slots.end()?"unbound/"+std::to_string(id):found->second;
}
std::string actor_owner(uint64_t id){return "actor/"+std::to_string(id);}
const BslActorTemplateDef* actor_template_definition(const std::string& id){for(const auto& d:compiled_actor_templates())if(d.id==id)return &d;return nullptr;}
void actor_template_view(const BslActorTemplateDef& d,BslActorTemplate& out){std::snprintf(out.id,sizeof out.id,"%s",d.id.c_str());std::snprintf(out.name,sizeof out.name,"%s",d.name.c_str());std::snprintf(out.model,sizeof out.model,"%s",d.model.c_str());std::snprintf(out.interaction,sizeof out.interaction,"%s",d.interaction.c_str());out.passive=d.passive;}
const BslActorInteractionDef* interaction_definition(const std::string& id){for(const auto& def:compiled_interactions())if(def.id==id)return &def;return nullptr;}
std::string conversation_epoch(){std::random_device rng;std::ostringstream out;out<<std::hex<<((static_cast<uint64_t>(rng())<<32)^static_cast<uint64_t>(rng()));return out.str();}

std::string level_identity(){
 std::ifstream input(std::filesystem::u8path(bsl_engine_level_path()),std::ios::binary);if(!input)throw std::runtime_error("Cannot identify loaded level.");
 std::uint64_t hash=14695981039346656037ULL;char byte;std::size_t count=0;
 while(input.get(byte)){if(++count>8*1024*1024)throw std::runtime_error("Level exceeds lab-data fingerprint limit.");hash^=static_cast<unsigned char>(byte);hash*=1099511628211ULL;}
 if(!input.eof())throw std::runtime_error("Cannot read loaded level.");
 std::ostringstream key;key<<bsl_engine_level_file()<<":"<<std::hex<<hash;return key.str();
}
std::filesystem::path save_path(){const char* override_path=std::getenv("BOR_SESSION_DATA_PATH");return override_path?std::filesystem::u8path(override_path):(std::getenv("LOCALAPPDATA")?std::filesystem::u8path(std::getenv("LOCALAPPDATA"))/"PowerSurge"/"session-data":std::filesystem::u8path(bsl_engine_data_dir()))/"lab-data"/"slot1.bsl";}
std::filesystem::path progress_path(){const char* path=std::getenv("BSL_PROGRESS_PATH");if(path)return std::filesystem::u8path(path);const char* local=std::getenv("LOCALAPPDATA");if(!local)throw std::runtime_error("Progress save directory unavailable.");return std::filesystem::u8path(local)/"PowerSurge"/"progress"/"lab-gameplay"/"slot1.bsp";}
// Registered actor command adapter uses the existing script factory. Signed: Mech.
class HostActorEvents final:public bsl::EventActorPort {public:bsl::Result spawn(const bsl::EventContext&,const bsl::EventCommand&) override;};
struct Session {
 bsl::Catalog catalog=bsl::load_compiled_snapshot();
 bsl::Inventory inventory{catalog};
 bsl::Events events{catalog,inventory};
 bsl::BorHealthEffects effects;
 std::unique_ptr<bsl::CharacterStats> progression;
 std::unique_ptr<bsl::CharacterLearning> learning;
 HostActorEvents actor_events;
 std::unique_ptr<bsl::EventRewards> event_rewards;
 std::unique_ptr<bsl::EventSkills> event_skills;
 std::unique_ptr<bsl::Quests> quests;
 std::unique_ptr<bsl::Commerce> commerce;
 std::unique_ptr<bsl::CalendarClock> clock;
 std::unique_ptr<bsl::Weather> weather;
 std::optional<bsl::WeatherOverride> weather_override;
 std::unique_ptr<bsl::CheckpointBook> checkpoints;
 std::unique_ptr<bsl::EquipmentStats> equipment;
 std::unique_ptr<bsl::LootDrops> loot;unsigned loot_refused=0;
 std::string last_checkpoint;
 bsl::DoorContacts door_contacts;std::optional<bsl::TravelContext> last_travel;unsigned travel_arrivals=0,travel_failures=0;
 std::optional<bsl::ProgressDocument> pending_progress;
 unsigned progress_failures=0;
 bool rewind_requested=false,checkpoint_fault=false,save_offer=false;
 unsigned checkpoint_captures=0,checkpoint_rewinds=0,checkpoint_failures=0,death_ticks=0;
 int last_damage=0,last_bonus=0,last_reward_code=0;unsigned reward_refused=0;
 bsl::Policies policies;
 bool player_required=false,player_ready=false,player_failed=false,player_retry_pending=false;
 unsigned player_wait=0,player_retries=0;bsl::Lease player_pause=0;std::string player_error;
 void player_clear_pause(){if(player_pause){policies.close(player_pause);player_pause=0;}}
 void player_assembled(){bool failed=player_failed;player_clear_pause();player_ready=true;player_failed=false;player_retry_pending=false;player_wait=0;player_error.clear();if(failed){barrier=true;bsl_bor_capture_input();}}
 void player_reset(){player_clear_pause();player_required=player_ready=player_failed=player_retry_pending=false;player_wait=player_retries=0;player_error.clear();}

 bsl::Conversation conversation{catalog,policies};
 std::array<bool,ALLEGRO_KEY_MAX> previous{};
 std::set<uint64_t> seeded;
 bsl::Lease menu=0;
 uint64_t owner=0,request_id=0,conversation_id=0,ui_frames=0,damage_notifications=0;
 double ui_seconds=0,simulation_seconds=0,calendar_seconds=0;
 bool barrier=false,captures_commands=false;
 int selected=0,choice=0,choice_first=0,choice_last=0,category_index=0;
 int menu_page=1,last_menu_page=1,menu_row=0,home_row=0;uint64_t menu_revision=0;bool menu_test_mix_confirm=false,menu_test_mix_applied=false;
 std::string last_event;
 std::map<uint64_t,uint64_t> used_actions;
 std::map<uint64_t,std::string> anchor_runs;
 uint64_t anchor_sequence=0,interaction_subject=0;
 bool interaction_requested=false;
 struct ActorTalk {uint64_t initiator,speaker;double range;bsl::EventContext context;std::string definition,run;};
 std::optional<ActorTalk> talking;
 std::map<std::string,std::pair<uint64_t,uint64_t>> conversation_runs;
 std::map<uint64_t,std::string> actor_interactions;
 std::set<uint64_t> actor_defaults_done;
 void resolve_actor_template_defaults(){
  if(!gameplay_enabled()||bsl_engine_editor())return;
  for(const auto& body:bindings){
   if(actor_defaults_done.count(body.first))continue;
   const auto* def=actor_template_definition(bsl_bor_actor_template_id(body.first));
   if(def&&bsl_bor_conversation_actor_live(body.first)&&bsl_host_actor_interaction_bind(body.first,def->interaction.c_str())==0)actor_defaults_done.insert(body.first);
  }
 }
 struct ActorPlacement {const BslActorPlacementDef* definition;uint64_t actor=0;unsigned attempts=0;int status=BSL_ACTOR_PLACEMENT_PENDING,retry=0;};
 std::string actor_placement_level;
 std::vector<ActorPlacement> actor_placements;
 void resolve_actor_placements(){
  if(!gameplay_enabled()||bsl_engine_editor())return;
  const std::string level=bsl_engine_level_file();
  if(actor_placement_level!=level){actor_placements.clear();actor_placement_level=level;for(const auto& def:compiled_actor_placements())if(def.level==level)actor_placements.push_back({&def});}
  for(auto& entry:actor_placements){
   if(entry.actor)continue;
   if(entry.retry>0){--entry.retry;continue;}
   ++entry.attempts;entry.retry=5;uint64_t actor=0;
   entry.status=bsl_level_actor_placement_resolve(entry.definition->placement.c_str(),entry.definition->object.c_str(),&actor);
   if(entry.status!=BSL_ACTOR_PLACEMENT_BOUND)continue;
   if(bsl_host_actor_interaction_bind(actor,entry.definition->interaction.c_str())!=0){entry.status=BSL_ACTOR_PLACEMENT_REFUSED;continue;}
   entry.actor=actor;
  }
 }
 void retire_actor_placement(uint64_t actor){for(auto& entry:actor_placements)if(entry.actor==actor){entry.actor=0;entry.status=BSL_ACTOR_PLACEMENT_PENDING;entry.retry=0;}}


 unsigned saves=0,loads=0;
 std::string notice="Disposable lab: starts at 80 HP with 3 strawberries.";
 explicit Session(bsl::PlayerRoster* prepared=nullptr){
  ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);for(int i=0;i<ALLEGRO_KEY_MAX;++i)previous[i]=al_key_down(&keys,i);barrier=bsl_bor_input_held();
  if(gameplay_enabled()){
   const auto& definition_roster=prepared?*prepared:roster();
   const auto& profile=definition_roster.profile();unsigned index=0;events.bind_actors(actor_events);
   if(progression_enabled()){progression=std::make_unique<bsl::CharacterStats>(compiled_stat_catalog());for(const auto& c:profile.characters)progression->character(definition_roster.character(c.id)->id,c.id);}
   if(learning_enabled()){learning=std::make_unique<bsl::CharacterLearning>(*progression,compiled_learning_catalog());for(const auto& c:profile.characters)learning->refresh(definition_roster.character(c.id)->id);}
   if(learning){std::map<std::string,std::string> owners;for(const auto& c:profile.characters)owners.emplace(definition_roster.character(c.id)->id,profile.player);event_rewards=std::make_unique<bsl::EventRewards>(*learning,std::move(owners));events.bind_rewards(*event_rewards);}
   if(learning){std::map<std::string,std::string> owners;for(const auto& c:profile.characters)owners.emplace(definition_roster.character(c.id)->id,profile.player);event_skills=std::make_unique<bsl::EventSkills>(*learning,std::move(owners));events.bind_skills(*event_skills);}
   if(learning&&!compiled_quests().empty()){std::map<std::string,std::string> owners;for(const auto& c:profile.characters)owners.emplace(definition_roster.character(c.id)->id,profile.player);quests=std::make_unique<bsl::Quests>(inventory,events,*learning,compiled_quests(),std::move(owners));events.bind_quests(*quests);}
   if(!compiled_commerce().shops.empty()){commerce=std::make_unique<bsl::Commerce>(inventory,events,compiled_commerce(),std::set<std::string>{profile.player});events.bind_trades(*commerce);}
   if(calendar_enabled()){auto rules=compiled_calendar();if(!rules)throw std::runtime_error("Calendar definitions unavailable.");clock=std::make_unique<bsl::CalendarClock>(*rules,calendar_seconds,std::set<std::string>{profile.player});events.bind_clock(*clock);weather=std::make_unique<bsl::Weather>(compiled_weather());}
   if(equipment_enabled())equipment=std::make_unique<bsl::EquipmentStats>(catalog,compiled_stat_catalog(),compiled_equipment());
   if(loot_enabled())loot=make_loot(catalog);
   if(checkpoint_enabled()){checkpoints=std::make_unique<bsl::CheckpointBook>();checkpoints->enter(bsl_engine_level_file());}
   for(const auto& item:profile.items)if(inventory.execute({"initial/"+std::to_string(index++),profile.player,bsl::Op::grant,item.definition,item.instance,"",item.quantity}).code!=bsl::Code::ok)throw std::runtime_error("invalid authored starting inventory");
   if(inventory.resize(profile.player,bsl_ui_inventory_default_slots()).code!=bsl::Code::ok)throw std::runtime_error("Starting bag exceeds configured slots");
   notice="Gameplay services: F9 switches character while standing still; one player-owned inventory. R starts a fresh disposable run.";
  }
  if(!prepared)for(const auto& binding:bindings)bind(binding.first,*binding.second);
 }
 void bind(uint64_t id,BorActor& actor){
  if(gameplay_enabled()){
   if(roster().controls(id)){effects.bind(roster().profile().player,actor);owner=id;player_assembled();}
   return;
  }
  effects.bind(owner_id(id),actor);
  if(seeded.insert(id).second){
   const auto owner=owner_id(id);
   if(inventory.execute({"seed/fruit",owner,bsl::Op::grant,"item:strawberry","","",3}).code!=bsl::Code::ok)throw std::runtime_error("cannot seed fruit");
   if(inventory.execute({"seed/sword",owner,bsl::Op::grant,"item:training_sword","sword/"+owner,"",1}).code!=bsl::Code::ok)throw std::runtime_error("cannot seed sword");
   actor.health=std::min(actor.health,std::max(1,actor.model->health-20));
  }
  if(!owner)owner=id;
 }
 bsl::EventContext event_context(const std::string& definition,const std::string& owner,uint64_t actor) const {
  bsl::EventContext context{owner,owner_id(actor)};const auto* event=catalog.event(definition);
  if(event&&std::any_of(event->commands.begin(),event->commands.end(),[](const bsl::EventCommand& c){return c.op==bsl::EventOp::grant_skill||c.op==bsl::EventOp::revoke_skill||c.op==bsl::EventOp::grant_reward||c.op==bsl::EventOp::accept_quest||c.op==bsl::EventOp::claim_quest||c.op==bsl::EventOp::spawn_character||c.op==bsl::EventOp::spawn_actor;})&&gameplay_enabled()){
   const auto* character=roster().bound(actor);if(character)context.character=character->id;
  }
  return context;
 }
 void reward_failure(bsl::Code code,const std::string& source,const std::string& character,const std::string& receipt){
  last_reward_code=static_cast<int>(code);
  if(code==bsl::Code::denied){notice="This character profile does not accept the registered reward.";return;}
  ++reward_refused;notice="REWARD REFUSED (code "+std::to_string(last_reward_code)+"): progress not granted; inspect development log.";
  std::fprintf(stderr,"BSL reward refused code=%d source=%s character=%s receipt=%s\n",last_reward_code,source.c_str(),character.c_str(),receipt.c_str());
 }
 BorActor* actor() const {auto found=bindings.find(owner);return found==bindings.end()?nullptr:found->second;}
 bool alive() const {auto* a=actor();return a&&a->model&&a->health>0&&a->state!=BOR_DEAD;}
 // Roster state may be dormant; only the selected body is required for scene assembly. Signed: Mech.
 bool selected_ready() const {const auto id=roster().selected().actor;auto at=bindings.find(id);return id&&owner==id&&at!=bindings.end()&&at->second&&at->second->initialized;}
 void open_menu(int page){++menu_revision;if(!menu)menu=policies.menu(owner_id(owner));menu_page=page;if(page)last_menu_page=page;menu_row=page?0:home_row;selected=0;category_index=0;}
 void menu_back(){++menu_revision;if(menu_page){last_menu_page=menu_page;menu_page=0;menu_row=home_row;}else close_menu();}
 void close_menu(){menu_test_mix_confirm=false;ps_ui_menu_clear();++menu_revision;if(menu){policies.close(menu);menu=0;}}
 void cancel(){close_menu();conversation.cancel();talking.reset();barrier=true;}
 bool dialogue_open() const {return conversation.state()==bsl::ConversationState::open;}
 bool modal() const {return menu||dialogue_open();}
 void interrupt_actor_conversation(uint64_t id){
  events.cancel_participant(actor_owner(id));
  if(conversation.hit(actor_owner(id))){barrier=true;captures_commands=true;interaction_requested=false;bsl_bor_capture_input();}
 }
 void dialogue_presentation(){
  auto at=talking?bindings.find(talking->speaker):bindings.end();
  double x=at==bindings.end()?NAN:at->second->x,y=at==bindings.end()?NAN:at->second->y-at->second->model->soft_height;
  bsl_ui_actor_dialogue_context(talking.has_value(),x,y,false,0,0,0,0);
 }
 void validate_actor_conversation(){
  if(talking&&dialogue_open()){
   const auto& talk=*talking;auto a=bindings.find(talk.initiator),b=bindings.find(talk.speaker);
   if(!bsl_bor_conversation_actor_live(talk.initiator)||!bsl_bor_conversation_actor_live(talk.speaker)){
    interrupt_actor_conversation(talk.initiator);interrupt_actor_conversation(talk.speaker);
   }else if(a==bindings.end()||b==bindings.end()||std::hypot(a->second->x-b->second->x,a->second->y-b->second->y)>talk.range){
    conversation.cancel();barrier=true;captures_commands=true;interaction_requested=false;bsl_bor_capture_input();
   }
  }
 }
 void validate_conversation_runs(){
  for(auto it=conversation_runs.begin();it!=conversation_runs.end();){
   const auto* run=events.run(it->first);
   if(!run||run->status==bsl::EventStatus::completed||run->status==bsl::EventStatus::cancelled||run->status==bsl::EventStatus::failed){it=conversation_runs.erase(it);continue;}
   if(!bsl_bor_conversation_actor_live(it->second.first)||!bsl_bor_conversation_actor_live(it->second.second)){events.cancel(it->first);it=conversation_runs.erase(it);continue;}
   ++it;
  }
 }
 bsl::Code choose_dialogue(int index){
  if(!conversation.choose(index))return bsl::Code::denied;
  choice=index;barrier=true;captures_commands=true;interaction_requested=false;bsl_bor_capture_input();
  const auto* def=conversation.definition();const auto outcome=def->outcomes.empty()?std::string{}:def->outcomes.at(static_cast<std::size_t>(index));
  if(outcome.empty()){notice="Conversation complete; no outcome event.";return bsl::Code::ok;}
  const auto run=(talking?"talk."+conversation_epoch()+".":"dialogue.")+std::to_string(conversation_id);
  const auto context=talking?talking->context:event_context(outcome,owner_id(owner),owner);
  if(talking)conversation_runs.emplace(run,std::make_pair(talking->initiator,talking->speaker));
  const auto accepted=events.start(run,outcome,context);
  if(accepted.code==bsl::Code::ok){last_event=run;if(talking)talking->run=run;}else conversation_runs.erase(run);
  result(accepted,"Choice accepted; authored event queued.");return accepted.code;
 }
 int fruit() const {auto* b=inventory.bag(gameplay_enabled()?roster().profile().player:owner_id(owner));if(!b)return 0;auto i=b->stacks.find("item:strawberry");return i==b->stacks.end()?0:i->second;}
 bool equipped() const {auto* b=inventory.bag(owner_id(owner));return b&&b->equipped.count("weapon");}
 bsl::StatModifiers modifiers(const std::string& character) const {return equipment?equipment->modifiers(inventory,roster().profile().player,progression->profile(character).definition):bsl::StatModifiers{};}
 bool assigned() const {auto* b=inventory.bag(owner_id(owner));return b&&b->assigned.count("ranged");}
 bsl::Result command(bsl::Op op,const std::string& definition="",const std::string& instance="",const std::string& slot=""){
  return inventory.execute({"ui/"+std::to_string(++request_id),owner_id(owner),op,definition,instance,slot,1},&effects);
 }
 void result(bsl::Result r,const std::string& success){notice=r.code==bsl::Code::ok?success:r.code==bsl::Code::denied?"Cannot do that now; nothing was consumed.":"Request rejected; inventory unchanged.";}
 std::vector<std::string> categories() const {
  std::set<std::string> found;for(const auto& item:inventory.entries(owner_id(owner)))found.insert(item.category);
  std::vector<std::string> result{"All"};result.insert(result.end(),found.begin(),found.end());return result;
 }
 std::vector<bsl::InventoryEntry> rows() const {
  auto result=inventory.entries(owner_id(owner));auto groups=categories();
  if(category_index>0&&category_index<static_cast<int>(groups.size())){
   const auto& group=groups[category_index];result.erase(std::remove_if(result.begin(),result.end(),[&](const bsl::InventoryEntry& item){return item.category!=group;}),result.end());
  }
  return result;
 }
 void normalize_selection(){
  auto groups=categories();if(category_index>=static_cast<int>(groups.size()))category_index=0;
  const auto count=static_cast<int>(rows().size());selected=count?std::max(0,std::min(selected,count-1)):0;
 }
 void change_category(int delta){
  if(!delta)return;
  ++menu_revision;const auto count=static_cast<int>(categories().size());category_index=(category_index+delta+count)%count;selected=0;
 }
 void menu_action(bool assign=false){
  normalize_selection();auto entries=rows();if(entries.empty()){notice="Inventory is empty.";return;}
  const auto item=entries[selected];
  if(assign){
   if(!item.usable){notice="This item has no mapped use effect.";return;}
   result(command(bsl::Op::assign,item.selection.definition,item.selection.instance,"ranged"),"Item assigned to K; assignment does not consume it.");
  }else if(!item.slot.empty()){
   if(equipment)result({static_cast<bsl::Code>(bsl_host_equipment_set(owner,item.selection.instance.c_str(),!item.equipped))},item.equipped?"Item unequipped; next action uses the revised effective stats.":"Item equipped; authored stat effects apply to the next action.");
   else if(item.equipped)result(command(bsl::Op::unequip,"","",item.slot),"Item unequipped.");
   else result(command(bsl::Op::equip,item.selection.definition,item.selection.instance,item.slot),"Item equipped (data binding; combat profile unchanged).");
  }else if(item.usable)result(command(bsl::Op::use,item.selection.definition),"Item used through its authored effect.");
  else notice="This item has no use or equipment action.";
  normalize_selection();
 }
 void menu_navigate(int delta){
  if(menu_page==1||menu_page==2){const int count=static_cast<int>(rows().size());selected=count?(selected+delta%count+count)%count:0;return;}
  int count=menu_page==0?bsl_ui_menu_option_count():menu_page==5?3:menu_page==6?5:0;
  if(count){menu_row=(menu_row+delta+count)%count;if(menu_page==0)home_row=menu_row;}
 }
 void menu_confirm(){++menu_revision;
  if(menu_page==0){home_row=menu_row;menu_page=bsl_ui_menu_option_page(menu_row);last_menu_page=menu_page;menu_row=0;selected=0;return;}
  if(menu_page==1||menu_page==2){menu_action(menu_page==2);return;}
  if(menu_page==5){if(menu_row==0)bsl_ui_toggle_motion();else if(menu_row==1)bsl_ui_cycle_layout();else menu_back();}
  else if(menu_page==6){if(menu_row==0)save_data();else if(menu_row==1)load_data();else if(menu_row==2)recover_data();else if(menu_row==3)bsl_engine_quit();else menu_back();}
 }
 void eligible_data() const {
  if(gameplay_enabled())throw std::runtime_error("Legacy lab snapshots are disabled for gameplay identities; compact progress saves remain pending.");
  if(!anchor_runs.empty())throw std::runtime_error("Placement-event journals are outside the legacy lab snapshot. Compact progress saves remain planned.");
  auto* a=actor();if(bindings.size()!=1||owner_id(owner)!="player/1"||!alive()||!a->grounded||a->state!=BOR_FREE||std::abs(a->vx)>1||std::abs(a->vy)>1||a->freeze>0||a->timeline_fault||dialogue_open())throw std::runtime_error("Close dialogue and stand still before saving/loading lab data.");
 }
 bsl::SessionData capture_data() const {
  eligible_data();bsl::SessionData snapshot;snapshot.mode="lab.data";snapshot.level=level_identity();snapshot.content=foundation_content_hash();
  snapshot.participants={{owner_id(owner),actor()->model->name,actor()->health}};snapshot.inventory=inventory.capture();snapshot.events=events.capture();snapshot.request_id=request_id;snapshot.conversation_id=conversation_id;snapshot.calendar_seconds=calendar_seconds;return snapshot;
 }
 void save_data(){if(progress_save_enabled()){bsl_host_progress_save();return;}try{auto snapshot=capture_data();bsl::SessionDataStore::write(catalog,save_path(),snapshot);++saves;notice="Lab data saved: inventory, events and HP. Compact progress saves remain planned.";}catch(const std::exception& e){notice=std::string("Lab data not saved: ")+e.what();}}
 void load_data(){if(progress_save_enabled()){bsl_host_progress_load();return;}try{
  eligible_data();auto snapshot=bsl::SessionDataStore::read(catalog,save_path());
  const auto& p=snapshot.participants.at(0);auto* a=actor();
  if(snapshot.mode!="lab.data"||snapshot.level!=level_identity()||snapshot.content!=foundation_content_hash()||snapshot.participants.size()!=1||p.id!=owner_id(owner)||p.character!=a->model->name||p.health>a->model->health)throw std::runtime_error("Lab data does not match this content, level or player.");
  std::string success="Lab data loaded. World position, enemies and physics remain where they are.";
  bsl::SessionDataStore::restore_services(catalog,snapshot,inventory,events);
  a->health=p.health;request_id=snapshot.request_id;conversation_id=snapshot.conversation_id;calendar_seconds=snapshot.calendar_seconds;weather_override.reset();bsl_particles_clear();bsl_ambient_clear();bsl_acoustic_clear();
  conversation.cancel();last_event.clear();selected=category_index=choice=choice_first=choice_last=0;barrier=true;++loads;notice.swap(success);
 }catch(const std::exception& e){notice=std::string("Lab data not loaded: ")+e.what();}}
 void recover_data(){if(progress_save_enabled()){bsl_host_progress_recover();return;}try{eligible_data();bsl::SessionDataStore::recover(catalog,save_path());notice="Previous lab data restored on disk. Choose Load lab data to apply it.";}catch(const std::exception& e){notice=std::string("Recovery refused: ")+e.what();}}


};
std::unique_ptr<Session> session;
bsl::Result HostActorEvents::spawn(const bsl::EventContext& context,const bsl::EventCommand& command){
 if(!session||!gameplay_enabled()||bsl_host_boot_busy()||bsl_engine_editor()||!session->alive())return {bsl::Code::denied};
 const auto& r=roster();if(context.player!=r.profile().player||context.character!=r.selected().id)return {bsl::Code::denied};
 if(command.op==bsl::EventOp::spawn_actor){if(!actor_template_definition(command.target))return {bsl::Code::invalid};return {static_cast<bsl::Code>(bsl_bor_event_spawn_actor(command.target.c_str(),command.x,command.y))};}
 if(command.op!=bsl::EventOp::spawn_character||!r.definition(command.target))return {bsl::Code::invalid};
 return {static_cast<bsl::Code>(bsl_bor_event_spawn_character(command.target.c_str(),command.x,command.y))};
}

enum class BootPhase {queued,assembling,returning,failed};
struct BootRoute {
 bsl::ProgressDocument document;std::string target,origin,error;BootPhase phase=BootPhase::queued;
 std::unique_ptr<Session> previous_session;std::unique_ptr<bsl::PlayerRoster> previous_roster;
 unsigned wait=0;bool return_loaded=false;
};
std::unique_ptr<BootRoute> boot;
BslBootProbe boot_report{};
bool continue_attempted=false;
enum class TravelPhase {queued,assembling,returning,failed};
struct TravelRoute {
 bsl::TravelContext context;std::string player,character,complete;double vx=0,vy=0;TravelPhase phase=TravelPhase::queued;
 std::optional<bsl::CheckpointRecord> checkpoint;std::optional<std::pair<double,double>> door_offset;unsigned wait=0;bool source_loaded=false;int health=0;
};
std::unique_ptr<TravelRoute> travel;
bool teleports_enabled(){const char* v=std::getenv("BSL_TELEPORT");return progress_boot_enabled()&&v&&std::string(v)=="1";}
const bsl::DoorRegistry& doors(){static const bsl::DoorRegistry registry(compiled_doors());return registry;}
bool travel_step();void travel_begin(const char*);bool travel_checkpoint(const bsl::CheckpointRecord&);void travel_contacts();void travel_failure(const std::string&);


int boundary_depth=0;bool fault_pending=false;
void fault(const char* message) noexcept {bsl_engine_error(message);fault_pending=true;}
template<class F> void guarded(F function) noexcept {
 ++boundary_depth;
 try{function();}catch(const std::exception& e){fault(e.what());}catch(...){fault("unknown native exception");}
 if(--boundary_depth==0&&fault_pending){if(!gameplay_enabled())session.reset();else if(session){session->cancel();session->notice="Service operation failed; committed gameplay state retained. See log.";}bsl_ui_release();fault_pending=false;}
}
bool checkpoint_tick();
void boot_fail(const std::string&);
void boot_stop(const std::string&);
bsl::Code checkpoint_capture(const bsl::CheckpointDef* definition);
// Diagnostic serialization is opt-in; authored strings must remain valid JSON.
void editor_json(std::ostream& out,const std::string& value){
 out<<'"';const char* hex="0123456789abcdef";
 for(unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);else if(c<32)out<<"\\u00"<<hex[c>>4]<<hex[c&15];else out<<static_cast<char>(c);}out<<'"';
}
void editor_records(std::ostream& out,const Session& s){
 if(!std::getenv("BSL_EDITOR_RECORD_PROBE"))return;
 const auto* item=s.catalog.item("item:strawberry");const auto* dialogue=s.catalog.dialogue("dialogue:training_choice");const auto* field=s.catalog.field("field:wind_lab");
 out<<",\"editor_records\":{\"content_hash\":";editor_json(out,foundation_content_hash());
 out<<",\"item_name\":";editor_json(out,item?item->name:"");out<<",\"item_description\":";editor_json(out,item?item->description:"");out<<",\"item_category\":";editor_json(out,item?item->category:"");
 const auto* effect=item?s.catalog.effect(item->effect):nullptr;out<<",\"effect_amount\":"<<(effect?effect->amount:0);
 out<<",\"dialogue_text\":";editor_json(out,dialogue?dialogue->text:"");out<<",\"first_choice\":";editor_json(out,dialogue&&!dialogue->choices.empty()?dialogue->choices[0]:"");
 out<<",\"first_info\":";editor_json(out,dialogue&&!dialogue->choice_help.empty()?dialogue->choice_help[0]:"");out<<",\"first_outcome\":";editor_json(out,dialogue&&!dialogue->outcomes.empty()?dialogue->outcomes[0]:"");
 double acceleration=0;if(field&&s.owner){bsl::WindHelper helper("editor/field",*field);helper.enter(owner_id(s.owner));const auto requests=helper.requests();if(!requests.empty())acceleration=requests[0].acceleration_x;}
 out<<",\"field_present\":"<<(field?"true":"false")<<",\"field_strength\":"<<(field?field->strength:0)<<",\"field_direction\":"<<(field?field->direction:0)<<",\"field_acceleration_request\":"<<acceleration<<",\"field_actor_coupling\":false}";
}
void trace(){
 const char* path=std::getenv("BOR_FOUNDATION_TRACE");if(!path||!session)return;
 const auto& s=*session;const auto* a=s.actor();const auto* event=s.events.run(s.last_event);BslWorldProbe world{};bsl_bor_probe(&world);BslUiProbe ui{};bsl_ui_probe(&ui);
 const auto* bag=s.inventory.bag(owner_id(s.owner));auto entries=s.rows();const auto index=static_cast<std::size_t>(s.selected);
 std::ofstream out(path,std::ios::trunc);if(!out)return;
 BslWeatherProbe weather{};bsl_host_weather_probe(&weather);BslLevelProbe level{};bsl_host_level_probe(&level);BslParticleProbe particles{};bsl_host_particle_probe(&particles);BslAmbientProbe ambience{};bsl_host_ambient_probe(&ambience);
 out<<std::setprecision(12)<<"{\"generation\":"<<session_generation<<",\"ui_frame\":"<<s.ui_frames<<",\"ui_seconds\":"<<s.ui_seconds<<",\"simulation_seconds\":"<<s.simulation_seconds<<",\"calendar_seconds\":"<<s.calendar_seconds
 <<",\"ambient_voices\":"<<ambience.voices<<",\"ambient_playing\":"<<ambience.playing<<",\"ambient_gain\":"<<ambience.gain<<",\"ambient_voice\":"<<ambience.first_voice
 <<",\"particle_count\":"<<particles.particles<<",\"particle_instances\":"<<particles.instances<<",\"particle_batches\":"<<particles.batches<<",\"particle_effect\":\""<<particles.weather_effect<<"\""
 <<",\"editor\":"<<(bsl_engine_editor()?"true":"false")<<",\"level_registered\":"<<(level.registered?"true":"false")<<",\"level_editable\":"<<(level.editable?"true":"false")<<",\"level_definition\":\""<<level.id<<"\""
 <<",\"weather_icon\":"<<weather.icon<<",\"weather_wind\":"<<weather.wind<<",\"weather_direction\":"<<weather.direction<<",\"weather_precipitation\":"<<weather.precipitation<<",\"weather_indoor\":"<<(weather.indoor?"true":"false")
 <<",\"calendar_factor\":"<<s.policies.calendar_factor()<<",\"paused\":"<<(s.policies.paused()?"true":"false")<<",\"menu\":"<<(s.menu?"true":"false")<<",\"dialogue\":"<<(s.dialogue_open()?"true":"false")
 <<",\"conversation_state\":"<<static_cast<int>(s.conversation.state())<<",\"barrier\":"<<(s.barrier?"true":"false")<<",\"blocked\":"<<((s.policies.blocked(owner_id(s.owner))||s.barrier)?"true":"false")
 <<",\"inventory_entries\":"<<s.inventory.entries(owner_id(s.owner)).size()<<",\"visible_entries\":"<<entries.size()<<",\"category_index\":"<<s.category_index<<",\"selected_definition\":\""<<(index<entries.size()?entries[index].selection.definition:"")<<"\""
 <<",\"menu_page\":"<<s.menu_page<<",\"menu_row\":"<<s.menu_row<<",\"ui_menu_progress\":"<<ui.menu<<",\"ui_dialogue_progress\":"<<ui.dialogue<<",\"message_top\":"<<ui.message_top<<",\"message_bottom\":"<<ui.message_bottom<<",\"message_target_bottom\":"<<ui.message_target_bottom
 <<",\"ui_layout\":"<<bsl_ui_layout_index()<<",\"choice_last\":"<<s.choice_last
 <<",\"choice_first\":"<<s.choice_first<<",\"choice_count\":"<<(s.dialogue_open()?s.conversation.definition()->choices.size():0)
 <<",\"event_status\":"<<(event?static_cast<int>(event->status):-1)<<",\"event_next\":"<<(event?event->next:0)
 <<",\"reward_flag\":"<<(s.events.flag("switch:training_reward",owner_id(s.owner))?"true":"false")<<",\"reward_choices\":"<<s.events.variable("variable:training_choices",owner_id(s.owner))
 <<",\"player_slot\":\""<<owner_id(s.owner)<<"\""
 <<",\"gameplay\":"<<(gameplay_enabled()?"true":"false")<<",\"character\":\""<<(gameplay_enabled()?roster().selected().id:"")<<"\""
 <<",\"stat_count\":"<<(s.progression?s.progression->profile(roster().selected().id).stats.size():0)<<",\"last_damage\":"<<s.last_damage<<",\"last_bonus\":"<<s.last_bonus
 <<",\"saves\":"<<s.saves<<",\"loads\":"<<s.loads<<",\"request_id\":"<<s.request_id<<",\"conversation_id\":"<<s.conversation_id
 <<",\"selection\":"<<s.selected<<",\"choice\":"<<s.choice<<",\"owner\":"<<s.owner<<",\"bindings\":"<<bindings.size()<<",\"seeded\":"<<s.seeded.size()<<",\"hp\":"<<(a?a->health:0)<<",\"fruit\":"<<s.fruit()
 <<",\"equipped\":"<<(s.equipped()?"true":"false")<<",\"assigned\":"<<(s.assigned()?"true":"false")<<",\"receipts\":"<<s.inventory.capture().receipts.size()<<",\"damage_notifications\":"<<s.damage_notifications
 <<",\"bor_tick\":"<<world.tick<<",\"actor_count\":"<<world.actor_count<<",\"platform_x\":"<<world.platform_x<<",\"platform_y\":"<<world.platform_y
 <<",\"x\":"<<(a?a->x:0)<<",\"y\":"<<(a?a->y:0)<<",\"vx\":"<<(a?a->vx:0)<<",\"vy\":"<<(a?a->vy:0)<<",\"action_id\":"<<(a?a->action_id:0)<<",\"actor_state\":"<<(a?static_cast<int>(a->state):0)<<",\"inventory_owners\":"<<(bag?1:0);
 // Fixed-capacity main-thread diagnostic snapshot; never used by hearing or audio callbacks. Signed: Mech.
 BslOwnedVoiceSnapshot sound[32]{};const unsigned sound_count=bsl_voice_snapshot(sound,32);
 out<<",\"owned_audio_paused\":"<<(bsl_voice_paused()?"true":"false")<<",\"owned_audio_voices\":[";
 for(unsigned i=0;i<sound_count;++i){const auto& v=sound[i];if(i)out<<",";out<<"{\"id\":"<<v.id<<",\"owner\":"<<v.owner<<",\"position\":"<<v.voice.position<<",\"playing\":"<<(v.voice.playing?"true":"false")<<",\"gain\":"<<v.voice.gain<<",\"rate\":"<<v.voice.rate<<"}";}out<<"]";
 editor_records(out,s);out<<"}\n";
}
bsl::RewindProgress checkpoint_progress(){
 auto& s=*session;const auto& r=roster();bsl::RewindProgress p;
 p.mode=r.profile().mode;p.save=r.profile().save;p.player=r.profile().player;p.selected=r.selected().definition;
 for(const auto& d:r.profile().characters){const auto& c=*r.character(d.id);auto at=bindings.find(c.actor);p.health.emplace(d.id,at==bindings.end()?c.detached_health:at->second->health);p.stats.emplace(c.id,s.progression->capture(c.id));}
 p.learning=s.learning->capture();p.inventory=s.inventory.capture();p.events=s.events.capture();p.events.runs.clear();p.calendar=s.calendar_seconds;return p;
}
bsl::Code checkpoint_capture(const bsl::CheckpointDef* def){
 if(bsl_host_boot_busy()||!session||!session->checkpoints||!session->alive())return bsl::Code::disabled;
 auto& s=*session;auto* a=s.actor();
 if(bsl_engine_editor()||s.captures_commands||s.modal()||!a->grounded||a->state!=BOR_FREE||a->freeze>0||a->timeline_fault||a->partner>=0)return bsl::Code::denied;
 if(def){double x=0,y=0;if(def->level!=s.checkpoints->level()||!bsl_bor_checkpoint_site(def->placement,def->kind==bsl::CheckpointKind::permanent,&x,&y)||std::hypot(a->x-x,a->y-y)>def->range||std::abs(a->vx)>1||std::abs(a->vy)>1)return bsl::Code::denied;}
 bsl::CheckpointRecord record;record.id=def?def->id:"checkpoint:level_entry";record.level=s.checkpoints->level();record.kind=def?def->kind:bsl::CheckpointKind::entry;record.priority=def?def->priority:0;record.x=a->x;record.y=a->y;record.facing=a->facing;record.progress=checkpoint_progress();if(s.loot)record.loot=s.loot->capture();
 const bool permanent=record.kind==bsl::CheckpointKind::permanent;
 if(s.quests)s.quests->validate_progress(record.progress.events,record.progress.learning);
 if(permanent)record.progress.health.at(record.progress.selected)=a->model->health;
 std::string notice=permanent?"Permanent checkpoint activated: HP recovered; save-offer signal available.":def?"Temporary checkpoint captured; rewind returns to this activation.":"Level-entry rewind point initialized.";
 s.checkpoints->record(std::move(record));if(permanent)a->health=a->model->health;
 if(def){s.save_offer=permanent;}
 s.checkpoint_fault=false;++s.checkpoint_captures;s.notice.swap(notice);return bsl::Code::ok;
}
bool checkpoint_restore(const bsl::CheckpointRecord* resume=nullptr){
 auto& s=*session;if(!resume)s.rewind_requested=false;
 const auto candidates=resume?std::vector<const bsl::CheckpointRecord*>{resume}:s.checkpoints->candidates();
 for(const auto* record:candidates){
  try{
   const auto& p=record->progress;const auto& profile=roster().profile();
   if(p.mode!=profile.mode||p.save!=profile.save||p.player!=profile.player||p.stats.size()!=profile.characters.size()||!std::isfinite(p.calendar)||p.calendar<0||p.calendar>1e12)throw std::invalid_argument("checkpoint identity/progress mismatch");
   if(s.clock&&!s.clock->calendar().valid(p.calendar))throw std::invalid_argument("checkpoint calendar out of range");
   auto next_roster=std::make_unique<bsl::PlayerRoster>(roster());
   if(next_roster->restore_progress(p.selected,p.health)!=bsl::Code::ok)throw std::invalid_argument("checkpoint character values rejected");
   std::unique_ptr<BslCharacterReconstruction,decltype(&bsl_bor_checkpoint_character_discard)> prepared(nullptr,bsl_bor_checkpoint_character_discard);
   auto target=next_roster->selected().actor;
   if(!target){
    prepared.reset(bsl_bor_checkpoint_character_prepare(p.selected.c_str(),record->x,record->y));
    target=bsl_bor_checkpoint_character_actor(prepared.get());auto body=bindings.find(target);
    if(!target||body==bindings.end())throw std::runtime_error("Checkpoint character factory unavailable or refused preparation.");
    if(next_roster->attach(p.selected,target,body->second->model->name,body->second->model->health)!=bsl::Code::ok)throw std::runtime_error("Checkpoint character preparation rejected.");
   }
   auto actor=bindings.find(target);if(actor==bindings.end())continue;
   double x=0,y=0;if(!bsl_bor_checkpoint_spot(target,record->x,record->y,true,&x,&y)){if(std::getenv("BOR_REWIND_TRACE"))std::fprintf(stderr,"BSL selected rest spot refused at %.2f,%.2f\n",record->x,record->y);continue;}
   auto next_stats=std::make_unique<bsl::CharacterStats>(*s.progression);
   for(const auto& d:profile.characters){const auto& c=*next_roster->character(d.id);auto value=p.stats.find(c.id);if(value==p.stats.end()||next_stats->restore(c.id,value->second)!=bsl::Code::ok)throw std::invalid_argument("checkpoint stat values rejected");}
   next_stats->clear_actions();auto next_learning=std::make_unique<bsl::CharacterLearning>(*next_stats,compiled_learning_catalog());
   for(const auto& d:profile.characters)next_learning->refresh(next_roster->character(d.id)->id);
   if(next_learning->restore(p.learning)!=bsl::Code::ok)throw std::invalid_argument("checkpoint learned state rejected");
   for(const auto& bag:p.inventory.bags)if(bag.first!=profile.player)throw std::invalid_argument("checkpoint bag owner rejected");
   for(const auto& value:p.events.values)if(!value.first.second.empty()&&value.first.second!=profile.player)throw std::invalid_argument("checkpoint global owner rejected");
   if(s.quests)s.quests->validate_progress(p.events,p.learning);
   bsl::RewindServices services(s.inventory,s.events,p.inventory,p.events);
   std::unique_ptr<bsl::LootDrops> next_loot;
   if(s.loot){next_loot=std::make_unique<bsl::LootDrops>(*s.loot);
    auto state=next_loot->capture();if(record->loot&&record->loot->epoch==state.epoch)next_loot->restore(*record->loot);
    else{state.outcomes.clear();next_loot->restore(state);} // Fresh level/disk resume never revives old transient drops.
   }
   std::vector<BslActorRewind> actors;
   for(const auto& binding:bindings){int hp=binding.second->model->health;
    if(const auto* c=next_roster->bound(binding.first))hp=p.health.at(c->definition);
    else {auto receipt=p.learning.receipts.find("defeat/"+std::to_string(binding.first));const auto* source=s.learning->defeat(binding.second->model->name);
     if(source&&receipt!=p.learning.receipts.end()&&receipt->second.source==source->id&&p.stats.count(receipt->second.character))hp=0;
     // A profile may reject XP while the accepted native defeat still owns a loot outcome.
     if(next_loot){auto death=next_loot->state().outcomes.find(binding.first);if(death!=next_loot->state().outcomes.end()&&next_loot->table(death->second.table).model==binding.second->model->name)hp=0;}
    }
    double px=binding.first==target?x:binding.second->spawn_x,py=binding.first==target?y:binding.second->spawn_y;
    if(hp>0&&!bsl_bor_checkpoint_spot(binding.first,px,py,binding.first==target,&px,&py))throw std::invalid_argument(std::string("actor reset point has no clear supported terrain: ")+binding.second->model->name+" at "+std::to_string(px)+","+std::to_string(py));
    actors.push_back({binding.first,hp,px,py});
   }
   std::unique_ptr<BslRewindPlan,decltype(&bsl_bor_rewind_discard)> bodies(bsl_bor_rewind_prepare(actors.data(),static_cast<unsigned>(actors.size()),target),bsl_bor_rewind_discard);
   if(!bodies){if(std::getenv("BOR_REWIND_TRACE")){std::fprintf(stderr,"BSL rewind body preflight refused target=%llu count=%u\n",static_cast<unsigned long long>(target),static_cast<unsigned>(actors.size()));for(const auto& plan:actors)std::fprintf(stderr,"  actor=%llu hp=%d at=%.2f,%.2f\n",static_cast<unsigned long long>(plan.lifetime),plan.health,plan.x,plan.y);}continue;}
   std::unique_ptr<bsl::CheckpointBook> next_book;
   if(resume){next_book=std::make_unique<bsl::CheckpointBook>();next_book->enter(record->level);next_book->record(*record);}
   auto effects=s.effects;effects.unbind(profile.player);effects.bind(profile.player,*actor->second);
   std::string restored=record->id;
   std::string notice="Rewound to "+record->id+"; selected character, player bag and registered progress restored.";
   // Commit only after every service and spatial preflight has succeeded. No script/event runs resume.
   s.cancel();services.commit();s.progression.swap(next_stats);s.learning.swap(next_learning);if(s.event_rewards)s.event_rewards->rebind(*s.learning);if(s.event_skills)s.event_skills->rebind(*s.learning);if(s.quests)s.quests->rebind(*s.learning);s.loot.swap(next_loot);gameplay_roster.swap(next_roster);bsl_bor_checkpoint_character_publish(prepared.get());s.effects=std::move(effects);
   s.owner=target;s.calendar_seconds=p.calendar;s.weather_override.reset();bsl_particles_clear();bsl_ambient_clear();bsl_acoustic_clear();s.used_actions.clear();s.anchor_runs.clear();s.conversation_runs.clear();s.talking.reset();s.last_event.clear();s.interaction_requested=false;s.interaction_subject=0;
   s.selected=s.category_index=s.choice=s.choice_first=s.choice_last=0;s.last_damage=s.last_bonus=s.last_reward_code=0;s.reward_refused=0;s.loot_refused=0;
   s.rewind_requested=false;s.barrier=true;s.captures_commands=true;s.death_ticks=0;s.checkpoint_fault=false;s.save_offer=record->kind==bsl::CheckpointKind::permanent;++s.checkpoint_rewinds;s.notice.swap(notice);s.last_checkpoint.swap(restored);
   if(next_book)s.checkpoints.swap(next_book);
   bsl_ui_reset_presentation();bsl_bor_rewind_apply(bodies.get(),actors.data(),static_cast<unsigned>(actors.size()),target,x,y,record->facing);bsl_bor_capture_input();return true;
  }catch(const std::exception& e){std::fprintf(stderr,"BSL checkpoint candidate %s refused: %s\n",record->id.c_str(),e.what());}
 }
 if(!resume){++s.checkpoint_failures;s.checkpoint_fault=true;}
 s.notice="Checkpoint rewind refused: no valid progress bundle and clear return point. Current progress retained.";return false;
}
bsl::ProgressSaveStore progress_store(){return bsl::ProgressSaveStore(session->catalog,roster().profile(),compiled_stat_catalog(),compiled_learning_catalog(),compiled_progress_save_policy(),compiled_checkpoints());}
bool progress_tick(){
 if(!session||!session->pending_progress)return false;
 auto& s=*session;
 try{
  const auto& d=*s.pending_progress;auto defs=compiled_checkpoints();const auto found=std::find_if(defs.begin(),defs.end(),[&](const auto& p){return p.id==d.checkpoint;});
  double x=0,y=0;if(found==defs.end()||found->level!=bsl_engine_level_file()||!bsl_bor_checkpoint_site(found->placement,true,&x,&y))throw std::runtime_error("Saved rest point is unavailable in this loaded level.");
  auto record=progress_store().reconstruct(d,x,y);
  std::string success="Compact progress loaded at its authored rest point; actors reconstructed, unfinished events cancelled.";
  if(!checkpoint_restore(&record))throw std::runtime_error("Saved progress has no safe reconstruction; current progress retained.");
  ++s.loads;s.notice.swap(success);
 }catch(const std::exception& e){++s.progress_failures;s.notice=std::string("Progress not loaded: ")+e.what();}
 s.pending_progress.reset();return true;
}
bool checkpoint_tick(){
 if(!session||!session->checkpoints)return false;
 auto& s=*session;
 if(!s.checkpoints->entry()&&s.alive()&&s.actor()->grounded&&s.actor()->state==BOR_FREE)checkpoint_capture(nullptr);
 if(!s.alive()&&s.actor()&&!s.checkpoint_fault){if(++s.death_ticks>=compiled_checkpoint_death_ticks())s.rewind_requested=true;}else if(s.alive())s.death_ticks=0;
 if(!s.rewind_requested)return false;
 if(teleports_enabled())for(const auto* r:s.checkpoints->candidates(true)){
  if(r->level==s.checkpoints->level())break;
  if(travel_checkpoint(*r))return true;
 }
 checkpoint_restore();return true;
}
bool preupdate(){
 if(!session)return false;
 session->resolve_actor_template_defaults();
 session->resolve_actor_placements();
 // Registry-driven setup may attach the selected body after the first preupdate. Signed: Mech.
 if(progress_boot_enabled()&&!continue_attempted&&session->alive()){continue_attempted=true;const char* value=std::getenv("BSL_PROGRESS_CONTINUE");if(value&&std::string(value)=="1")bsl_host_progress_load();}
 if(travel){
  if(++travel->wait>=180){if(travel->phase==TravelPhase::assembling){travel_failure("Destination actor setup timed out.");return true;}else if(travel->phase==TravelPhase::returning){travel->phase=TravelPhase::failed;session->notice="Teleport recovery stopped; source setup unavailable. Esc quits.";}}
  if(travel->phase==TravelPhase::failed){ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);if(al_key_down(&keys,ALLEGRO_KEY_ESCAPE))bsl_engine_quit();return true;}
  return travel->phase==TravelPhase::queued||(travel->phase==TravelPhase::returning&&!travel->source_loaded);
 }
 if(boot){
  if((boot->phase==BootPhase::assembling||(boot->phase==BootPhase::returning&&boot->return_loaded))&&++boot->wait>=120){if(boot->phase==BootPhase::assembling)boot_fail("Destination did not complete native setup.");else boot_stop("Source level did not complete native setup.");return true;}
  if(boot->phase==BootPhase::failed){ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);if(!bsl_shell_keyboard()&&al_key_down(&keys,ALLEGRO_KEY_R)&&!session->previous[ALLEGRO_KEY_R]&&bsl_engine_has_level(boot->origin.c_str())){boot->phase=BootPhase::returning;boot->return_loaded=false;boot->wait=0;bsl_engine_load_level(boot->origin.c_str());}if(!bsl_shell_keyboard()&&al_key_down(&keys,ALLEGRO_KEY_ESCAPE)&&!session->previous[ALLEGRO_KEY_ESCAPE])bsl_engine_quit();for(int i=0;i<ALLEGRO_KEY_MAX;i++)session->previous[i]=al_key_down(&keys,i);return true;}
  return boot->phase==BootPhase::queued||(boot->phase==BootPhase::returning&&!boot->return_loaded);
 }

 auto& s=*session;
 // Only explicit bootstrap requirements are monitored; later actor removal is not startup failure.
 if(gameplay_enabled()&&s.player_required&&!s.player_ready&&!bsl_engine_editor()){
  const auto id=roster().selected().actor;
  if(id&&bindings.count(id))s.player_assembled();
  else if(s.player_wait<120&&++s.player_wait==120){
   s.player_failed=true;s.barrier=true;s.captures_commands=true;s.cancel();bsl_ui_reset_presentation();
   if(!s.player_pause)s.player_pause=s.policies.menu("player-assembly",true);
   const auto& selected=roster().selected();const auto* def=roster().definition(selected.definition);
   s.player_error="PLAYER SETUP: "+(def?def->name:selected.definition)+"\n"+selected.definition+" has no world body.\nF12: clear spawn; leave editor. R: retry here (keeps edits/state). Esc: quit.";
   bsl_bor_capture_input();
  }
  if(s.player_failed){
   ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);
   if(!bsl_shell_keyboard()&&al_key_down(&keys,ALLEGRO_KEY_R)&&!s.previous[ALLEGRO_KEY_R])bsl_host_player_retry();
   if(!bsl_shell_keyboard()&&al_key_down(&keys,ALLEGRO_KEY_ESCAPE)&&!s.previous[ALLEGRO_KEY_ESCAPE])bsl_engine_quit();
   for(int i=0;i<ALLEGRO_KEY_MAX;i++)s.previous[i]=al_key_down(&keys,i);
   double dt=bsl_engine_delta();if(!std::isfinite(dt)||dt<0)dt=0;s.ui_seconds+=dt;++s.ui_frames;
   bsl_ambient_tick(0,true);bsl_acoustic_tick(0,true);trace();return true;
  }
  if(!s.player_ready){
   // Continue script construction/spatial refresh, while native gameplay and its clock wait.
   ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);for(int i=0;i<ALLEGRO_KEY_MAX;i++)s.previous[i]=al_key_down(&keys,i);
   double dt=bsl_engine_delta();if(!std::isfinite(dt)||dt<0)dt=0;s.ui_seconds+=dt;++s.ui_frames;s.captures_commands=true;
   bsl_ambient_tick(0,true);bsl_acoustic_tick(0,true);bsl_ui_update(s.ui_seconds,false,0,false,false);trace();return false;
  }
 }
 ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);
 auto held=[&](int key){return !bsl_shell_keyboard()&&al_key_down(&keys,key);};
 bool native_menu_confirm_edge=false;
 auto edge=[&](int key){return (held(key)&&!s.previous[key])||(key==ALLEGRO_KEY_H&&native_menu_confirm_edge);};
 bool consume_frame=false;
 if(!bsl_shell_present()&&edge(ALLEGRO_KEY_F8)){bsl_ui_cycle_layout();consume_frame=true;}
 if(gameplay_enabled()&&!s.modal()&&!bsl_engine_editor()&&edge(ALLEGRO_KEY_F9)){
  const auto& chars=roster().profile().characters;size_t next=0;for(size_t i=0;i<chars.size();++i)if(chars[i].id==roster().selected().definition)next=(i+1)%chars.size();
  bsl_host_select_character(chars[next].id.c_str());consume_frame=true;
 }
 s.validate_actor_conversation();
 if(!s.alive()){if(s.modal())s.cancel();}
 if(bsl_engine_editor()){
  bsl_ambient_tick(0,true);bsl_acoustic_tick(0,true);
  if(s.modal())s.cancel();
  for(int i=0;i<ALLEGRO_KEY_MAX;++i)s.previous[i]=held(i);
  trace();
  return false;
 }
 bool was_modal=s.modal();
 if(ps_ui_enabled()&&s.dialogue_open()&&!s.menu){
  PsUiCommand command{};while(ps_ui_command(s.talking?s.talking->initiator:s.owner,s.conversation_id,&command)){
   if(!s.dialogue_open())break;
   if(command.action==PS_UI_SELECT)s.choice=command.index;
   else if(command.action==PS_UI_CONFIRM){s.choose_dialogue(command.index);consume_frame=true;}
   else {s.conversation.cancel();consume_frame=true;}
  }
 }
 if(s.menu&&!edge(ALLEGRO_KEY_ESCAPE)&&!edge(ALLEGRO_KEY_J)&&!edge(ALLEGRO_KEY_I)&&!edge(ALLEGRO_KEY_P)&&!edge(ALLEGRO_KEY_F12)){
  PsUiMenuCommand command{};while(ps_ui_menu_command(s.owner,s.menu_revision,&command)){
   if(!s.menu)break;
   bool item=command.action==PS_MENU_SELECT||command.action==PS_MENU_ACTIVATE||command.action==PS_MENU_ASSIGN;
   if(item){auto rows=s.rows();if((s.menu_page!=1&&s.menu_page!=2)||command.index<0||size_t(command.index)>=rows.size())continue;const auto& entry=rows[command.index];if(entry.selection.definition!=command.definition||entry.selection.instance!=command.instance)continue;s.selected=command.index;}
   switch(command.action){
    case PS_MENU_SELECT:break;
    case PS_MENU_ACTIVATE:if(ps_lab_automatic()&&s.menu_test_mix_confirm){native_menu_confirm_edge=true;s.menu_test_mix_confirm=false;s.menu_test_mix_applied=true;}s.menu_confirm();break;
    case PS_MENU_ASSIGN:++s.menu_revision;s.menu_action(true);break;
    case PS_MENU_CATEGORY:if(s.menu_page==1||s.menu_page==2)s.change_category(command.index);break;
    case PS_MENU_PAGE:for(int i=0;i<bsl_ui_menu_option_count();i++)if(bsl_ui_menu_option_page(i)==command.index){s.menu_page=0;s.menu_row=i;s.menu_confirm();break;}break;
    case PS_MENU_BACK:s.menu_back();break;
    case PS_MENU_RESUME:s.close_menu();break;
   }consume_frame=true;
  }
 }
 if(edge(ALLEGRO_KEY_F12)){s.cancel();bsl_ui_reset_presentation();}
 else if(s.alive()){
  if(edge(ALLEGRO_KEY_I)){
   if(s.menu)s.close_menu();else s.open_menu(1);
   s.barrier=true;consume_frame=true;
  }else if(edge(ALLEGRO_KEY_P)||(!s.modal()&&edge(ALLEGRO_KEY_ESCAPE))){
   if(s.menu)s.close_menu();else s.open_menu(0);
   s.barrier=true;consume_frame=true;
  }else if(!s.menu&&edge(ALLEGRO_KEY_T)){
   if(s.dialogue_open())s.conversation.cancel();
   else{s.talking.reset();s.conversation.open(held(ALLEGRO_KEY_LSHIFT)?"dialogue:choice_list":"dialogue:training_choice",owner_id(s.owner),{owner_id(s.owner)});s.choice=0;s.choice_first=0;++s.conversation_id;}
   s.barrier=true;consume_frame=true;
  }else if(s.modal()&&(!ps_ui_enabled()||s.menu)&&!consume_frame){
   int delta=(edge(ALLEGRO_KEY_DOWN)||edge(ALLEGRO_KEY_S))-(edge(ALLEGRO_KEY_UP)||edge(ALLEGRO_KEY_W));
   if(s.menu){
    if(s.menu_page==1||s.menu_page==2){s.change_category((edge(ALLEGRO_KEY_E))-(edge(ALLEGRO_KEY_Q)));delta=delta*bsl_ui_inventory_columns()+(edge(ALLEGRO_KEY_RIGHT)||edge(ALLEGRO_KEY_D))-(edge(ALLEGRO_KEY_LEFT)||edge(ALLEGRO_KEY_A));}
    s.menu_navigate(delta);
   }else {const int count=static_cast<int>(s.conversation.definition()->choices.size());s.choice=(s.choice+delta+count)%count;}
   if(edge(ALLEGRO_KEY_ESCAPE)||edge(ALLEGRO_KEY_J)){
    if(s.menu)s.menu_back();else s.conversation.cancel();consume_frame=true;
   }else if(s.menu&&edge(ALLEGRO_KEY_R)){
    s.cancel();bsl_engine_restart();consume_frame=true;
   }else if(s.menu&&edge(ALLEGRO_KEY_K)){
    if(s.menu_page==1||s.menu_page==2)s.menu_action(true);
    consume_frame=true;
   }else if(edge(ALLEGRO_KEY_ENTER)||edge(ALLEGRO_KEY_H)){
    if(s.menu)s.menu_confirm();
    else s.choose_dialogue(s.choice);
    consume_frame=true;
   }
  }
 }
 if(s.modal()||was_modal||consume_frame)s.barrier=true;
 if(s.barrier&&!s.modal()&&!bsl_bor_input_held())s.barrier=false;
 if(s.barrier||s.modal()||consume_frame)bsl_bor_capture_input();
 double dt=bsl_engine_delta();if(!std::isfinite(dt)||dt<0)dt=0;
 auto slice=s.policies.sample(dt);s.ui_seconds+=slice.ui;s.simulation_seconds+=slice.simulation;if(s.clock){if(s.clock->advance(slice.calendar)!=bsl::Code::ok)s.notice="Calendar limit reached; time retained.";}else s.calendar_seconds+=slice.calendar;++s.ui_frames;
 bsl_particles_tick(slice.simulation);bsl_ambient_tick(slice.simulation,s.policies.paused());bsl_acoustic_tick(0,s.policies.paused());
 s.dialogue_presentation();
 bool show_dialogue=s.dialogue_open()&&!s.menu&&!bsl_ui_menu_visible();
 bool show_help=show_dialogue&&!s.conversation.definition()->choice_help.empty()&&!s.conversation.definition()->choice_help.at(static_cast<std::size_t>(s.choice)).empty();
 bsl_ui_update(s.ui_seconds,s.menu!=0,s.menu_page,show_dialogue,show_help);

 const bool interact_edge=edge(ALLEGRO_KEY_E);
 const bool reward_edge=edge(ALLEGRO_KEY_V);
 const bool checkpoint_edge=edge(ALLEGRO_KEY_C),rewind_edge=edge(ALLEGRO_KEY_BACKSPACE);
 for(int i=0;i<ALLEGRO_KEY_MAX;++i)s.previous[i]=held(i);
 s.captures_commands=s.modal()||was_modal||consume_frame||s.barrier;
 if(s.captures_commands)s.interaction_requested=false;
 if(!s.captures_commands&&s.alive()&&interact_edge){s.interaction_requested=true;s.interaction_subject=s.owner;}
 if(s.learning&&!s.captures_commands&&s.alive()&&reward_edge)bsl_host_reward(s.owner,"source:lab_exploration","lab/exploration");
 if(s.checkpoints&&!s.captures_commands&&checkpoint_edge&&s.alive()){
  const bsl::CheckpointDef* nearest=nullptr;double best=1e30;
  const auto definitions=compiled_checkpoints();
  for(const auto& def:definitions)if(def.level==s.checkpoints->level()){
   double x=0,y=0;if(!bsl_bor_checkpoint_site(def.placement,def.kind==bsl::CheckpointKind::permanent,&x,&y))continue;
   const auto* a=s.actor();double distance=std::hypot(a->x-x,a->y-y);
   if(distance<=def.range&&distance<best){best=distance;nearest=&def;}
  }
  // Keep a named definition copy alive while its capture operation executes.
  if(nearest){auto def=*nearest;checkpoint_capture(&def);}else s.notice="No checkpoint within activation range.";
 }
 if(s.checkpoints&&!s.captures_commands&&rewind_edge)bsl_host_checkpoint_request();

 if(s.dialogue_open()){
  const auto& choices=s.conversation.definition()->choices;const char* labels[64];
  for(std::size_t i=0;i<choices.size();++i)labels[i]=choices[i].c_str();
  bsl_ui_choice_view(labels,static_cast<int>(choices.size()),s.choice,&s.choice_first,&s.choice_last);
 }

 trace();return s.policies.paused();
}
void render(){
 if(!session||bsl_engine_editor())return;
 bsl_particles_render();bsl_acoustic_render();
 auto& s=*session;auto* a=s.actor();s.dialogue_presentation();
 if(s.player_failed){bsl_ui_draw(s.player_error.c_str(),"","",s.ui_seconds);return;}
 auto compact=[](std::string text,std::size_t limit){
  for(char& ch:text)if(ch=='\n'||ch=='\r'||ch=='\t')ch=' ';
  if(text.size()>limit){std::size_t at=limit;while(at&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;text=text.substr(0,at)+"...";}return text;
 };
 std::string assigned_name="none";const auto* bag=s.inventory.bag(owner_id(s.owner));
 if(bag){auto slot=bag->assigned.find("ranged");if(slot!=bag->assigned.end()){auto* item=s.catalog.item(slot->second.definition);if(item)assigned_name=compact(item->name,50);}}
 std::ostringstream status;status<<"DATA LAB  |  I Inventory   T Dialogue   R Reset   F12 Level editor\nHP "<<(a?a->health:0)<<"/"<<(a&&a->model?a->model->health:0)<<"   Items "<<s.inventory.entries(owner_id(s.owner)).size()<<"   K: "<<assigned_name<<"   Calendar "<<std::fixed<<std::setprecision(1)<<s.calendar_seconds<<"s";
 status<<"\nF8: window layout   Left Shift+T: long choice test";
 auto stat_text=[&]{std::ostringstream text;if(s.progression)for(const auto& id:s.progression->profile(roster().selected().id).stats){auto v=s.progression->view(roster().selected().id,id);auto effective=s.progression->effective_units(roster().selected().id,id,s.modifiers(roster().selected().id));text<<s.progression->definition(id).name<<": "<<std::fixed<<std::setprecision(3)<<static_cast<double>(v.units)/v.scale;if(effective!=v.units)text<<" effective "<<static_cast<double>(effective)/v.scale;if(v.stage)text<<"  stage "<<v.stage<<" XP "<<static_cast<double>(v.into)/v.xp_scale<<"/"<<static_cast<double>(v.next)/v.xp_scale;text<<"\n";}return text.str();};
 if(s.progression)status<<"\n"<<stat_text()<<"Last HP damage "<<s.last_damage<<" (+"<<s.last_bonus<<")";
 if(s.learning){const auto* ability=s.learning->slot(roster().selected().id,0);status<<"\nV: claim exploration objective (once/run)   "<<(ability&&!ability->capability.empty()?"Jump twice: learned traversal capability":"U: extra lab ability")<<"\n";
  status<<(ability?ability->name:"No learned-action slot in this profile")<<(ability?(s.learning->learned(roster().selected().id,ability->id)?" [unlocked]":" [locked]"):"")<<"\n"<<compact(s.notice,140);}
  if(s.reward_refused)status<<"\nREWARDS REFUSED: "<<s.reward_refused<<" (last code "<<s.last_reward_code<<"). Finish/reset this bounded lab run.";

 if(s.loot){for(const auto& pair:s.loot->state().outcomes){const auto& drop=pair.second;if(drop.item.empty()||drop.collected)continue;
  const auto* item=s.catalog.item(drop.item);auto label=std::to_string(drop.quantity)+" "+item->name+" / Down + Ctrl-LT";bsl_ui_loot_marker(drop.x,drop.y,label.c_str());}
  if(s.loot_refused)status<<"\nLOOT REFUSED: "<<s.loot_refused<<"; see development log.";
 }
 if(pickups_enabled())status<<"\nPickups: hold Down, then press Ctrl/LT (no attack chord).";
 if(s.clock){auto date=s.clock->date();status<<"\n"<<date.month_name<<" "<<date.day<<", "<<date.year<<" "<<date.era<<"  "<<std::setfill('0')<<std::setw(2)<<date.hour<<":"<<std::setw(2)<<date.minute<<"  "<<date.phase;}
 if(s.checkpoints){auto candidates=s.checkpoints->candidates(teleports_enabled());status<<"\nC: activate nearby checkpoint   Backspace: test rewind\nCheckpoint: "<<(candidates.empty()?"none":candidates.front()->id)<<"   Rewinds "<<s.checkpoint_rewinds;
  if(s.save_offer)status<<(progress_save_enabled()?"   [Save rest point: Menu > Quit Game]":"   [permanent save-offer signal]");
  if(s.checkpoint_failures)status<<"   RESTORE FAILURES "<<s.checkpoint_failures;
 }
 std::string title,body;
 const bool show_menu=s.menu||bsl_ui_menu_visible();
 if(show_menu){
  const int page=s.menu_page?s.menu_page:s.last_menu_page;
  title=bsl_ui_menu_page_title(page);
  if(page==1||page==2){
   s.normalize_selection();auto entries=s.rows();auto groups=s.categories();body="Category: "+groups[s.category_index]+"   (Q/E change)\n";
   if(entries.empty())body+="No items in this category.\n";
   else {const auto& item=entries[s.selected];body+=compact(item.name,60)+": "+compact(item.description,140)+"\n";body+="Action: "+std::string(page==2?"Assign to subweapon action":!item.slot.empty()?(item.equipped?"Unequip":"Equip"):item.usable?"Use":"No use action")+"\n";}
   body+=(ps_ui_menu_active()?std::string("\n"):std::string("Arrows/WASD: select   H/Enter: action   K: assign\nQ/E: category   J/Esc: back   I/P: resume\n"))+compact(s.notice,96);
  }else if(page==3){
   std::string equipped="none";for(const auto& item:s.inventory.entries(owner_id(s.owner)))if(item.equipped)equipped=item.name;
   BslCharacterDefinition display{};const char* title=gameplay_enabled()&&bsl_host_character_definition(roster().selected().definition.c_str(),&display)==0?display.name:"Hokusai Michio";
   body=std::string(title)+"\n\nHP: "+std::to_string(a?a->health:0)+" / "+std::to_string(a&&a->model?a->model->health:0)+"\nWeapon: "+equipped+"\nAssigned subweapon: "+assigned_name+"\n\nMP is not configured.\n\nJ/Esc: back   I/P: resume";
   if(s.progression)body+="\n"+stat_text();

   if(s.commerce){for(const auto& c:s.commerce->definitions().currencies)body+="\n"+c.name+": "+std::to_string(s.commerce->balance(c.id,owner_id(s.owner)));for(const auto& shop:s.commerce->definitions().shops)for(const auto& o:shop.offers){const int available=s.commerce->stock(shop.id,o.id,owner_id(s.owner));body+="\n"+shop.name+" stock: "+(available<0?std::string("unlimited"):std::to_string(available));}}
  }else if(page==7){
   if(s.quests){const char* states[]={"locked","available","active","ready","completed","invalid"};for(const auto& q:s.quests->definitions()){const auto view=s.quests->view(q.id,owner_id(s.owner));body+="\n\n"+q.name+" ["+states[static_cast<int>(view.status)]+"]";for(const auto& o:view.objectives)body+="\n"+o.label+": "+std::to_string(o.current)+" / "+std::to_string(o.required);}}
   if(body.empty())body="No quests are registered.";
   body+="\n\nJ/Esc: back   I/P: resume";
  }else if(page==4)body="No map has been assigned to this level.\n\nArea/world map services remain planned.\n\nJ/Esc: back   I/P: resume";
  else if(page==5){
   const std::string options[]={std::string("UI animation: ")+(bsl_ui_reduced_motion()?"off":"on"),"Cycle dialogue layout","Back"};
   for(int i=0;i<3;++i)body+=(s.menu_row==i?"> ":"  ")+options[i]+"\n";
   body+="\nEnter/H: change   J/Esc: back\nPresentation preferences affect this session only.";
  }else if(page==6){
   const bool progress=progress_save_enabled();
   const char* options[]={progress?"Save activated rest point":"Save lab data",progress?"Load saved progress":"Load lab data",progress?"Recover previous progress":"Recover previous lab data","Quit Without Saving","Cancel"};
   for(int i=0;i<5;++i)body+=std::string(s.menu_row==i?"> ":"  ")+options[i]+"\n";
   body+=(progress?"\nSave the last permanent activation, including its selected progress.\nLoading reconstructs actors at the authored rest point.\n":"\nLab data only: inventory, event state and player HP.\nWorld position, enemies and physics stay live.\n");
   body+="\nEnter/H: action   J/Esc: back\n"+compact(s.notice,240);
  }
 }
 bsl_ui_draw(status.str().c_str(),"","",s.ui_seconds);
 if(ps_ui_enabled()&&!show_menu)ps_ui_meter(s.owner,a?a->health:0,0,a&&a->model?a->model->health:0,ps_ui_preview_mp());
 std::string character_name="Hokusai Michio";if(gameplay_enabled()){const auto* c=roster().bound(s.owner);const auto* d=c?roster().definition(c->definition):nullptr;if(d)character_name=d->name;}
 if(show_menu){auto entries=s.rows();std::vector<BslInventoryCell> cells;for(const auto& item:entries)cells.push_back({item.name.c_str(),item.quantity,item.equipped,item.assigned});std::string month,day,year,time;if(s.clock){auto d=s.clock->date();month=d.month_name;day=std::to_string(d.day);year=std::to_string(d.year)+" "+d.era;std::ostringstream hh;hh<<std::setfill('0')<<std::setw(2)<<d.hour<<":"<<std::setw(2)<<d.minute;time=hh.str();}BslMenuView view{s.menu_page?s.menu_page:s.last_menu_page,s.menu_row,s.home_row,a?a->health:0,a&&a->model?a->model->health:0,s.calendar_seconds,character_name.c_str(),assigned_name.c_str(),title.c_str(),body.c_str(),s.clock?month.c_str():nullptr,s.clock?day.c_str():nullptr,s.clock?year.c_str():nullptr,s.clock?time.c_str():nullptr,cells.data(),static_cast<int>(cells.size()),s.selected,bag&&bag->slot_limit?bag->slot_limit:bsl_ui_inventory_default_slots()};std::vector<PsUiMenuItem> keys;for(const auto& entry:entries)keys.push_back({entry.selection.definition.c_str(),entry.selection.instance.c_str(),entry.quantity});PsUiMenuModel pointer{s.owner,s.menu_revision,s.menu!=0,s.menu_page,s.selected,static_cast<int>(keys.size()),keys.data()};ps_ui_menu_begin(&pointer);bsl_ui_menu(&view);ps_ui_menu_end();}
 else if((s.dialogue_open()||bsl_ui_dialogue_visible()||ps_ui_conversation_visible())&&s.conversation.definition()){
  const auto* d=s.conversation.definition();const char* labels[64];
  for(std::size_t i=0;i<d->choices.size();++i)labels[i]=d->choices[i].c_str();
  const char* help=d->choice_help.empty()?"":d->choice_help.at(static_cast<std::size_t>(s.choice)).c_str();
  if(ps_ui_enabled()){PsUiConversation view{s.talking?s.talking->initiator:s.owner,s.conversation_id,d->text.c_str(),labels,static_cast<int>(d->choices.size()),s.choice,help,s.ui_seconds,s.dialogue_open(),bsl_ui_reduced_motion()};ps_ui_conversation(&view);}
  else bsl_ui_conversation(d->text.c_str(),labels,static_cast<int>(d->choices.size()),s.choice,s.choice_first,help);
 }

}
}
extern "C" bool bsl_gameplay_enabled(void){return gameplay_enabled();}
extern "C" bool bsl_host_controlled(uint64_t id){return gameplay_enabled()&&gameplay_roster&&gameplay_roster->controls(id);}
// Registry queries expose authored defaults plus live attachment identity; they never spawn or select. Signed: Mech.
extern "C" int bsl_host_character_definition(const char* definition,BslCharacterDefinition* out){
 if(!out)return static_cast<int>(bsl::Code::invalid);
 *out={};int code=static_cast<int>(bsl::Code::disabled);
 guarded([&]{
  if(!gameplay_enabled())return;
  code=static_cast<int>(bsl::Code::invalid);if(!definition)return;
  const auto& r=roster();const auto* d=r.definition(definition);const auto* c=r.character(definition);if(!d||!c)return;
  std::snprintf(out->id,sizeof out->id,"%s",d->id.c_str());std::snprintf(out->name,sizeof out->name,"%s",d->name.c_str());std::snprintf(out->model,sizeof out->model,"%s",d->model.c_str());
  out->health=d->health;out->actor=c->actor;out->selected=r.selected().definition==definition;code=static_cast<int>(bsl::Code::ok);
 });return code;
}
extern "C" bool bsl_host_character_at(unsigned index,BslCharacterDefinition* out){
 if(!out)return false;
 *out={};bool ok=false;guarded([&]{if(!gameplay_enabled())return;const auto& chars=roster().profile().characters;if(index<chars.size())ok=bsl_host_character_definition(chars[index].id.c_str(),out)==0;});return ok;
}
extern "C" int bsl_host_bind_character(uint64_t id,const char* definition){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!gameplay_enabled())return;
  code=static_cast<int>(bsl::Code::invalid);auto found=bindings.find(id);if(!definition||found==bindings.end()||!found->second->model)return;
  if(*bsl_bor_actor_template_id(id)){code=static_cast<int>(bsl::Code::conflict);return;}
  auto& r=roster();if(const auto* existing=r.bound(id)){code=static_cast<int>(existing->definition==definition?bsl::Code::ok:bsl::Code::conflict);return;}auto result=r.attach(definition,id,found->second->model->name,found->second->model->health);code=static_cast<int>(result);
  if(result!=bsl::Code::ok)return;
  found->second->health=r.character(definition)->detached_health;
  if(found->second->health<=0){bor_start(found->second,"fall",BOR_DEAD,nullptr,nullptr);found->second->state=BOR_DEAD;found->second->active=false;}
  if(session&&r.controls(id))session->bind(id,*found->second);
 });return code;
}
extern "C" int bsl_host_choose_initial_character(const char* definition){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!gameplay_enabled())return;
  code=static_cast<int>(bsl::Code::invalid);if(!definition)return;
  code=static_cast<int>(bsl::Code::denied);if(bsl_host_boot_busy()||bsl_engine_editor()||(session&&session->owner))return;
  code=static_cast<int>(roster().choose_initial(definition));
 });return code;
}
extern "C" int bsl_host_select_character(const char* definition){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!gameplay_enabled()||!session)return;
  code=static_cast<int>(bsl::Code::invalid);if(!definition)return;
  auto& r=roster();const auto* next=r.character(definition);if(!next)return;
  code=static_cast<int>(bsl::Code::denied);auto at=bindings.find(next->actor);auto* old=session->actor();
  if(bsl_host_boot_busy()||at==bindings.end()||!old||session->modal()||bsl_engine_editor()||session->policies.blocked(r.profile().player)||!bsl_bor_switch_ready(session->owner)||!bsl_bor_switch_ready(next->actor))return;
  auto safe=[](BorActor* a){return a&&a->health>0&&a->state==BOR_FREE&&a->grounded&&a->partner<0&&a->freeze<=0&&!a->timeline_fault&&std::abs(a->vx)<1&&std::abs(a->vy)<1;};
  if(!safe(old)||!safe(at->second))return;
  if(r.controls(next->actor)){code=static_cast<int>(bsl::Code::ok);return;}
  auto prepared=session->effects;prepared.unbind(r.profile().player);prepared.bind(r.profile().player,*at->second);
  auto result=r.select(definition);code=static_cast<int>(result);if(result!=bsl::Code::ok)return;
  session->effects=std::move(prepared);session->events.cancel_participant(r.profile().player);session->owner=next->actor;session->interaction_requested=false;session->barrier=true;
  session->notice="Controlled character changed; the player's inventory is unchanged.";bsl_bor_capture_input();
 });return code;
}
extern "C" int bsl_host_equipment_set(uint64_t actor,const char* instance,bool equipped){int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
 if(!session||!session->equipment)return;
 code=static_cast<int>(bsl::Code::denied);if(bsl_host_boot_busy()||!instance||!roster().controls(actor)||!session->alive()||session->dialogue_open()||bsl_engine_editor())return;
 const auto* bag=session->inventory.bag(roster().profile().player);if(!bag)return;
 auto found=bag->unique.find(instance);if(found==bag->unique.end()){code=static_cast<int>(bsl::Code::invalid);return;}const auto* item=session->catalog.item(found->second);if(!item||item->slot.empty())return;
 if(!equipped){auto slot=bag->equipped.find(item->slot);if(slot==bag->equipped.end()||slot->second!=instance)return;}
 code=static_cast<int>(session->command(equipped?bsl::Op::equip:bsl::Op::unequip,equipped?found->second:"",equipped?instance:"",item->slot).code);
});return code;}
extern "C" void bsl_host_equipment_probe(const char* stat,BslEquipmentProbe* out){if(!out)return;*out={};guarded([&]{if(!session||!session->equipment)return;out->enabled=true;const auto* bag=session->inventory.bag(roster().profile().player);out->equipped=bag?static_cast<unsigned>(bag->equipped.size()):0;const auto& character=roster().selected().id;const auto values=session->progression->capture(character);if(!stat||!values.count(stat))return;out->present=true;out->base=session->progression->view(character,stat).units;out->effective=session->progression->effective_units(character,stat,session->modifiers(character));});}
extern "C" int bsl_host_use_item(const char* definition){int code=static_cast<int>(bsl::Code::disabled);guarded([&]{if(!bsl_host_boot_busy()&&session&&definition&&session->alive())code=static_cast<int>(session->command(bsl::Op::use,definition).code);});return code;}
extern "C" void bsl_host_gameplay_probe(BslGameplayProbe* out){
 if(!out)return;
 *out={};guarded([&]{if(!gameplay_enabled()||!session)return;const auto& r=roster();out->actor=r.selected().actor;auto* a=session->actor();out->health=a?a->health:0;out->items=session->fruit();std::snprintf(out->player,sizeof out->player,"%s",r.profile().player.c_str());std::snprintf(out->character,sizeof out->character,"%s",r.selected().id.c_str());});
}
extern "C" int bsl_host_action_begin(uint64_t source,uint64_t action,const char* tag){
 int bonus=0;guarded([&]{
  if(bsl_host_boot_busy()||!session||!session->progression||!bindings.count(source))return;
  const auto* c=roster().bound(source);if(!c||!tag||bindings.at(source)->action_id!=action)return;
  // Capture the initiating character; later character selection never redirects credit.
  bonus=static_cast<int>(std::clamp<std::int64_t>(session->progression->begin(c->id,source,action,tag,session->modifiers(c->id)).output("combat.damage_bonus"),0,2147483647));
  if(session->learning)session->learning->refresh(c->id);
 });return bonus;
}
extern "C" void bsl_host_combat_result(uint64_t source,uint64_t action,uint64_t target,uint64_t episode,int actual,bool defeated,int bonus){guarded([&]{
 if(bsl_host_boot_busy()||!session||!session->progression||!bindings.count(source)||!bindings.count(target)||!roster().bound(source))return;
 if(session->progression->hit(source,action,target,episode,actual,defeated)){
  session->last_damage=actual;session->last_bonus=bonus;
  if(session->learning){
   const auto character=session->progression->action_character(source,action);session->learning->refresh(character);
   const auto* victim=bindings.at(target);
   if(defeated&&victim->health==0&&victim->model&&!bor_is_player(victim)&&!victim->model->training_dummy){
    if(session->loot&&session->loot->supports(victim->model->name)){
     const auto result=session->loot->defeat(target,victim->model->name,victim->x,victim->y);
     if(result.code!=bsl::Code::ok){++session->loot_refused;session->notice="Enemy loot refused; see development log.";std::fprintf(stderr,"BSL loot refused code=%d victim=%llu\n",static_cast<int>(result.code),static_cast<unsigned long long>(target));}
    }
    if(const auto* reward=session->learning->defeat(victim->model->name)){
     const auto result=session->learning->award(character,reward->id,"defeat/"+std::to_string(target));
     if(result.code==bsl::Code::ok&&!result.replay)session->notice=result.unlocked.empty()?"Enemy reward credited to the initiating character.":"Character growth unlocked an additional registered action.";
     if(result.code!=bsl::Code::ok)session->reward_failure(result.code,reward->id,character,"defeat/"+std::to_string(target));
    }
   }
  }
 }
});}
extern "C" int bsl_host_reward(uint64_t actor,const char* source,const char* receipt){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(bsl_host_boot_busy()||!session||!session->learning||!source||!receipt||!roster().controls(actor)||!session->alive()||session->captures_commands||bsl_engine_editor())return;
  const auto* origin=session->learning->source(source);
  if(!origin||!origin->model.empty()){code=static_cast<int>(bsl::Code::denied);return;}
  const auto result=session->learning->award(roster().bound(actor)->id,source,receipt);code=static_cast<int>(result.code);
  if(result.code==bsl::Code::ok)session->notice=result.replay?"This objective reward was already claimed.":result.unlocked.empty()?"Objective reward credited to this character.":"Objective complete: registered action unlocked.";
  else session->reward_failure(result.code,source,roster().bound(actor)->id,receipt);
 });return code;
}
extern "C" bool bsl_host_capability_allowed(uint64_t actor,const char* capability){
 if(!capability||!*capability)return false;
 bool allowed=!gameplay_enabled();guarded([&]{
  if(!gameplay_enabled())return;
  if(!learning_enabled()){allowed=true;return;}
  if(!session||!session->learning)return;
  const auto* character=roster().bound(actor);if(character)allowed=session->learning->permits(character->id,capability);
 });return allowed;
}
extern "C" int bsl_host_ability_use(uint64_t actor,unsigned slot){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(bsl_host_boot_busy()||!session||!session->learning||!roster().controls(actor)||!session->alive()||session->captures_commands||bsl_engine_editor())return;
  const auto& character=roster().bound(actor)->id;const auto* ability=session->learning->slot(character,slot);
  if(!ability||!ability->capability.empty()||!session->learning->learned(character,ability->id)){code=static_cast<int>(bsl::Code::denied);return;}
  code=static_cast<int>(bsl_bor_ability(actor,ability->animation.c_str(),ability->tag.c_str())?bsl::Code::ok:bsl::Code::busy);
 });return code;
}
extern "C" void bsl_host_learning_probe(BslLearningProbe* out){
 if(!out)return;
 *out={};guarded([&]{if(!session||!session->learning)return;const auto& character=roster().selected().id;const auto* a=session->learning->slot(character,0);out->present=a!=nullptr;out->receipts=static_cast<unsigned>(session->learning->receipts());out->refused=session->reward_refused;out->last_reward_code=session->last_reward_code;if(a){out->learned=session->learning->learned(character,a->id);std::snprintf(out->ability,sizeof out->ability,"%s",a->id.c_str());std::snprintf(out->animation,sizeof out->animation,"%s",a->animation.c_str());}});
}
extern "C" int bsl_host_checkpoint_activate(uint64_t actor,const char* definition){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{if(!session||!session->checkpoints||!definition||!roster().controls(actor))return;code=static_cast<int>(bsl::Code::invalid);for(const auto& d:compiled_checkpoints())if(d.id==definition){code=static_cast<int>(checkpoint_capture(&d));return;}});return code;
}
extern "C" int bsl_host_checkpoint_request(void){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{if(bsl_host_boot_busy()||!session||!session->checkpoints||bsl_engine_editor())return;if(session->checkpoints->candidates(teleports_enabled()).empty()){code=static_cast<int>(bsl::Code::denied);return;}session->rewind_requested=true;session->checkpoint_fault=false;code=0;});return code;
}
extern "C" void bsl_host_checkpoint_probe(BslCheckpointProbe* out){
 if(!out)return;
 *out={};guarded([&]{if(!session||!session->checkpoints)return;auto& s=*session;out->enabled=true;out->temporary=s.checkpoints->temporary();out->save_offer=s.save_offer;out->requested=s.rewind_requested;out->captures=s.checkpoint_captures;out->rewinds=s.checkpoint_rewinds;out->failures=s.checkpoint_failures;std::snprintf(out->restored,sizeof out->restored,"%s",s.last_checkpoint.c_str());auto records=s.checkpoints->candidates(teleports_enabled());if(!records.empty())std::snprintf(out->chosen,sizeof out->chosen,"%s",records.front()->id.c_str());});
}
extern "C" void bsl_host_stat_probe(const char* stat,BslStatProbe* out){
 if(!out)return;
 *out={};guarded([&]{if(!stat||!session||!session->progression)return;auto values=session->progression->capture(roster().selected().id);if(!values.count(stat))return;auto p=session->progression->view(roster().selected().id,stat);out->present=true;out->units=p.units;out->scale=p.scale;out->total=p.xp;out->into=p.into;out->next=p.next;out->stage=p.stage;out->last_damage=session->last_damage;out->last_bonus=session->last_bonus;});
}
extern "C" int bsl_host_stat_test_seed(const char* stat,uint64_t xp){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{if(stat&&std::getenv("BSL_PROGRESSION_TEST")&&session&&session->progression){auto values=session->progression->capture(roster().selected().id);if(!values.count(stat)){code=static_cast<int>(bsl::Code::invalid);return;}values.at(stat).xp=xp;code=static_cast<int>(session->progression->restore(roster().selected().id,values));}});return code;
}
extern "C" int bsl_host_progress_save(void){
 int result=static_cast<int>(bsl::Code::disabled);guarded([&]{if(!session||!progress_save_enabled())return;auto& s=*session;result=static_cast<int>(bsl::Code::denied);
  try{if(bsl_engine_editor()||s.dialogue_open()||s.pending_progress||bsl_host_boot_busy()||!s.save_offer)throw std::runtime_error("Activate a permanent rest point before saving.");
   const bsl::CheckpointRecord* record=nullptr;for(const auto* p:s.checkpoints->candidates())if(p->kind==bsl::CheckpointKind::permanent)record=p;
   if(!record)throw std::runtime_error("No permanent activation in this loaded level.");
   std::string success="Saved selected progress from the permanent activation. Later gains are not part of that save.";auto store=progress_store();store.write(progress_path(),store.select(*record));++s.saves;s.notice.swap(success);result=static_cast<int>(bsl::Code::ok);
  }catch(const std::exception& e){++s.progress_failures;s.notice=std::string("Progress not saved: ")+e.what();}
 });return result;
}
extern "C" int bsl_host_progress_load(void){
 int result=static_cast<int>(bsl::Code::disabled);guarded([&]{if(!session||!progress_save_enabled())return;auto& s=*session;result=static_cast<int>(bsl::Code::denied);
  try{if(bsl_engine_editor()||s.dialogue_open()||s.pending_progress)throw std::runtime_error("Close dialogue/editor before loading.");auto d=progress_store().read(progress_path());
   auto defs=compiled_checkpoints();const auto found=std::find_if(defs.begin(),defs.end(),[&](const auto& p){return p.id==d.checkpoint;});if(found==defs.end())throw std::runtime_error("Saved destination is not registered.");
   if(progress_boot_enabled()){
    if(bsl_host_boot_busy()||!s.alive())throw std::runtime_error("Wait for the current scene operation before continuing.");
    if(!bsl_engine_has_level(found->level.c_str()))throw std::runtime_error("Saved destination file is unavailable; current level/progress retained.");
    auto next=std::make_unique<BootRoute>();next->document=std::move(d);next->target=found->level;next->origin=bsl_engine_level_file();
    s.cancel();boot=std::move(next);s.notice="Saved progress validated; loading its authored destination.";bsl_engine_load_level(boot->target.c_str());
   }else{
    if(found->level!=bsl_engine_level_file())throw std::runtime_error("Load the saved destination level first; cross-level routing is pending.");
    std::optional<bsl::ProgressDocument> prepared(std::move(d));s.pending_progress.swap(prepared);s.cancel();s.notice="Saved progress validated; reconstruction queued.";
   }
   result=static_cast<int>(bsl::Code::ok);
  }catch(const std::exception& e){++s.progress_failures;s.notice=std::string("Progress not loaded: ")+e.what();}
 });return result;
}
extern "C" int bsl_host_progress_recover(void){
 int result=static_cast<int>(bsl::Code::disabled);guarded([&]{if(!session||!progress_save_enabled())return;auto& s=*session;result=static_cast<int>(bsl::Code::denied);
  try{if(bsl_engine_editor()||s.pending_progress||bsl_host_boot_busy())throw std::runtime_error("Finish the current operation first.");progress_store().recover(progress_path());s.notice="Previous progress restored on disk. Choose Load saved progress to apply it.";result=static_cast<int>(bsl::Code::ok);
  }catch(const std::exception& e){++s.progress_failures;s.notice=std::string("Progress recovery refused: ")+e.what();}
 });return result;
}
extern "C" void bsl_host_progress_probe(BslProgressProbe* out){if(!out)return;*out={};if(session&&progress_save_enabled()){out->enabled=true;out->reward=session->events.flag("switch:training_reward",roster().profile().player);out->choices=session->events.variable("variable:training_choices",roster().profile().player);out->pending=boot||session->pending_progress.has_value();out->saves=session->saves;out->loads=session->loads;out->failures=session->progress_failures;}}
namespace {
void boot_stop(const std::string& why){boot->phase=BootPhase::failed;session->notice="Continue stopped: "+why+" R retries the source level; Esc quits. Gameplay remains gated.";}
void boot_fail(const std::string& why){
 if(!boot)return;
 std::fprintf(stderr,"BSL Continue refused: %s\n",why.c_str());
 auto notice="Continue refused: "+why+" Returning to the previous level with its prior progress.";
 if(boot->previous_session){session->cancel();session.swap(boot->previous_session);gameplay_roster.swap(boot->previous_roster);}
 ++boot_report.failures;if(session){++session->progress_failures;session->notice.swap(notice);}
 boot->phase=BootPhase::returning;boot->wait=0;boot->return_loaded=false;boot->error=why;
 if(bsl_engine_has_level(boot->origin.c_str()))bsl_engine_load_level(boot->origin.c_str());else boot_stop("Source level is unavailable.");
}
}
extern "C" bool bsl_host_boot_busy(void){return boot!=nullptr||travel!=nullptr;}
extern "C" void bsl_host_level_begin(const char* level){guarded([&]{
 if(!progress_boot_enabled()||!level)return;
 boot_report.observations=0;boot_report.observed_reward=false;boot_report.observed_items=0;boot_report.observed_xp=0;boot_report.observed_actors=0;boot_report.mutations_refused=false;
 if(!session){session=std::make_unique<Session>();++session_generation;}
 session->weather_override.reset();bsl_particles_clear();bsl_ambient_clear();bsl_acoustic_clear();
 if(travel){travel_begin(level);return;}
 if(!boot){session->checkpoints->enter(level);return;}
 if(boot->phase==BootPhase::returning){
  if(boot->origin==level){boot->return_loaded=true;session->checkpoints->enter(level);}else boot_stop("Unexpected level during fallback.");return;
 }
 if(boot->phase!=BootPhase::queued||boot->target!=level){boot_fail("Unexpected destination lifecycle.");return;}
 try{
  if(!bindings.empty())throw std::runtime_error("Old actors were not released before scene construction.");
  auto progress=progress_store().reconstruct(boot->document,0,0).progress;
  auto next_roster=std::make_unique<bsl::PlayerRoster>(compiled_gameplay_profile());
  if(next_roster->restore_progress(progress.selected,progress.health)!=bsl::Code::ok)throw std::runtime_error("Saved roster refused.");
  auto next=std::make_unique<Session>(next_roster.get());
  for(const auto& c:next_roster->profile().characters){const auto& id=next_roster->character(c.id)->id;if(next->progression->restore(id,progress.stats.at(id))!=bsl::Code::ok)throw std::runtime_error("Saved stats refused.");}
  if(next->learning->restore(progress.learning)!=bsl::Code::ok)throw std::runtime_error("Saved abilities refused.");
  bsl::RewindServices services(next->inventory,next->events,progress.inventory,progress.events);services.commit();if(next->clock&&!next->clock->calendar().valid(progress.calendar))throw std::runtime_error("Saved calendar out of range.");next->calendar_seconds=progress.calendar;
  next->request_id=session->request_id;next->conversation_id=session->conversation_id;next->anchor_sequence=session->anchor_sequence;next->saves=session->saves;next->loads=session->loads;next->progress_failures=session->progress_failures;
  next->notice="Saved services ready; constructing fresh destination objects.";
  boot->previous_session=std::move(session);boot->previous_roster=std::move(gameplay_roster);session=std::move(next);gameplay_roster=std::move(next_roster);++session_generation;
  boot->phase=BootPhase::assembling;boot->wait=0;
 }catch(const std::exception& e){boot_fail(e.what());}
});}
extern "C" bool bsl_host_boot_step(void){bool waiting=false;guarded([&]{
 if(travel){waiting=travel_step();return;}
 if(!boot)return;
 waiting=true;
 if(boot->phase==BootPhase::failed||boot->phase==BootPhase::queued||(boot->phase==BootPhase::returning&&!boot->return_loaded))return;
 if(!session->selected_ready())return;
 if(boot->phase==BootPhase::returning){boot.reset();return;}
 try{
  auto defs=compiled_checkpoints();const auto found=std::find_if(defs.begin(),defs.end(),[&](const auto& d){return d.id==boot->document.checkpoint;});double x=0,y=0;
  if(found==defs.end()||!bsl_bor_checkpoint_site(found->placement,true,&x,&y))throw std::runtime_error("Destination rest point is missing.");
  auto record=progress_store().reconstruct(boot->document,x,y);std::string success="Continued at the saved rest point; fresh level objects initialized from selected progress.";
  if(!checkpoint_restore(&record))throw std::runtime_error("Destination has no safe native recovery plan.");
  ++session->loads;++boot_report.arrivals;session->notice.swap(success);boot.reset();
 }catch(const std::exception& e){boot_fail(e.what());}
});return waiting;}
extern "C" bool bsl_host_pickup_available(uint64_t placement){bool available=false;guarded([&]{if(!session||!pickups_enabled())return;for(const auto& d:compiled_pickups())if(d.placement==placement&&d.level==bsl_engine_level_file()){available=!session->events.flag(d.flag,roster().profile().player);return;}});return available;}
extern "C" const char* bsl_host_pickup_label(uint64_t placement){const char* label="";guarded([&]{if(!session||!pickups_enabled())return;for(const auto& d:compiled_pickups())if(d.placement==placement&&d.level==bsl_engine_level_file()){const auto* item=session->catalog.item(d.item);if(item)label=item->name.c_str();return;}});return label;}
extern "C" int bsl_host_pickup_try(uint64_t actor){int result=-1;guarded([&]{
 if(!session||!pickups_enabled()||bsl_host_boot_busy()||!roster().controls(actor)||!session->alive()||session->captures_commands||session->modal()||bsl_engine_editor()||!bsl_bor_pickup_ready(actor))return;
 const auto* current=session->actor();const bsl::PickupDef* chosen=nullptr;uint64_t loot_id=0;double best=0;std::string key;
 for(const auto& d:compiled_pickups()){
  if(d.level!=bsl_engine_level_file()||session->events.flag(d.flag,roster().profile().player))continue;
  double x=0,y=0;if(!bsl_bor_pickup_site(d.placement,&x,&y))continue;double distance=std::hypot(current->x-x,current->y-y);
  if(distance<=d.range&&(key.empty()||distance<best||(distance==best&&d.id<key))){chosen=&d;best=distance;key=d.id;}
 }
 if(session->loot)for(const auto& pair:session->loot->state().outcomes){const auto& d=pair.second;if(d.item.empty()||d.collected)continue;
  auto distance=std::hypot(current->x-d.x,current->y-d.y);auto id="loot/"+std::to_string(pair.first);
  if(distance<=session->loot->table(d.table).range&&(key.empty()||distance<best||(distance==best&&id<key))){chosen=nullptr;loot_id=pair.first;best=distance;key=id;}
 }
 if(key.empty())return;
 const auto item=chosen?chosen->item:session->loot->state().outcomes.at(loot_id).item;const int quantity=chosen?chosen->quantity:session->loot->state().outcomes.at(loot_id).quantity;
 std::string success="Collected "+std::to_string(quantity)+" "+session->catalog.item(item)->name+" into the player's inventory.";
 const auto claim=chosen?bsl::PickupClaims::claim(session->inventory,session->events,*chosen,roster().profile().player):session->loot->claim(session->inventory,loot_id,roster().profile().player);result=static_cast<int>(claim.code);
 if(claim.code==bsl::Code::ok)session->notice.swap(success);else session->notice="Pickup refused; inventory and world availability retained. Free bag space and press the trigger again.";
});return result;}
extern "C" void bsl_host_loot_probe(BslLootProbe* out){if(!out)return;*out={};guarded([&]{if(!session||!session->loot)return;out->enabled=true;out->refused=session->loot_refused;out->random=session->loot->state().random;out->epoch=session->loot->state().epoch;out->outcomes=static_cast<unsigned>(session->loot->state().outcomes.size());for(const auto& pair:session->loot->state().outcomes){const auto& d=pair.second;if(d.item.empty())++out->empty;else if(d.collected)++out->collected;else ++out->pending;}});}
extern "C" bool bsl_host_loot_view(unsigned index,BslLootView* out){if(!out)return false;*out={};bool found=false;guarded([&]{if(bsl_host_boot_busy()||!session||!session->loot)return;for(const auto& pair:session->loot->state().outcomes){const auto& d=pair.second;if(d.item.empty()||d.collected)continue;if(index--!=0)continue;out->victim=pair.first;out->quantity=d.quantity;out->x=d.x;out->y=d.y;out->range=session->loot->table(d.table).range;std::snprintf(out->item,sizeof out->item,"%s",d.item.c_str());found=true;break;}});return found;}
extern "C" bool bsl_host_progress_flag(const char* id){bool value=false;guarded([&]{if(session&&id&&session->catalog.flag(id))value=session->events.flag(id,roster().profile().player);});return value;}
extern "C" int bsl_host_progress_variable(const char* id){int value=0;guarded([&]{if(session&&id&&session->catalog.variable(id))value=session->events.variable(id,roster().profile().player);});return value;}
extern "C" void bsl_host_boot_observe(void){guarded([]{
 if(!session||!progress_boot_enabled())return;
 if(!boot_report.observations){
  boot_report.observed_reward=session->events.flag("switch:training_reward",roster().profile().player);const auto* bag=session->inventory.bag(roster().profile().player);auto fruit=bag?bag->stacks.find("item:strawberry"):std::map<std::string,int>::const_iterator{};boot_report.observed_items=bag&&fruit!=bag->stacks.end()?fruit->second:0;boot_report.observed_actors=static_cast<unsigned>(bindings.size());
  const auto values=session->progression->capture(roster().selected().id);auto xp=values.find("stat:experience");boot_report.observed_xp=xp==values.end()?0:xp->second.xp;
 }
 ++boot_report.observations;
 if(boot&&session->owner&&std::getenv("BSL_PROGRESS_BOOT_TEST"))boot_report.mutations_refused=bsl_host_reward(session->owner,"source:lab_exploration","boot/constructor")!=0&&bsl_host_anchor_start(0xb00f,"event:training_reward",session->owner)!=0;
});}
extern "C" void bsl_host_boot_probe(BslBootProbe* out){if(!out)return;*out=boot_report;out->enabled=progress_boot_enabled();out->busy=boot!=nullptr;std::snprintf(out->level,sizeof out->level,"%s",bsl_engine_level_file());}
// Loaded-level startup expectation: explicit opt-in, one bootstrap consumer. Signed: Mech.
extern "C" void bsl_host_player_required(bool required){guarded([&]{
 if(!gameplay_enabled()||!session)return;
 auto& s=*session;s.player_required=required;
 if(!required){s.player_clear_pause();s.player_failed=false;s.player_retry_pending=false;s.player_wait=0;s.player_error.clear();}
 else if(s.owner&&bindings.count(s.owner))s.player_assembled();
});}
extern "C" void bsl_host_player_assembly_probe(BslPlayerAssemblyProbe* out){if(!out)return;*out={};out->enabled=gameplay_enabled();guarded([&]{
 if(!session||!out->enabled)return;
 auto& s=*session;out->required=s.player_required;out->ready=s.player_ready;out->failed=s.player_failed;out->retry_pending=s.player_retry_pending;out->waiting=s.player_wait;out->retries=s.player_retries;
 std::snprintf(out->character,sizeof out->character,"%s",roster().selected().definition.c_str());std::snprintf(out->message,sizeof out->message,"%s",s.player_error.c_str());
});}
extern "C" bool bsl_host_player_retry(void){bool accepted=false;guarded([&]{
 if(!session||!session->player_required||!session->player_failed||bsl_host_boot_busy()||bsl_engine_editor())return;
 auto& s=*session;s.player_clear_pause();s.player_failed=false;s.player_retry_pending=true;s.player_wait=0;s.player_error.clear();++s.player_retries;s.barrier=true;s.captures_commands=true;bsl_bor_capture_input();accepted=true;
});return accepted;}
extern "C" bool bsl_host_player_assembly_waiting(void){return gameplay_enabled()&&session&&session->player_required&&!session->player_ready;}
extern "C" bool bsl_host_player_take_retry(void){if(!session||!session->player_retry_pending||bsl_host_boot_busy()||bsl_engine_editor())return false;session->player_retry_pending=false;return true;}
extern "C" void bsl_host_enable(void){guarded([]{if(!session){session=std::make_unique<Session>();++session_generation;}if(session->checkpoints)session->checkpoints->enter(bsl_engine_level_file());});}
extern "C" void bsl_host_shutdown(void){guarded([]{
 bsl_particles_release();bsl_ambient_release();bsl_acoustic_clear();
 if(gameplay_enabled()&&session){session->player_reset();if(session->loot)session->loot=make_loot(session->catalog);if(session->checkpoints){session->checkpoints->leave();session->pending_progress.reset();session->rewind_requested=false;session->death_ticks=0;session->save_offer=false;session->checkpoint_fault=false;}session->cancel();session->events.cancel_participant(roster().profile().player);session->anchor_runs.clear();session->conversation_runs.clear();session->interaction_requested=false;session->owner=0;}
 else session.reset();
 bsl_ui_release();
});}
extern "C" bool bsl_host_begin_lab_run(void){bool accepted=false;guarded([&]{
 // A menu must not detach live actors or interrupt an in-level travel transaction.
 if(!bindings.empty())return;
 bsl_particles_release();bsl_ambient_release();bsl_acoustic_clear();bsl_ui_release();
 boot.reset();travel.reset();session.reset();gameplay_roster.reset();player_slots.clear();
 continue_attempted=false;boot_report={};accepted=true;
});return accepted;}
// The selector owns no gameplay state until this explicit, prevalidated commit. Signed: Atlas.
extern "C" int bsl_host_begin_selected_run(const char* definition){
 int code=static_cast<int>(bsl::Code::invalid);guarded([&]{
  if(!definition)return;
  auto prepared=std::make_unique<bsl::PlayerRoster>(compiled_gameplay_profile());
  code=static_cast<int>(prepared->choose_initial(definition));if(code!=static_cast<int>(bsl::Code::ok))return;
  code=static_cast<int>(bsl::Code::denied);
  if(!bindings.empty()||bsl_host_boot_busy()||bsl_engine_editor())return;
  if(!bsl_host_begin_lab_run())return;
  gameplay_roster=std::move(prepared);code=static_cast<int>(bsl::Code::ok);
 });return code;
}
extern "C" void bsl_host_reset(void){guarded([]{bsl_particles_clear();bsl_ambient_clear();bsl_acoustic_clear();if(session){
 boot.reset();travel.reset();bsl_ui_reset_presentation();session.reset();
 if(gameplay_enabled()){
  auto& r=roster();std::vector<std::pair<uint64_t,std::string>> attached;for(const auto& e:bindings)if(auto* c=r.bound(e.first))attached.emplace_back(e.first,c->definition);
  gameplay_roster=fresh_roster();
  for(const auto& e:attached){auto* a=bindings.at(e.first);if(roster().attach(e.second,e.first,a->model->name,a->model->health)!=bsl::Code::ok)throw std::runtime_error("cannot reset character binding");a->health=roster().character(e.second)->detached_health;}
 }
 session=std::make_unique<Session>();++session_generation;
}});}
extern "C" void bsl_host_bind(uint64_t lifetime,BorActor* actor){guarded([&]{if(!actor||!actor->model)return;if(gameplay_enabled()){bindings[lifetime]=actor;return;}if(!(actor->model->type&BOR_TYPE_PLAYER))return;if(!player_slots.count(lifetime)){int slot=1;for(;;++slot){bool taken=false;for(const auto& entry:player_slots)if(entry.second=="player/"+std::to_string(slot))taken=true;if(!taken)break;}player_slots[lifetime]="player/"+std::to_string(slot);}bindings[lifetime]=actor;if(session)session->bind(lifetime,*actor);});}
extern "C" void bsl_host_unbind(uint64_t lifetime){guarded([&]{
 if(session){session->interrupt_actor_conversation(lifetime);session->actor_interactions.erase(lifetime);session->actor_defaults_done.erase(lifetime);session->retire_actor_placement(lifetime);}
 bsl_host_particle_cancel_owner(lifetime);bsl_acoustic_cancel_owner(lifetime);
 if(gameplay_enabled()){
  auto found=bindings.find(lifetime);if(found==bindings.end())return;
  auto& r=roster();if(r.bound(lifetime)){
   if(session&&r.controls(lifetime)){session->events.cancel_participant(r.profile().player);session->effects.unbind(r.profile().player);session->cancel();session->owner=0;session->interaction_requested=false;}
   r.detach(lifetime,found->second->health);
  }
  if(session){session->used_actions.erase(lifetime);if(session->progression)session->progression->retire(lifetime);}
  bindings.erase(found);return;
 }
 if(session){session->events.cancel_participant(owner_id(lifetime));session->effects.unbind(owner_id(lifetime));if(session->owner==lifetime){session->cancel();session->owner=0;}}if(session){session->used_actions.erase(lifetime);}bindings.erase(lifetime);player_slots.erase(lifetime);});}
extern "C" void bsl_host_hit(uint64_t lifetime){guarded([&]{if(session)session->interrupt_actor_conversation(lifetime);if(gameplay_enabled()&&(!gameplay_roster||!gameplay_roster->controls(lifetime)))return;if(session){if(session->interaction_subject==lifetime)session->interaction_requested=false;session->events.cancel_participant(owner_id(lifetime));++session->damage_notifications;session->conversation.hit(owner_id(lifetime));if(session->owner==lifetime&&!session->alive())session->cancel();}});}
extern "C" void bsl_host_use_assigned(uint64_t lifetime,uint64_t action){guarded([&]{
 if(bsl_host_boot_busy()||!session||!bindings.count(lifetime)||(gameplay_enabled()&&!roster().controls(lifetime)))return;
 if(session->used_actions.count(lifetime)&&session->used_actions.at(lifetime)==action)return;
 session->used_actions[lifetime]=action;
 auto owner=owner_id(lifetime);auto* bag=session->inventory.bag(owner);if(!bag)return;
 auto selected=bag->assigned.find("ranged");if(selected==bag->assigned.end())return;
 const auto item=selected->second;
 auto result=session->inventory.execute({"action/"+std::to_string(++session->request_id),owner,bsl::Op::use,item.definition,item.instance,"",1},&session->effects);
 session->result(result,"Assigned item used at ranged-action activation.");
});}
extern "C" bool bsl_host_input_locked(uint64_t lifetime){bool locked=false;guarded([&]{locked=bsl_host_boot_busy()|| (session&&((session->owner==lifetime&&session->barrier)||session->policies.blocked(owner_id(lifetime))||session->policies.blocked(actor_owner(lifetime))));});return locked;}
extern "C" bool bsl_host_captures_commands(void){return bsl_host_boot_busy()|| (session&&session->captures_commands);}
extern "C" void bsl_bor_audio_test_tick(void);
extern "C" void bsl_bor_player_assembly_test_tick(void);
extern "C" void bsl_bor_player_assembly_test_render(void);
extern "C" void bsl_bor_actor_conversation_test_render(void);
extern "C" bool bsl_host_preupdate(void){bool paused=false;guarded([&]{paused=preupdate();bsl_bor_calendar_test_tick();bsl_bor_weather_test_tick();bsl_bor_audio_test_tick();bsl_bor_player_assembly_test_tick();});return paused;}
extern "C" void bsl_host_render(void){guarded([]{render();bsl_bor_player_assembly_test_render();bsl_bor_actor_conversation_test_render();});}

extern "C" void bsl_host_simulation_tick(void){guarded([]{
 if(!session)return;
 if(bsl_host_boot_busy()||progress_tick()||checkpoint_tick())return;
 travel_contacts();if(travel)return;
 const auto* before=session->events.run(session->last_event);
 const auto previous=before?before->status:bsl::EventStatus::completed;
 bsl_level_events_tick();
 session->validate_conversation_runs();
 session->events.tick();
 const auto* after=session->events.run(session->last_event);
 if(after&&after->status!=previous){
  if(after->status==bsl::EventStatus::completed)session->notice="Authored event complete.";
  else if(after->status==bsl::EventStatus::failed)session->notice="Event stopped: command denied; earlier effects retained.";
 }
 bsl_level_sequence_exit_tick();
});}

// Catalog discovery never creates a Session or applies the lab loadout. Signed: Atlas.
namespace {
const bsl::Catalog& anchor_catalog(){static const auto catalog=bsl::load_compiled_snapshot();return catalog;}
std::string placement_owner(uint64_t id){std::ostringstream text;text<<"placement/"<<std::hex<<id;return text.str();}
bool anchor_terminal(bsl::EventStatus status){return status==bsl::EventStatus::completed||status==bsl::EventStatus::cancelled||status==bsl::EventStatus::failed;}
}
extern "C" void bsl_host_commerce_probe(BslCommerceProbe* out){if(!out)return;*out={};guarded([&]{if(!session||!session->commerce)return;out->enabled=true;out->balance=session->commerce->balance("currency:coins",roster().profile().player);out->stock=session->commerce->stock("shop:training","offer:berries",roster().profile().player);});}
extern "C" void bsl_host_quest_probe(const char* id,BslQuestProbe* out){if(!out)return;*out={};guarded([&]{if(!session||!session->quests||!id)return;const auto view=session->quests->view(id,roster().profile().player);out->enabled=true;out->status=static_cast<int>(view.status);out->total=static_cast<int>(view.objectives.size());for(const auto& o:view.objectives)if(o.met)++out->met;});}
extern "C" int bsl_host_event_count(void){int count=0;guarded([&]{count=static_cast<int>(anchor_catalog().events().size());});return count;}
extern "C" const char* bsl_host_event_id(int index){const char* result="";guarded([&]{const auto& events=anchor_catalog().events();if(index>=0&&static_cast<size_t>(index)<events.size()){auto it=events.begin();std::advance(it,index);result=it->first.c_str();}});return result;}
extern "C" bool bsl_host_event_exists(const char* definition){bool found=false;guarded([&]{found=definition&&anchor_catalog().event(definition);});return found;}
extern "C" uint64_t bsl_host_interactor(double* x,double* y){
 if(bsl_host_boot_busy()||!session||!x||!y||bsl_engine_editor()||session->captures_commands||session->policies.paused())return 0;
 auto found=bindings.find(session->owner);if(found==bindings.end()||session->policies.blocked(owner_id(found->first)))return 0;
 auto* actor=found->second;if(!actor||actor->health<=0||actor->state==BOR_DEAD)return 0;
 *x=actor->x;*y=actor->y;return found->first;
}
extern "C" uint64_t bsl_host_interaction_take(double* x,double* y){
 if(!session||!session->interaction_requested)return 0;
 session->interaction_requested=false;
 const auto subject=session->interaction_subject;
 return bsl_host_interactor(x,y)==subject?subject:0;
}
extern "C" int bsl_host_prepare_sequence_exit(uint64_t actor){
 int code=static_cast<int>(bsl::Code::denied);guarded([&]{
  double x=0,y=0;
  if(!actor||!session||session->modal()||bsl_host_interactor(&x,&y)!=actor||!bsl_bor_sequence_exit_ready(actor))return;
  session->events.cancel_participant(owner_id(actor));session->cancel();session->anchor_runs.clear();session->interaction_requested=false;
  session->barrier=true;session->captures_commands=true;bsl_bor_capture_input();
  session->notice="Sequence exit accepted; current player progress retained.";code=0;
 });return code;
}
extern "C" void bsl_host_interaction_reset(void){if(session){session->interaction_requested=false;ALLEGRO_KEYBOARD_STATE keys;al_get_keyboard_state(&keys);session->previous[ALLEGRO_KEY_E]=al_key_down(&keys,ALLEGRO_KEY_E);}}
extern "C" unsigned bsl_host_actor_placement_count(void){return session?static_cast<unsigned>(session->actor_placements.size()):0;}
extern "C" bool bsl_host_actor_placement_probe(unsigned index,BslActorPlacementProbe* out){
 if(!out)return false;
 *out={};if(!session||index>=session->actor_placements.size())return false;
 const auto& state=session->actor_placements[index];const auto& def=*state.definition;out->actor=state.actor;out->attempts=state.attempts;out->status=state.status;
 std::snprintf(out->id,sizeof out->id,"%s",def.id.c_str());std::snprintf(out->placement,sizeof out->placement,"%s",def.placement.c_str());std::snprintf(out->interaction,sizeof out->interaction,"%s",def.interaction.c_str());std::snprintf(out->object,sizeof out->object,"%s",def.object.c_str());std::snprintf(out->level,sizeof out->level,"%s",def.level.c_str());return true;
}
extern "C" int bsl_host_actor_placement_reset(uint64_t actor){
 if(!actor||!session)return static_cast<int>(bsl::Code::disabled);
 for(const auto& state:session->actor_placements)if(state.actor==actor)return bsl_host_actor_interaction_bind(actor,state.definition->interaction.c_str());
 const auto* def=actor_template_definition(bsl_bor_actor_template_id(actor));if(def)return bsl_host_actor_interaction_bind(actor,def->interaction.c_str());
 return static_cast<int>(bsl::Code::invalid);
}
extern "C" bool bsl_host_actor_template_definition(const char* id,BslActorTemplate* out){if(!out)return false;*out={};if(!id)return false;const auto* d=actor_template_definition(id);if(!d)return false;actor_template_view(*d,*out);return true;}
extern "C" bool bsl_host_actor_template_at(unsigned index,BslActorTemplate* out){if(!out)return false;*out={};const auto& defs=compiled_actor_templates();if(index>=defs.size())return false;actor_template_view(defs[index],*out);return true;}
extern "C" const char* bsl_host_actor_template_for(uint64_t actor){return bsl_bor_actor_template_id(actor);}
extern "C" int bsl_host_actor_template_bind(uint64_t actor,const char* id){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!gameplay_enabled()||!session)return;
  code=static_cast<int>(bsl::Code::invalid);const auto* def=id?actor_template_definition(id):nullptr;auto body=bindings.find(actor);
  if(!def||body==bindings.end()||!body->second->model||def->model!=body->second->model->name||!bsl_bor_conversation_actor_live(actor))return;
  code=static_cast<int>(bsl::Code::conflict);if(roster().bound(actor))return;
  const char* existing=bsl_bor_actor_template_id(actor);if(*existing){code=static_cast<int>(std::string(existing)==id?bsl::Code::ok:bsl::Code::conflict);return;}
  code=bsl_host_actor_interaction_bind(actor,def->interaction.c_str());
  if(code==0){if(bsl_bor_actor_template_record(actor,id,def->passive))session->actor_defaults_done.insert(actor);else code=static_cast<int>(bsl::Code::invalid);}
 });return code;
}
extern "C" int bsl_host_actor_interaction_bind(uint64_t actor,const char* definition){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!gameplay_enabled()||!session)return;
  code=static_cast<int>(bsl::Code::invalid);if(!actor||!definition||!bindings.count(actor)||!bsl_bor_conversation_actor_live(actor))return;
  if(*definition&&!interaction_definition(definition))return;
  auto& entries=session->actor_interactions;auto found=entries.find(actor);
  if(found!=entries.end()&&found->second==definition){code=0;return;}
  session->interrupt_actor_conversation(actor);
  if(*definition)entries[actor]=definition;else entries.erase(actor);code=0;
 });return code;
}
extern "C" const char* bsl_host_actor_interaction_definition(uint64_t actor){
 if(!session)return "";
 auto found=session->actor_interactions.find(actor);return found==session->actor_interactions.end()?"":found->second.c_str();
}
extern "C" uint64_t bsl_host_actor_interaction_nearest(uint64_t initiator,double* distance_squared){
 uint64_t selected=0;if(distance_squared)*distance_squared=INFINITY;
 if(!gameplay_enabled()||!session||!distance_squared||session->modal()||session->policies.paused()||bsl_host_boot_busy()||bsl_engine_editor()||!roster().controls(initiator)||!bsl_bor_conversation_actor_ready(initiator))return 0;
 auto origin=bindings.find(initiator);if(origin==bindings.end())return 0;
 double best=INFINITY;
 for(const auto& entry:session->actor_interactions){
  const auto id=entry.first;auto actor=bindings.find(id);const auto* def=interaction_definition(entry.second);
  if(id==initiator||!def||actor==bindings.end()||!bsl_bor_conversation_actor_ready(id))continue;
  const double dx=actor->second->x-origin->second->x,dy=actor->second->y-origin->second->y,distance=dx*dx+dy*dy;
  if(distance<=def->range*def->range&&(distance<best||(distance==best&&id<selected))){best=distance;selected=id;}
 }
 *distance_squared=best;return selected;
}
extern "C" int bsl_host_interaction_dispatch(uint64_t initiator,uint64_t anchor,const char* event,double distance_squared){
 if(anchor&&(!event||!bsl_host_event_exists(event)||!std::isfinite(distance_squared)||distance_squared<0))return static_cast<int>(bsl::Code::invalid);
 double actor_distance=INFINITY;const auto speaker=bsl_host_actor_interaction_nearest(initiator,&actor_distance);
 // Existing helper wins an exact-distance tie; do not perform two actions for one input edge.
 if(speaker&&(!anchor||actor_distance<distance_squared)){
  const auto* def=interaction_definition(bsl_host_actor_interaction_definition(speaker));
  return def?bsl_host_conversation_open(initiator,speaker,def->dialogue.c_str(),def->range,def->calendar_factor):static_cast<int>(bsl::Code::invalid);
 }
 if(anchor)return bsl_host_anchor_start(anchor,event,initiator);
 return static_cast<int>(bsl::Code::denied);
}
extern "C" int bsl_host_conversation_open(uint64_t initiator,uint64_t speaker,const char* definition,double range,double factor){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!gameplay_enabled()||!session)return;
  auto& s=*session;code=static_cast<int>(bsl::Code::invalid);
  if(!definition||!s.catalog.dialogue(definition)||!std::isfinite(range)||range<0||range>4096||!std::isfinite(factor)||factor<0||factor>1||!initiator||!speaker||initiator==speaker)return;
  auto a=bindings.find(initiator),b=bindings.find(speaker);code=static_cast<int>(bsl::Code::denied);
  if(bsl_host_boot_busy()||bsl_engine_editor()||s.modal()||s.policies.paused()||!roster().controls(initiator)||a==bindings.end()||b==bindings.end()||!bsl_bor_conversation_actor_ready(initiator)||!bsl_bor_conversation_actor_ready(speaker)||s.conversation_id==UINT64_MAX)return;
  if(std::hypot(a->second->x-b->second->x,a->second->y-b->second->y)>range)return;
  const auto* character=roster().bound(initiator);if(!character)return;
  Session::ActorTalk next{initiator,speaker,range,{actor_owner(speaker),roster().profile().player,character->id},definition,{}};
  s.conversation.open(definition,next.context.owner,{next.context.player,actor_owner(initiator),actor_owner(speaker)},factor);
  s.talking=std::move(next);s.choice=s.choice_first=0;++s.conversation_id;s.barrier=true;s.captures_commands=true;s.interaction_requested=false;bsl_bor_capture_input();code=static_cast<int>(bsl::Code::ok);
 });return code;
}
extern "C" int bsl_host_conversation_choose(uint64_t initiator,int choice){
 int code=static_cast<int>(bsl::Code::denied);guarded([&]{
  if(session)session->validate_actor_conversation();
  if(!session||session->menu||session->policies.paused()||!session->talking||!session->dialogue_open()||session->talking->initiator!=initiator||!roster().controls(initiator)||bsl_host_boot_busy()||bsl_engine_editor())return;
  const auto* def=session->conversation.definition();if(choice<0||static_cast<std::size_t>(choice)>=def->choices.size()){code=static_cast<int>(bsl::Code::invalid);return;}
  code=static_cast<int>(session->choose_dialogue(choice));
 });return code;
}
extern "C" int bsl_host_conversation_cancel(uint64_t participant){
 int code=static_cast<int>(bsl::Code::denied);guarded([&]{
  if(!session||!session->talking||!session->dialogue_open()||(session->talking->initiator!=participant&&session->talking->speaker!=participant))return;
  session->conversation.cancel();session->barrier=true;session->captures_commands=true;session->interaction_requested=false;bsl_bor_capture_input();code=0;
 });return code;
}
extern "C" bool bsl_host_conversation_locked(uint64_t actor){return session&&session->talking&&session->dialogue_open()&&(session->talking->initiator==actor||session->talking->speaker==actor);}
extern "C" void bsl_host_conversation_probe(BslConversationProbe* out){
 if(!out)return;
 *out={};out->run_status=-1;guarded([&]{if(!session)return;auto& s=*session;out->open=s.dialogue_open();out->state=static_cast<int>(s.conversation.state());out->choice=s.conversation.choice();
  if(!s.talking)return;
  const auto& t=*s.talking;out->actor_owned=true;out->initiator=t.initiator;out->speaker=t.speaker;
  std::snprintf(out->definition,sizeof out->definition,"%s",t.definition.c_str());std::snprintf(out->owner,sizeof out->owner,"%s",t.context.owner.c_str());std::snprintf(out->player,sizeof out->player,"%s",t.context.player.c_str());std::snprintf(out->character,sizeof out->character,"%s",t.context.character.c_str());std::snprintf(out->run,sizeof out->run,"%s",t.run.c_str());if(const auto* run=s.events.run(t.run))out->run_status=static_cast<int>(run->status);
 });
}
extern "C" int bsl_host_anchor_start(uint64_t placement,const char* definition,uint64_t lifetime){
 int code=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!session)return;
  code=static_cast<int>(bsl::Code::invalid);
  auto found=bindings.find(lifetime);if(!placement||!definition||!session->catalog.event(definition)||found==bindings.end())return;
  if(bsl_host_boot_busy()||(gameplay_enabled()&&!roster().controls(lifetime))||bsl_engine_editor()||session->policies.paused()||session->policies.blocked(owner_id(lifetime))||!found->second||found->second->health<=0||found->second->state==BOR_DEAD){code=static_cast<int>(bsl::Code::denied);return;}
  auto previous=session->anchor_runs.find(placement);
  if(previous!=session->anchor_runs.end()){auto* run=session->events.run(previous->second);if(run&&!anchor_terminal(run->status)){code=static_cast<int>(bsl::Code::busy);return;}}
  if(session->anchor_sequence==UINT64_MAX){code=static_cast<int>(bsl::Code::denied);return;}
  std::ostringstream key;key<<"anchor."<<std::hex<<placement<<"."<<++session->anchor_sequence;
  auto result=session->events.start(key.str(),definition,session->event_context(definition,placement_owner(placement),lifetime));
  code=static_cast<int>(result.code);
  if(result.code==bsl::Code::ok){session->anchor_runs[placement]=key.str();session->last_event=key.str();session->notice="Placed event queued.";}
  else session->notice="Placed event rejected; check definition or session journal capacity.";
 });return code;
}
extern "C" void bsl_host_anchor_cancel(uint64_t placement){guarded([&]{if(session)session->events.cancel_participant(placement_owner(placement));});}
extern "C" void bsl_host_anchor_probe(uint64_t placement,BslAnchorRun* out){
 if(!out)return;
 *out={};out->status=-1;
 guarded([&]{if(!session)return;auto found=session->anchor_runs.find(placement);if(found==session->anchor_runs.end())return;auto* run=session->events.run(found->second);if(!run)return;out->status=static_cast<int>(run->status);out->next=static_cast<unsigned>(run->next);out->error=static_cast<int>(run->error);std::snprintf(out->owner,sizeof out->owner,"%s",run->context.owner.c_str());std::snprintf(out->player,sizeof out->player,"%s",run->context.player.c_str());});
}

#include "teleport_host.inc"

// Read-only view: no Session creation, time advancement, or policy token acquisition. Signed: Mech.
extern "C" void bsl_host_world_time_probe(BslWorldTimeProbe* out){if(!out)return;*out={};guarded([&]{if(!session)return;out->available=true;out->paused=session->policies.paused();out->generation=session_generation;out->simulation=session->simulation_seconds;out->ui=session->ui_seconds;});}
extern "C" void bsl_host_calendar_probe(BslCalendarProbe* out){if(!out)return;*out={};guarded([&]{if(!session||!session->clock)return;auto& s=*session;auto d=s.clock->date();out->enabled=true;out->elapsed=s.calendar_seconds;out->simulation=s.simulation_seconds;out->ui=s.ui_seconds;out->factor=s.policies.calendar_factor();out->paused=s.policies.paused();out->year=d.year;out->month=d.month;out->day=d.day;out->hour=d.hour;out->minute=d.minute;out->second=d.second;std::snprintf(out->phase,sizeof out->phase,"%s",d.phase.c_str());std::snprintf(out->month_name,sizeof out->month_name,"%s",d.month_name.c_str());});}
extern "C" int ps_ui_pilot_mix_confirm(int phase){if(!ps_lab_automatic()||!session||!session->menu)return -1;if(phase==0){session->menu_test_mix_confirm=true;session->menu_test_mix_applied=false;return 0;}return session->menu_test_mix_applied?1:0;}
extern "C" int ps_ui_pilot_inventory(){int code=-1;guarded([&]{if(!ps_lab_automatic()||!std::getenv("PS_UI_PILOT")||!session||session->modal()||!session->alive()||bsl_engine_editor())return;session->actor()->health=std::max(1,session->actor()->health-20);session->open_menu(1);session->barrier=true;code=0;});return code;}
extern "C" int ps_ui_pilot_open(const char* definition){
 int code=-1;guarded([&]{if(!std::getenv("PS_UI_PILOT")||!session||!definition||!session->catalog.dialogue(definition))return;auto& s=*session;if(!s.alive()||s.menu||s.dialogue_open()||bsl_engine_editor())return;s.talking.reset();s.conversation.open(definition,owner_id(s.owner),{owner_id(s.owner)});s.choice=s.choice_first=0;++s.conversation_id;s.barrier=true;s.captures_commands=true;code=0;});return code;
}
extern "C" int bsl_host_calendar_test_policy(int mode){int code=static_cast<int>(bsl::Code::disabled);guarded([&]{if((!std::getenv("BSL_CALENDAR_TEST")&&!std::getenv("BSL_WEATHER_TEST")&&!std::getenv("BSL_AUDIO_TEST"))||!session||!session->clock||bsl_host_boot_busy())return;auto& s=*session;s.cancel();if(mode==1)s.open_menu(3);else if(mode==2)s.conversation.open("dialogue:training_choice",roster().profile().player,{roster().profile().player});else if(mode!=0)return;code=0;});return code;}

extern "C" int bsl_host_inventory_resize(uint64_t actor,int slots,int rows){
 int result=static_cast<int>(bsl::Code::invalid);guarded([&]{
  if(!session||!bindings.count(actor)||owner_id(actor).compare(0,7,"player/")!=0||slots<0||rows<0||(slots>0)==(rows>0)||rows>128/bsl_ui_inventory_columns())return;
  result=static_cast<int>(session->inventory.resize(owner_id(actor),slots?slots:rows*bsl_ui_inventory_columns()).code);session->normalize_selection();
 });return result;
}

namespace {
bsl::WeatherView current_weather(Session& s,const bsl::WeatherOverride* override){
 const auto& c=s.clock->calendar().definition();auto date=s.clock->date();double seconds=s.calendar_seconds*c.numerator/c.denominator+c.hour*3600+c.minute*60+c.second;
 double fraction=(date.month-1+(date.day-1+(date.hour*3600+date.minute*60+date.second)/86400.0)/c.months.at(date.month-1).days)/c.months.size();
 const auto& level=level_registry().effective(bsl_engine_level_file());const auto& w=level.weather;
 std::string location=level.country.empty()?level.locality:level.country+", "+level.locality;
 bsl::WeatherContext context{w.region,location,w.indoor,w.preset,w.wind_factor,w.precipitation_factor,w.hemisphere=="inherit"?"":w.hemisphere};
 return s.weather->project(seconds,fraction,context,override);
}
}
extern "C" void bsl_host_weather_probe(BslWeatherProbe* out){
 if(!out)return;
 *out={};out->icon=-1;
 guarded([&]{if(!session||!session->clock||!session->weather)return;auto& s=*session;auto v=current_weather(s,s.weather_override?&*s.weather_override:nullptr);auto date=s.clock->date();out->enabled=true;out->indoor=v.indoor;out->icon=v.icon;out->daytime=date.hour<6||date.hour>=18;out->wind=v.wind;out->direction=v.direction;out->precipitation=v.amount;out->dust_rate=v.dust_rate;out->particle_speed=v.particle_speed;out->wind_speed=v.wind_speed;
 std::snprintf(out->season,sizeof out->season,"%s",v.season.c_str());std::snprintf(out->hemisphere,sizeof out->hemisphere,"%s",v.hemisphere.c_str());std::snprintf(out->region,sizeof out->region,"%s",v.region.c_str());std::snprintf(out->location,sizeof out->location,"%s",v.location.c_str());std::snprintf(out->preset,sizeof out->preset,"%s",v.preset.c_str());std::snprintf(out->description,sizeof out->description,"%s",v.description.c_str());std::snprintf(out->kind,sizeof out->kind,"%s",v.precipitation.c_str());});
}

extern "C" int bsl_host_weather_override(const BslWeatherOverride* request){
 int result=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!session||!session->clock||!session->weather)return;
  result=static_cast<int>(bsl::Code::busy);if(bsl_host_boot_busy()||session->pending_progress)return;
  result=static_cast<int>(bsl::Code::invalid);if(!request)return;
  bsl::WeatherOverride value;value.preset=request->preset?request->preset:"";value.hemisphere=request->hemisphere?request->hemisphere:"";value.wind_factor=request->wind_factor;value.amount_factor=request->precipitation_factor;value.wind_override=request->wind_override;value.amount_override=request->precipitation_override;value.wind=request->wind;value.direction=request->direction;value.amount=request->precipitation;
  try{current_weather(*session,&value);}catch(const std::invalid_argument&){return;}
  session->weather_override=std::move(value);result=static_cast<int>(bsl::Code::ok);
 });return result;
}
extern "C" int bsl_host_weather_reset(void){
 int result=static_cast<int>(bsl::Code::disabled);guarded([&]{if(!session||!session->weather)return;result=static_cast<int>(bsl::Code::busy);if(bsl_host_boot_busy()||session->pending_progress)return;session->weather_override.reset();result=static_cast<int>(bsl::Code::ok);});return result;
}

namespace {
void write_level_probe(BslLevelProbe& out,const bsl::LevelDefinition& d,const char* scene,bool registered){
 out.enabled=true;out.registered=registered;out.editable=d.editable;out.indoor=d.weather.indoor;out.wind_factor=d.weather.wind_factor;out.precipitation_factor=d.weather.precipitation_factor;
 std::snprintf(out.id,sizeof out.id,"%s",d.id.c_str());std::snprintf(out.scene,sizeof out.scene,"%s",scene);std::snprintf(out.parent,sizeof out.parent,"%s",d.parent.c_str());std::snprintf(out.name,sizeof out.name,"%s",d.name.c_str());std::snprintf(out.country,sizeof out.country,"%s",d.country.c_str());std::snprintf(out.locality,sizeof out.locality,"%s",d.locality.c_str());std::snprintf(out.weather_region,sizeof out.weather_region,"%s",d.weather.region.c_str());std::snprintf(out.hemisphere,sizeof out.hemisphere,"%s",d.weather.hemisphere.c_str());std::snprintf(out.weather_preset,sizeof out.weather_preset,"%s",d.weather.preset.c_str());
}
}
extern "C" void bsl_host_level_probe(BslLevelProbe* out){
 if(!out)return;
 *out={};guarded([&]{const char* scene=bsl_engine_level_file();if(!scene||!*scene)return;const auto& registry=level_registry();write_level_probe(*out,registry.effective(scene),scene,registry.by_scene(scene)!=nullptr);});
}
extern "C" bool bsl_host_level_editable(void){BslLevelProbe out{};bsl_host_level_probe(&out);return out.enabled&&out.editable;}
extern "C" unsigned bsl_host_level_count(void){unsigned count=0;guarded([&]{count=static_cast<unsigned>(level_registry().catalog().records.size());});return count;}
extern "C" bool bsl_host_level_definition(const char* id,BslLevelProbe* out){
 if(!out)return false;
 *out={};guarded([&]{if(!id)return;const auto* d=level_registry().by_id(id);if(d)write_level_probe(*out,*d,d->scene.c_str(),true);});return out->enabled;
}

extern "C" bool bsl_host_level_terrain(const char* scene,BslLevelTerrain* out){
 if(!out)return false;
 *out={};guarded([&]{if(!scene)return;const auto* d=level_registry().by_scene(scene);if(!d||d->terrain.document.empty())return;
  std::snprintf(out->id,sizeof out->id,"%s",d->id.c_str());std::snprintf(out->document,sizeof out->document,"%s",d->terrain.document.c_str());std::snprintf(out->materials,sizeof out->materials,"%s",d->terrain.materials.c_str());
 });return out->document[0]!='\0';
}
