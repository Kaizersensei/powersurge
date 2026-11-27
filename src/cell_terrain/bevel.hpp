// Visual contour relief over authored coverage. No collision mutation. Signed: President.
#pragma once
#include "terrain.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace cell {
struct BevelNormal {double x=0,y=0,z=1,weight=0;};
// Exact separable squared Euclidean transform, then bilinear signed-distance sampling.
class BevelDistance {
 int size_=0,left_=0,top_=0;std::vector<double> values_;
 static void transform(std::vector<double>& f){
  int n=int(f.size());std::vector<int> v(n);std::vector<double> z(n+1),out(n);int k=0;v[0]=0;z[0]=-1e30;z[1]=1e30;
  for(int q=1;q<n;++q){double s;do{s=((f[q]+double(q)*q)-(f[v[k]]+double(v[k])*v[k]))/(2.*(q-v[k]));if(s>z[k])break;--k;}while(k>=0);++k;v[k]=q;z[k]=s;z[k+1]=1e30;}
  k=0;for(int q=0;q<n;++q){while(z[k+1]<q)++k;out[q]=(q-v[k])*double(q-v[k])+f[v[k]];}f.swap(out);
 }
 std::vector<double> squared(const std::vector<uint8_t>& mask,bool foreground){
  std::vector<double> a(mask.size()),line(size_);for(size_t n=0;n<a.size();++n)a[n]=(bool(mask[n])==foreground)?0:1e12;
  for(int y=0;y<size_;++y){for(int x=0;x<size_;++x)line[x]=a[y*size_+x];transform(line);for(int x=0;x<size_;++x)a[y*size_+x]=line[x];}
  for(int x=0;x<size_;++x){for(int y=0;y<size_;++y)line[y]=a[y*size_+x];transform(line);for(int y=0;y<size_;++y)a[y*size_+x]=line[y];}return a;
 }
public:
 BevelDistance(const Field& field,Key key,int halo,int shape,size_t& work){
  size_=side+2*halo;left_=key.first*side-halo;top_=key.second*side-halo;size_t cost=size_t(size_)*size_;
  if(cost>8*1024*1024||work>8*1024*1024-cost)throw std::runtime_error("Bevel rebuild exceeds8M distance samples; reduce width or enabled shapes");work+=cost;
  std::vector<uint8_t> mask(cost);for(int y=0;y<size_;++y)for(int x=0;x<size_;++x){int id=field.at(left_+x,top_+y);mask[y*size_+x]=shape?id==shape:id!=0;}
  auto outside=squared(mask,false),inside=squared(mask,true);values_.resize(cost);
  for(size_t n=0;n<cost;++n)values_[n]=(mask[n]?1:-1)*(std::sqrt(mask[n]?outside[n]:inside[n])*step-step*.5);
 }
 double at(double x,double y)const{
  x=std::clamp(x/step-.5-left_,0.,double(size_-1));y=std::clamp(y/step-.5-top_,0.,double(size_-1));int a=int(x),b=int(y),c=std::min(a+1,size_-1),d=std::min(b+1,size_-1);double u=x-a,v=y-b;
  return (values_[b*size_+a]*(1-u)+values_[b*size_+c]*u)*(1-v)+(values_[d*size_+a]*(1-u)+values_[d*size_+c]*u)*v;
 }
 std::pair<double,double> slope(double x,double y,const Bevel& b)const{
  double distance=at(x,y),width=b.width_m*base;if(distance<0||distance>=width||width<=0||b.depth_m<=0)return {0,0};
  double u=std::clamp(distance/width,.001,.999),slope=b.depth_m*base/width;
  if(b.profile=="round")slope*=(1-u)/std::sqrt(std::max(.001,1-(1-u)*(1-u)));
  if(b.profile=="concave")slope*=u/std::sqrt(std::max(.001,1-u*u));
  double radius=.75+b.smoothness*3,dx=(at(x+radius,y)-at(x-radius,y))/(2*radius),dy=(at(x,y+radius)-at(x,y-radius))/(2*radius);
  double length=std::hypot(dx,dy);if(length>1){dx/=length;dy/=length;}return {-dx*slope,-dy*slope};
 }
};
inline size_t bevel_work_for(const Field& field,Key key,const std::map<int,Bevel>& settings){
 auto block=field.blocks.find(key);if(block==field.blocks.end())return 0;std::set<int> shapes;double width=0;bool joined=false;size_t own=0;
 for(int id:block->second.cells){auto b=settings.find(id);if(b!=settings.end()&&b->second.enabled&&b->second.width_m>0&&b->second.depth_m>0)shapes.insert(id);}
 for(int id:shapes){auto& b=settings.at(id);width=std::max(width,b.width_m);joined|=b.contact!="separate";own+=b.contact!="continuous";}
 if(shapes.empty())return 0;size_t span=side+2*(size_t(std::ceil(width*base/step))+4);return span*span*(own+size_t(joined));
}
inline size_t validate_bevel_work(const Field& field,const std::map<int,Bevel>& settings){
 if(std::none_of(settings.begin(),settings.end(),[](const auto& b){return b.second.enabled&&b.second.width_m>0&&b.second.depth_m>0;}))return 0;
 size_t total=0;for(auto& block:field.blocks){size_t cost=bevel_work_for(field,block.first,settings);if(cost>8*1024*1024||total>8*1024*1024-cost)throw std::runtime_error("Terrain bevels exceed8M full-rebuild distance samples; reduce width or enabled shapes");total+=cost;}return total;
}
inline std::array<BevelNormal,chunk*chunk> bevel_chunk(const Field& field,Key key,const std::map<int,Bevel>& settings,size_t& work){
 std::array<BevelNormal,chunk*chunk> output{};auto block=field.blocks.find(key);if(block==field.blocks.end())return output;
 std::set<int> shapes;double width=0;bool need_union=false;
 for(int id:block->second.cells){auto b=settings.find(id);if(b!=settings.end()&&b->second.enabled&&b->second.width_m>0&&b->second.depth_m>0){shapes.insert(id);width=std::max(width,b->second.width_m);need_union|=b->second.contact!="separate";}}
 if(shapes.empty())return output;int halo=int(std::ceil(width*base/step))+4;
 std::optional<BevelDistance> union_distance;if(need_union)union_distance.emplace(field,key,halo,0,work);
 for(int shape:shapes){const auto& b=settings.at(shape);std::optional<BevelDistance> own;if(b.contact!="continuous")own.emplace(field,key,halo,shape,work);
  for(int y=0;y<chunk;++y)for(int x=0;x<chunk;++x){if(block->second.cells[(y/step)*side+x/step]!=shape)continue;double wx=key.first*chunk+x+.5,wy=key.second*chunk+y+.5;std::pair<double,double> gradient;
   if(b.contact=="separate")gradient=own->slope(wx,wy,b);
   else{gradient=union_distance->slope(wx,wy,b);if(b.contact=="soft_seam"&&own->at(wx,wy)+.01<union_distance->at(wx,wy)){auto seam=own->slope(wx,wy,b);gradient.first+=(seam.first-gradient.first)*.25;gradient.second+=(seam.second-gradient.second)*.25;}}
   double length=std::sqrt(1+gradient.first*gradient.first+gradient.second*gradient.second);auto& n=output[y*chunk+x];n={gradient.first/length,gradient.second/length,1/length,std::hypot(gradient.first,gradient.second)>.000001?1.:0.};
  }
 }
 return output;
}
}
