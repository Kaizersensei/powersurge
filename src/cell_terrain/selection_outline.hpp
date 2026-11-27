// Read-only, chunk-cached coverage contours for editor selection. Signed: President.
#pragma once
#include "terrain.hpp"
#include <algorithm>
#include <tuple>
#include <stdexcept>
namespace cell {
class SelectionOutline {
 std::map<Key,std::vector<Edge>> cached_;
public:
 uint64_t builds=0;
 void clear(){cached_.clear();}
 void invalidate(const std::set<Key>& changed){for(auto k:changed)for(auto d:{Key{0,0},Key{-1,0},Key{1,0},Key{0,-1},Key{0,1}})cached_.erase({k.first+d.first,k.second+d.second});}
 const std::vector<Edge>& lines(const Field& field,Key key){
  auto old=cached_.find(key);if(old!=cached_.end())return old->second;
  auto block=field.blocks.find(key);static const std::vector<Edge> empty;if(block==field.blocks.end())return empty;
  using Group=std::tuple<int,bool,int>;std::map<Group,std::vector<std::pair<int,int>>> runs;
  auto edge=[&](int shape,bool vertical,int fixed,int from){runs[{shape,vertical,fixed}].push_back({from,from+step});};
  for(int y=0;y<side;y++)for(int x=0;x<side;x++){
   int id=block->second.cells[y*side+x];if(!id)continue;int wx=key.first*side+x,wy=key.second*side+y;
   if(field.at(wx,wy-1)!=id)edge(id,false,wy*step,wx*step);
   if(field.at(wx,wy+1)!=id)edge(id,false,(wy+1)*step,wx*step);
   if(field.at(wx-1,wy)!=id)edge(id,true,wx*step,wy*step);
   if(field.at(wx+1,wy)!=id)edge(id,true,(wx+1)*step,wy*step);
  }
  std::vector<Edge> result;for(auto& group:runs){auto& intervals=group.second;std::sort(intervals.begin(),intervals.end());int from=intervals[0].first,to=intervals[0].second;auto emit=[&]{int shape=std::get<0>(group.first),fixed=std::get<2>(group.first);bool vertical=std::get<1>(group.first);result.push_back(vertical?Edge{fixed,from,fixed,to,shape}:Edge{from,fixed,to,fixed,shape});};for(size_t i=1;i<intervals.size();i++){if(intervals[i].first<=to)to=std::max(to,intervals[i].second);else{emit();from=intervals[i].first;to=intervals[i].second;}}emit();}
  ++builds;return cached_.emplace(key,std::move(result)).first->second;
 }
};
// Exact unit-edge comparison includes chunk seams, holes and material/shape boundaries.
inline int selection_outline_checks(){
 int checks=0;auto check=[&](bool okay){if(!okay)throw std::runtime_error("Terrain selection contour/cache mismatch");checks++;};
 Field f;f.doc.shapes={{1,"material:stone","square",false,1,0},{2,"material:wood","square",false,1,0}};SelectionOutline cache;
 auto paint=[&](Stroke s){f.doc.strokes.push_back(s);cache.invalidate(f.paint(s));};
 using Unit=std::tuple<int,int,int,int,int>;auto expand=[](std::set<Unit>& units,const Edge& e){if(e.y==e.v)for(int x=std::min(e.x,e.u);x<std::max(e.x,e.u);x+=step)units.emplace(e.shape,x,e.y,x+step,e.y);else for(int y=std::min(e.y,e.v);y<std::max(e.y,e.v);y+=step)units.emplace(e.shape,e.x,y,e.x,y+step);};
 auto cached=[&]{std::set<Unit> units;for(auto& block:f.blocks)for(auto& e:cache.lines(f,block.first))expand(units,e);return units;};
 auto matches=[&]{std::set<Unit> expected;for(auto& e:f.edges())expand(expected,e);check(cached()==expected);};
 paint({1,false,32,32,96,32,32,0,.5});matches();auto built=cache.builds;auto original=cached();check(cache.builds==built&&!original.empty());
 paint({1,false,-128,-96,-128,-96,16,0,.5});matches();auto disconnected=cached();check(std::any_of(disconnected.begin(),disconnected.end(),[](auto& e){return std::get<1>(e)<0;}));
 paint({1,true,64,32,64,32,8,0,.5});matches(); // both sides of a chunk boundary must become exposed
 paint({2,false,112,32,112,32,32,0,.5});matches();auto both=cached();int one=0,two=0;for(auto& e:both){one+=std::get<0>(e)==1;two+=std::get<0>(e)==2;}check(one>0&&two>0);built=cache.builds;cached();check(cache.builds==built);
 paint({1,true,-128,-96,-128,-96,64,0,.5});matches();cache.clear();matches();check(cache.builds>built);
 return checks;
}
}
