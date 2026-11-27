// Stateless shared inputs for sky/lighting; world axes east +X, up +Y, north +Z. Signed: President.
#pragma once
#include "environment.h"
#include "../foundation/calendar.hpp"
#include <array>
namespace ps::environment {
struct CalendarView {double day_fraction=0,year_fraction=0,weather_season_fraction=0;};
CalendarView calendar_view(const bsl::Calendar&,double elapsed);
// Stylized astronomical projection over arbitrary authored years, not an Earth ephemeris.
struct Cycle {double latitude_degrees=35,axial_tilt_degrees=23.44,northern_summer_fraction=.5,twilight_degrees=6;};
struct Celestial {std::array<double,3> direction{};double altitude_degrees=0,visibility=0;};
struct Projection {
 Celestial sun,moon;double daylight=0;
 std::array<double,2> atmospheric_wind{},local_wind{};
 double atmospheric_precipitation=0,local_precipitation=0;
 // Cloud/card/wave animation consumes simulation_seconds, never calendar or wall-clock time.
 double animation_seconds=0;
};
Projection project(const BslEnvironmentProbe&,const Cycle&);
// In-engine proof; phases share only disposable test state, never normal progress.
int native_check(int phase,char* error,unsigned capacity);
}
