// One shared CPU view projection for the runtime and native preview. Signed: President.
#include "sky_profile.hpp"
#include <cstring>
namespace ps::environment {
std::array<double,2> CloudMotion::advance(const SkyProfile& p,const BslEnvironmentProbe& input){
 auto wind=project(input,p.cycle).atmospheric_wind;double elapsed=ready&&generation==input.generation&&input.simulation_seconds>=last_time?input.simulation_seconds-last_time:input.simulation_seconds;
 if(!ready||generation!=input.generation||input.simulation_seconds<last_time)offset={};
 for(int i=0;i<2;++i){double speed=p.velocity[i]+wind[i]*p.wind_response;offset[i]=std::fmod(offset[i]-elapsed*speed,256.);}
 ready=true;generation=input.generation;last_time=input.simulation_seconds;return offset;
}
SkyFrame sky_frame(const SkyProfile& p,const SkyAttachment& a,const BslEnvironmentProbe& input,double camera_y,int width,int height){
 if(width<1||height<1||!std::isfinite(camera_y)||!std::isfinite(a.origin_y)||!std::isfinite(a.pixels_per_metre)||a.pixels_per_metre<=0||p.palette.size()<2)throw std::invalid_argument("Invalid sky view");auto orbit=project(input,p.cycle);
 auto first=p.palette.begin(),next=first;while(next!=p.palette.end()&&next->phase<=input.day_fraction)++next;
 auto previous=next==first?p.palette.end()-1:next-1;double end=next==p.palette.end()?1:next->phase;auto& right=next==p.palette.end()?p.palette.front():*next;double t=(input.day_fraction-previous->phase)/(end-previous->phase);
 auto mix=[&](Color l,Color r){for(int i=0;i<3;++i)l[i]+=(r[i]-l[i])*t;return l;};SkyFrame out;out.top=mix(previous->top,right.top);out.horizon_color=mix(previous->horizon,right.horizon);out.bottom=mix(previous->bottom,right.bottom);
 double altitude=(a.origin_y-camera_y)/a.pixels_per_metre;out.horizon=std::clamp(p.horizon+(altitude-p.reference_altitude)*a.pixels_per_metre*p.altitude_parallax/height,p.min_horizon,p.max_horizon);out.daylight=orbit.daylight;
 out.cloud_color=p.cloud_color;out.cloud_coverage=p.cloud_coverage;out.cloud_opacity=p.clouds?p.cloud_opacity:0;out.cloud_scale=p.cloud_scale;out.cloud_softness=p.cloud_softness;out.seed=p.seed;
 auto weather=p.weather.find(input.atmosphere.preset);if(weather!=p.weather.end()){auto& w=weather->second;out.cloud_color=w.cloud_color;out.cloud_coverage=w.coverage;out.cloud_opacity=p.clouds?w.opacity:0;for(int i=0;i<3;++i){out.top[i]*=w.sky_tint[i];out.horizon_color[i]*=w.sky_tint[i];out.bottom[i]*=w.sky_tint[i];}}
 for(auto& channel:out.cloud_color)channel*=p.night_brightness+(1-p.night_brightness)*orbit.daylight;
 for(int i=0;i<2;++i){double speed=p.velocity[i]+orbit.atmospheric_wind[i]*p.wind_response;out.cloud_offset[i]=std::fmod(-orbit.animation_seconds*speed,256.);}
 auto body=[&](const Celestial& c,const SkyBody& b){double angle=p.azimuth*3.141592653589793/180;double horizontal=c.direction[0]*std::cos(angle)-c.direction[2]*std::sin(angle);return std::array<double,4>{.5+horizontal*.43,out.horizon-c.direction[1]*.8,b.radius,b.enabled?b.energy*c.visibility:0};};out.sun=body(orbit.sun,p.sun);out.moon=body(orbit.moon,p.moon);out.sun_color=p.sun.color;out.moon_color=p.moon.color;return out;
}
}
