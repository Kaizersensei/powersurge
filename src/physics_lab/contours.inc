// Whole-field contour experiment, not a game adapter. Signed: Mech.
#include "../cell_terrain/terrain.hpp"
#include <box2d/box2d.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using Point=std::pair<int,int>;
struct Contours {std::vector<std::vector<Point>> loops;int edges=0;bool valid=true;};
static bool covered(const cell::Field& f,int x,int y,int route){auto s=f.shape(f.at(x,y));return s&&s->route==route;}
static Contours boundary(const cell::Field& f,int route){
 using Edge=std::pair<Point,Point>;
 Contours result;std::vector<Edge> edges;std::map<Point,std::vector<int>> outgoing;std::set<Edge> original;
 auto add=[&](Point a,Point b){if(!original.emplace(a,b).second)result.valid=false;outgoing[a].push_back(int(edges.size()));edges.push_back({a,b});result.edges++;};
 for(const auto& chunk:f.blocks)for(int y=0;y<32;y++)for(int x=0;x<32;x++){
  int gx=chunk.first.first*32+x,gy=chunk.first.second*32+y;if(!covered(f,gx,gy,route))continue;
  if(!covered(f,gx,gy-1,route))add({gx,gy},{gx+1,gy});
  if(!covered(f,gx+1,gy,route))add({gx+1,gy},{gx+1,gy+1});
  if(!covered(f,gx,gy+1,route))add({gx+1,gy+1},{gx,gy+1});
  if(!covered(f,gx-1,gy,route))add({gx,gy+1},{gx,gy});
 }
 if(!result.valid)return result;
 // Keep occupied cells on the left: at a diagonal pinch prefer the left turn.
 // Successors are chosen before traversal, then checked to be a bijection.
 std::vector<int> next(edges.size(),-1),incoming(edges.size(),0);
 for(size_t i=0;i<edges.size();i++){
  const auto& e=edges[i];auto found=outgoing.find(e.second);if(found==outgoing.end()){result.valid=false;return result;}
  int dx=e.second.first-e.first.first,dy=e.second.second-e.first.second,best=-3,chosen=-1;
  for(int j:found->second){auto q=edges[j].second;int ex=q.first-e.second.first,ey=q.second-e.second.second;int cross=dx*ey-dy*ex,dot=dx*ex+dy*ey;int score=cross>0?2:dot>0?1:cross<0?0:-1;if(score>best){best=score;chosen=j;}}
  if(chosen<0){result.valid=false;return result;}next[i]=chosen;incoming[chosen]++;
 }
 for(int n:incoming)if(n!=1){result.valid=false;return result;}
 std::vector<bool> visited(edges.size(),false);std::set<Edge> reconstructed;
 for(size_t first=0;first<edges.size();first++)if(!visited[first]){
  size_t current=first;std::vector<Point> loop;std::set<Point> vertices;
  do{if(visited[current]||!vertices.insert(edges[current].first).second){result.valid=false;return result;}visited[current]=true;loop.push_back(edges[current].first);current=size_t(next[current]);}while(current!=first);
  std::vector<Point> corners;
  for(size_t i=0;i<loop.size();i++){
   Point a=loop[(i+loop.size()-1)%loop.size()],b=loop[i],c=loop[(i+1)%loop.size()];
   if((b.first-a.first)*(c.second-b.second)!=(b.second-a.second)*(c.first-b.first))corners.push_back(b);
  }
  if(corners.size()<3){result.valid=false;return result;}
  for(size_t i=0;i<corners.size();i++){
   Point a=corners[i],b=corners[(i+1)%corners.size()];int dx=(b.first>a.first)-(b.first<a.first),dy=(b.second>a.second)-(b.second<a.second);if((dx!=0)==(dy!=0)){result.valid=false;return result;}
   while(a!=b){Point c={a.first+dx,a.second+dy};if(!reconstructed.emplace(a,c).second){result.valid=false;return result;}a=c;}
  }
  result.loops.push_back(corners);
 }
 if(reconstructed!=original)result.valid=false;
 return result;
}
