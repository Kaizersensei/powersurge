/* Stable placement validation inside Surge's spatial authority. GPL-3.0-or-later. Signed: Mech. */
int bsl_level_actor_placement_resolve(const char* placement,const char* expected,uint64_t* actor)
{
    if(!actor) return BSL_ACTOR_PLACEMENT_REFUSED;
    if(!placement || !expected) { *actor = 0; return BSL_ACTOR_PLACEMENT_REFUSED; }
    *actor = 0;
    surgescript_object_t* object = level_get_entity_by_id(placement);
    if(!object || surgescript_object_is_killed(object)) return BSL_ACTOR_PLACEMENT_MISSING;
    if(!entity_info_exists(object) || !entity_info_is_persistent(object)) return BSL_ACTOR_PLACEMENT_TRANSIENT;
    if(strcmp(surgescript_object_name(object), expected)) return BSL_ACTOR_PLACEMENT_WRONG_OBJECT;
    *actor = bsl_bor_entity_actor(surgescript_object_handle(object));
    return *actor ? BSL_ACTOR_PLACEMENT_BOUND : BSL_ACTOR_PLACEMENT_NO_CONTROLLER;
}
