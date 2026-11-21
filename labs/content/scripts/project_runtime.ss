// Shared project setup: every level entry route uses the same services. Signed: Atlas.
using SurgeEngine.Input;
using SurgeEngine.Level;
using SurgeEngine.Player;

object "BSL Runtime Setup" is "entity", "awake", "private", "setup"
{
 session = spawn("FoundationSession");
 bridge = spawn("BORBridge");
 characters = spawn("BSL Character Registry");
 actors = spawn("BSL Actor Registry");
 hud = spawn("BOR HUD");
 state "main"
 {
  Player.active.collider.enabled = false;
  bridge.tick();
  hud.update(bridge.status());
 }
}
object "BSL Pause and Quit" is "entity", "awake", "private", "setup"
{
 input = Input("bor");
 menuInput = Input("default");
 gate = spawn("FoundationInputGate");
 blocked = true;
 state "main"
 {
  if(gate.captured) blocked = true;
  else if(blocked) {
   if(!input.buttonDown("fire3") && !input.buttonDown("fire4") && !input.buttonPressed("fire3") && !input.buttonPressed("fire4") && !menuInput.buttonDown("fire3") && !menuInput.buttonDown("fire4") && !menuInput.buttonPressed("fire3") && !menuInput.buttonPressed("fire4")) blocked = false;
  }
  else if(!Level.cleared && (input.buttonPressed("fire3") || input.buttonPressed("fire4") || menuInput.buttonPressed("fire3") || menuInput.buttonPressed("fire4"))) Level.pause();
 }
}
