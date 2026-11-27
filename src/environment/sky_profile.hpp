// Versioned sky definitions; no clocks, image assets or runtime state. Signed: President.
#pragma once
#include "projection.hpp"
#include "../authoring/documents.hpp"
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ps::environment {
using Color=std::array<double,3>;
struct SkyStop {double phase;Color top,horizon,bottom;};
struct SkyWeather {double coverage,opacity;Color cloud_color,sky_tint;};
struct SkyBody {bool enabled;double radius,energy;Color color;};
struct SkyProfile {
 std::string id;Cycle cycle;double azimuth=180,horizon=.62,reference_altitude=0,altitude_parallax=.01,min_horizon=-2,max_horizon=3;
 std::vector<SkyStop> palette;SkyBody sun{},moon{};bool clouds=false;
 double cloud_scale=4,cloud_coverage=.5,cloud_softness=.12,cloud_opacity=.7,wind_response=.002,seed=0,night_brightness=.18;
 std::array<double,2> velocity{};Color cloud_color{1,1,1};std::map<std::string,SkyWeather> weather;
};
struct SkyAttachment {std::string profile;double origin_y=0,pixels_per_metre=32;bool visible_indoors=false;};
inline SkyProfile sky_profile(const authoring::Json& j){
 auto need=[](bool okay,const char* why){if(!okay)throw std::runtime_error(why);};
 auto number=[&](const authoring::Json& o,const char* key,double low,double high){need(o.contains(key)&&o[key].is_number(),"Sky numeric field required");double n=o[key].get<double>();need(std::isfinite(n)&&n>=low&&n<=high,"Sky parameter out of range");return n;};
 auto flag=[&](const authoring::Json& o,const char* key){need(o.contains(key)&&o[key].is_boolean(),"Sky boolean required");return o[key].get<bool>();};
 auto color=[&](const authoring::Json& o,const char* key){auto& a=o.at(key);need(a.is_array()&&a.size()==3,"Sky color requires3 sRGB channels");Color out{};for(int i=0;i<3;++i){need(a[i].is_number(),"Sky color channel required");out[i]=a[i].get<double>();need(std::isfinite(out[i])&&out[i]>=0&&out[i]<=1,"Sky color out of range");}return out;};
 SkyProfile p;p.id=j.at("id").get<std::string>();auto& c=j.at("cycle");p.cycle={number(c,"latitude_degrees",0,89),number(c,"axial_tilt_degrees",0,45),number(c,"northern_summer_fraction",0,1),number(c,"twilight_degrees",.1,30)};need(p.cycle.northern_summer_fraction<1,"Summer phase must be below1");
 p.azimuth=number(j,"view_azimuth_degrees",0,360);need(p.azimuth<360,"Sky azimuth360 aliases0");auto& h=j.at("horizon");p.horizon=number(h,"fraction",-2,3);p.reference_altitude=number(h,"reference_altitude_m",-100000,100000);p.altitude_parallax=number(h,"altitude_parallax",-1,1);p.min_horizon=number(h,"minimum_fraction",-2,3);p.max_horizon=number(h,"maximum_fraction",-2,3);need(p.min_horizon<p.max_horizon&&p.horizon>=p.min_horizon&&p.horizon<=p.max_horizon,"Sky horizon bounds invalid");
 auto& stops=j.at("palette");need(stops.is_array()&&stops.size()>=2&&stops.size()<=16,"Sky palette requires2–16 stops");double previous=-1;for(auto& row:stops){double phase=number(row,"phase",0,1);need(phase<1&&phase>previous,"Sky palette phases must increase below1");previous=phase;p.palette.push_back({phase,color(row,"top"),color(row,"horizon"),color(row,"bottom")});}need(p.palette.front().phase==0,"Sky palette starts at phase0");
 auto body=[&](const authoring::Json& b){return SkyBody{flag(b,"enabled"),number(b,"radius_fraction",.001,.2),number(b,"energy",0,4),color(b,"color")};};p.sun=body(j.at("sun"));p.moon=body(j.at("moon"));auto& cloud=j.at("clouds");need(cloud.at("program")=="builtin:sky-clouds-v1","Unsupported sky cloud program");p.clouds=flag(cloud,"enabled");p.cloud_scale=number(cloud,"scale",.25,32);p.cloud_coverage=number(cloud,"coverage",0,1);p.cloud_softness=number(cloud,"softness",.01,.5);p.cloud_opacity=number(cloud,"opacity",0,1);p.wind_response=number(cloud,"wind_response",0,.1);p.seed=number(cloud,"seed",0,10000);p.night_brightness=number(cloud,"night_brightness",0,1);p.cloud_color=color(cloud,"color");
 auto& velocity=cloud.at("velocity_noise_per_second");need(velocity.is_array()&&velocity.size()==2,"Sky cloud velocity requires two components");for(int i=0;i<2;++i){need(velocity[i].is_number(),"Sky cloud velocity number required");p.velocity[i]=velocity[i].get<double>();need(std::isfinite(p.velocity[i])&&std::abs(p.velocity[i])<=1,"Sky cloud velocity out of range");}
 auto& weather=j.at("weather");need(weather.is_object()&&weather.size()<=64,"Sky weather mapping must be bounded object");for(auto it=weather.begin();it!=weather.end();++it)p.weather.emplace(it.key(),SkyWeather{number(it.value(),"coverage",0,1),number(it.value(),"opacity",0,1),color(it.value(),"cloud_color"),color(it.value(),"sky_tint")});return p;
}
// Used by the actual form when explicitly enabling a minimal/null attachment.
inline void sky_attachment_defaults(authoring::Json& value,double origin){
 if(value.is_null())value=authoring::Json::object();
 auto defaults=authoring::Json{{"profile",nullptr},{"origin_y_px",origin},{"px_per_m",32},{"visible_indoors",false}};
 for(auto& field:defaults.items())if(!value.contains(field.key()))value[field.key()]=field.value();
}
struct CloudMotion {
 bool ready=false;uint64_t generation=0;double last_time=0;std::array<double,2> offset{};
 std::array<double,2> advance(const SkyProfile&,const BslEnvironmentProbe&);
};
struct SkyFrame {
 Color top{},horizon_color{},bottom{},cloud_color{};double horizon=0,cloud_coverage=0,cloud_opacity=0,cloud_scale=0,cloud_softness=0,seed=0,daylight=0;
 std::array<double,2> cloud_offset{};std::array<double,4> sun{},moon{};Color sun_color{},moon_color{};
};
SkyFrame sky_frame(const SkyProfile&,const SkyAttachment&,const BslEnvironmentProbe&,double camera_y,int width,int height);
}
