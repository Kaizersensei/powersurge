// Generic non-roster actor bodies; BOR owns actions/stats and Surge owns physics. Signed: Mech.
using SurgeEngine.Actor;
using SurgeEngine.Level;
using SurgeEngine.Vector2;
object "BSL Registry Actor" is "entity", "awake", "private", "gizmo", "editor-gizmo"
{
 actor = null;
 spriteModel = null;
 definition = null;
 controller = spawn("BORController");
 state "main" {}
 fun initialize(id)
 {
  if(definition != null) return definition == id;
  model = controller.actorModel(id);
  if(model == null || (spriteModel != null && spriteModel != model)) return false;
  if(actor == null) { actor = Actor("BOR " + model); spriteModel = model; }
  if(!controller.initActor(id)) { actor.visible = false; return false; }
  definition = id;actor.visible = true;return true;
 }
 fun setInteraction(id) { return controller.setInteraction(id); }
 fun interactionDefinition() { return controller.interactionDefinition(); }
 fun resetInteractionBinding() { return controller.resetInteractionBinding(); }
 fun beginConversation(initiator,id,range,calendarFactor) { return controller.beginConversation(initiator,id,range,calendarFactor); }
 fun chooseConversation(choice) { return controller.chooseConversation(choice); }
 fun onLeaveEditor() { if(definition != null) controller.rebase(); }
 fun onRenderGizmos(x,y) { if(definition != null) controller.onRenderGizmos(x,y); }
}
object "BSL Actor Registry"
{
 registry = spawn("BORController");
 state "main" {}
 fun definitionAt(index) { return registry.actorAt(index); }
 fun create(definition,x,y)
 {
  if(!registry.validActorPosition(x,y) || registry.actorModel(definition) == null) return null;
  body = Level.spawnEntity("BSL Registry Actor",Vector2(x,y));
  if(!body.initialize(definition)) { body.destroy();return null; }
  return body;
 }
}
