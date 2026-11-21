// Original flag-driven world fixture; a condition consumer, not serialized geometry. Signed: Atlas.
using SurgeEngine.Actor;
using SurgeEngine.Brick;
object "BSL Progress Gate" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BOR Moving Platform");
 brick = Brick("BOR Moving Platform");
 progress = spawn("BSLProgressView");
 opened = false;
 state "main" { reconcile(); }
 fun constructor() { reconcile(); progress.observe(); }
 fun reconcile() { opened = progress.flag("switch:training_reward"); brick.enabled = !opened; actor.visible = !opened; }
 fun get_opened() { return opened; }
 fun onRenderGizmos(x,y) { brick.onRenderGizmos(x,y); }
}

object "BSL Boot Observer" is "entity", "awake", "private"
{
 progress = spawn("BSLProgressView");
 state "main" {}
 fun constructor() { progress.observe(); }
}
