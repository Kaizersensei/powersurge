// Generic door placement; conditions and travel belong to native services. Signed: Atlas.
using SurgeEngine.Actor;
using SurgeEngine.UI.Text;
using SurgeEngine.Vector2;
object "BSL Teleporter" is "entity", "awake"
{
 actor = Actor("Checkpoint");
 label = Text("HUD");
 parameters = spawn("BSL Instance Parameters");
 state "main" {}
 fun onInstanceParameter(key, value) { parameters.author(key, value); }
 fun resetInstanceParameters() { parameters.reset(); }
 fun constructor() { label.text = "Teleporter\nUp: activate"; label.offset = Vector2(-42, -104); }
 fun get_width() { return parameters.value("width", 56); }
 fun set_width(value) { parameters.override("width", value); }
 fun get_height() { return parameters.value("height", 256); }
 fun set_height(value) { parameters.override("height", value); }
 fun get_originX() { return parameters.value("originX", 28); }
 fun set_originX(value) { parameters.override("originX", value); }
 fun get_originY() { return parameters.value("originY", 160); }
 fun set_originY(value) { parameters.override("originY", value); }
 fun get_requireAction() { return parameters.value("requireAction", true); }
 fun set_requireAction(value) { parameters.override("requireAction", value); }
 fun get_preserveVelocity() { return parameters.value("preserveVelocity", false); }
 fun set_preserveVelocity(value) { parameters.override("preserveVelocity", value); }
 fun get_enabled() { return parameters.value("enabled", true); }
 fun set_enabled(value) { parameters.override("enabled", value); }
 fun get_facing() { return parameters.value("facing", 0); }
 fun set_facing(value) { parameters.override("facing", value); }
}
