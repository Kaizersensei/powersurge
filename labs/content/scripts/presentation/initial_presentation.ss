// Original C2 presentation reconstructed through authored layout data. Signed: Mech.
using SurgeEngine.Actor;
using SurgeEngine.Transform;
using SurgeEngine.Vector2;
using SurgeEngine.Input;
using SurgeEngine.Level;
using SurgeEngine.UI.Text;
using SurgeEngine.Video.Screen;
using SurgeEngine.Audio.Sound;

// Exact marker consumed by the native bootstrap adapter. This owns no gameplay session.
object "BSL Presentation Only" is "setup", "private" { state "main" {} }

object "BSL Presentation Sprite" is "entity", "detached", "awake", "private"
{
    transform = Transform();
    actor = null;
    actors = [];
    config = null;
    rectangle = null;
    canvas = null;
    public x = 0;
    public y = 0;
    public alpha = 1;
    public angle = 0;
    public frame = 0;
    state "main" { if(actor != null) refresh(); }
    fun initialize(spec, rect, size, z)
    {
        config = spec; rectangle = rect; canvas = size;
        x = rect[0]; y = rect[1];
        for(i=0;i<spec["sheets"].length;i++) {
            item=Actor(spec["sheets"][i]["sprite"]); item.zindex=z; item.visible=false; actors.push(item);
        }
        actor=actors[0]; refresh(); return this;
    }
    fun refresh()
    {
        fit = Math.min(Screen.width / canvas[0], Screen.height / canvas[1]);
        transform.position = Vector2((Screen.width-canvas[0]*fit)/2+x*fit, (Screen.height-canvas[1]*fit)/2+y*fit);
        transform.localScale = Vector2(rectangle[2]/config["width"]*fit, rectangle[3]/config["height"]*fit);
        transform.angle = angle;
        selected=config["frame_map"][Math.max(0,Math.min(config["frames"]-1,Math.floor(frame)))];
        for(i=0;i<actors.length;i++) actors[i].visible=i==selected[0];
        actor=actors[selected[0]]; actor.alpha=Math.max(0,Math.min(1,alpha)); actor.anim=selected[1];
    }
}

object "BSL Presentation Text" is "entity", "detached", "awake", "private"
{
    transform = Transform();
    config = null;
    canvas = null;
    rectangle = null;
    role = "";
    label = null;
    lastIndex = -1;
    public text = "";
    public visible = true;
    state "main" { if(config != null) refresh(); }
    fun initialize(font, kind, rect, size, value)
    {
        config=font; role=kind; rectangle=rect; canvas=size; text=value;
        refresh(); return this;
    }
    fun refresh()
    {
        fit = Math.min(Screen.width/canvas[0], Screen.height/canvas[1]);
        index = Math.max(1,Math.min(config["steps"],Math.round(fit/config["scale_step"])));
        if(index != lastIndex) {
            if(label != null) label.destroy();
            label=Text("BSL Presentation "+role+" "+index); label.align="center"; label.zindex=1;
            lastIndex=index;
        }
        transform.position=Vector2((Screen.width-canvas[0]*fit)/2+rectangle[0]*fit,(Screen.height-canvas[1]*fit)/2+rectangle[1]*fit);
        label.maxWidth=rectangle[2]*fit; label.text=text; label.visible=visible;
    }
}

object "BSL Presentation Input" is "private"
{
    custom=Input("bsl_presentation");
    fallback=Input("default");
    state "main" {}
    fun accept() { return custom.buttonPressed("fire1") || custom.buttonPressed("fire3") || fallback.buttonPressed("fire1") || fallback.buttonPressed("fire3"); }
    fun cancel() { return custom.buttonPressed("fire2") || custom.buttonPressed("fire4") || fallback.buttonPressed("fire2") || fallback.buttonPressed("fire4"); }
    fun held() { return custom.buttonDown("fire1") || custom.buttonDown("fire2") || custom.buttonDown("fire3") || custom.buttonDown("fire4") || fallback.buttonDown("fire1") || fallback.buttonDown("fire2") || fallback.buttonDown("fire3") || fallback.buttonDown("fire4"); }
    fun previous() { return custom.buttonPressed("up") || fallback.buttonPressed("up"); }
    fun next() { return custom.buttonPressed("down") || fallback.buttonPressed("down"); }
}

