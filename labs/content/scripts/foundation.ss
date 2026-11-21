using SurgeEngine.Input;
using SurgeEngine.Level;
// Loaded-level service activation; UI and rules use the native foundation. Signed: Atlas.
object "Foundation Lab Setup" is "entity", "awake", "private", "setup"
{
 session = spawn("FoundationSession");
 state "main" {}
}

// Gate only player pause/back requests; authored engine pause APIs stay available.
object "Foundation Pause and Quit"
{
 input = Input("default");
 gate = spawn("FoundationInputGate");
 pauseInputBlocked = true;
 state "main"
 {
  if(gate.captured) pauseInputBlocked = true;
  else if(pauseInputBlocked) {
   if(!input.buttonDown("fire3") && !input.buttonDown("fire4") && !input.buttonPressed("fire3") && !input.buttonPressed("fire4"))
    pauseInputBlocked = false;
  }
  else if(!Level.cleared && (input.buttonPressed("fire3") || input.buttonPressed("fire4")))
   Level.pause();
 }
}
