// Original checkpoint adapters. Checkpoint sprite is upstream MIT artwork; original notices retained in runtime.
// Gameplay activation/rewind belongs to the native service. Signed: Atlas.
using SurgeEngine.Actor;
using SurgeEngine.UI.Text;
using SurgeEngine.Vector2;
object "BSL Temporary Checkpoint" is "entity", "awake"
{
 actor = Actor("Checkpoint");
 label = Text("HUD");
 state "main" {}
 fun constructor() { label.text = "C: Halfway"; label.offset = Vector2(-42,-78); }
}
object "BSL Permanent Checkpoint" is "entity", "awake"
{
 actor = Actor("Checkpoint");
 label = Text("HUD");
 state "main" {}
 fun constructor() { label.text = "C: Rest point"; label.offset = Vector2(-42,-78); }
}