object "BSL C2 Intro" is "setup", "entity", "detached", "awake", "private"
{
    layout=spawn("BSL Presentation Layout").data;
    cfg=layout["intro"];
    input=spawn("BSL Presentation Input");
    startSound=Sound(layout["audio"]["start"]["path"]);
    finishSound=Sound(layout["audio"]["finish"]["path"]);
    logo=null;
    mid=null;
    finish=null;
    age=0;
    finishAge=0;
    gate=true;
    phase="start";
    state "main"
    {
        age+=Time.delta;
        if(gate && !input.held()) gate=false;
        if(!gate && age>=cfg["skip_after"] && (input.accept() || input.cancel())) {
            startSound.stop(); finishSound.stop(); Level.next=cfg["title_sequence_index"]; Level.loadNext(); state="out";
        }
        else if(phase=="start") {
            logo.alpha=Math.min(1,age/cfg["fade_in"]);
            logo.frame=frameAt(layout["assets"]["logo_start"]["durations"],age);
            if(age>=duration(layout["assets"]["logo_start"]["durations"])) {
                logo.alpha=0; mid.alpha=1; phase="mid";
            }
        }
        else if(phase=="mid") {
            if((!startSound.playing && age>=cfg["minimum_mid"]) || age>=cfg["audio_watchdog"]) {
                mid.alpha=0; finish.alpha=1; finishSound.play(); phase="finish"; finishAge=0;
            }
        }
        else if(phase=="finish") {
            finishAge+=Time.delta;
            finish.frame=frameAt(layout["assets"]["logo_finish"]["durations"],finishAge);
            finish.alpha=Math.max(0,1-finishAge/cfg["finish_fade"]);
            if(finishAge>=duration(layout["assets"]["logo_finish"]["durations"])+cfg["after_finish"]) {
                Level.next=cfg["title_sequence_index"]; Level.loadNext(); state="out";
            }
        }
    }
    state "out" {}
    fun constructor()
    {
        Console.print("BSL presentation: intro");
        logo=spawn("BSL Presentation Sprite").initialize(layout["assets"]["logo_start"],cfg["logo_rect"],cfg["canvas"],0.5);
        mid=spawn("BSL Presentation Sprite").initialize(layout["assets"]["logo_mid"],cfg["logo_rect"],cfg["canvas"],0.5);
        finish=spawn("BSL Presentation Sprite").initialize(layout["assets"]["logo_finish"],cfg["logo_rect"],cfg["canvas"],0.5);
        logo.alpha=0; mid.alpha=0; finish.alpha=0; startSound.play();
    }
    fun destructor() { startSound.stop(); finishSound.stop(); }
    fun duration(values) { sum=0; for(i=0;i<values.length;i++) sum+=values[i]; return sum; }
    fun frameAt(values,t) { sum=0; for(i=0;i<values.length;i++) { sum+=values[i]; if(t<sum) return i; } return values.length-1; }
}

object "BSL C2 Title" is "setup", "entity", "detached", "awake", "private"
{
    layout=spawn("BSL Presentation Layout").data;
    cfg=layout["title"];
    input=spawn("BSL Presentation Input");
    background=null;
    shuriken=null;
    sword=null;
    wordmark=null;
    portrait=null;
    prompt=null;
    age=0;
    arrived=-1;
    spin=0;
    speed=cfg["shuriken_rotation_speed"];
    gate=true;
    state "main"
    {
        age+=Time.delta;
        background.alpha=Math.min(1,age/cfg["background_fade"]);
        portrait.alpha=Math.min(1,age/cfg["portrait_fade"]);
        portrait.x=Math.max(cfg["portrait_target_x"],portrait.x*Math.pow(cfg["portrait_decay_per_source_tick"],Time.delta*cfg["source_tick_rate"]));
        if(background.alpha>0.5) shuriken.x=Math.min(cfg["shuriken_target_x"],shuriken.x+cfg["shuriken_speed"]*Time.delta);
        if(shuriken.x>=cfg["shuriken_target_x"]) {
            if(arrived<0) arrived=age;
            speed=Math.max(0,speed-cfg["shuriken_deceleration"]*Time.delta);
            if(age-arrived>=cfg["sword_delay"]) {
                sword.x=Math.min(cfg["sword_target_x"],sword.x+cfg["sword_speed"]*Time.delta);
                wordmark.alpha=Math.min(1,(age-arrived-cfg["sword_delay"])/cfg["title_fade"]);
                speed=0;
            }
        }
        spin+=speed*Time.delta; shuriken.angle=spin;
        prompt.visible=wordmark.alpha>=1;
        if(gate && !input.held()) gate=false;
        if(!gate && input.accept()) { Level.next=cfg["menu_sequence_index"]; Level.loadNext(); state="out"; }
    }
    state "out" {}
    fun constructor()
    {
        Console.print("BSL presentation: title");
        background=spawn("BSL Presentation Sprite").initialize(layout["assets"]["background"],cfg["background_rect"],cfg["canvas"],0);
        shuriken=spawn("BSL Presentation Sprite").initialize(layout["assets"]["shuriken"],cfg["shuriken_rect"],cfg["canvas"],0.3);
        sword=spawn("BSL Presentation Sprite").initialize(layout["assets"]["sword"],cfg["sword_rect"],cfg["canvas"],0.4);
        wordmark=spawn("BSL Presentation Sprite").initialize(layout["assets"]["wordmark"],cfg["title_rect"],cfg["canvas"],0.5);
        portrait=spawn("BSL Presentation Sprite").initialize(layout["assets"]["portrait"],cfg["portrait_rect"],cfg["canvas"],0.1);
        prompt=spawn("BSL Presentation Text").initialize(layout["font"],"hint",cfg["prompt_rect"],cfg["canvas"],"<color="+layout["menu"]["color"]+">"+cfg["prompt"]+"</color>");
        background.alpha=0; portrait.alpha=0; wordmark.alpha=0; prompt.visible=false;
    }
}

