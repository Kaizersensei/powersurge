// Lab marker; actual availability belongs to selected claim progress. Signed: Atlas.
using SurgeEngine.UI.Text;
using SurgeEngine.Vector2;
object "BSL Pickup" is "entity", "awake", "gizmo", "editor-gizmo"
{
 pickup = spawn("BSLPickupView");
 label = Text("HUD");
 state "main" { label.text = pickup.available ? pickup.label + "\nDown + Ctrl/LT" : ""; }
 fun constructor() { label.offset = Vector2(-42, -26); label.maxWidth = 160; }
 fun get_available() { return pickup.available; }
 // The Text child renders the marker; the tagged parent needs the editor callback.
 fun onRenderGizmos(x,y) {}
}
