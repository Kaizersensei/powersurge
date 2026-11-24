// Bounded, file-authored event sequencing; no map/battle loop. Signed: Atlas.
#pragma once
#include "foundation.hpp"
#include "teleport.hpp"

namespace bsl {
enum class EventStatus { queued, running, waiting, completed, cancelled, failed };
struct EventContext { std::string owner, player; std::string character={}; std::optional<TravelContext> travel={}; };
class EventRewardPort {
public:
 virtual ~EventRewardPort()=default;
 virtual Result grant(const EventContext&,const EventCommand&)=0;
};
class EventSkillPort {
public:
 virtual ~EventSkillPort()=default;
 virtual bool knows_ability(const std::string&) const noexcept=0;
 virtual Result invoke(const EventContext&,const EventCommand&)=0;
};
class EventQuestPort {
public:
 virtual ~EventQuestPort()=default;
 virtual Result invoke(const EventContext&,const EventCommand&)=0;
};
class EventClockPort {
public:
 virtual ~EventClockPort()=default;
 virtual Result skip(const EventContext&,int game_seconds)=0;
};
class EventTradePort {
public:
 virtual ~EventTradePort()=default;
 virtual Result invoke(const EventContext&,const EventCommand&,const std::string& transaction)=0;
};
// World instantiation stays in the host adapter, not the event interpreter. Signed: Mech.
class EventActorPort {
public:
 virtual ~EventActorPort()=default;
 virtual Result spawn(const EventContext&,const EventCommand&)=0;
};
struct EventRun {
 std::string id, definition;
 EventContext context;
 EventStatus status=EventStatus::queued;
 std::size_t next=0;
 int wait_ticks=0;
 Code error=Code::ok;
};
struct EventState {
 std::string catalog_signature;
 std::map<std::pair<std::string,std::string>,int> values;
 std::map<std::string,EventRun> runs;
};
class Events {
public:
 Events(const Catalog&,Inventory&);
 Events(const Events&)=delete;
 Events& operator=(const Events&)=delete;
 // Bind once during session construction; the adapter shares the session's lifetime.
 void bind_rewards(EventRewardPort&);
 void bind_skills(EventSkillPort&);
 void bind_quests(EventQuestPort&);
 void bind_trades(EventTradePort&);
 void bind_clock(EventClockPort&);
 void bind_actors(EventActorPort&);
 Result start(const std::string& run,const std::string& definition,const EventContext&);
 // Call exactly once per unpaused host simulation tick (60 Hz); budget is shared across runs.
 void tick(unsigned command_budget=16);
 bool cancel(const std::string& run) noexcept;
 void cancel_participant(const std::string& identity) noexcept;
 const EventRun* run(const std::string&) const noexcept;
 bool flag(const std::string& definition,const std::string& player="") const;
 int variable(const std::string& definition,const std::string& player="") const;
 EventState capture() const;
 void restore(const EventState&);
 const std::string& catalog_signature() const noexcept {return catalog_.signature();}
private:
 const Catalog catalog_;
 Inventory& inventory_;
 EventRewardPort* rewards_=nullptr;
 EventSkillPort* skills_=nullptr;
 EventQuestPort* quests_=nullptr;
 EventTradePort* trades_=nullptr;
 EventClockPort* clock_=nullptr;
 EventActorPort* actors_=nullptr;
 std::map<std::pair<std::string,std::string>,int> values_;
 std::map<std::string,EventRun> runs_;
 std::string key(Scope,const std::string&) const;
 int value(const std::string&,Scope,int,const std::string&) const;
 Code execute(EventRun&,const EventCommand&);
 friend class SessionDataStore;
 friend class RewindServices;
 friend class PickupClaims;
 friend class Quests;
 friend class Commerce;
};
}
