// Shared controller; placeholder test graphics. Signed: Mech.
using SurgeEngine.Actor;
object "BSL Physics Ball" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BSL Physics Ball");
 controller = spawn("BSLPhysicsPropController");
 state "main" { controller.update("ball"); }
 fun onRenderGizmos(x,y) {}
}
object "BSL Physics Stone" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BSL Physics Stone");
 controller = spawn("BSLPhysicsPropController");
 state "main" { controller.update("stone"); }
 fun onRenderGizmos(x,y) {}
}
object "BSL Physics Cupboard" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BSL Physics Cupboard");
 controller = spawn("BSLPhysicsPropController");
 state "main" { controller.update("cupboard"); }
 fun onRenderGizmos(x,y) {}
}
object "BSL Physics Boulder" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BSL Physics Boulder");
 controller = spawn("BSLPhysicsPropController");
 state "main" { controller.update("boulder"); }
 fun onRenderGizmos(x,y) {}
}
object "BSL Physics Star" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BSL Physics Star");
 controller = spawn("BSLPhysicsPropController");
 state "main" { controller.update("star"); }
 fun onRenderGizmos(x,y) {}
}
