// BOR-style selection over pure catalog metadata and cosmetic previews. Signed: Mech.
using SurgeEngine.Actor;
using SurgeEngine.Transform;
using SurgeEngine.Vector2;
using SurgeEngine.Input;
using SurgeEngine.Level;
using SurgeEngine.Video.Screen;
using SurgeEngine.Audio.Sound;

object "BSL Character Select Solo" is "setup", "private"
{
    selector=spawn("BSL Character Selector");
    state "main" {}
    fun constructor() { selector.initialize("solo"); }
}
object "BSL Character Select Lobby" is "setup", "private"
{
    selector=spawn("BSL Character Selector");
    state "main" {}
    fun constructor() { selector.initialize("lobby"); }
}

object "BSL Selector Device" is "private"
{
    primary=null;
    alternate=null;
    gate=true;
    state "main" { if(primary!=null && gate && !held()) gate=false; }
    fun initialize(code)
    {
        primary=Input(code==0 ? "bsl_select_keyboard" : "bsl_select_pad"+code);
        if(code==0) alternate=Input("bsl_select_arrows");
        return this;
    }
    fun held()
    {
        return primary.buttonDown("fire1") || primary.buttonDown("fire2") || primary.buttonDown("fire3") || primary.buttonDown("fire4") || primary.buttonDown("left") || primary.buttonDown("right") || (alternate!=null && (alternate.buttonDown("fire1") || alternate.buttonDown("fire2") || alternate.buttonDown("fire3") || alternate.buttonDown("fire4") || alternate.buttonDown("left") || alternate.buttonDown("right")));
    }
    fun pressed(button) { return !gate && (primary.buttonPressed(button) || (alternate!=null && alternate.buttonPressed(button))); }
    fun confirm() { return pressed("fire1") || pressed("fire3"); }
    fun cancel() { return pressed("fire2") || pressed("fire4"); }
    fun left() { return pressed("left"); }
    fun right() { return pressed("right"); }
}

object "BSL Selector Slot" is "entity", "detached", "awake", "private"
{
    transform=Transform();
    actions=spawn("BSL Presentation Actions");
    layout=spawn("BSL Presentation Layout").data;
    cfg=layout["selector"];
    shape=null;
    actor=null;
    currentModel=null;
    age=0;
    phase="select";
    slotIndex=0;
    public device=-1;
    public definition=null;
    public ready=false;
    public choice=0;
    nameLabel=null;
    deviceLabel=null;
    statusLabel=null;
    state "main"
    {
        if(shape!=null) {
            if(actor!=null && definition!=null) {
                age+=Time.delta;
                fit=Math.min(Screen.width/cfg["canvas"][0],Screen.height/cfg["canvas"][1]);
                transform.position=Vector2((Screen.width-cfg["canvas"][0]*fit)/2+shape["preview"][0]*fit,(Screen.height-cfg["canvas"][1]*fit)/2+shape["preview"][1]*fit);
                transform.localScale=Vector2(shape["preview_scale"]*fit,shape["preview_scale"]*fit);
                actor.visible=actions.preview(actor,definition,age,phase);
            }
            refreshText();
        }
    }
    fun initialize(spec,index)
    {
        shape=spec;slotIndex=index;
        nameLabel=spawn("BSL Presentation Text").initialize(layout["font"],cfg["font_roles"]["name"],shape["name_rect"],cfg["canvas"],"");
        deviceLabel=spawn("BSL Presentation Text").initialize(layout["font"],cfg["font_roles"]["device"],shape["device_rect"],cfg["canvas"],"");
        statusLabel=spawn("BSL Presentation Text").initialize(layout["font"],cfg["font_roles"]["status"],shape["status_rect"],cfg["canvas"],"");
        refreshText();return this;
    }
    fun assign(id,source,index)
    {
        device=source;choice=index;ready=false;
        showDefinition(id);return this;
    }
    fun showDefinition(id)
    {
        definition=id;ready=false;phase="select";age=0;
        model=actions.characterModel(id);
        if(model!=currentModel) {
            if(actor!=null) actor.destroy();
            actor=model==null ? null : Actor("BOR "+model);
            if(actor!=null) actor.zindex=0.5;
            currentModel=model;
        }
        if(actor!=null) actor.visible=actions.preview(actor,definition,0,phase);
        refreshText();
    }
    fun setReady(value) { ready=value;phase=ready ? "pick" : "select";age=0;refreshText(); }
    fun leave()
    {
        device=-1;definition=null;ready=false;age=0;
        if(actor!=null) actor.visible=false;
        refreshText();
    }
    fun refreshText()
    {
        nameLabel.text="<color="+cfg["color"]+">"+(definition==null ? "Player "+(slotIndex+1) : actions.characterName(definition))+"</color>";
        deviceLabel.text="<color="+cfg["disabled_color"]+">"+(device<0 ? "" : cfg["device_names"][device])+"</color>";
        value=definition==null ? cfg["join_text"] : (ready ? cfg["ready_text"] : cfg["choose_text"]);
        statusLabel.text="<color="+(ready ? cfg["ready_color"] : cfg["color"])+">"+value+"</color>";
    }
}

