// Small cosmetic reaction episodes; simulation/AI authority remains elsewhere. Signed: Atlas.
#pragma once
#include <algorithm>
#include <cmath>
namespace bsl {
struct ReactionTuning {double lifetime=2.5,fade=.5,inflate=4.0/60,peak=1.12,end=.7,rise=24;};
struct ReactionBubble {
 int kind=0,queued=0;double age=0,x=0,y=0;
 void emit(int next){if(next<0||next>3)return;if(next==0){queued=0;return;}if(next==2&&kind==1){queued=2;return;}kind=next;queued=0;age=0;}
 void tick(double dt,const ReactionTuning& t){if(!kind||!std::isfinite(dt)||dt<=0)return;age+=dt;if(age>=t.lifetime){kind=queued;queued=0;age=0;}}
 double alpha(const ReactionTuning& t)const{return kind?std::clamp((t.lifetime-age)/t.fade,0.0,1.0):0;}
 double scale(const ReactionTuning& t)const{if(!kind)return 0;if(age<t.inflate){double f=std::clamp(age/t.inflate,0.0,1.0);return t.peak*(1-std::pow(1-f,3));}double f=std::clamp((age-t.inflate)/(t.lifetime-t.inflate),0.0,1.0);return t.peak+(t.end-t.peak)*f;}
 double rise(const ReactionTuning& t)const{return t.rise*std::clamp(age/t.lifetime,0.0,1.0);}
};
}
