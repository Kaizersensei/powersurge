// Spatial event identity; commands run only in the shared native Events service.
// GPL-3.0-or-later. Signed: Atlas.
object "BSL Event Anchor" is "entity", "awake", "gimmick", "bsl-event"
{
    parameters = spawn("BSL Instance Parameters");
    owner = spawn("BSLAnchorOwner");
    state "main" {}
    fun get_radius() { return parameters.value("radius", 96); }
    fun set_radius(value) { parameters.override("radius", value); }
    fun get_enabled() { return parameters.value("enabled", true); }
    fun set_enabled(value) { before = this.enabled; parameters.override("enabled", value); if(before && !this.enabled) owner.cancel(); }
    fun onInstanceParameter(key, value) { before = this.enabled; parameters.author(key, value); if(before && !this.enabled) owner.cancel(); }
    fun resetInstanceParameters() { before = this.enabled; parameters.reset(); if(before && !this.enabled) owner.cancel(); }
}
