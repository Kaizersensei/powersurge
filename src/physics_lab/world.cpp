// Experimental single-threaded body registry over Box2D. Signed: Mech.
#include "world.h"
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <vector>
namespace { constexpr size_t MAX_IDS=4096, MAX_CONTACTS=1024;constexpr float MAX_VALUE=10000.f;
bool finite(float n){return std::isfinite(n)&&std::abs(n)<=MAX_VALUE;}
struct Entry {uint64_t id=0;b2BodyId body=b2_nullBodyId;BslPhysicsMode mode=BSL_BODY_STATIC;BslPhysicsShape shape=BSL_SHAPE_CIRCLE;};
enum class JointKind { distance, revolute };
struct JointEntry {uint64_t a=0,b=0;b2JointId joint=b2_nullJointId;JointKind kind=JointKind::distance;};
}
struct BslPhysicsWorld {
 b2WorldId world=b2_nullWorldId;std::map<uint64_t,Entry> bodies;std::set<uint64_t> used;
 std::vector<BslPhysicsContact> contacts;unsigned dropped=0;
 std::map<uint64_t,JointEntry> joints;std::set<uint64_t> used_joints;
 ~BslPhysicsWorld(){if(B2_IS_NON_NULL(world))b2DestroyWorld(world);}
};
extern "C" BslPhysicsWorld* bsl_physics_world_create(float gx,float gy){
 if(!finite(gx)||!finite(gy))return nullptr;
 try{auto w=std::make_unique<BslPhysicsWorld>();w->contacts.reserve(MAX_CONTACTS);b2WorldDef d=b2DefaultWorldDef();d.gravity={gx,gy};w->world=b2CreateWorld(&d);if(B2_IS_NULL(w->world))return nullptr;return w.release();}catch(...){return nullptr;}
}
extern "C" void bsl_physics_world_destroy(BslPhysicsWorld* w){delete w;}
extern "C" bool bsl_physics_body_create(BslPhysicsWorld* w,const BslPhysicsBodyDef* d){
 if(!w||!d||!d->id||w->used.count(d->id)||w->used.size()>=MAX_IDS||d->mode<BSL_BODY_STATIC||d->mode>BSL_BODY_DYNAMIC||d->shape<BSL_SHAPE_CIRCLE||d->shape>BSL_SHAPE_STAR5||!d->category)return false;
 if(!finite(d->x)||!finite(d->y)||!finite(d->vx)||!finite(d->vy)||!finite(d->angle)||!finite(d->angular_velocity)||!finite(d->half_width)||!finite(d->half_height)||!finite(d->density)||!finite(d->friction)||!finite(d->restitution)||d->half_width<.02f||d->density<0||d->friction<0||d->restitution<0||d->restitution>1||d->half_height<0||(d->shape!=BSL_SHAPE_CIRCLE&&d->half_height<.02f)||(d->shape==BSL_SHAPE_STAR5&&(d->half_width<.1f||d->half_height<.1f)))return false;
 try{
  auto inserted=w->bodies.emplace(d->id,Entry{d->id,b2_nullBodyId,d->mode,d->shape});if(!inserted.second)return false;auto it=inserted.first;
  try{w->used.insert(d->id);}catch(...){w->bodies.erase(it);return false;}
  b2BodyDef bd=b2DefaultBodyDef();bd.type=d->mode==BSL_BODY_DYNAMIC?b2_dynamicBody:d->mode==BSL_BODY_KINEMATIC?b2_kinematicBody:b2_staticBody;bd.position={d->x,d->y};bd.linearVelocity={d->vx,d->vy};bd.rotation={std::cos(d->angle),std::sin(d->angle)};bd.angularVelocity=d->angular_velocity;bd.isBullet=d->bullet;if(d->gravity_free)bd.gravityScale=0;bd.userData=&it->second;
  auto body=b2CreateBody(w->world,&bd);if(B2_IS_NULL(body)){w->used.erase(d->id);w->bodies.erase(it);return false;}it->second.body=body;
  b2ShapeDef sd=b2DefaultShapeDef();sd.density=d->density;sd.material.friction=d->friction;sd.material.restitution=d->restitution;sd.filter.categoryBits=d->category;sd.filter.maskBits=d->mask;sd.enableContactEvents=true;
  b2ShapeId shape=b2_nullShapeId;
  if(d->shape==BSL_SHAPE_CIRCLE){b2Circle c={{0,0},d->half_width};shape=b2CreateCircleShape(body,&sd,&c);}
  else if(d->shape==BSL_SHAPE_BOX){float r=d->mode==BSL_BODY_DYNAMIC&&!d->gravity_free?std::min(.08f,std::min(d->half_width,d->half_height)*.15f):0.f;auto b=r>0?b2MakeRoundedBox(d->half_width-r,d->half_height-r,r):b2MakeBox(d->half_width,d->half_height);shape=b2CreatePolygonShape(body,&sd,&b);}
  else {
   // Ten non-overlapping triangles share one body. Inner/outer radius is provisional.
   constexpr float pi=3.14159265358979323846f;
   const float sx=d->half_width/.9510565163f,sy=d->half_height/.9045084972f,center_y=.0954915028f*sy;
   for(int i=0;i<10;i++){
    float a=-pi/2+i*pi/5,b=-pi/2+(i+1)*pi/5;
    float ra=i%2?.45f:1.f,rb=(i+1)%2?.45f:1.f;
    b2Vec2 vertices[3]={{0,center_y},{sx*ra*std::cos(a),center_y+sy*ra*std::sin(a)},{sx*rb*std::cos(b),center_y+sy*rb*std::sin(b)}};
    auto hull=b2ComputeHull(vertices,3);if(hull.count!=3){shape=b2_nullShapeId;break;}
    auto polygon=b2MakePolygon(&hull,0);shape=b2CreatePolygonShape(body,&sd,&polygon);if(B2_IS_NULL(shape))break;
   }
  }
  if(B2_IS_NULL(shape)){b2DestroyBody(body);w->used.erase(d->id);w->bodies.erase(it);return false;}return true;
 }catch(...){return false;}
}

