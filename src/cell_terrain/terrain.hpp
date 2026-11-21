// Shared authored coverage and material-shape model. Signed: President.
#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <set>
namespace cell {
constexpr int base=32,step=2,side=32,chunk=side*step,limit=2048;
struct Shape {int id=0;std::string material="material:stone",preset="square";bool blend=false;int blend_cells=1,route=0;};
struct Stroke {int shape=0;bool erase=false;double x=0,y=0,u=0,v=0;int size=32,mode=0;double curve=.5;};
struct Document {std::vector<Shape> shapes;std::vector<Stroke> strokes;};
using Key=std::pair<int,int>;
struct Block {std::array<uint16_t,side*side> cells{};};
struct Mix {int other=0;double weight=0;};
struct Edge {int x,y,u,v,shape;};
class Field {
public:
 Document doc;std::map<Key,Block> blocks;
 static int floor_div(int x,int by);
 int at(int x,int y)const;const Shape* shape(int id)const;
 const Shape* sample(double x,double y)const;
 void rebuild();std::set<Key> paint(const Stroke& stroke);
 std::vector<Edge> edges()const;
 std::vector<Edge> outline()const;
 std::map<Key,std::array<Mix,side*side>> blends()const;
 std::map<Key,std::array<Mix,side*side>> blends_for(const std::set<Key>& targets)const;
private:
 std::map<Key,std::array<Mix,side*side>> blend_region(const std::set<Key>* targets)const;
 void put(int x,int y,int id);
};
std::string encode(const Document& doc);
Document decode(const std::string& text);
void validate(const Document& doc);
}
