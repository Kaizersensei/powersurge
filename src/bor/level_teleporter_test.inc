/* Opt-in proof using the real VM, inspector, native travel and level lifecycle. GPL-3.0-or-later. Signed: Atlas. */
static void bsl_teleporter_test_set(surgescript_object_t* object,const char* key,bool boolean,double number)
{
    surgescript_var_t* value=surgescript_var_create();
    if(boolean) surgescript_var_set_bool(value,number!=0); else surgescript_var_set_number(value,number);
    const surgescript_var_t* args[]={value};char setter[64];snprintf(setter,sizeof setter,"set_%s",key);
    surgescript_object_call_function(object,setter,args,1,NULL);surgescript_var_destroy(value);
}
static bool bsl_teleporter_test_poll(void)
{
    const char* mode=getenv("BSL_TELEPORT_INSTANCE_TEST");if(!mode)return false;
    static int stage,frames,checks,failures;static uint64_t first_actor;static int health,items;
    if(++frames<30)return false;
    if(!strcmp(mode,"ui")){
        static bool ready;
        if(!ready){ready=true;editor_enable();editor_selected_entity_type=EDT_INSPECT;bsl_inspect_id=0x7e1e0001;bsl_inspect_page=0;editor_camera=v2d_new(960,9952);}
        BslDoorLive live={0};bool valid=bsl_bor_teleport_live(0x7e1e0001,&live);
        const char* path=getenv("BSL_TELEPORT_INSTANCE_REPORT");FILE* report=path?fopen(path,"w"):NULL;
        if(report){fprintf(report,"{\"valid\":%s,\"width\":%.3f,\"require_action\":%s,\"enabled\":%s,\"page\":%d,\"size\":[%d,%d]}\n",valid?"true":"false",live.width,live.require_action?"true":"false",live.enabled?"true":"false",bsl_inspect_page,VIDEO_SCREEN_W,VIDEO_SCREEN_H);fclose(report);}
        return false;
    }
    BslGameplayProbe player={0};BslTeleportProbe travel={0};bsl_host_gameplay_probe(&player);bsl_host_teleport_probe(&travel);
#define TI_CHECK(expr,message) do{bool ok=(expr);checks++;if(!ok)failures++;logfile_message("TELEPORT_INSTANCE %s: %s",ok?"PASS":"FAIL",message);}while(0)
    if(stage==0){
        surgescript_object_t* local=level_get_entity_by_id("7e1e0001");BslDoorLive live;
        TI_CHECK(local&&bsl_bor_teleport_live(0x7e1e0001,&live),"registered teleporter exposes a typed live snapshot");
        if(!local){stage=99;goto finish;}
        TI_CHECK(bsl_bor_teleport_live(0x7e1e0003,&live)&&live.preserve_velocity,"definition-only momentum policy loads without override lines");
        if(!strcmp(mode,"read")){
            TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.x==324&&live.width==96&&!live.require_action,"fresh process restores moved authored placement and properties");
            stage=99;goto finish;
        }
        TI_CHECK(player.actor&&travel.enabled&&!travel.busy,"live gameplay services are ready for the property/travel proof");
        first_actor=player.actor;health=player.health;items=player.items;
        editor_enable();editor_selected_entity_type=EDT_INSPECT;bsl_inspect_id=0x7e1e0001;
        int width=bsl_instance_field("BSL Teleporter","width");
        bsl_inspect_change(0x7e1e0001,width,true,80);editor_action_undo();
        TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.width==56,"undo returns to the compiled definition default");
        editor_action_redo();TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.width==80,"redo updates actual trigger width");
        bsl_teleporter_test_set(local,"width",false,160);bsl_inspect_change(0x7e1e0001,width,true,96);editor_action_undo();editor_action_redo();
        TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.width==160,"runtime override survives authored changes and undo/redo");
        bsl_teleporter_test_set(local,"width",false,-1);
        TI_CHECK(!bsl_bor_teleport_live(0x7e1e0001,&live),"invalid trusted-script geometry safely refuses a live snapshot");
        surgescript_object_call_function(local,"resetInstanceParameters",NULL,0,NULL);
        TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.width==96,"explicit object reset reveals the latest authored value");
        bsl_teleporter_test_set(local,"enabled",true,0);
        bsl_inspect_change(0x7e1e0001,bsl_instance_field("BSL Teleporter","enabled"),true,1);
        TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&!live.enabled,"authored enabled changes preserve an explicit runtime disable");
        surgescript_object_call_function(local,"resetInstanceParameters",NULL,0,NULL);
        bsl_inspect_change(0x7e1e0001,bsl_instance_field("BSL Teleporter","requireAction"),true,0);
        TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.enabled&&!live.require_action,"reset and authored activation reach the native trigger policy");
        editor_grid_size=16;editor_cursor.world_position=v2d_new(260,9952);editor_cursor.screen_position=v2d_new(260,300);
        bsl_inspect_drag_begin(local);editor_cursor.world_position.x+=64;editor_cursor.screen_position.x+=64;bsl_inspect_drag_update(false);
        TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.x==324,"committed dragging changes live door origin without registry rebuild");
        editor_cursor.world_position=v2d_new(live.x-live.origin_x+1,live.y-live.origin_y+1);surgescript_object_t* picked=NULL;bsl_inspect_pick(local,&picked);
        TI_CHECK(picked==local,"Inspect picks the trigger region outside its icon");
        TI_CHECK(level_save(file),"native save publishes authored fields with the same placement identity");
        editor_disable();TI_CHECK(bsl_bor_teleport(player.actor,324,9953,0,0,1),"place player inside moved contact trigger");stage=1;frames=0;return false;
    }
    if(stage==1){
        if(travel.busy&&frames<180)return false;
        TI_CHECK(travel.arrivals==1&&!travel.busy,"moved source and edited contact policy trigger real same-level travel");
        TI_CHECK(player.actor==first_actor&&player.health==health&&player.items==items,"local travel preserves actor ownership, health and inventory");
        TI_CHECK(bsl_bor_teleport(player.actor,200,9953,0,0,1),"reach conditional shared-route source");
        TI_CHECK(bsl_host_teleport_request(player.actor,"door:annex",true)!=0,"shared usability condition remains active");
        TI_CHECK(bsl_host_anchor_start(0xb51f04,"event:door_unlock",player.actor)==0,"existing shared event unlocks the route");stage=2;frames=0;return false;
    }
    if(stage==2){
        TI_CHECK(bsl_host_teleport_request(player.actor,"door:annex",true)==0,"request cross-level route after its condition succeeds");stage=3;frames=0;return false;
    }
    if(stage==3){
        if(travel.busy&&frames<180)return false;
        TI_CHECK(!strcmp(file,"levels/teleport-annex.lev")&&travel.arrivals==2&&!travel.busy,"cross-level travel reaches the expected destination once");
        if(strcmp(file,"levels/teleport-annex.lev")){stage=99;goto finish;}
        TI_CHECK(travel.target_x==500,"arrival offset resolves the moved target instead of its compiled origin");
        TI_CHECK(player.health==health&&player.items==items&&bsl_host_progress_variable("variable:arrivals")==1,"travel retains gameplay stats and runs its shared completion event once");
        BslDoorLive live;TI_CHECK(bsl_bor_teleport_live(0x7e1e0005,&live)&&!live.enabled&&!live.require_action,"disabled contact destination accepts arrival without immediate bounce-back");
        bsl_teleporter_test_set(level_get_entity_by_id("7e1e0005"),"enabled",true,1);stage=4;frames=0;return false;
    }
    if(stage==4){
        TI_CHECK(travel.arrivals==2&&!travel.busy,"enabling an overlapped arrival door does not fabricate a new contact entry");
        TI_CHECK(bsl_bor_teleport(player.actor,620,9953,0,0,1),"exit destination region before retriggering");stage=5;frames=0;return false;
    }
    if(stage==5){
        TI_CHECK(bsl_bor_teleport(player.actor,500,9953,0,0,1),"re-enter live destination region");stage=6;frames=0;return false;
    }
    if(stage==6){
        if(travel.busy&&frames<180)return false;
        TI_CHECK(!strcmp(file,"levels/checkpoint-services.lev")&&travel.arrivals==3,"leaving and re-entering enables contact return to the recorded departure");
        TI_CHECK(bsl_host_progress_variable("variable:arrivals")==2,"return completion remains a single shared event");
        BslDoorLive live;TI_CHECK(bsl_bor_teleport_live(0x7e1e0001,&live)&&live.x==324&&live.width==96&&!live.require_action,"scene reload restores saved placement overrides");stage=99;
    }
finish:
    if(stage==99){
        const char* path=getenv("BSL_TELEPORT_INSTANCE_REPORT");FILE* report=path?fopen(path,"w"):NULL;
        if(report){fprintf(report,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(report);}
        engine_quit();return true;
    }
#undef TI_CHECK
    return false;
}
