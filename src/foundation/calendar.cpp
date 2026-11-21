// Original elapsed-time projection; no wall clock, scheduler or weather renderer. Signed: Atlas.
#include "calendar.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace bsl {
namespace {bool label(const std::string& s){return !s.empty()&&s.size()<=128&&s.find('\0')==std::string::npos;}}
Calendar::Calendar(CalendarDef d):definition_(std::move(d)){
 const auto& c=definition_;
 if(!c.numerator||c.numerator>86400||!c.denominator||c.denominator>1000000||c.year<0||c.year>1000000||!label(c.era)||c.months.empty()||c.months.size()>24||c.phases.empty()||c.phases.size()>24||c.month<1||c.month>static_cast<int>(c.months.size())||c.hour<0||c.hour>23||c.minute<0||c.minute>59||c.second<0||c.second>59)throw std::invalid_argument("invalid calendar definition");
 std::set<std::string> months,phases;std::int64_t preceding=0;int index=0;
 for(const auto& m:c.months){if(!label(m.name)||!months.insert(m.name).second||m.days<1||m.days>100)throw std::invalid_argument("invalid calendar month");year_days_+=m.days;if(++index<c.month)preceding+=m.days;}
 if(c.day<1||c.day>c.months[c.month-1].days)throw std::invalid_argument("invalid calendar start day");
 int previous=-1;for(const auto& p:c.phases){if(!label(p.name)||!phases.insert(p.name).second||p.second<0||p.second>86399||p.second<=previous)throw std::invalid_argument("invalid day phase");previous=p.second;}
 if(c.phases.front().second!=0)throw std::invalid_argument("day phases must start at midnight");
 origin_=(c.year*year_days_+preceding+c.day-1)*86400+c.hour*3600+c.minute*60+c.second;
}
bool Calendar::valid(double value) const noexcept{return std::isfinite(value)&&value>=0&&value<=maximum_elapsed;}
CalendarDate Calendar::project(double elapsed) const {
 if(!valid(elapsed))throw std::invalid_argument("calendar elapsed clock out of range");
 const auto total=origin_+static_cast<std::int64_t>(std::floor(static_cast<long double>(elapsed)*definition_.numerator/definition_.denominator));
 auto days=total/86400;int seconds=static_cast<int>(total%86400);CalendarDate d;d.year=days/year_days_;days%=year_days_;d.month=1;
 for(const auto& m:definition_.months){if(days<m.days){d.month_name=m.name;break;}days-=m.days;++d.month;}
 d.day=static_cast<int>(days)+1;d.hour=seconds/3600;d.minute=seconds/60%60;d.second=seconds%60;d.era=definition_.era;
 for(const auto& p:definition_.phases){if(p.second>seconds)break;d.phase=p.name;}return d;
}
CalendarClock::CalendarClock(CalendarDef d,double& elapsed,std::set<std::string> players):calendar_(std::move(d)),elapsed_(elapsed),players_(std::move(players)){
 if(!calendar_.valid(elapsed_)||players_.empty())throw std::invalid_argument("invalid calendar owner/clock");
 for(const auto& p:players_)if(!label(p))throw std::invalid_argument("invalid calendar player");
}
Code CalendarClock::advance(double delta) noexcept{
 if(!calendar_.valid(elapsed_)||!std::isfinite(delta)||delta<0||!calendar_.valid(elapsed_+delta))return Code::invalid;
 elapsed_+=delta;return Code::ok;
}
Result CalendarClock::skip(const EventContext& context,int seconds){
 if(!players_.count(context.player))return {Code::denied};
 if(!calendar_.valid(elapsed_)||seconds<1||seconds>31536000)return {Code::invalid};
 const auto& d=calendar_.definition();long double exact=static_cast<long double>(elapsed_)+static_cast<long double>(seconds)*d.denominator/d.numerator;
 double next=static_cast<double>(exact);if(static_cast<long double>(next)<exact)next=std::nextafter(next,std::numeric_limits<double>::infinity());
 if(!calendar_.valid(next))return {Code::denied};
 elapsed_=next;return {Code::ok};
}
}
