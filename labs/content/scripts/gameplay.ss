// Additional ownership fixture, inert unless gameplay candidate enables the service. Signed: Atlas.
using SurgeEngine.Actor;
object "BSL Alternate Michio" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BOR michio_sword");
 controller = spawn("BORController");
 state "main" {}
 fun constructor() { controller.init("michio_sword"); controller.bindCharacter("character:michio_alternate"); }
 fun onLeaveEditor() { controller.rebase(); }
 fun onRenderGizmos(x,y) { controller.onRenderGizmos(x,y); }
}

// No-progression profile for the separate stat service course. Signed: Atlas.
object "BSL Arcade Michio" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BOR michio_sword");
 controller = spawn("BORController");
 state "main" {}
 fun constructor() { controller.init("michio_sword"); controller.bindCharacter("character:michio_arcade"); }
 fun onLeaveEditor() { controller.rebase(); }
 fun onRenderGizmos(x,y) { controller.onRenderGizmos(x,y); }
}