object "BSL C2 Main Menu" is "setup", "entity", "detached", "awake", "private"
{
    layout=spawn("BSL Presentation Layout").data;
    cfg=layout["menu"];
    input=spawn("BSL Presentation Input");
    actions=spawn("BSL Presentation Actions");
    moveSound=Sound(layout["audio"]["move"]["path"]);
    acceptSound=Sound(layout["audio"]["accept"]["path"]);
    backSound=Sound(layout["audio"]["back"]["path"]);
    labels=[];
    hint=null;
    selected=0;
    gate=true;
    state "main"
    {
        if(gate) { if(!input.held()) gate=false; }
        else if(input.previous()) { selected=(selected+labels.length-1)%labels.length; moveSound.play(); refresh(); }
        else if(input.next()) { selected=(selected+1)%labels.length; moveSound.play(); refresh(); }
        else if(input.cancel()) { backSound.play(); Level.next=cfg["title_sequence_index"]; Level.loadNext(); state="out"; }
        else if(input.accept()) activate();
    }
    state "out" {}
    fun constructor()
    {
        Console.print("BSL presentation: main menu");
        spawn("BSL Presentation Sprite").initialize(layout["assets"]["background"],cfg["background_rect"],cfg["canvas"],0);
        spawn("BSL Presentation Sprite").initialize(layout["assets"]["portrait"],cfg["portrait_rect"],cfg["canvas"],0.1);
        spawn("BSL Presentation Sprite").initialize(layout["assets"]["shuriken"],cfg["shuriken_rect"],cfg["canvas"],0.3);
        spawn("BSL Presentation Sprite").initialize(layout["assets"]["sword"],cfg["sword_rect"],cfg["canvas"],0.4);
        spawn("BSL Presentation Sprite").initialize(layout["assets"]["wordmark"],cfg["title_rect"],cfg["canvas"],0.5);
        for(i=0;i<cfg["options"].length;i++) {
            rect=[cfg["first_option"][0],cfg["first_option"][1]+i*cfg["spacing"],cfg["label_width"],cfg["spacing"]];
            labels.push(spawn("BSL Presentation Text").initialize(layout["font"],"menu",rect,cfg["canvas"],""));
        }
        hint=spawn("BSL Presentation Text").initialize(layout["font"],"hint",cfg["hint_rect"],cfg["canvas"],"");
        refresh();
    }
    fun refresh()
    {
        for(i=0;i<labels.length;i++) {
            option=cfg["options"][i]; color=option["enabled"] ? (i==selected ? cfg["selected_color"] : cfg["color"]) : cfg["disabled_color"];
            labels[i].text="<color="+color+">"+(i==selected ? cfg["cursor"] : cfg["no_cursor"])+option["label"]+"</color>";
        }
        option=cfg["options"][selected];
        hint.text="<color="+cfg["color"]+">"+(option["enabled"] ? cfg["hint"] : option["reason"])+"</color>";
    }
    fun activate()
    {
        option=cfg["options"][selected];
        Console.print("BSL presentation action: "+option["action"]+" "+option["target"]);
        if(!option["enabled"]) { backSound.play(); return; }
        if(option["action"]=="new_game") {
            acceptSound.play(); Level.load(option["target"]); state="out";
        }
        else if(option["action"]=="load") { acceptSound.play(); Level.load(option["target"]); state="out"; }
        else if(option["action"]=="quit") { Level.abort(); state="out"; }
    }
}
