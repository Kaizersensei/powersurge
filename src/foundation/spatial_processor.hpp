// Per-voice stereo filtering and smoothed balance. No allocation/locks in process(). Signed: Atlas.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include "signalsmith/filters.h"
namespace bsl {
class SpatialProcessor {
 using Filter=signalsmith::filters::BiquadStatic<float>;
 std::array<Filter,256> bank_;Filter left_,right_;
 std::atomic<uint64_t> target_{0};float gain_=0,balance_=0,cutoff_=20000,alpha_;unsigned phase_=0;
public:
 static_assert(std::atomic<uint64_t>::is_always_lock_free,"audio parameter snapshots must be lock free");
 explicit SpatialProcessor(double frequency,double smoothing=.045):alpha_(float(1-std::exp(-1/(frequency*smoothing)))){
  for(size_t i=0;i<bank_.size();++i)bank_[i].lowpassQ(std::min(.45,350*std::pow(20000.0/350,double(i)/255)/frequency),.7071067811865476);
  left_.copyFrom(bank_.back());right_.copyFrom(bank_.back());set(0,0,20000);
 }
 void set(double gain,double balance,double cutoff){
  uint64_t g=uint64_t(std::clamp(gain,0.0,1.0)*65535),b=uint64_t((std::clamp(balance,-1.0,1.0)+1)*32767.5),c=uint64_t(std::clamp(cutoff,350.0,20000.0));
  target_.store(g|(b<<16)|(c<<32),std::memory_order_relaxed);
 }
 void process(float* samples,unsigned frames){
  const uint64_t t=target_.load(std::memory_order_relaxed);const float g=float(t&65535)/65535,b=float((t>>16)&65535)/32767.5f-1,c=float((t>>32)&65535);
  for(unsigned i=0;i<frames;++i){gain_+=alpha_*(g-gain_);balance_+=alpha_*(b-balance_);cutoff_+=alpha_*(c-cutoff_);
   if((phase_++&63)==0){int index=int(std::round(std::log(std::max(350.f,cutoff_)/350)/std::log(20000.0/350)*255));index=std::clamp(index,0,255);left_.copyFrom(bank_[size_t(index)]);right_.copyFrom(bank_[size_t(index)]);}
   samples[i*2]=left_(samples[i*2])*gain_*(1-std::max(0.f,balance_));samples[i*2+1]=right_(samples[i*2+1])*gain_*(1+std::min(0.f,balance_));
  }
 }
};
// Zero-lookahead stereo-linked sample-peak limiter. Immediate attack, exponential release.
// It controls sample peaks, not reconstructed inter-sample true peaks.
class MasterLimiter {
 static_assert(std::atomic<float>::is_always_lock_free,"master diagnostics must be lock free");
 float gain_=1,release_=float(1-std::exp(-1/(44100*.12))),ceiling_=.95f;
 std::atomic<float> reduction_{0},peak_{0};
public:
 void prepare(double frequency,double ceiling=.95,double release=.12){gain_=1;ceiling_=float(ceiling);release_=float(1-std::exp(-1/(frequency*release)));}
 void process(float* data,unsigned frames){float peak=0,minimum=1;
  for(unsigned i=0;i<frames;++i){float& l=data[2*i];float& r=data[2*i+1];if(!std::isfinite(l))l=0;if(!std::isfinite(r))r=0;
   float input=std::max(std::abs(l),std::abs(r)),need=input>ceiling_?ceiling_/input:1;
   if(need<gain_)gain_=need;else gain_+=release_*(need-gain_);
   l*=gain_;r*=gain_;minimum=std::min(minimum,gain_);peak=std::max(peak,std::max(std::abs(l),std::abs(r)));
  }reduction_.store(1-minimum,std::memory_order_relaxed);peak_.store(peak,std::memory_order_relaxed);
 }
 float reduction()const{return reduction_.load(std::memory_order_relaxed);}float peak()const{return peak_.load(std::memory_order_relaxed);}
};

}
