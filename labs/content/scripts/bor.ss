// Stock Surge objects wrapping the native BOR adapter. Signed: Atlas.
using SurgeEngine.Actor;
using SurgeEngine.Player;
using SurgeEngine.UI.Text;
using SurgeEngine.Vector2;
using SurgeEngine.Input;
using SurgeEngine.Level;
using SurgeEngine.Brick;
using SurgeEngine.Behaviors.CircularMovement;

object "BOR Setup" is "entity", "awake", "private", "setup"
{
    bridge = spawn("BORBridge");
    characters = spawn("BSL Character Registry");
    actors = spawn("BSL Actor Registry");
    controls = Input("bor");
    hud = spawn("BOR HUD");
    pauseInputBlocked = true;
    state "main"
    {
        Player.active.collider.enabled = false;
        if(bridge.uiCapturesInput) pauseInputBlocked = true;
        else if(pauseInputBlocked) {
            if(!controls.buttonDown("fire3") && !controls.buttonPressed("fire3")) pauseInputBlocked = false;
        }
        else if(controls.buttonPressed("fire3")) Level.pause();
        bridge.tick();
        hud.update(bridge.status());
    }
}
object "BOR HUD" is "entity", "awake", "private", "detached"
{
    text = Text("HUD");
    state "main" {}
    fun constructor() { text.offset = Vector2(8, 8); text.maxWidth = 944; }
    fun update(value) { text.text = value; }
}

object "BOR Michio" is "entity", "awake", "gizmo", "editor-gizmo"
{
    actor = Actor("BOR michio_sword");
    controller = spawn("BORController");
    state "main" {}
    fun constructor() { controller.init("michio_sword"); controller.bindCharacter("character:michio"); }
    fun onLeaveEditor() { controller.rebase(); }
    fun onRenderGizmos(x, y) { controller.onRenderGizmos(x, y); }
}

object "BOR Red Grunt" is "entity", "awake", "gizmo", "editor-gizmo"
{
    actor = Actor("BOR red_grunt");
    controller = spawn("BORController");
    state "main" {}
    fun constructor() { controller.init("red_grunt"); controller.setWakeRange(800); }
    fun onLeaveEditor() { controller.rebase(); }
    fun onRenderGizmos(x, y) { controller.onRenderGizmos(x, y); }
}

object "BOR Green Grunt" is "entity", "awake", "gizmo", "editor-gizmo"
{
    actor = Actor("BOR Green_Grunt");
    controller = spawn("BORController");
    state "main" {}
    fun constructor() { controller.init("Green_Grunt"); controller.setWakeRange(800); }
    fun onLeaveEditor() { controller.rebase(); }
    fun onRenderGizmos(x, y) { controller.onRenderGizmos(x, y); }
}

object "BOR Big Rat" is "entity", "awake", "gizmo", "editor-gizmo"
{
    actor = Actor("BOR Rat_Big");
    controller = spawn("BORController");
    state "main" {}
    fun constructor() { controller.init("Rat_Big"); controller.setWakeRange(800); }
    fun onLeaveEditor() { controller.rebase(); }
    fun onRenderGizmos(x, y) { controller.onRenderGizmos(x, y); }
}

object "BOR Hedgehog" is "entity", "awake", "gizmo", "editor-gizmo"
{
    actor = Actor("BOR Hedgehog_Brown");
    controller = spawn("BORController");
    state "main" {}
    fun constructor() { controller.init("Hedgehog_Brown"); controller.setWakeRange(800); }
    fun onLeaveEditor() { controller.rebase(); }
    fun onRenderGizmos(x, y) { controller.onRenderGizmos(x, y); }
}

// Demonstration support: normal Surge Brick + movement behavior, no actor-specific carry script.
object "BOR Moving Ledge" is "entity", "awake", "gimmick"
{
    actor = Actor("BOR Moving Platform");
    brick = Brick("BOR Moving Platform");
    movement = CircularMovement();
    parameters = spawn("BSL Instance Parameters");
    state "main" { movement.radius = this.radius; movement.rate = this.rate; }
    fun constructor()
    {
        brick.type = "cloud";
        movement.radius = 48;
        movement.rate = 0.125;
        movement.scale = Vector2(1, 0.5);
    }
    fun get_radius() { return parameters.value("radius", 48); }
    fun set_radius(value) { parameters.override("radius", value); }
    fun get_rate() { return parameters.value("rate", 0.125); }
    fun set_rate(value) { parameters.override("rate", value); }
    fun onInstanceParameter(key, value) { parameters.author(key, value); _applyParameters(); }
    fun resetInstanceParameters() { parameters.reset(); _applyParameters(); }
    fun _applyParameters() { movement.radius = this.radius; movement.rate = this.rate; }
}