namespace {
bool segment_intersects(const BslPhysicsPoint& a,const BslPhysicsPoint& b,const BslPhysicsPoint& c,const BslPhysicsPoint& d){
 const bool av=a.x==b.x,cv=c.x==d.x;
 auto between=[](float v,float x,float y){return v>=std::min(x,y)&&v<=std::max(x,y);};
 if(av&&cv)return a.x==c.x&&std::max(std::min(a.y,b.y),std::min(c.y,d.y))<=std::min(std::max(a.y,b.y),std::max(c.y,d.y));
 if(!av&&!cv)return a.y==c.y&&std::max(std::min(a.x,b.x),std::min(c.x,d.x))<=std::min(std::max(a.x,b.x),std::max(c.x,d.x));
 if(av)return between(a.x,c.x,d.x)&&between(c.y,a.y,b.y);
 return between(c.x,a.x,b.x)&&between(a.y,c.y,d.y);
}
bool valid_contour(const BslPhysicsContourDef* d){
 if(!d||!d->points||d->count<3||d->count>1024||!d->id||!d->category)return false;
 double area=0;
 for(unsigned i=0;i<d->count;i++){
  const auto& a=d->points[i];const auto& b=d->points[(i+1)%d->count];const auto& c=d->points[(i+2)%d->count];
  if(!finite(a.x)||!finite(a.y)||!finite(b.x)||!finite(b.y))return false;
  const float dx=b.x-a.x,dy=b.y-a.y;
  if((dx!=0&&dy!=0)||(dx==0&&dy==0)||std::abs(dx)+std::abs(dy)<.01f)return false;
  if((dx!=0&&c.y==b.y&&(c.x-b.x)*dx<0)||(dy!=0&&c.x==b.x&&(c.y-b.y)*dy<0))return false;
  area+=double(a.x)*b.y-double(b.x)*a.y;
 }
 if(area==0)return false;
 for(unsigned i=0;i<d->count;i++)for(unsigned j=i+1;j<d->count;j++){
  if(j==i+1||(i==0&&j==d->count-1))continue;
  if(segment_intersects(d->points[i],d->points[(i+1)%d->count],d->points[j],d->points[(j+1)%d->count]))return false;
 }
 return true;
}
}
extern "C" bool bsl_physics_contour_create(BslPhysicsWorld* w,const BslPhysicsContourDef* d){
 if(!w||!valid_contour(d)||w->used.count(d->id)||w->used.size()>=MAX_IDS)return false;
 try{
  std::vector<b2Vec2> points;points.reserve(d->count);
  for(unsigned i=0;i<d->count;i++)points.push_back({d->points[i].x,d->points[i].y});
  auto inserted=w->bodies.emplace(d->id,Entry{d->id,b2_nullBodyId,BSL_BODY_STATIC});if(!inserted.second)return false;auto it=inserted.first;
  try{w->used.insert(d->id);}catch(...){w->bodies.erase(it);return false;}
  b2BodyDef bd=b2DefaultBodyDef();bd.userData=&it->second;
  auto body=b2CreateBody(w->world,&bd);if(B2_IS_NULL(body)){w->used.erase(d->id);w->bodies.erase(it);return false;}it->second.body=body;
  b2ChainDef cd=b2DefaultChainDef();cd.points=points.data();cd.pointCount=int(points.size());cd.isLoop=true;cd.filter.categoryBits=d->category;cd.filter.maskBits=d->mask;
  auto chain=b2CreateChain(body,&cd);
  if(B2_IS_NULL(chain)){b2DestroyBody(body);w->used.erase(d->id);w->bodies.erase(it);return false;}
  return true;
 }catch(...){return false;}
}
extern "C" bool bsl_physics_body_destroy(BslPhysicsWorld* w,uint64_t id){
 if(!w)return false;
 auto it=w->bodies.find(id);if(it==w->bodies.end())return false;
 for(auto joint=w->joints.begin();joint!=w->joints.end();){if(joint->second.a==id||joint->second.b==id){b2DestroyJoint(joint->second.joint);joint=w->joints.erase(joint);}else ++joint;}
 b2DestroyBody(it->second.body);w->bodies.erase(it);return true;
}
extern "C" bool bsl_physics_body_enabled(BslPhysicsWorld* w,uint64_t id,bool enabled){
 if(!w)return false;
 auto it=w->bodies.find(id);if(it==w->bodies.end())return false;
 bool current=b2Body_IsEnabled(it->second.body);
 if(current!=enabled){if(enabled)b2Body_Enable(it->second.body);else b2Body_Disable(it->second.body);}
 return true;
}
extern "C" bool bsl_physics_body_state(BslPhysicsWorld* w,uint64_t id,BslPhysicsBodyState* out){if(!w||!out)return false;auto it=w->bodies.find(id);if(it==w->bodies.end())return false;auto p=b2Body_GetPosition(it->second.body),v=b2Body_GetLinearVelocity(it->second.body);auto r=b2Body_GetRotation(it->second.body);*out={p.x,p.y,v.x,v.y,std::atan2(r.s,r.c),b2Body_GetAngularVelocity(it->second.body)};return true;}
extern "C" bool bsl_physics_body_velocity(BslPhysicsWorld* w,uint64_t id,float x,float y){if(!w||!finite(x)||!finite(y))return false;auto it=w->bodies.find(id);if(it==w->bodies.end()||it->second.mode==BSL_BODY_STATIC)return false;b2Body_SetLinearVelocity(it->second.body,{x,y});return true;}
extern "C" bool bsl_physics_body_impulse(BslPhysicsWorld* w,uint64_t id,float x,float y){if(!w||!finite(x)||!finite(y))return false;auto it=w->bodies.find(id);if(it==w->bodies.end()||it->second.mode!=BSL_BODY_DYNAMIC)return false;b2Body_ApplyLinearImpulseToCenter(it->second.body,{x,y},true);return true;}
extern "C" bool bsl_physics_body_mass(BslPhysicsWorld* w,uint64_t id,float* out){if(!w||!out)return false;auto it=w->bodies.find(id);if(it==w->bodies.end()||it->second.mode!=BSL_BODY_DYNAMIC)return false;*out=b2Body_GetMass(it->second.body);return std::isfinite(*out)&&*out>0;}
extern "C" bool bsl_physics_body_impulse_at(BslPhysicsWorld* w,uint64_t id,float x,float y,float px,float py){if(!w||!finite(x)||!finite(y)||!finite(px)||!finite(py))return false;auto it=w->bodies.find(id);if(it==w->bodies.end()||it->second.mode!=BSL_BODY_DYNAMIC)return false;b2Body_ApplyLinearImpulse(it->second.body,{x,y},{px,py},true);return true;}
namespace {
struct AttackQuery {std::vector<BslPhysicsHit> hits;b2Pos center;};
bool attack_query_callback(b2ShapeId shape,void* context){
 auto& q=*static_cast<AttackQuery*>(context);auto body=b2Shape_GetBody(shape);auto* e=static_cast<Entry*>(b2Body_GetUserData(body));
 if(!e||e->mode!=BSL_BODY_DYNAMIC||q.hits.size()>=2048)return true;
 auto p=b2Shape_GetClosestPoint(shape,q.center);q.hits.push_back({e->id,p.x,p.y});return true;
}
}
extern "C" bool bsl_physics_query_capsule(BslPhysicsWorld* w,float ax,float ay,float bx,float by,float radius,BslPhysicsHit* out,unsigned capacity,unsigned* count){
 if(!w||!count||(!out&&capacity)||!finite(ax)||!finite(ay)||!finite(bx)||!finite(by)||!finite(radius)||radius<.01f)return false;
 try{
  AttackQuery q{};q.center={(ax+bx)*.5f,(ay+by)*.5f};b2Vec2 points[2]={{ax,ay},{bx,by}};
  auto proxy=b2MakeProxy(points,(ax==bx&&ay==by)?1:2,radius);auto filter=b2DefaultQueryFilter();filter.categoryBits=16;filter.maskBits=8;
  b2World_OverlapShape(w->world,{0,0},&proxy,filter,attack_query_callback,&q);
  std::sort(q.hits.begin(),q.hits.end(),[](const BslPhysicsHit& a,const BslPhysicsHit& b){return a.id<b.id;});
  q.hits.erase(std::unique(q.hits.begin(),q.hits.end(),[](const BslPhysicsHit& a,const BslPhysicsHit& b){return a.id==b.id;}),q.hits.end());
  *count=unsigned(q.hits.size());if(q.hits.size()>capacity)return false;std::copy(q.hits.begin(),q.hits.end(),out);return true;
 }catch(...){return false;}
}
extern "C" bool bsl_physics_kinematic_box_pose(BslPhysicsWorld* w,uint64_t id,float x,float y,float angle,float hw,float hh,float vx,float vy,float omega){
 if(!w||!finite(x)||!finite(y)||!finite(angle)||!finite(hw)||!finite(hh)||!finite(vx)||!finite(vy)||!finite(omega)||hw<.02f||hh<.02f)return false;
 auto it=w->bodies.find(id);if(it==w->bodies.end()||(it->second.mode!=BSL_BODY_KINEMATIC&&it->second.mode!=BSL_BODY_DYNAMIC)||it->second.shape!=BSL_SHAPE_BOX)return false;
 b2ShapeId shape=b2_nullShapeId;if(b2Body_GetShapes(it->second.body,&shape,1)!=1||B2_IS_NULL(shape))return false;
 auto old=b2Shape_GetPolygon(shape);
 if(old.count!=4)return false;
 // Avoid rebuilding the fixture on each tick when animation dimensions are unchanged.
 float old_hw=(old.vertices[1].x-old.vertices[0].x)*.5f;
 float old_hh=(old.vertices[2].y-old.vertices[1].y)*.5f;
 if(std::abs(old_hw-hw)>1e-6f||std::abs(old_hh-hh)>1e-6f){auto box=b2MakeBox(hw,hh);b2Shape_SetPolygon(shape,&box);}
 b2Body_SetTransform(it->second.body,{x,y},{std::cos(angle),std::sin(angle)});
 b2Body_SetLinearVelocity(it->second.body,{vx,vy});b2Body_SetAngularVelocity(it->second.body,omega);return true;
}
extern "C" bool bsl_physics_body_awake(BslPhysicsWorld* w,uint64_t id,bool* out){if(!w||!out)return false;auto it=w->bodies.find(id);if(it==w->bodies.end())return false;*out=b2Body_IsAwake(it->second.body);return true;}
extern "C" bool bsl_physics_body_wake(BslPhysicsWorld* w,uint64_t id){if(!w)return false;auto it=w->bodies.find(id);if(it==w->bodies.end()||it->second.mode==BSL_BODY_STATIC)return false;b2Body_SetAwake(it->second.body,true);return true;}
namespace {
bool joint_base(BslPhysicsWorld* w,uint64_t id,uint64_t a,uint64_t b,float ax,float ay,float bx,float by){
 if(!w||!id||a==b||w->used_joints.count(id)||w->used_joints.size()>=MAX_IDS||!finite(ax)||!finite(ay)||!finite(bx)||!finite(by))return false;
 auto ia=w->bodies.find(a),ib=w->bodies.find(b);return ia!=w->bodies.end()&&ib!=w->bodies.end()&&(ia->second.mode==BSL_BODY_DYNAMIC||ib->second.mode==BSL_BODY_DYNAMIC);
}
template<class Create> bool add_joint(BslPhysicsWorld* w,uint64_t id,uint64_t a,uint64_t b,JointKind kind,Create create){
 try{auto inserted=w->joints.emplace(id,JointEntry{a,b,b2_nullJointId,kind});if(!inserted.second)return false;auto it=inserted.first;
  try{w->used_joints.insert(id);}catch(...){w->joints.erase(it);return false;}
  auto joint=create();if(B2_IS_NULL(joint)){w->used_joints.erase(id);w->joints.erase(it);return false;}it->second.joint=joint;return true;
 }catch(...){return false;}
}
}
extern "C" bool bsl_physics_distance_create(BslPhysicsWorld* w,const BslDistanceJointDef* d){
 if(!d||!joint_base(w,d->id,d->body_a,d->body_b,d->ax,d->ay,d->bx,d->by)||!finite(d->length)||!finite(d->min_length)||!finite(d->max_length)||!finite(d->hertz)||!finite(d->damping)||d->length<.01f||d->hertz<0||d->damping<0||(d->limit&&(!d->spring||d->min_length<.01f||d->max_length<d->min_length)))return false;
 b2DistanceJointDef def=b2DefaultDistanceJointDef();def.base.bodyIdA=w->bodies.at(d->body_a).body;def.base.bodyIdB=w->bodies.at(d->body_b).body;def.base.localFrameA.p={d->ax,d->ay};def.base.localFrameB.p={d->bx,d->by};def.base.collideConnected=d->collide_connected;def.length=d->length;def.enableSpring=d->spring;def.enableLimit=d->limit;def.hertz=d->hertz;def.dampingRatio=d->damping;if(d->limit){def.minLength=d->min_length;def.maxLength=d->max_length;}
 return add_joint(w,d->id,d->body_a,d->body_b,JointKind::distance,[&]{return b2CreateDistanceJoint(w->world,&def);});
}
extern "C" bool bsl_physics_revolute_create(BslPhysicsWorld* w,const BslRevoluteJointDef* d){
 constexpr float limit=3.110176f;
 if(!d||!joint_base(w,d->id,d->body_a,d->body_b,d->ax,d->ay,d->bx,d->by)||!finite(d->lower_angle)||!finite(d->upper_angle)||!finite(d->motor_speed)||!finite(d->max_motor_torque)||d->max_motor_torque<0||(d->limit&&(d->lower_angle< -limit||d->upper_angle>limit||d->lower_angle>d->upper_angle)))return false;
 b2RevoluteJointDef def=b2DefaultRevoluteJointDef();def.base.bodyIdA=w->bodies.at(d->body_a).body;def.base.bodyIdB=w->bodies.at(d->body_b).body;def.base.localFrameA.p={d->ax,d->ay};def.base.localFrameB.p={d->bx,d->by};def.base.collideConnected=d->collide_connected;def.enableLimit=d->limit;def.lowerAngle=d->lower_angle;def.upperAngle=d->upper_angle;def.enableMotor=d->motor;def.motorSpeed=d->motor_speed;def.maxMotorTorque=d->max_motor_torque;
 return add_joint(w,d->id,d->body_a,d->body_b,JointKind::revolute,[&]{return b2CreateRevoluteJoint(w->world,&def);});
}
extern "C" bool bsl_physics_joint_destroy(BslPhysicsWorld* w,uint64_t id){if(!w)return false;auto it=w->joints.find(id);if(it==w->joints.end())return false;b2DestroyJoint(it->second.joint);w->joints.erase(it);return true;}
extern "C" bool bsl_physics_joint_exists(BslPhysicsWorld* w,uint64_t id){return w&&w->joints.count(id)!=0;}
extern "C" bool bsl_physics_distance_length(BslPhysicsWorld* w,uint64_t id,float* out){if(!w||!out)return false;auto it=w->joints.find(id);if(it==w->joints.end()||it->second.kind!=JointKind::distance)return false;*out=b2DistanceJoint_GetCurrentLength(it->second.joint);return true;}
extern "C" bool bsl_physics_revolute_speed(BslPhysicsWorld* w,uint64_t id,float* out){if(!w||!out)return false;auto it=w->joints.find(id);if(it==w->joints.end()||it->second.kind!=JointKind::revolute)return false;*out=b2RevoluteJoint_GetCurrentSpeed(it->second.joint);return true;}
extern "C" bool bsl_physics_step(BslPhysicsWorld* w,float seconds,int substeps){
 if(!w||!std::isfinite(seconds)||seconds<=0||seconds>.1f||substeps<1||substeps>8)return false;
 w->contacts.clear();w->dropped=0;b2World_Step(w->world,seconds,substeps);auto events=b2World_GetContactEvents(w->world);
 for(int i=0;i<events.beginCount;i++){auto a=events.beginEvents[i].shapeIdA,b=events.beginEvents[i].shapeIdB;if(!b2Shape_IsValid(a)||!b2Shape_IsValid(b))continue;auto* x=static_cast<Entry*>(b2Body_GetUserData(b2Shape_GetBody(a)));auto* y=static_cast<Entry*>(b2Body_GetUserData(b2Shape_GetBody(b)));if(!x||!y)continue;if(w->contacts.size()==MAX_CONTACTS){w->dropped++;continue;}w->contacts.push_back({std::min(x->id,y->id),std::max(x->id,y->id)});}
 return true;
}
extern "C" unsigned bsl_physics_contact_count(const BslPhysicsWorld* w){return w?unsigned(w->contacts.size()):0;}
extern "C" bool bsl_physics_contact_at(const BslPhysicsWorld* w,unsigned i,BslPhysicsContact* out){if(!w||!out||i>=w->contacts.size())return false;*out=w->contacts[i];return true;}
extern "C" unsigned bsl_physics_contact_dropped(const BslPhysicsWorld* w){return w?w->dropped:0;}

