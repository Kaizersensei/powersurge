// Original live-session travel lifecycle. Included by foundation_host.cpp. Signed: Atlas.
namespace {
bool live_door(const bsl::DoorDef& definition,bsl::DoorDef& out,BslDoorLive& properties){
 if(definition.level!=bsl_engine_level_file()||!bsl_bor_teleport_live(definition.placement,&properties))return false;
 out=definition;out.x=properties.x;out.y=properties.y;out.width=properties.width;out.height=properties.height;
 out.origin_x=properties.origin_x;out.origin_y=properties.origin_y;out.action=properties.require_action;
 out.preserve_velocity=properties.preserve_velocity;out.facing=properties.facing;return true;
}
void arrive_live(const bsl::TravelPoint& point){
 session->door_contacts.clear();
 for(const auto& definition:doors().all()){
  bsl::DoorDef door;BslDoorLive live{};
  if(live_door(definition,door,live))
   (void)session->door_contacts.request(door,doors().contains(door,point.level,point.x,point.y),false);
 }
}
void travel_failure(const std::string& why){
 if(!travel)return;
 if(travel->context.source.level==travel->context.target.level){++session->travel_failures;session->notice="Teleport refused: "+why+" Actor and level retained.";travel.reset();return;}
 ++session->travel_failures;travel->phase=TravelPhase::returning;travel->wait=0;travel->source_loaded=false;
 session->notice="Teleport refused: "+why+" Returning to source with current progress.";
 if(bsl_engine_has_level(travel->context.source.level.c_str()))bsl_engine_load_level(travel->context.source.level.c_str());
 else{travel->phase=TravelPhase::failed;session->notice="Teleport stopped: source level unavailable. Esc quits.";}
}
void travel_begin(const char* level){
 if(travel->phase==TravelPhase::returning){if(travel->context.source.level==level){travel->source_loaded=true;session->checkpoints->enter(level);}else travel->phase=TravelPhase::failed;return;}
 if(travel->phase!=TravelPhase::queued||travel->context.target.level!=level){travel_failure("Unexpected destination lifecycle.");return;}
 travel->phase=TravelPhase::assembling;travel->wait=0;session->checkpoints->enter(level);
}
bool travel_checkpoint(const bsl::CheckpointRecord& record){
 if(travel||boot||!bsl_engine_has_level(record.level.c_str()))return false;
 auto route=std::make_unique<TravelRoute>();auto* a=session->actor();
 route->context={{bsl_engine_level_file(),"",a?a->x:0,a?a->y:0,a?a->facing:1},{record.level,"",record.x,record.y,record.facing}};
 route->player=roster().profile().player;route->character=roster().selected().id;route->health=a?a->health:0;route->checkpoint=record;
 session->cancel();session->rewind_requested=false;travel=std::move(route);bsl_engine_load_level(record.level.c_str());return true;
}
bool travel_step(){
 if(!travel)return false;
 if(travel->phase==TravelPhase::queued||travel->phase==TravelPhase::failed||(travel->phase==TravelPhase::returning&&!travel->source_loaded))return true;
 // Non-selected roster members may deliberately have no world body. Signed: Mech.
 if(!session->selected_ready())return true;
 auto& s=*session;const bool returning=travel->phase==TravelPhase::returning;
 auto point=returning?travel->context.source:travel->context.target;
 if(roster().profile().player!=travel->player||roster().selected().id!=travel->character){travel_failure("Controlled character changed during transfer.");return true;}
 if(travel->checkpoint&&!returning){
  if(!checkpoint_restore(&*travel->checkpoint)){travel_failure("No valid destination checkpoint reconstruction.");return true;}
 }else{
  if(!point.door.empty()){
   const auto* definition=doors().door(point.door);bsl::DoorDef door;BslDoorLive live{};
   if(!definition||definition->level!=point.level||!live_door(*definition,door,live)){
    if(returning){point.door.clear();}else{travel_failure("Destination door missing or its live properties are invalid.");return true;}
   }else if(!returning&&travel->door_offset){
    /* Resolve a registered-door offset at arrival, after destination placements finish loading. */
    point.x=door.x+travel->door_offset->first;point.y=door.y+travel->door_offset->second;
    if(!bsl::valid_travel_context({point,point})){travel_failure("Destination offset is outside supported bounds.");return true;}
    travel->context.target=point;
   }
  }
  if(returning&&travel->checkpoint){
   if(!checkpoint_restore()){travel->phase=TravelPhase::failed;s.notice="Checkpoint destination and source fallback refused. Esc quits.";return true;}
  }else if(!bsl_bor_teleport(s.owner,point.x,point.y,returning?0:travel->vx,returning?0:travel->vy,point.facing)){
   if(returning){travel->phase=TravelPhase::failed;s.notice="Teleport source recovery obstructed. Esc quits.";}else if(travel->context.source.level==travel->context.target.level){++s.travel_failures;s.notice="Teleport refused: destination obstructed; actor unchanged.";travel.reset();}else travel_failure("Destination obstructed.");return true;
  }
 }
 if(!returning){
  const auto context=travel->context;const auto complete=travel->complete;
  s.last_travel=context;if(s.progression)s.progression->retire(s.owner);++s.travel_arrivals;arrive_live(context.target);s.notice=travel->checkpoint?"Returned to permanent checkpoint and rewound its activation progress.":"Teleport complete; current gameplay progress retained.";
  travel.reset();s.barrier=true;s.captures_commands=true;bsl_bor_capture_input();
  if(!complete.empty()){
   const auto run="travel-"+std::to_string(++s.anchor_sequence);auto ec=s.event_context(complete,roster().profile().player,s.owner);ec.character=roster().selected().id;ec.travel=context;
   auto result=s.events.start(run,complete,ec);if(result.code==bsl::Code::ok)s.last_event=run;else s.notice="Teleport complete; linked event refused.";
  }
 }else{ arrive_live(point);travel.reset();s.barrier=true;s.captures_commands=true;bsl_bor_capture_input(); }
 return true;
}
void travel_contacts(){
 if(!teleports_enabled()||!session->alive()||session->modal()||session->captures_commands||session->policies.paused()||session->policies.blocked(roster().profile().player)||bsl_engine_editor())return;
 auto* a=session->actor();bool fresh=bsl_bor_teleport_action();
 for(const auto& definition:doors().all()){
  bsl::DoorDef d;BslDoorLive live{};
  bool inside=live_door(definition,d,live)&&doors().contains(d,bsl_engine_level_file(),a->x,a->y);
  if(!inside){session->door_contacts.request(definition,false,false);continue;}
  if(!live.enabled){(void)session->door_contacts.request(d,true,false);continue;}
  if(bsl_host_teleport_request(session->owner,d.id.c_str(),fresh)==0)return;
 }
}
}
extern "C" int bsl_host_teleport_request(uint64_t actor,const char* id,bool fresh){
 int result=static_cast<int>(bsl::Code::disabled);guarded([&]{
  if(!teleports_enabled()||!session)return;
  result=static_cast<int>(bsl::Code::denied);
  if(bsl_host_boot_busy()||!id||!roster().controls(actor)||!session->alive()||session->modal()||session->captures_commands||session->policies.paused()||session->policies.blocked(roster().profile().player)||bsl_engine_editor())return;
  const auto* definition=doors().door(id);if(!definition){result=static_cast<int>(bsl::Code::invalid);return;}
  bsl::DoorDef current;BslDoorLive live{};
  if(!live_door(*definition,current,live)){session->notice="Teleport refused: placement missing or live properties invalid.";return;}
  const auto* d=&current;auto* a=session->actor();
  bool inside=doors().contains(*d,bsl_engine_level_file(),a->x,a->y);
  bool requested=session->door_contacts.request(*d,inside,live.enabled&&fresh);
  if(!live.enabled||!requested)return;
  if(!d->condition.empty()&&!session->events.flag(d->condition,roster().profile().player)){session->notice="Teleporter is activated but not usable: requirements unmet.";return;}
  auto route=std::make_unique<TravelRoute>();route->context.source={d->level,d->id,a->x,a->y,a->facing};
  try{route->context.target=doors().resolve(*d,a->facing,session->last_travel);}catch(const std::exception& e){session->notice=e.what();return;}
  if(d->destination==bsl::DoorDestination::door)route->door_offset=std::make_pair(d->offset_x,d->offset_y);
  if(!bsl_engine_has_level(route->context.target.level.c_str())){session->notice="Teleport refused: destination file unavailable.";return;}
  route->player=roster().profile().player;route->character=roster().selected().id;route->complete=d->complete;route->health=a->health;
  if(d->preserve_velocity){route->vx=a->vx;route->vy=a->vy;}
  const bool same=route->context.source.level==route->context.target.level;
  if(same)route->phase=TravelPhase::assembling;
  session->cancel();session->events.cancel_participant(roster().profile().player);session->anchor_runs.clear();session->interaction_requested=false;
  travel=std::move(route);result=0;if(!same)bsl_engine_load_level(travel->context.target.level.c_str());
 });return result;
}
extern "C" void bsl_host_teleport_probe(BslTeleportProbe* out){if(!out)return;*out={};guarded([&]{if(!session||!teleports_enabled())return;out->enabled=true;out->busy=travel!=nullptr;out->arrivals=session->travel_arrivals;out->failures=session->travel_failures;const auto* event=session->events.run(session->last_event);out->completion_context=event&&event->context.travel&&session->last_travel&&*event->context.travel==*session->last_travel;if(session->last_travel){const auto& c=*session->last_travel;out->source_x=c.source.x;out->source_y=c.source.y;out->target_x=c.target.x;out->target_y=c.target.y;std::snprintf(out->source,sizeof out->source,"%s",c.source.level.c_str());std::snprintf(out->target,sizeof out->target,"%s",c.target.level.c_str());std::snprintf(out->door,sizeof out->door,"%s",c.source.door.c_str());}});}

/* Safe template lookup for level loading/editor previews; no active gameplay session is required. */
extern "C" bool bsl_host_teleport_default(uint64_t placement,const char* field,double* value){
 if(!field||!value)return false;
 bool found=false;guarded([&]{
  for(const auto& d:doors().all())if(d.placement==placement&&d.level==bsl_engine_level_file()){
   const std::string key(field);
   if(key=="width")*value=d.width;else if(key=="height")*value=d.height;
   else if(key=="originX")*value=d.origin_x;else if(key=="originY")*value=d.origin_y;
   else if(key=="requireAction")*value=d.action?1:0;else if(key=="preserveVelocity")*value=d.preserve_velocity?1:0;
   else if(key=="enabled")*value=1;else if(key=="facing")*value=d.facing;else return;
   found=true;return;
  }
 });return found;
}
