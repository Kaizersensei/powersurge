/* Experimental level-scoped object factory; not an authoring schema. Signed: Mech. */
#pragma once
#include "world.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BslObjectFactory BslObjectFactory;
typedef struct BslObjectDefinition {
 const char* id;BslPhysicsShape shape;float half_width,half_height,density,friction,restitution;
 uint64_t category,mask;
} BslObjectDefinition;
/* world must outlive factory. Caller reserves [first_id,last_id] exclusively.
 * Solver IDs are transient, unique within the world lifetime, and never serialized. */
BslObjectFactory* bsl_object_factory_create(BslPhysicsWorld* world,uint64_t first_id,uint64_t last_id);
void bsl_object_factory_destroy(BslObjectFactory* factory);
bool bsl_object_factory_define(BslObjectFactory* factory,const BslObjectDefinition* definition);
bool bsl_object_factory_spawn(BslObjectFactory* factory,const char* definition_id,const char* placement_id,float x,float y,float angle);
bool bsl_object_factory_remove(BslObjectFactory* factory,const char* placement_id);
bool bsl_object_factory_lookup(BslObjectFactory* factory,const char* placement_id,uint64_t* transient_body_id);
#ifdef __cplusplus
}
#endif