extern "C" bool bsl_physics_body_touching(BslPhysicsWorld* w,uint64_t id,uint64_t* out,unsigned capacity,unsigned* required){
 if(!w||!required||(!out&&capacity))return false;
 auto it=w->bodies.find(id);if(it==w->bodies.end())return false;
 try{
  int n=b2Body_GetContactCapacity(it->second.body);if(n<0||n>16384)return false;
  std::vector<b2ContactData> data(static_cast<size_t>(n));int count=b2Body_GetContactData(it->second.body,data.data(),n);
  std::vector<uint64_t> ids;ids.reserve(static_cast<size_t>(count));
  for(int i=0;i<count;i++){
   auto a=data[i].shapeIdA,b=data[i].shapeIdB;
   if(!b2Shape_IsValid(a)||!b2Shape_IsValid(b))return false;
   auto* x=static_cast<Entry*>(b2Body_GetUserData(b2Shape_GetBody(a)));auto* y=static_cast<Entry*>(b2Body_GetUserData(b2Shape_GetBody(b)));
   if(!x||!y)return false;
   if(x->id==id&&y->id!=id)ids.push_back(y->id);else if(y->id==id&&x->id!=id)ids.push_back(x->id);else return false;
  }
  std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
  *required=unsigned(ids.size());if(capacity<ids.size())return false;
  std::copy(ids.begin(),ids.end(),out);return true;
 }catch(...){return false;}
}
struct BslContactTracker {
 uint64_t owner;unsigned grace,limit;
 std::map<uint64_t,unsigned> active;std::vector<uint64_t> begins,ends;
};
extern "C" BslContactTracker* bsl_contact_tracker_create(uint64_t owner,unsigned grace,unsigned limit){
 if(!owner||grace>60||!limit||limit>4096)return nullptr;
 try{return new BslContactTracker{owner,grace,limit,{},{},{}};}catch(...){return nullptr;}
}
extern "C" void bsl_contact_tracker_destroy(BslContactTracker* t){delete t;}
extern "C" void bsl_contact_tracker_reset(BslContactTracker* t){if(t){t->active.clear();t->begins.clear();t->ends.clear();}}
extern "C" bool bsl_contact_tracker_update(BslContactTracker* t,const uint64_t* ids,unsigned count){
 if(!t||(!ids&&count)||count>16384)return false;
 try{
  std::set<uint64_t> current;
  for(unsigned i=0;i<count;i++){if(!ids[i]||ids[i]==t->owner)return false;current.insert(ids[i]);}
  if(current.size()>t->limit)return false;
  auto next=t->active;std::vector<uint64_t> begins,ends;
  for(auto i=next.begin();i!=next.end();){if(current.count(i->first)){i->second=0;++i;}else if(++i->second>t->grace){ends.push_back(i->first);i=next.erase(i);}else ++i;}
  for(auto id:current)if(!next.count(id)){next.emplace(id,0);begins.push_back(id);}
  if(next.size()>t->limit)return false;
  t->active.swap(next);t->begins.swap(begins);t->ends.swap(ends);return true;
 }catch(...){return false;}
}
extern "C" unsigned bsl_contact_tracker_begin_count(const BslContactTracker* t){return t?unsigned(t->begins.size()):0;}
extern "C" unsigned bsl_contact_tracker_end_count(const BslContactTracker* t){return t?unsigned(t->ends.size()):0;}
extern "C" bool bsl_contact_tracker_begin_at(const BslContactTracker* t,unsigned i,uint64_t* out){if(!t||!out||i>=t->begins.size())return false;*out=t->begins[i];return true;}
extern "C" bool bsl_contact_tracker_end_at(const BslContactTracker* t,unsigned i,uint64_t* out){if(!t||!out||i>=t->ends.size())return false;*out=t->ends[i];return true;}