object "BOR Pole" is "entity", "awake", "gimmick", "gizmo", "editor-gizmo"
{
    parameters = spawn("BSL Instance Parameters");
    region = spawn("BORClimbRegion");
    state "main" { region.configure(this.width, this.height, this.speedMultiplier, this.enabled); }
    fun constructor() { region.init("pole"); }
    fun onRenderGizmos(x, y) { region.onRenderGizmos(x, y); }
    fun get_width() { return parameters.value("width", 24); }
    fun set_width(value) { parameters.override("width", value); }
    fun get_height() { return parameters.value("height", 174); }
    fun set_height(value) { parameters.override("height", value); }
    fun get_speedMultiplier() { return parameters.value("speedMultiplier", 1.0); }
    fun set_speedMultiplier(value) { parameters.override("speedMultiplier", value); }
    fun get_enabled() { return parameters.value("enabled", true); }
    fun set_enabled(value) { parameters.override("enabled", value); }
    fun onInstanceParameter(key, value) { parameters.author(key, value); _applyParameters(); }
    fun resetInstanceParameters() { parameters.reset(); _applyParameters(); }
    fun _applyParameters() { region.configure(this.width, this.height, this.speedMultiplier, this.enabled); }
}

object "BOR Ladder" is "entity", "awake", "gimmick", "gizmo", "editor-gizmo"
{
    parameters = spawn("BSL Instance Parameters");
    region = spawn("BORClimbRegion");
    state "main" { region.configure(this.width, this.height, this.speedMultiplier, this.enabled); }
    fun constructor() { region.init("ladder"); }
    fun onRenderGizmos(x, y) { region.onRenderGizmos(x, y); }
    fun get_width() { return parameters.value("width", 24); }
    fun set_width(value) { parameters.override("width", value); }
    fun get_height() { return parameters.value("height", 294); }
    fun set_height(value) { parameters.override("height", value); }
    fun get_speedMultiplier() { return parameters.value("speedMultiplier", 1.0); }
    fun set_speedMultiplier(value) { parameters.override("speedMultiplier", value); }
    fun get_enabled() { return parameters.value("enabled", true); }
    fun set_enabled(value) { parameters.override("enabled", value); }
    fun onInstanceParameter(key, value) { parameters.author(key, value); _applyParameters(); }
    fun resetInstanceParameters() { parameters.reset(); _applyParameters(); }
    fun _applyParameters() { region.configure(this.width, this.height, this.speedMultiplier, this.enabled); }
}

// Bars: editable local points form straight, angled or curved polylines. Signed: Atlas.
object "BOR Bar" is "entity", "awake", "gimmick", "gizmo", "editor-gizmo"
{
 public path = "0,0;192,0";
 public width = 24;
 public speedMultiplier = 1.0;
 public enabled = true;
 region = spawn("BORClimbRegion");
 state "main" { region.configure(width, 192, speedMultiplier, enabled); region.setPath(path); }
 fun constructor() { region.init("bar"); }
 fun onRenderGizmos(x, y) { region.onRenderGizmos(x, y); }
}

object "BOR Bar Straight" is "entity", "awake", "gimmick"
{
 bar = spawn("BOR Bar");
 state "main" {}
 fun constructor() { bar.path = "0,0;384,0"; }
}

object "BOR Bar Peak" is "entity", "awake", "gimmick"
{
 bar = spawn("BOR Bar");
 state "main" {}
 fun constructor() { bar.path = "0,0;192,-72;384,0"; }
}

object "BOR Bar Curve" is "entity", "awake", "gimmick"
{
 bar = spawn("BOR Bar");
 state "main" {}
 fun constructor() { bar.path = "0,0;48,-22;96,-36;160,-42;224,-43;288,-50;336,-64;384,-88"; }
}

object "BOR Training Log" is "entity", "awake", "gizmo", "editor-gizmo"
{
 actor = Actor("BOR Training_Log");
 controller = spawn("BORController");
 state "main" {}
 fun constructor() { controller.init("Training_Log"); }
 fun onLeaveEditor() { controller.rebase(); }
 fun onRenderGizmos(x, y) { controller.onRenderGizmos(x, y); }
}
