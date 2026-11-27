// Stateless authored-calendar celestial and atmosphere projection. Signed: President.
#include "projection.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
namespace ps::environment {
namespace {
constexpr double pi=3.14159265358979323846;
bool range(double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;}
double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
Celestial body(std::array<double,3> direction,double twilight){
 Celestial b;b.direction=direction;b.altitude_degrees=std::asin(std::clamp(direction[1],-1.,1.))*180/pi;
 b.visibility=smooth((b.altitude_degrees+twilight)/(2*twilight));return b;
}
}
CalendarView calendar_view(const bsl::Calendar& calendar,double elapsed){
 auto date=calendar.project(elapsed);auto& c=calendar.definition();
 long double scaled=static_cast<long double>(elapsed)*c.numerator/c.denominator;
 double second_fraction=static_cast<double>(scaled-std::floor(scaled));
 double day=(date.hour*3600+date.minute*60+date.second+second_fraction)/86400.;
 int days=0,before=0;for(size_t i=0;i<c.months.size();++i){days+=c.months[i].days;if(int(i)+1<date.month)before+=c.months[i].days;}
 // Preserve legacy equal-month seasonal phase separately from elapsed-day annual phase.
 return {day,(before+date.day-1+day)/days,(date.month-1+(date.day-1+day)/c.months.at(date.month-1).days)/c.months.size()};
}
Projection project(const BslEnvironmentProbe& source,const Cycle& c){
 if(!source.available||!range(source.day_fraction,0,1)||source.day_fraction==1||!range(source.year_fraction,0,1)||source.year_fraction==1||!range(source.simulation_seconds,0,1e15)||!range(c.latitude_degrees,0,89)||!range(c.axial_tilt_degrees,0,45)||!range(c.northern_summer_fraction,0,1)||c.northern_summer_fraction==1||!range(c.twilight_degrees,.1,30))throw std::invalid_argument("Invalid environment/celestial projection inputs");
 auto hemisphere=std::string(source.atmosphere.hemisphere);
 if(hemisphere!="north"&&hemisphere!="south")throw std::invalid_argument("Unknown environment hemisphere");
 double latitude=c.latitude_degrees*pi/180*(hemisphere=="south"?-1:1);
 double declination=c.axial_tilt_degrees*pi/180*std::cos(2*pi*(source.year_fraction-c.northern_summer_fraction));
 double hour=2*pi*(source.day_fraction-.5),cosd=std::cos(declination),sind=std::sin(declination);
 std::array<double,3> sun{-cosd*std::sin(hour),std::sin(latitude)*sind+std::cos(latitude)*cosd*std::cos(hour),std::cos(latitude)*sind-std::sin(latitude)*cosd*std::cos(hour)};
 Projection out;out.sun=body(sun,c.twilight_degrees);for(auto& axis:sun)axis=-axis;
 // First proof uses an explicitly antipodal moon. Lunar phase/orbit belongs to a later profile.
 out.moon=body(sun,c.twilight_degrees);out.daylight=out.sun.visibility;out.animation_seconds=source.simulation_seconds;
 auto wind=[](const BslEnvironmentWeather& v){if(!range(v.wind,0,10)||!range(v.direction_degrees,0,360)||v.direction_degrees==360||!range(v.precipitation,0,10))throw std::invalid_argument("Invalid environment weather bands");double angle=v.direction_degrees*pi/180;return std::array<double,2>{v.wind*std::cos(angle),v.wind*std::sin(angle)};};
 // Wind convention remains screen/world X right,Y down; celestial vectors use Y up.
 out.atmospheric_wind=wind(source.atmosphere);out.local_wind=wind(source.local);out.atmospheric_precipitation=source.atmosphere.precipitation;out.local_precipitation=source.local.precipitation;return out;
}
}
