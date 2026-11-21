// Original bounded 2D propagation model. World units are pixels. Signed: Atlas.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>
namespace bsl::acoustic {
struct Point {double x=0,y=0;};
inline double distance(Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y);}
struct Cell {int x=0,y=0;bool operator==(Cell b)const{return x==b.x&&y==b.y;}bool operator!=(Cell b)const{return !(*this==b);}bool operator<(Cell b)const{return x<b.x||(x==b.x&&y<b.y);}};
struct Hash {size_t operator()(Cell c)const{return size_t(uint64_t(uint32_t(c.x))*0x9e3779b1u)^size_t(uint32_t(c.y));}};
struct Bands {double low=1,mid=1,high=1;};
inline Bands multiply(Bands a,Bands b){return {a.low*b.low,a.mid*b.mid,a.high*b.high};}
struct Material {Bands entry{.8,.55,.25},per_cell{.94,.82,.58};};
struct Settings {
 int cell_size=16,chunk_size=32;unsigned expansions=4096,max_sources=24,max_nodes=65536;
 double schedule=.1,max_range=1600,bend_low=.97,bend_mid=.88,bend_high=.68;
 double distance_scale=400,minimum_cutoff=350,maximum_cutoff=20000,vertical_loss=.8,smoothing=.045;
 std::vector<Material> materials{{},{{.85,.65,.35},{.96,.88,.72}},{{.65,.38,.12},{.88,.65,.35}},{{.92,.8,.6},{.98,.94,.84}}};
};
// Sparse chunks contain occupancy only; decorative/cloud terrain is never inserted by default.
class Grid {
 using Chunk=std::array<uint8_t,1024>;
 std::map<Cell,Chunk> chunks_;uint64_t revision_=1;
 static int floor_div(int n,int d){return n>=0?n/d:int(-((-int64_t(n)+d-1)/d));}
public:
 static constexpr int size=16;
 static bool valid(Point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=10000000&&std::abs(p.y)<=10000000;}
 static Cell cell(Point p){if(!valid(p))return {1000001,1000001};return {int(std::floor(p.x/size)),int(std::floor(p.y/size))};}
 static Point center(Cell p){return {(p.x+.5)*size,(p.y+.5)*size};}
 uint64_t revision()const{return revision_;}size_t chunks()const{return chunks_.size();}
 uint8_t at(Cell c)const{int x=floor_div(c.x,32),y=floor_div(c.y,32);auto p=chunks_.find({x,y});return p==chunks_.end()?0:p->second[size_t((c.y-y*32)*32+c.x-x*32)];}
 bool set(Cell c,uint8_t material){if(at(c)==material)return false;int x=floor_div(c.x,32),y=floor_div(c.y,32);auto& ch=chunks_[{x,y}];ch[size_t((c.y-y*32)*32+c.x-x*32)]=material;++revision_;return true;}
 void clear(){chunks_.clear();++revision_;}
 bool segment_open(Point a,Point b)const{
  int steps=std::max(1,int(std::ceil(distance(a,b)/4)));for(int i=0;i<=steps;++i){double t=double(i)/steps;if(at(cell({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t})))return false;}return true;
 }
};
struct Source {uint64_t id=0,owner=0,tick=0;Point origin;uint32_t category=1;Bands bands;double range=1200,lifetime=2;bool noise=false;};
struct Listener {uint64_t id=0;Point origin;Bands thresholds{.035,.035,.035};uint32_t categories=~0u;};
struct Result {bool resolved=false;uint64_t source=0,revision=0;Bands received{0,0,0};Point arrival;double length=0;unsigned crossings=0,bends=0;std::vector<Point> path;};
struct Playback {double gain=0,balance=0,cutoff=20000;};
struct Work {unsigned expansions=0,deferred=0,saturated=0;uint64_t total_expansions=0,cache_hits=0,invalidations=0,node_limit=0;};
class World {
 struct Node {double cost=std::numeric_limits<double>::infinity();Cell parent;bool settled=false;};
 struct Pending {double cost;Cell cell;bool operator<(const Pending& b)const{return cost>b.cost;}};
 struct Field {Source source;double age=0;uint64_t revision=0;Cell origin;std::unordered_map<Cell,Node,Hash> nodes;std::priority_queue<Pending> queue;bool limited=false;};
 Settings settings_;Grid grid_;std::map<uint64_t,Field> fields_;std::map<uint64_t,std::set<uint64_t>> heard_;uint64_t next_=1,tick_=0;size_t cursor_=0;Work work_;
 void rebuild(Field& f){f.nodes.clear();f.queue={};f.revision=grid_.revision();f.origin=Grid::cell(f.source.origin);f.nodes[f.origin]={0,f.origin,false};f.queue.push({0,f.origin});f.limited=false;++work_.invalidations;}
 void expand(Field& f){
  auto q=f.queue.top();f.queue.pop();auto it=f.nodes.find(q.cell);if(it==f.nodes.end()||it->second.settled||q.cost>it->second.cost+1e-9)return;
  it->second.settled=true;
  static constexpr int delta[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
  for(auto& d:delta){Cell to{q.cell.x+d[0],q.cell.y+d[1]};Point point=Grid::center(to);if(distance(point,f.source.origin)>f.source.range)continue;
   auto a=grid_.at(q.cell),b=grid_.at(to);
   // No diagonal shortcut through a pair of solid corners. Transmission still uses cardinal steps.
   if(d[0]&&d[1]&&(a||b||grid_.at({q.cell.x+d[0],q.cell.y})||grid_.at({q.cell.x,q.cell.y+d[1]})))continue;
   double cost=q.cost+(d[0]&&d[1]?1.41421356237:1);
   if(b){const auto& m=settings_.materials.at(std::min<size_t>(b,settings_.materials.size()-1));cost+=-std::log(std::max(.0001,m.per_cell.mid))*20;if(!a)cost+=-std::log(std::max(.0001,m.entry.mid))*20;}
   auto n=f.nodes.find(to);if(n==f.nodes.end()){
    if(f.nodes.size()>=settings_.max_nodes){if(!f.limited){f.limited=true;++work_.node_limit;}continue;}
    n=f.nodes.emplace(to,Node{}).first;
   }
   if(cost+1e-9<n->second.cost){n->second.cost=cost;n->second.parent=q.cell;f.queue.push({cost,to});}
  }
 }
public:
 explicit World(Settings s={}):settings_(std::move(s)){}
 Grid& grid(){return grid_;}const Grid& grid()const{return grid_;}const Settings& settings()const{return settings_;}const Work& work()const{return work_;}
 size_t sources()const{return fields_.size();}
 void clear(){fields_.clear();heard_.clear();grid_.clear();work_={};cursor_=0;}
 uint64_t emit(Source s){if(fields_.size()>=settings_.max_sources||!Grid::valid(s.origin)||!std::isfinite(s.range)||s.range<=0||s.range>settings_.max_range||!std::isfinite(s.lifetime)||s.lifetime<=0||s.lifetime>86400||!std::isfinite(s.bands.low)||!std::isfinite(s.bands.mid)||!std::isfinite(s.bands.high)||s.bands.low<0||s.bands.mid<0||s.bands.high<0||s.bands.low>1||s.bands.mid>1||s.bands.high>1)return 0;s.id=next_++;if(!s.tick)s.tick=tick_;Field f;f.source=s;rebuild(f);fields_.emplace(s.id,std::move(f));return s.id;}
 bool move(uint64_t id,Point p){auto f=fields_.find(id);if(f==fields_.end()||!Grid::valid(p))return false;f->second.source.origin=p;if(Grid::cell(p)!=f->second.origin)rebuild(f->second);return true;}
 void remove(uint64_t id){fields_.erase(id);for(auto& h:heard_)h.second.erase(id);}
 void cancel_owner(uint64_t owner){for(auto i=fields_.begin();i!=fields_.end();)if(i->second.source.owner==owner){auto id=i->first;++i;remove(id);}else ++i;heard_.erase(owner);}
 void forget_listener(uint64_t id){heard_.erase(id);}
 void tick(double dt){
  work_.expansions=work_.deferred=0;if(!std::isfinite(dt)||dt<=0)return;++tick_;
  for(auto i=fields_.begin();i!=fields_.end();){i->second.age+=dt;if(i->second.age>=i->second.source.lifetime){auto id=i->first;++i;remove(id);}else{if(i->second.revision!=grid_.revision())rebuild(i->second);++i;}}
  std::vector<Field*> pending;for(auto& f:fields_)if(!f.second.queue.empty())pending.push_back(&f.second);
  while(work_.expansions<settings_.expansions&&!pending.empty()){
   cursor_%=pending.size();auto* f=pending[cursor_];expand(*f);++work_.expansions;++work_.total_expansions;
   if(f->queue.empty())pending.erase(pending.begin()+cursor_);else ++cursor_;
  }
  work_.deferred=unsigned(pending.size());if(work_.deferred)++work_.saturated;
 }
 Result sample(uint64_t id,Point listener){
  Result r;r.source=id;r.revision=grid_.revision();auto fi=fields_.find(id);if(fi==fields_.end()||!Grid::valid(listener))return r;
  auto& f=fi->second;if(f.revision!=grid_.revision())return r;Cell c=Grid::cell(listener);if(grid_.at(c))return r;auto node=f.nodes.find(c);if(node==f.nodes.end()||!node->second.settled)return r;
  std::vector<Cell> cells;for(size_t i=0;i<f.nodes.size();++i){cells.push_back(c);if(c==f.origin)break;auto n=f.nodes.find(c);if(n==f.nodes.end())return r;c=n->second.parent;}
  if(cells.empty()||cells.back()!=f.origin)return r;
  std::reverse(cells.begin(),cells.end());
  std::vector<Point> raw;raw.reserve(cells.size()+2);raw.push_back(f.source.origin);Bands loss;bool solid=false;Point exit=f.source.origin;bool crossed=false;
  for(auto cell:cells){auto material=grid_.at(cell);auto p=Grid::center(cell);raw.push_back(p);
   if(material){const auto& m=settings_.materials.at(std::min<size_t>(material,settings_.materials.size()-1));if(!solid){loss=multiply(loss,m.entry);++r.crossings;}loss=multiply(loss,m.per_cell);crossed=true;}
   else if(solid)exit=p;
   solid=material!=0;
  }
  raw.push_back(listener);
  // String-pull only wholly open stretches. Keep transmission segments, including listener-facing exits.
  r.path.push_back(raw.front());for(size_t i=0;i+1<raw.size();){size_t end=i+1;if(!grid_.at(Grid::cell(raw[i])))for(size_t j=i+2;j<raw.size();++j){if(!grid_.segment_open(raw[i],raw[j]))break;end=j;}r.path.push_back(raw[end]);i=end;}
  // Collapse collinear transmission points without merging a solid entry and exit.
  std::vector<Point> path;for(auto p:r.path){while(path.size()>=2){auto a=path[path.size()-2],b=path.back();double cross=(b.x-a.x)*(p.y-b.y)-(b.y-a.y)*(p.x-b.x);if(std::abs(cross)>1e-5)break;path.pop_back();}path.push_back(p);}r.path=std::move(path);
  for(size_t i=1;i<r.path.size();++i)r.length+=distance(r.path[i-1],r.path[i]);
  for(size_t i=1;i+1<r.path.size();++i){auto a=r.path[i-1],b=r.path[i],c1=r.path[i+1];double ab=distance(a,b),bc=distance(b,c1);if(ab>1&&bc>1&&((b.x-a.x)*(c1.x-b.x)+(b.y-a.y)*(c1.y-b.y))/(ab*bc)<.985&& !grid_.at(Grid::cell(b)))++r.bends;}
  for(unsigned i=0;i<r.bends;++i)loss=multiply(loss,{settings_.bend_low,settings_.bend_mid,settings_.bend_high});
  double falloff=1/(1+std::pow(r.length/settings_.distance_scale,2));if(r.length>f.source.range)falloff=0;
  r.received=multiply(f.source.bands,{loss.low*falloff,loss.mid*falloff,loss.high*falloff});r.arrival=f.source.origin;
  if(crossed)r.arrival=exit;
  // Final visible bend after the last solid exit wins over the transmitted exit.
  for(size_t i=1;i+1<r.path.size();++i)if(distance(r.path[i],listener)>1&&grid_.segment_open(r.path[i],listener)) {r.arrival=r.path[i];break;}
  if(!crossed&&r.path.size()<=2)r.arrival=f.source.origin;
  r.resolved=true;++work_.cache_hits;return r;
 }
 std::vector<Result> hear(const Listener& l){std::vector<Result> out;for(auto& f:fields_){const auto& s=f.second.source;if(!s.noise||s.owner==l.id||!(s.category&l.categories)||heard_[l.id].count(s.id))continue;auto r=sample(s.id,l.origin);if(r.resolved&&(r.received.low>=l.thresholds.low||r.received.mid>=l.thresholds.mid||r.received.high>=l.thresholds.high)){heard_[l.id].insert(s.id);out.push_back(std::move(r));}}return out;}
 Playback playback(const Result& r,Point camera,Point half)const{
  Playback p;if(!r.resolved)return p;p.gain=std::clamp((r.received.low+r.received.mid+r.received.high)/3,0.0,1.0);p.balance=std::clamp((r.arrival.x-camera.x)/std::max(1.0,half.x),-1.0,1.0);
  double vertical=std::min(1.0,std::abs(r.arrival.y-camera.y)/std::max(1.0,half.y));double ratio=r.received.high/std::max(1e-9,r.received.low);p.cutoff=std::clamp(settings_.maximum_cutoff*ratio*(1-settings_.vertical_loss*vertical),settings_.minimum_cutoff,settings_.maximum_cutoff);return p;
 }
};
} // namespace bsl::acoustic
