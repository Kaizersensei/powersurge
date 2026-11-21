// Original weather-to-ambience mix plan, separate from native voice ownership. Signed: Atlas.
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
namespace bsl {
struct AmbientLayer {std::string id,path,driver;double max_gain=.3,fade_seconds=1;};
struct AmbientDefinition {std::vector<AmbientLayer> layers;};
class AmbientMix {
 AmbientDefinition def_;std::vector<double> gain_;
public:
 explicit AmbientMix(AmbientDefinition d):def_(std::move(d)){
  if(def_.layers.size()>8)throw std::invalid_argument("too many ambient layers");
  for(size_t i=0;i<def_.layers.size();++i){const auto& l=def_.layers[i];
   if(l.id.rfind("ambience:",0)||l.id.size()>128||l.id.size()<10||l.id.find('\0')!=std::string::npos||l.path.rfind("samples/",0)||l.path.size()>256||l.path.find("..")!=std::string::npos||l.path.find_first_of("\\:\n\r")!=std::string::npos||l.path.find('\0')!=std::string::npos||!std::isfinite(l.max_gain)||l.max_gain<0||l.max_gain>1||!std::isfinite(l.fade_seconds)||l.fade_seconds<.01||l.fade_seconds>30||(l.driver!="wind"&&l.driver!="rain"&&l.driver!="snow"&&l.driver!="ash"))throw std::invalid_argument("invalid ambient layer");
   for(size_t j=0;j<i;++j)if(def_.layers[j].id==l.id)throw std::invalid_argument("duplicate ambient layer");
  }
  gain_.resize(def_.layers.size());
 }
 void update(double dt,bool paused,bool indoor,double wind,const std::string& kind,double amount){
  if(!std::isfinite(dt)||dt<0||!std::isfinite(wind)||wind<0||wind>10||!std::isfinite(amount)||amount<0||amount>10)throw std::invalid_argument("invalid ambient context");
  if(paused)return;
  for(size_t i=0;i<gain_.size();++i){const auto& l=def_.layers[i];double target=indoor?0:l.max_gain*(l.driver=="wind"?wind:l.driver==kind?amount:0)/10;
   double step=std::min(dt,.25)*l.max_gain/l.fade_seconds;gain_[i]+=std::clamp(target-gain_[i],-step,step);
  }
 }
 void clear(){std::fill(gain_.begin(),gain_.end(),0);}
 const AmbientDefinition& definition()const noexcept{return def_;}
 const std::vector<double>& gains()const noexcept{return gain_;}
};
}
