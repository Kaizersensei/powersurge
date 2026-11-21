// Local sequence helper; no RPG record or event binding required. GPL-3.0-or-later. Signed: Atlas.
using SurgeEngine.Actor;
using SurgeEngine.UI.Text;
using SurgeEngine.Vector2;
object "BSL Sequence Exit" is "entity", "awake"
{
 actor = Actor("null");
 label = Text("HUD");
 parameters = spawn("BSL Instance Parameters");
 labelMode = -1;
 state "main"
 {
  mode = this.enabled ? (this.requireAction ? 1 : 0) : 2;
  if(mode != labelMode) {
   label.text = mode == 2 ? "Sequence exit\nDisabled" : (mode == 1 ? "Sequence exit\nUp: next level" : "Sequence exit\nContact entry");
   labelMode = mode;
  }
 }
 fun constructor() { label.text = "Sequence exit"; label.offset = Vector2(-48, -56); }
 fun onInstanceParameter(key, value) { parameters.author(key, value); }
 fun resetInstanceParameters() { parameters.reset(); }
 fun get_width() { return parameters.value("width", 96); }
 fun set_width(value) { parameters.override("width", value); }
 fun get_height() { return parameters.value("height", 128); }
 fun set_height(value) { parameters.override("height", value); }
 fun get_originX() { return parameters.value("originX", 48); }
 fun set_originX(value) { parameters.override("originX", value); }
 fun get_originY() { return parameters.value("originY", 128); }
 fun set_originY(value) { parameters.override("originY", value); }
 fun get_requireAction() { return parameters.value("requireAction", true); }
 fun set_requireAction(value) { parameters.override("requireAction", value); }
 fun get_enabled() { return parameters.value("enabled", true); }
 fun set_enabled(value) { parameters.override("enabled", value); }
}
