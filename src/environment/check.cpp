// Focused native proof using synthetic calendars plus the actual disposable host. Signed: President.
#include "projection.hpp"
#include "../foundation/foundation_host.h"
#include "../foundation/weather.hpp"
#include "../labs/lab.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
namespace ps::environment {
namespace {
int checks=0,expected_phase=0;unsigned long long generation=0;BslEnvironmentProbe before{};
void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);++checks;}
bool near(double a,double b,double epsilon=1e-9){return std::abs(a-b)<epsilon;}
void unit(const Celestial& c){auto& d=c.direction;check(near(d[0]*d[0]+d[1]*d[1]+d[2]*d[2],1),"Celestial direction lost unit length");check(c.visibility>=0&&c.visibility<=1,"Visibility left unit range");}
void service_cases(){
 bsl::CalendarDef def;def.numerator=3;def.denominator=2;def.year=0;def.month=1;def.day=1;def.era="test";def.months={{"short",10},{"medium",20},{"long",30}};def.phases={{"day",0}};
 bsl::Calendar calendar(def);auto v=calendar_view(calendar,.5);check(near(v.day_fraction,.75/86400),"Subsecond rational calendar phase lost");check(v.weather_season_fraction>0&&calendar.project(.5).second==0,"Smooth phase changed the existing whole-second date selector");
 v=calendar_view(calendar,10*86400/1.5);check(near(v.year_fraction,1./6)&&near(v.weather_season_fraction,1./3),"Unequal authored months were silently reinterpreted");
 v=calendar_view(calendar,60*86400/1.5);check(v.year_fraction==0&&v.day_fraction==0&&calendar.project(60*86400/1.5).year==1,"Year-zero rollover mismatch");
 def.day=10;def.hour=23;def.minute=59;def.second=59;bsl::Calendar boundary(def);v=calendar_view(boundary,2);check(near(v.day_fraction,2./86400)&&boundary.project(2).month==2,"Initial clock offset/midnight rollover mismatch");
 bool refused=false;try{calendar_view(calendar,-1);}catch(const std::invalid_argument&){refused=true;}check(refused,"Invalid elapsed calendar accepted");
 BslEnvironmentProbe input{};input.available=true;std::snprintf(input.atmosphere.hemisphere,sizeof input.atmosphere.hemisphere,"north");input.year_fraction=.5;input.day_fraction=.5;input.simulation_seconds=12;
 Cycle cycle;auto noon=project(input,cycle);unit(noon.sun);unit(noon.moon);check(noon.daylight==1&&noon.sun.altitude_degrees>70,"Northern summer noon projection incorrect");
 input.day_fraction=0;auto midnight=project(input,cycle);check(midnight.daylight==0&&midnight.moon.visibility==1,"Night visibility mismatch");check(near(noon.sun.direction[0],-noon.moon.direction[0])&&near(noon.sun.direction[1],-noon.moon.direction[1]),"Antipodal proof moon contract changed");
 input.day_fraction=.25;auto east=project(input,cycle);input.day_fraction=.75;auto west=project(input,cycle);check(east.sun.direction[0]>0&&west.sun.direction[0]<0,"East/up/north basis reversed");
 input.day_fraction=.5;input.year_fraction=0;auto winter=project(input,cycle);check(winter.sun.altitude_degrees<noon.sun.altitude_degrees,"Seasonal declination ignored");
 std::snprintf(input.atmosphere.hemisphere,sizeof input.atmosphere.hemisphere,"south");auto southern_summer=project(input,cycle);input.year_fraction=.5;auto southern_winter=project(input,cycle);check(southern_summer.sun.altitude_degrees>southern_winter.sun.altitude_degrees,"Hemisphere inversion missing or applied twice");
 cycle.latitude_degrees=89;check(project(input,cycle).daylight==0,"Polar winter was forced into fixed sunrise");cycle=Cycle{};
 input.atmosphere.wind=4;input.atmosphere.direction_degrees=90;input.atmosphere.precipitation=3;input.local.wind=0;input.local.precipitation=0;input.indoor=true;auto indoor=project(input,cycle);check(near(indoor.atmospheric_wind[0],0)&&near(indoor.atmospheric_wind[1],4)&&indoor.local_wind[1]==0&&indoor.local_precipitation==0&&indoor.atmospheric_precipitation==3,"Atmosphere and local effects were conflated");
 input.ui_seconds=1e6;input.calendar_elapsed=1e8;input.paused=true;auto paused=project(input,cycle);check(paused.animation_seconds==12&&paused.sun.direction==indoor.sun.direction,"UI/calendar elapsed replaced explicit animation or day inputs");
 // Camera, viewport, mute and output gain are deliberately absent from this API.
 auto repeat=project(input,cycle);check(repeat.sun.direction==paused.sun.direction&&repeat.local_wind==paused.local_wind,"Projection is not repeatable");
 auto refuses=[&](BslEnvironmentProbe bad,Cycle settings){bool rejected=false;try{project(bad,settings);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Invalid projection parameter accepted");};
 auto bad=input;bad.available=false;refuses(bad,cycle);bad=input;bad.day_fraction=1;refuses(bad,cycle);bad=input;bad.year_fraction=std::numeric_limits<double>::quiet_NaN();refuses(bad,cycle);bad=input;bad.atmosphere.wind=-1;refuses(bad,cycle);cycle.latitude_degrees=90;refuses(input,cycle);cycle=Cycle{};cycle.twilight_degrees=0;refuses(input,cycle);
 // Indoor masking remains in Weather; its pre-mask projection can feed visible outdoor sky.
 bsl::WeatherDef weather{"region",{{"rain","Rain","rain",0,3,4,90}},{{"season",0,"rain"}},{{"region","north",86400,{}}}};bsl::Weather climate(weather);bsl::WeatherContext context{"region","test",true,"",.5,.5,""};auto local=climate.project(0,0,context);context.indoor=false;auto atmosphere=climate.project(0,0,context);check(local.wind==0&&local.amount==0&&atmosphere.wind==2&&atmosphere.amount==1.5,"Indoor unmasking discarded level modifiers");
 bsl::WeatherOverride modifier;modifier.wind_override=true;modifier.wind=7;modifier.direction=180;atmosphere=climate.project(0,0,context,&modifier);check(atmosphere.wind==7&&atmosphere.direction==180,"External weather override no longer precedes projection");
}
}
int native_check(int phase,char* error,unsigned capacity){
 if(error&&capacity)error[0]=0;
 try{
  if(!ps_lab_automatic())throw std::runtime_error("Environment proof requires a disposable native laboratory");
  if(phase==0){checks=expected_phase=0;generation=ps_lab_generation();service_cases();}
  check(phase==expected_phase&&generation==ps_lab_generation(),"Environment check lifecycle/generation mismatch");
  BslEnvironmentProbe current{};bsl_host_environment_probe(&current);check(current.available&&current.level[0]&&current.region[0],"Actual host environment is unavailable");
  BslWorldTimeProbe world{};BslCalendarProbe clock{};BslWeatherProbe weather{};bsl_host_world_time_probe(&world);bsl_host_calendar_probe(&clock);bsl_host_weather_probe(&weather);
  check(world.available&&clock.enabled&&weather.enabled&&world.generation==current.generation&&world.simulation==current.simulation_seconds&&clock.elapsed==current.calendar_elapsed&&clock.factor==current.calendar_factor&&world.paused==current.paused,"Shared host clock ownership diverged");
  check(weather.wind==current.local.wind&&weather.precipitation==current.local.precipitation&&std::string(weather.preset)==current.local.preset&&std::string(weather.hemisphere)==current.local.hemisphere,"Existing weather consumers changed semantics");
  auto projection=project(current,Cycle{});unit(projection.sun);unit(projection.moon);check(projection.animation_seconds==world.simulation,"World animation acquired another clock");
  BslEnvironmentProbe again{};bsl_host_environment_probe(&again);check(again.simulation_seconds==current.simulation_seconds&&again.calendar_elapsed==current.calendar_elapsed&&again.generation==current.generation,"Read-only environment query advanced time");
  if(phase==0){
   BslWeatherOverride override{};override.wind_factor=override.precipitation_factor=1;override.wind_override=true;override.wind=7;override.direction=45;check(bsl_host_weather_override(&override)==0,"Actual weather override refused");bsl_host_environment_probe(&again);check(again.atmosphere.wind==7&&again.atmosphere.direction_degrees==45,"Snapshot lost external weather override");check(bsl_host_weather_reset()==0,"Weather override reset failed");
   check(bsl_host_calendar_test_policy(1)==0,"Actual menu pause policy refused");bsl_host_environment_probe(&before);
  }else if(phase==1){
   check(current.paused&&current.simulation_seconds==before.simulation_seconds&&current.calendar_elapsed==before.calendar_elapsed&&current.day_fraction==before.day_fraction&&current.ui_seconds>before.ui_seconds,"Menu pause did not freeze environment while UI continued");
   check(bsl_host_calendar_test_policy(2)==0,"Actual dialogue policy refused");bsl_host_environment_probe(&before);
  }else if(phase==2){
   double simulation=current.simulation_seconds-before.simulation_seconds;check(!current.paused&&near(current.calendar_factor,.1)&&simulation>0&&near((current.calendar_elapsed-before.calendar_elapsed)/simulation,.1,1e-6),"Dialogue did not slow only calendar to10percent");check(bsl_host_calendar_test_policy(0)==0,"Dialogue policy release failed");bsl_host_environment_probe(&before);
  }else if(phase==3){
   double simulation=current.simulation_seconds-before.simulation_seconds;check(!current.paused&&current.calendar_factor==1&&simulation>0&&near((current.calendar_elapsed-before.calendar_elapsed)/simulation,1,1e-6),"Calendar did not resume without backlog");
  }
  ++expected_phase;return checks;
 }catch(const std::exception& e){bsl_host_weather_reset();bsl_host_calendar_test_policy(0);if(error&&capacity)std::snprintf(error,capacity,"%s",e.what());return -1;}
}
}
