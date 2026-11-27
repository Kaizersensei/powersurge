// Copied main-thread environment view; no additional clock or mutable climate. Signed: President.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BslEnvironmentWeather {
 double wind,direction_degrees,precipitation;
 char preset[129],kind[129],season[129],hemisphere[8];
} BslEnvironmentWeather;
typedef struct BslEnvironmentProbe {
 bool available,paused,indoor;
 uint64_t generation;
 double simulation_seconds,ui_seconds,calendar_elapsed,calendar_factor;
 /* Seasonal phase is smooth; the existing Weather selector retains whole-second steps. */
 double day_fraction,year_fraction,weather_season_fraction;
 char level[129],region[129],diagnostic[257];
 /* Atmosphere is effective level weather BEFORE indoor suppression, not regional baseline. */
 BslEnvironmentWeather atmosphere,local;
} BslEnvironmentProbe;
void bsl_host_environment_probe(BslEnvironmentProbe* out);
#ifdef __cplusplus
}
#endif