// Real input and trusted private probes call the same dispatch methods below.
object "BSL Character Selector" is "entity", "detached", "awake", "private"
{
    layout=spawn("BSL Presentation Layout").data;
    cfg=layout["selector"];
    actions=spawn("BSL Presentation Actions");
    moveSound=Sound(layout["audio"]["move"]["path"]);
    acceptSound=Sound(layout["audio"]["accept"]["path"]);
    backSound=Sound(layout["audio"]["back"]["path"]);
    devices=[];
    slots=[];
    catalog=[];
    mode="";
    message="";
    messageLabel=null;
    state "waiting" {}
    state "main"
    {
        for(d=0;d<devices.length && state=="main";d++) {
            input=devices[d];
            if(input.cancel()) deviceCancel(d);
            else if(input.confirm()) deviceConfirm(d);
            else if(input.left()) deviceMove(d,-1);
            else if(input.right()) deviceMove(d,1);
        }
        refreshMessage();
    }
    state "out" {}
    fun initialize(kind)
    {
        mode=kind;
        spawn("BSL Presentation Sprite").initialize(layout["assets"]["selector_background"],cfg["background_rect"],cfg["canvas"],0);
        title=mode=="solo" ? cfg["solo_title"] : cfg["lobby_title"];
        spawn("BSL Presentation Text").initialize(layout["font"],cfg["font_roles"]["heading"],cfg["heading_rect"],cfg["canvas"],"<color="+cfg["color"]+">"+title+"</color>");
        spawn("BSL Presentation Text").initialize(layout["font"],cfg["font_roles"]["help"],cfg["help_rect"],cfg["canvas"],"<color="+cfg["color"]+">"+cfg["help"]+"</color>");
        messageLabel=spawn("BSL Presentation Text").initialize(layout["font"],cfg["font_roles"]["message"],cfg["message_rect"],cfg["canvas"],"");
        for(i=0;i<actions.characterCount();i++) {
            id=actions.characterAt(i);
            if(id!=null) catalog.push(id);
        }
        count=mode=="solo" ? 1 : cfg["max_players"];
        for(i=0;i<count;i++) slots.push(spawn("BSL Selector Slot").initialize(mode=="solo" ? cfg["solo_slot"] : cfg["lobby_slots"][i],i));
        for(i=0;i<cfg["device_names"].length;i++) devices.push(spawn("BSL Selector Device").initialize(i));
        message=catalog.length==0 ? cfg["empty_text"] : (mode=="solo" ? cfg["solo_help"] : cfg["lobby_help"]);
        state="main";refreshMessage();
        Console.print("BSL selector: ready mode="+mode+" characters="+catalog.length);
        return this;
    }
    fun deviceSlot(device)
    {
        for(i=0;i<slots.length;i++) if(slots[i].device==device) return i;
        return -1;
    }
    fun validDevice(device) { return device>=0 && device<devices.length && Math.floor(device)==device; }
    fun participantCount() { count=0;for(i=0;i<slots.length;i++) if(slots[i].device>=0) count++;return count; }
    fun readyCount() { count=0;for(i=0;i<slots.length;i++) if(slots[i].device>=0 && slots[i].ready) count++;return count; }
    fun selection(device) { index=deviceSlot(device);return index<0 ? null : slots[index].definition; }
    fun isReady(device) { index=deviceSlot(device);return index>=0 && slots[index].ready; }
    fun currentMessage() { return message; }
    fun modeName() { return mode; }
    fun hostSlot() { for(i=0;i<slots.length;i++) if(slots[i].device>=0) return i;return -1; }
    fun refreshMessage() { if(messageLabel!=null) messageLabel.text="<color="+cfg["color"]+">"+message+"</color>"; }
    fun deviceConfirm(device)
    {
        if(state!="main" || !validDevice(device) || catalog.length==0) return false;
        index=deviceSlot(device);
        if(index<0) {
            for(i=0;i<slots.length;i++) if(slots[i].device<0) {
                slots[i].assign(catalog[0],device,0);message=cfg["host_text"];acceptSound.play();
                Console.print("BSL selector join: device="+device+" slot="+i);refreshMessage();return true;
            }
            message=cfg["full_text"];refreshMessage();return false;
        }
        if(!slots[index].ready) {
            slots[index].setReady(true);message=cfg["host_text"];acceptSound.play();
            Console.print("BSL selector ready: device="+device+" character="+slots[index].definition);refreshMessage();return true;
        }
        if(index!=hostSlot()) { message=cfg["host_text"];refreshMessage();return false; }
        if(readyCount()!=participantCount()) { message=cfg["waiting_text"];refreshMessage();return false; }
        id=slots[index].definition;
        if(!actions.beginSelectedGame(id,participantCount(),device)) {
            message=actions.lastError;backSound.play();
            Console.print("BSL selector refused: "+message);refreshMessage();return false;
        }
        acceptSound.play();Console.print("BSL selector launch: "+id+" device="+device+" mode="+mode);
        if(mode=="solo") Level.loadNext();
        else Level.load(cfg["direct_course"]);
        state="out";return true;
    }
    fun deviceCancel(device)
    {
        if(state!="main" || !validDevice(device)) return false;
        index=deviceSlot(device);
        if(index>=0) {
            if(slots[index].ready) { slots[index].setReady(false);Console.print("BSL selector unready: device="+device); }
            else { slots[index].leave();Console.print("BSL selector leave: device="+device); }
            message=participantCount()==0 ? (mode=="solo" ? cfg["solo_help"] : cfg["lobby_help"]) : cfg["host_text"];
            backSound.play();refreshMessage();return true;
        }
        if(participantCount()==0) { Console.print("BSL selector: back");Level.abort();state="out";return true; }
        return false;
    }
    fun deviceMove(device,direction)
    {
        if(state!="main" || !validDevice(device) || (direction!=-1 && direction!=1) || catalog.length==0) return false;
        index=deviceSlot(device);
        if(index<0 || slots[index].ready) return false;
        slot=slots[index];slot.choice=(slot.choice+direction+catalog.length)%catalog.length;
        slot.showDefinition(catalog[slot.choice]);moveSound.play();
        Console.print("BSL selector choose: device="+device+" character="+slot.definition);return true;
    }
}
