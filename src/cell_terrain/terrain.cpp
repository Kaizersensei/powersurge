// Original bounded sparse-field implementation. Signed: President.
#include "terrain.hpp"
#include "materials.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <tuple>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cctype>
namespace cell {
namespace {
void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
bool safe(const std::string& s){return !s.empty()&&s.size()<=128&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isalnum(c)||c==':'||c=='_'||c=='-'||c=='.';});}
// A square brush is a continuous swept box, not a chain of sampled stamps.
bool square_sweep(double x,double y,double a,double b,double c,double d,double radius){
 double low=0,high=1;
 auto slab=[&](double point,double start,double delta){if(std::abs(delta)<1e-12)return std::abs(point-start)<=radius;double u=(point-radius-start)/delta,v=(point+radius-start)/delta;if(u>v)std::swap(u,v);low=std::max(low,u);high=std::min(high,v);return low<=high;};
 return slab(x,a,c-a)&&slab(y,b,d-b);
}
double distance(double x,double y,double a,double b,double c,double d){double dx=c-a,dy=d-b,t=(dx*dx+dy*dy)>0?std::clamp(((x-a)*dx+(y-b)*dy)/(dx*dx+dy*dy),0.,1.):0.;return std::hypot(x-a-t*dx,y-b-t*dy);}
struct Json {std::map<std::string,Json> object;std::vector<Json> array;std::string string;double number=0;int type=0;};
class Reader {
 const std::string& s;size_t p=0;
 void ws(){while(p<s.size()&&std::isspace(static_cast<unsigned char>(s[p])))p++;}
 char peek(){ws();return p<s.size()?s[p]:0;}
 void eat(char c){require(peek()==c,"Malformed terrain JSON");p++;}
 unsigned hex4(){
  unsigned n=0;for(int i=0;i<4;i++){require(p<s.size(),"Incomplete Unicode escape");unsigned char c=s[p++];n*=16;
   if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else require(false,"Invalid Unicode escape");}
  return n;
 }
 std::string str(){
  eat('"');std::string r;
  while(p<s.size()&&s[p]!='"'){
   unsigned char c=s[p++];require(c>=32,"Unescaped JSON control character");
   if(c!='\\')r+=static_cast<char>(c);
   else {
    require(p<s.size(),"Incomplete JSON escape");char e=s[p++];
    if(e=='"'||e=='\\'||e=='/')r+=e;
    else if(e=='b')r+='\b';else if(e=='f')r+='\f';else if(e=='n')r+='\n';else if(e=='r')r+='\r';else if(e=='t')r+='\t';
    else if(e=='u'){
     unsigned n=hex4();
     if(n>=0xd800&&n<=0xdbff){require(p+2<=s.size()&&s[p]=='\\'&&s[p+1]=='u',"Missing Unicode low surrogate");p+=2;unsigned low=hex4();require(low>=0xdc00&&low<=0xdfff,"Invalid Unicode low surrogate");n=0x10000+((n-0xd800)<<10)+(low-0xdc00);}
     else require(n<0xdc00||n>0xdfff,"Unpaired Unicode surrogate");
     if(n<0x80)r+=static_cast<char>(n);
     else if(n<0x800){r+=static_cast<char>(0xc0|(n>>6));r+=static_cast<char>(0x80|(n&63));}
     else if(n<0x10000){r+=static_cast<char>(0xe0|(n>>12));r+=static_cast<char>(0x80|((n>>6)&63));r+=static_cast<char>(0x80|(n&63));}
     else{r+=static_cast<char>(0xf0|(n>>18));r+=static_cast<char>(0x80|((n>>12)&63));r+=static_cast<char>(0x80|((n>>6)&63));r+=static_cast<char>(0x80|(n&63));}
    }else require(false,"Unsupported JSON escape");
   }
   require(r.size()<=4096,"JSON string limit");
  }
  eat('"');return r;
 }
 Json value(int depth){
  require(depth<16,"Terrain JSON nesting limit");Json v;char c=peek();
  if(c=='{'){v.type=1;eat('{');if(peek()!='}')do{std::string key=str();eat(':');require(v.object.emplace(key,value(depth+1)).second,"Duplicate JSON field");if(peek()!=',')break;eat(',');}while(true);eat('}');}
  else if(c=='['){v.type=2;eat('[');if(peek()!=']')do{require(v.array.size()<10000,"Terrain array limit");v.array.push_back(value(depth+1));if(peek()!=',')break;eat(',');}while(true);eat(']');}
  else if(c=='"'){v.type=3;v.string=str();}
  else if(s.compare(p,4,"true")==0){v.type=4;v.number=1;p+=4;}
  else if(s.compare(p,5,"false")==0){v.type=4;p+=5;}
  else if(s.compare(p,4,"null")==0){v.type=6;p+=4;}
  else {require(c=='-'||(c>='0'&&c<='9'),"Invalid JSON token");v.type=5;char* end=nullptr;v.number=std::strtod(s.c_str()+p,&end);require(end!=s.c_str()+p&&std::isfinite(v.number),"Invalid terrain number");p=end-s.c_str();}
  return v;
 }
public:explicit Reader(const std::string& text):s(text){require(s.size()<=4*1024*1024,"Terrain file limit");}
 Json read(){Json v=value(0);require(!peek(),"Trailing terrain JSON");return v;}
};
const Json& get(const Json& v,const char* key,int type){auto i=v.object.find(key);require(v.type==1&&i!=v.object.end()&&i->second.type==type,"Missing or mistyped terrain field");return i->second;}
double num(const Json& v,const char* k){return get(v,k,5).number;}
int integer(const Json& v,const char* k){double n=num(v,k);require(n>=-10000000&&n<=10000000&&std::floor(n)==n,"Invalid terrain integer");return static_cast<int>(n);}
}
int Field::floor_div(int x,int by){int q=x/by;return q-(x%by<0);}
int Field::at(int x,int y)const {int cx=floor_div(x,side),cy=floor_div(y,side);auto i=blocks.find({cx,cy});return i==blocks.end()?0:i->second.cells[(y-cy*side)*side+x-cx*side];}
const Shape* Field::shape(int id)const {for(const auto& s:doc.shapes)if(s.id==id)return &s;return nullptr;}
const Shape* Field::sample(double x,double y)const {if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>limit||std::abs(y)>limit)return nullptr;return shape(at(static_cast<int>(std::floor(x/step)),static_cast<int>(std::floor(y/step))));}
void Field::put(int x,int y,int id){int cx=floor_div(x,side),cy=floor_div(y,side);auto i=blocks.find({cx,cy});if(i==blocks.end()){if(!id)return;require(blocks.size()<max_chunks,"Terrain exceeds the authored world bounds");i=blocks.emplace(Key{cx,cy},Block{}).first;}i->second.cells[(y-cy*side)*side+x-cx*side]=id;}
void validate_bevel(const Bevel& b){
 require(b.profile=="linear"||b.profile=="round"||b.profile=="concave","Invalid bevel profile");require(b.contact=="separate"||b.contact=="continuous"||b.contact=="soft_seam","Invalid bevel contact mode");require(std::isfinite(b.width_m)&&std::isfinite(b.depth_m)&&std::isfinite(b.smoothness)&&b.width_m>=0&&b.width_m<=32&&b.depth_m>=0&&b.depth_m<=32&&b.smoothness>=0&&b.smoothness<=1,"Invalid bevel dimensions");
}
void validate(const Document& d){
 require(!d.shapes.empty()&&d.shapes.size()<=128&&d.strokes.size()<=4096,"Terrain document budget exceeded");
 std::map<int,bool> ids;
 for(const auto& s:d.shapes){if(s.bevel)validate_bevel(*s.bevel);require(s.id>0&&s.id<65536&&ids.emplace(s.id,true).second,"Invalid/duplicate shape ID");require(safe(s.material)&&(s.preset=="square"||s.preset=="round"),"Invalid material/preset");require(s.blend_cells>=0&&s.blend_cells<=4&&s.route>=0&&s.route<=2,"Invalid shape properties");}
 for(const auto& s:d.strokes){require(ids.count(s.shape),"Unknown stroke shape");require(std::isfinite(s.x)&&std::isfinite(s.y)&&std::isfinite(s.u)&&std::isfinite(s.v)&&std::isfinite(s.curve),"Nonfinite stroke");
 require(std::abs(s.x)<=limit&&std::abs(s.y)<=limit&&std::abs(s.u)<=limit&&std::abs(s.v)<=limit,"Stroke exceeds building limits");require(s.size>=8&&s.size<=128&&s.size%8==0&&s.mode>=0&&s.mode<=2&&s.curve>=0&&s.curve<=1,"Invalid brush/curve");}
}
std::set<Key> Field::paint(const Stroke& s){
 std::set<Key> changed;
 const auto* sh=shape(s.shape);require(sh,"Unknown stroke shape");
 double dx=s.u-s.x,dy=s.v-s.y,length=std::hypot(dx,dy),offset=(s.mode==1?1:s.mode==2?-1:0)*s.curve*length*.5;
 double nx=length>0?-dy/length:0,ny=length>0?dx/length:0;
 double cx=(s.x+s.u)*.5+nx*offset,cy=(s.y+s.v)*.5+ny*offset;
 bool square=sh->preset=="square";
 int segments=s.mode==0?1:32;
 std::vector<std::pair<double,double>> path;path.reserve(segments+1);
 for(int j=0;j<=segments;j++){double t=j/double(segments),u=1-t;path.push_back({u*u*s.x+2*u*t*cx+t*t*s.u,u*u*s.y+2*u*t*cy+t*t*s.v});}
 int left=std::max(-limit,static_cast<int>(std::floor(std::min({s.x,s.u,cx})-s.size*.5)));
 int top=std::max(-limit,static_cast<int>(std::floor(std::min({s.y,s.v,cy})-s.size*.5)));
 int right=std::min(limit,static_cast<int>(std::ceil(std::max({s.x,s.u,cx})+s.size*.5)));
 int bottom=std::min(limit,static_cast<int>(std::ceil(std::max({s.y,s.v,cy})+s.size*.5)));
 for(int y=floor_div(top,step);y<static_cast<int>(std::ceil(bottom/double(step)));y++)for(int x=floor_div(left,step);x<static_cast<int>(std::ceil(right/double(step)));x++){
  double px=x*step+step*.5,py=y*step+step*.5;bool hit=false;
  for(size_t j=1;j<path.size()&&!hit;j++)hit=square?square_sweep(px,py,path[j-1].first,path[j-1].second,path[j].first,path[j].second,s.size*.5):distance(px,py,path[j-1].first,path[j-1].second,path[j].first,path[j].second)<=s.size*.5;
  if(hit&&(!s.erase||at(x,y)==s.shape)){
   int value=s.erase?0:s.shape;if(at(x,y)!=value){put(x,y,value);changed.insert({floor_div(x,side),floor_div(y,side)});}
  }
 }
 if(s.erase)for(const auto& k:changed){auto i=blocks.find(k);if(i!=blocks.end()&&std::none_of(i->second.cells.begin(),i->second.cells.end(),[](int n){return n!=0;}))blocks.erase(i);}
 return changed;
}
void Field::rebuild(){validate(doc);Field next;next.doc=doc;for(const auto& s:doc.strokes)next.paint(s);blocks.swap(next.blocks);}
std::vector<Edge> Field::edges()const {
 std::vector<Edge> r;for(const auto& c:blocks)for(int y=0;y<side;y++)for(int x=0;x<side;x++){
  int wx=c.first.first*side+x,wy=c.first.second*side+y,id=c.second.cells[y*side+x];if(!id)continue;
  if(at(wx,wy-1)!=id)r.push_back({wx*step,wy*step,(wx+1)*step,wy*step,id});
  if(at(wx+1,wy)!=id)r.push_back({(wx+1)*step,wy*step,(wx+1)*step,(wy+1)*step,id});
  if(at(wx,wy+1)!=id)r.push_back({(wx+1)*step,(wy+1)*step,wx*step,(wy+1)*step,id});
  if(at(wx-1,wy)!=id)r.push_back({wx*step,(wy+1)*step,wx*step,wy*step,id});
 }return r;
}
// Merge collinear contour runs; retain shape boundaries. Used by the editor preview.
std::vector<Edge> Field::outline()const {
 using Group=std::tuple<bool,int,int>;
 std::map<Group,std::vector<std::pair<int,int>>> groups;
 for(const auto& e:edges()){
  bool vertical=e.x==e.u;int fixed=vertical?e.x:e.y,a=vertical?e.y:e.x,b=vertical?e.v:e.u;
  groups[{vertical,fixed,e.shape}].push_back({std::min(a,b),std::max(a,b)});
 }
 std::vector<Edge> result;
 for(auto& group:groups){
  auto& spans=group.second;std::sort(spans.begin(),spans.end());
  bool vertical=std::get<0>(group.first);int fixed=std::get<1>(group.first),shape=std::get<2>(group.first),a=spans.front().first,b=spans.front().second;
  auto append=[&](){result.push_back(vertical?Edge{fixed,a,fixed,b,shape}:Edge{a,fixed,b,fixed,shape});};
  for(size_t i=1;i<spans.size();i++)if(spans[i].first<=b)b=std::max(b,spans[i].second);else{append();a=spans[i].first;b=spans[i].second;}
  append();
 }
 return result;
}
std::map<Key,std::array<Mix,side*side>> Field::blends()const {return blend_region(nullptr);}
std::map<Key,std::array<Mix,side*side>> Field::blends_for(const std::set<Key>& targets)const {return blend_region(&targets);}
std::map<Key,std::array<Mix,side*side>> Field::blend_region(const std::set<Key>* targets)const {
 std::set<Key> domain;
 if(targets){int width=0;for(const auto& s:doc.shapes)if(s.blend)width=std::max(width,s.blend_cells*base);
  int halo=(width+chunk-1)/chunk;
  for(auto k:*targets)for(int y=-halo;y<=halo;y++)for(int x=-halo;x<=halo;x++){Key n{k.first+x,k.second+y};if(blocks.count(n))domain.insert(n);}
 }else for(const auto& c:blocks)domain.insert(c.first);
 std::map<Key,std::array<Mix,side*side>> out;std::map<Key,std::array<int,side*side>> dist;
 struct Q {int x,y;};std::deque<Q> q;
 const int ax[]={-1,1,0,0},ay[]={0,0,-1,1};
 for(const auto& k:domain){out[k]={};dist[k].fill(100000);}
 auto index=[](int x,int y){return (y-Field::floor_div(y,side)*side)*side+x-Field::floor_div(x,side)*side;};
 for(const auto& k:domain)for(int y=0;y<side;y++)for(int x=0;x<side;x++){
  int wx=k.first*side+x,wy=k.second*side+y,id=at(wx,wy);const auto* s=shape(id);if(!s||!s->blend||!s->blend_cells)continue;
  for(int d=0;d<4;d++){int other=at(wx+ax[d],wy+ay[d]);const auto* n=shape(other);if(n&&n->material!=s->material&&n->route==s->route){int i=index(wx,wy);out[k][i].other=other;dist[k][i]=0;q.push_back({wx,wy});break;}}
 }
 while(!q.empty()){Q p=q.front();q.pop_front();Key k{floor_div(p.x,side),floor_div(p.y,side)};int i=index(p.x,p.y),id=at(p.x,p.y),d=dist[k][i];const auto* s=shape(id);Mix& m=out[k][i];const auto* other=shape(m.other);
  double width=s->blend_cells*base;double maximum=other&&other->blend&&other->blend_cells?.5:1.;
  m.weight=maximum*std::max(0.,1.-d*step/width);
  if((d+1)*step>=width)continue;
  for(int n=0;n<4;n++){int x=p.x+ax[n],y=p.y+ay[n];if(at(x,y)!=id)continue;Key nk{floor_div(x,side),floor_div(y,side)};if(!domain.count(nk))continue;int ni=index(x,y);if(dist[nk][ni]<=d+1)continue;dist[nk][ni]=d+1;out[nk][ni].other=m.other;q.push_back({x,y});}
 }return out;
}
std::string encode(const Document& d){
 validate(d);std::ostringstream o;o.precision(12);o<<"{\n \"version\":1,\n \"base_cell\":32,\n \"sample_step\":2,\n \"shapes\":[";
 for(size_t i=0;i<d.shapes.size();i++){const auto& s=d.shapes[i];if(i)o<<",";o<<"\n  {\"id\":"<<s.id<<",\"material\":\""<<s.material<<"\",\"preset\":\""<<s.preset<<"\",\"blend\":"<<(s.blend?"true":"false")<<",\"blend_cells\":"<<s.blend_cells<<",\"route\":"<<s.route;if(s.bevel){const auto& b=*s.bevel;o<<",\"bevel\":{\"enabled\":"<<(b.enabled?"true":"false")<<",\"profile\":\""<<b.profile<<"\",\"contact\":\""<<b.contact<<"\",\"width_m\":"<<b.width_m<<",\"depth_m\":"<<b.depth_m<<",\"smoothness\":"<<b.smoothness<<"}";}o<<"}";}
 o<<"\n ],\n \"strokes\":[";for(size_t i=0;i<d.strokes.size();i++){const auto& s=d.strokes[i];if(i)o<<",";o<<"\n  {\"shape\":"<<s.shape<<",\"erase\":"<<(s.erase?"true":"false")<<",\"x\":"<<s.x<<",\"y\":"<<s.y<<",\"u\":"<<s.u<<",\"v\":"<<s.v<<",\"size\":"<<s.size<<",\"mode\":"<<s.mode<<",\"curve\":"<<s.curve<<"}";}
 return o.str()+"\n ]\n}\n";
}
Document decode(const std::string& text){
 Json j=Reader(text).read();require(integer(j,"version")==1&&integer(j,"base_cell")==base&&integer(j,"sample_step")==step,"Unsupported terrain version/resolution");require(j.object.size()==5,"Unknown terrain root fields");Document d;
 for(const auto& n:get(j,"shapes",2).array){require(n.object.size()==6||(n.object.size()==7&&n.object.count("bevel")),"Unknown shape fields");Shape s;if(n.object.count("bevel")){const auto& b=get(n,"bevel",1);require(b.object.size()==6,"Unknown bevel fields");Bevel v;v.enabled=get(b,"enabled",4).number!=0;v.profile=get(b,"profile",3).string;v.contact=get(b,"contact",3).string;v.width_m=num(b,"width_m");v.depth_m=num(b,"depth_m");v.smoothness=num(b,"smoothness");s.bevel=v;}s.id=integer(n,"id");s.material=get(n,"material",3).string;s.preset=get(n,"preset",3).string;s.blend=get(n,"blend",4).number!=0;s.blend_cells=integer(n,"blend_cells");s.route=integer(n,"route");d.shapes.push_back(s);}
 for(const auto& n:get(j,"strokes",2).array){require(n.object.size()==9,"Unknown stroke fields");Stroke s;s.shape=integer(n,"shape");s.erase=get(n,"erase",4).number!=0;s.x=num(n,"x");s.y=num(n,"y");s.u=num(n,"u");s.v=num(n,"v");s.size=integer(n,"size");s.mode=integer(n,"mode");s.curve=num(n,"curve");d.strokes.push_back(s);}
 validate(d);return d;
}
std::vector<Material> decode_materials(const std::string& text){
 Json root=Reader(text).read();require(integer(root,"version")==1&&root.object.size()==2,"Unsupported material registry");
 const auto& records=get(root,"materials",2).array;require(!records.empty()&&records.size()<=4096,"Material count must be 1..4096");
 std::set<std::string> ids;std::vector<Material> out;
 auto reference=[](const Json& value){require(value.type==6||(value.type==3&&safe(value.string)),"Invalid material hook reference");return value.type==6?std::string{}:value.string;};
 for(const auto& record:records){
  require(record.type==1&&record.object.size()==9,"Unexpected material fields");
  Material m;m.id=get(record,"id",3).string;m.name=get(record,"name",3).string;m.texture=get(record,"texture",3).string;
  require(safe(m.id)&&ids.insert(m.id).second,"Invalid/duplicate material ID");
  require(!m.name.empty()&&m.name.size()<=128&&std::none_of(m.name.begin(),m.name.end(),[](unsigned char c){return c<32;}),"Invalid material display name");
  require(m.texture.size()<=256&&m.texture.rfind("textures/",0)==0&&m.texture.size()>13,"Material texture must be a relative textures PNG path");
  require(std::all_of(m.texture.begin(),m.texture.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-'||c=='.'||c=='/';}),"Unsafe texture path");
  size_t first=0;while(first<m.texture.size()){size_t end=m.texture.find('/',first);if(end==std::string::npos)end=m.texture.size();auto part=m.texture.substr(first,end-first);require(!part.empty()&&part!="."&&part!="..","Unsafe texture path component");first=end+1;}
  std::string suffix=m.texture.substr(m.texture.size()-4);std::transform(suffix.begin(),suffix.end(),suffix.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});require(suffix==".png","Material texture must be PNG");
  const auto& tint=get(record,"tint",2).array;require(tint.size()==3,"Material tint needs three channels");
  for(int i=0;i<3;i++){require(tint[i].type==5&&tint[i].number>=0&&tint[i].number<=255&&std::floor(tint[i].number)==tint[i].number,"Invalid material tint");m.tint[i]=static_cast<int>(tint[i].number);}
  auto hook=[&](const char* key){auto entry=record.object.find(key);require(entry!=record.object.end(),"Missing material hook");return reference(entry->second);};
  m.shader=hook("shader");m.physical_profile=hook("physical_profile");m.acoustic_profile=hook("acoustic_profile");
  const auto& feedback=get(record,"feedback",1);require(feedback.object.size()==4,"Unexpected material feedback fields");
  for(auto key:{"impact","footstep","damage","destruction"}){auto entry=feedback.object.find(key);require(entry!=feedback.object.end(),"Missing material feedback field");m.feedback[key]=reference(entry->second);}
  for(const auto& reaction:get(record,"reactions",2).array){auto id=reference(reaction);require(!id.empty(),"Null material reaction");m.reactions.push_back(id);}
  out.push_back(std::move(m));
 }
 return out;
}

}
