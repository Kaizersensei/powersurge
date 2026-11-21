// Data-authored repeatable calendar over the existing policy-scaled elapsed clock. Signed: Atlas.
#pragma once
#include "events.hpp"
namespace bsl {
struct CalendarMonth {std::string name;int days=30;};
struct CalendarPhase {std::string name;int second=0;};
struct CalendarDef {
 unsigned numerator=1,denominator=1;std::int64_t year=1;int month=1,day=1,hour=0,minute=0,second=0;
 std::string era;std::vector<CalendarMonth> months;std::vector<CalendarPhase> phases;
};
struct CalendarDate {std::int64_t year=0;int month=0,day=0,hour=0,minute=0,second=0;std::string month_name,phase,era;};
class Calendar {
public:
 explicit Calendar(CalendarDef);
 bool valid(double elapsed) const noexcept;
 CalendarDate project(double elapsed) const;
 const CalendarDef& definition() const noexcept{return definition_;}
 static constexpr double maximum_elapsed=1e9;
private:CalendarDef definition_;std::int64_t year_days_=0,origin_=0;
};
// Mutable time stays in Session.calendar_seconds; this adapter owns no second game clock.
class CalendarClock final:public EventClockPort {
public:
 CalendarClock(CalendarDef,double& elapsed,std::set<std::string> players);
 Code advance(double policy_elapsed) noexcept;
 Result skip(const EventContext&,int game_seconds) override;
 CalendarDate date() const{return calendar_.project(elapsed_);}
 const Calendar& calendar() const noexcept{return calendar_;}
private:Calendar calendar_;double& elapsed_;std::set<std::string> players_;
};
}
