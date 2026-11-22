/* Experimental Box2D service boundary. Units: meters, seconds, radians. Signed: Mech. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BslPhysicsWorld BslPhysicsWorld;
typedef enum BslPhysicsMode { BSL_BODY_STATIC=0, BSL_BODY_KINEMATIC=1, BSL_BODY_DYNAMIC=2 } BslPhysicsMode;
typedef enum BslPhysicsShape { BSL_SHAPE_CIRCLE=0, BSL_SHAPE_BOX=1, BSL_SHAPE_STAR5=2 } BslPhysicsShape;
typedef struct BslPhysicsBodyDef {
 uint64_t id; BslPhysicsMode mode; BslPhysicsShape shape;
 float x,y,vx,vy,angle,angular_velocity,half_width,half_height,density,friction,restitution;
 uint64_t category,mask; bool bullet,gravity_free;
} BslPhysicsBodyDef;
/* Closed, simple axis-aligned loop; points use world meters and exclude a repeated closing point.
 * Front face is to the right of each directed edge. Up to 1024 points. */
typedef struct BslPhysicsPoint {float x,y;} BslPhysicsPoint;
typedef struct BslPhysicsContourDef {
 uint64_t id; const BslPhysicsPoint* points; unsigned count; uint64_t category,mask;
} BslPhysicsContourDef;
typedef struct BslPhysicsBodyState {float x,y,vx,vy,angle,angular_velocity;} BslPhysicsBodyState;
typedef struct BslPhysicsContact {uint64_t a,b;} BslPhysicsContact;
/* Joint anchors are body-local meters; joint IDs have their own world-lifetime namespace. */
typedef struct BslDistanceJointDef {
 uint64_t id,body_a,body_b;float ax,ay,bx,by,length,min_length,max_length,hertz,damping;
 bool spring,limit,collide_connected;
} BslDistanceJointDef;
typedef struct BslRevoluteJointDef {
 uint64_t id,body_a,body_b;float ax,ay,bx,by,lower_angle,upper_angle,motor_speed,max_motor_torque;
 bool limit,motor,collide_connected;
} BslRevoluteJointDef;
/* No mutation during step/callback; begin contacts must be consumed before the next step. */
BslPhysicsWorld* bsl_physics_world_create(float gravity_x,float gravity_y);
void bsl_physics_world_destroy(BslPhysicsWorld* world);
bool bsl_physics_body_create(BslPhysicsWorld* world,const BslPhysicsBodyDef* def);
bool bsl_physics_contour_create(BslPhysicsWorld* world,const BslPhysicsContourDef* def);
bool bsl_physics_body_destroy(BslPhysicsWorld* world,uint64_t id);
/* Disabled bodies retain their world-lifetime identity but do not participate in simulation. */
bool bsl_physics_body_enabled(BslPhysicsWorld* world,uint64_t id,bool enabled);
bool bsl_physics_body_state(BslPhysicsWorld* world,uint64_t id,BslPhysicsBodyState* out);
bool bsl_physics_body_velocity(BslPhysicsWorld* world,uint64_t id,float vx,float vy);
bool bsl_physics_body_impulse(BslPhysicsWorld* world,uint64_t id,float x,float y);
bool bsl_physics_body_mass(BslPhysicsWorld* world,uint64_t id,float* out);
bool bsl_physics_body_impulse_at(BslPhysicsWorld* world,uint64_t id,float x,float y,float px,float py);
typedef struct BslPhysicsHit {uint64_t id;float x,y;} BslPhysicsHit;
/* Capsule overlap returns unique dynamic prop bodies and the closest point on each. */
bool bsl_physics_query_capsule(BslPhysicsWorld* world,float ax,float ay,float bx,float by,float radius,BslPhysicsHit* out,unsigned capacity,unsigned* count);
/* Experimental driven-box update for kinematic or dynamic bodies; world meters, radians and per-second velocities.
 * Position is immediate, velocity applies on the next step. Caller owns interpolation,
 * safe teleport policy and the distinction between frame resizing and continuous motion. */
bool bsl_physics_kinematic_box_pose(BslPhysicsWorld* world,uint64_t id,float x,float y,float angle,float half_width,float half_height,float vx,float vy,float angular_velocity);
/* Solver sleep is island-aware; wake is a request, not a substitute for impulse application. */
bool bsl_physics_body_awake(BslPhysicsWorld* world,uint64_t id,bool* out);
bool bsl_physics_body_wake(BslPhysicsWorld* world,uint64_t id);
bool bsl_physics_distance_create(BslPhysicsWorld* world,const BslDistanceJointDef* def);
bool bsl_physics_revolute_create(BslPhysicsWorld* world,const BslRevoluteJointDef* def);
bool bsl_physics_joint_destroy(BslPhysicsWorld* world,uint64_t id);
bool bsl_physics_joint_exists(BslPhysicsWorld* world,uint64_t id);
bool bsl_physics_distance_length(BslPhysicsWorld* world,uint64_t id,float* out);
bool bsl_physics_revolute_speed(BslPhysicsWorld* world,uint64_t id,float* out);
bool bsl_physics_step(BslPhysicsWorld* world,float seconds,int substeps);
unsigned bsl_physics_contact_count(const BslPhysicsWorld* world);
bool bsl_physics_contact_at(const BslPhysicsWorld* world,unsigned index,BslPhysicsContact* out);
unsigned bsl_physics_contact_dropped(const BslPhysicsWorld* world);
/* Complete body-pair snapshot after a step. required is set on insufficient capacity;
 * out remains untouched. Includes Box2D speculative contacts; not a damage event. */
bool bsl_physics_body_touching(BslPhysicsWorld* world,uint64_t id,uint64_t* out,unsigned capacity,unsigned* required);
typedef struct BslContactTracker BslContactTracker;
/* One tracker per owner; once per simulation step, provide the COMPLETE snapshot.
 * grace is missing steps retained before end; max_pairs is a provisional bound.
 * Failed updates preserve state/events; removal/scene teardown requires reset/destroy. */
BslContactTracker* bsl_contact_tracker_create(uint64_t owner,unsigned grace,unsigned max_pairs);
void bsl_contact_tracker_destroy(BslContactTracker* tracker);
void bsl_contact_tracker_reset(BslContactTracker* tracker);
bool bsl_contact_tracker_update(BslContactTracker* tracker,const uint64_t* ids,unsigned count);
unsigned bsl_contact_tracker_begin_count(const BslContactTracker* tracker);
unsigned bsl_contact_tracker_end_count(const BslContactTracker* tracker);
bool bsl_contact_tracker_begin_at(const BslContactTracker* tracker,unsigned index,uint64_t* out);
bool bsl_contact_tracker_end_at(const BslContactTracker* tracker,unsigned index,uint64_t* out);

#ifdef __cplusplus
}
#endif
