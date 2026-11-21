/* Real native helper/sequence proof, opt-in on disposable levels only. GPL-3.0-or-later. Signed: Atlas. */
static void bsl_sequence_test_set(surgescript_object_t* object,const char* name,bool boolean,double number)
{
    surgescript_var_t* v=surgescript_var_create();if(boolean)surgescript_var_set_bool(v,number!=0);else surgescript_var_set_number(v,number);
    const surgescript_var_t* args[]={v};char setter[64];snprintf(setter,sizeof setter,"set_%s",name);
    surgescript_object_call_function(object,setter,args,1,NULL);surgescript_var_destroy(v);
}
static bool bsl_sequence_test_health(uint64_t actor,int health,double x,double y)
{
    BslActorRewind state={actor,health,x,y};BslRewindPlan* plan=bsl_bor_rewind_prepare(&state,1,actor);
    if(!plan)return false;
    bsl_bor_rewind_apply(plan,&state,1,actor,x,y,1);bsl_bor_rewind_discard(plan);return true;
}
static bool bsl_sequence_exit_test_poll(void)
{
    const char* mode=getenv("BSL_SEQUENCE_EXIT_TEST");if(!mode)return false;
    static int stage,frames,total,checks,failures,expected_items;static uint64_t first_actor,xp;
    static const char* phase="preparing";
    if(++frames<30&&!jump_to_next_stage)return false;
    BslGameplayProbe player={0};bsl_host_gameplay_probe(&player);BslSequenceExitView view;
    surgescript_object_t* object=level_get_entity_by_id("e04e01");
#define SX_CHECK(expr,message) do{bool okay=(expr);checks++;if(!okay)failures++;logfile_message("SEQUENCE_EXIT %s: %s",okay?"PASS":"FAIL",message);if(!okay)logfile_message("SEQUENCE_EXIT stage=%d cursor=%d file=%s actor=%llu hp=%d items=%d jump=%d",stage,quest_next_level(),file,(unsigned long long)player.actor,player.health,player.items,jump_to_next_stage);}while(0)
    if(++total>3200){SX_CHECK(false,"native proof timeout");stage=99;}
    if(stage==0){
        if(!player.actor||!bsl_bor_sequence_exit_ready(player.actor))return false;
        SX_CHECK(object&&bsl_sequence_exit_view(object,&view)&&view.width==96&&view.require_action&&view.enabled,"local helper loads without a database definition");
        if(!object){stage=99;goto report;}
        SX_CHECK(bsl_event_find(0xe04e01)<0,"local exit has no forced common-event binding");
        editor_enable();editor_selected_entity_type=EDT_INSPECT;bsl_inspect_id=0xe04e01;
        int width=bsl_instance_field("BSL Sequence Exit","width");bsl_inspect_change(0xe04e01,width,true,128);
        bsl_sequence_test_set(object,"width",false,160);bsl_inspect_change(0xe04e01,width,true,256);editor_action_undo();editor_action_redo();
        SX_CHECK(bsl_sequence_exit_view(object,&view)&&view.width==160,"authored edits/undo preserve runtime overrides");
        bsl_sequence_test_set(object,"width",false,-1);SX_CHECK(!bsl_sequence_exit_view(object,&view),"invalid runtime geometry is refused");
        surgescript_object_call_function(object,"resetInstanceParameters",NULL,0,NULL);
        SX_CHECK(bsl_sequence_exit_view(object,&view)&&view.width==256,"explicit runtime reset reveals latest authored width");
        bsl_inspect_change(0xe04e01,bsl_instance_field("BSL Sequence Exit","requireAction"),true,0);
        editor_cursor.world_position=v2d_new(view.x-view.origin_x+1,view.y-view.origin_y+1);surgescript_object_t* picked=NULL;bsl_inspect_pick(object,&picked);
        SX_CHECK(picked==object,"Inspect selects local exit bounds outside its icon");
        SX_CHECK(level_save(file),"local settings save in the level with stable identity");editor_disable();
        first_actor=player.actor;expected_items=player.items;
        if(strcmp(mode,"journey")){
            SX_CHECK(bsl_sequence_exit_context()!=NULL,"standalone or missing-next context is explicitly unavailable");
            bsl_level_sequence_exit_tick();
            SX_CHECK(bsl_sequence_exits[0].primed&&!bsl_sequence_exits[0].inside,"contact is primed outside before testing an unavailable entry");
            scripting_util_set_world_position(object,v2d_new(160,10016));stage=10;frames=0;return false;
        }
        SX_CHECK(bsl_sequence_exit_context()==NULL&&quest_next_level()==1,"authored sequence starts at its actual cursor");
        bool prepared=bsl_host_prepare_sequence_exit(player.actor)==0;
        SX_CHECK(prepared,"prepare an input-blocked context on the same actor");
        if(!prepared){stage=99;goto report;}
        bsl_level_sequence_exit_tick();SX_CHECK(bsl_sequence_test_health(player.actor,72,700,9953),"restore the same actor inside the contact region");
        stage=1;frames=0;return false;
    }
    if(stage==10){
        bsl_level_sequence_exit_tick();
        SX_CHECK(!jump_to_next_stage&&bsl_host_interactor(&view.x,&view.y)==player.actor,"unavailable sequence does not advance or capture player input");
        SX_CHECK(!strcmp(mode,"standalone")||strstr(bsl_sequence_exit_context(),"unavailable")!=NULL,"missing next asset is refused before dispatch");stage=99;goto report;
    }
    if(stage==1){
        SX_CHECK(!jump_to_next_stage&&player.actor==first_actor&&player.health==72,"same-lifetime recovery into contact primes without advancing");
        SX_CHECK(bsl_bor_teleport(player.actor,160,9953,0,0,1),"leave the contact region");
        SX_CHECK(bsl_host_anchor_start(0xe040ff,"event:training_reward",player.actor)==0,"commit an existing authored inventory/switch event before traversal");
        stage=2;frames=0;return false;
    }
    if(stage==2){
        SX_CHECK(player.items==expected_items+1&&bsl_host_progress_flag("switch:training_reward"),"authored progress is committed before exit");expected_items=player.items;
        SX_CHECK(bsl_sequence_test_health(player.actor,72,700,9953),"rewind directly into contact without an interactor-loss frame");
        SX_CHECK(!bsl_sequence_exits[0].primed&&!bsl_sequence_exits[0].pending,"official reconstruction callback explicitly re-primes exits");
        bsl_level_sequence_exit_tick();SX_CHECK(!jump_to_next_stage,"continuously eligible recovered actor does not fabricate contact entry");
        SX_CHECK(bsl_bor_teleport(player.actor,160,9953,0,0,1),"leave direct recovery region before gameplay request");stage=21;frames=0;return false;
    }
    if(stage==21){
        if(!bsl_bor_sequence_exit_ready(player.actor)&&frames<180)return false;
        SX_CHECK(bsl_bor_ability(player.actor,"attack1","armed"),"begin an actual committed BOR attack");
        scripting_util_set_world_position(object,v2d_new(160,10016));bsl_level_sequence_exit_tick();
        SX_CHECK(!jump_to_next_stage&&!bsl_bor_sequence_exit_ready(player.actor),"contact exit waits for committed attack recovery");
        editor_enable();editor_disable();
        SX_CHECK(!bsl_sequence_exits[0].pending&&!bsl_sequence_exits[0].primed,"edit/play clears an unfinished gesture and re-primes contact");
        stage=3;frames=0;return false;
    }
    if(stage==3){
        if(!bsl_bor_sequence_exit_ready(player.actor)&&frames<180)return false;
        SX_CHECK(!jump_to_next_stage,"overlapping edit resume does not fabricate a contact exit");
        scripting_util_set_world_position(object,v2d_new(700,10016));bsl_level_sequence_exit_tick();
        SX_CHECK(bsl_bor_ability(player.actor,"attack1","armed"),"begin committed attack for an uncancelled exit request");
        scripting_util_set_world_position(object,v2d_new(160,10016));bsl_level_sequence_exit_tick();
        SX_CHECK(!jump_to_next_stage&&bsl_sequence_exits[0].pending==player.actor,"entry queues one actor-owned exit until commitment finishes");
        stage=4;phase="waiting_commitment";frames=30;
    }
    if(stage==4&&jump_to_next_stage){
        BslStatProbe stat={0};bsl_host_stat_probe("stat:armed",&stat);xp=stat.total;
        SX_CHECK(quest_next_level()==1&&bsl_sequence_exits[0].fired,"ready exit dispatches once at the first cursor");stage=5;phase="await_repeat";frames=0;
    }
    if(stage==5&&quest_next_level()==2){
        SX_CHECK(!strcmp(file,"levels/checkpoint-services.lev")&&bsl_sequence_exit_count==1&&!bsl_sequence_exits[0].fired,"repeated scene gets a fresh helper lifetime at cursor two");
        BslStatProbe stat={0};bsl_host_stat_probe("stat:armed",&stat);
        SX_CHECK(player.actor!=first_actor&&player.health==72&&player.items==expected_items&&stat.total==xp&&bsl_host_progress_flag("switch:training_reward"),"character health/inventory/stats/switch survive real sequence unload/reload");
        SX_CHECK(bsl_sequence_exit_view(object,&view)&&view.width==256&&!view.require_action,"saved local parameters reconstruct in repeated member");
        bsl_sequence_test_set(object,"requireAction",true,1);SX_CHECK(bsl_bor_teleport(player.actor,700,9953,0,0,1),"enter action exit in repeated member");
        stage=6;phase="await_action";frames=0;return false;
    }
    if(stage==6&&jump_to_next_stage){
        SX_CHECK(quest_next_level()==2,"fresh Up input dispatches the second repeated entry");stage=7;phase="await_final_member";frames=0;
    }
    if(stage==7&&quest_next_level()==3){
        SX_CHECK(!strcmp(file,"levels/teleport-annex.lev")&&!jump_to_next_stage,"held Up does not skip the final member at its overlapping spawn");
        SX_CHECK(player.health==72&&player.items==expected_items,"final member keeps the same player progress");
        stage=8;phase="release_action";frames=0;return false;
    }
    if(stage==8&&!bsl_bor_sequence_exit_action_held()&&bsl_bor_sequence_exit_ready(player.actor)&&bsl_sequence_exits[0].primed&&bsl_sequence_exits[0].armed){
        SX_CHECK(!jump_to_next_stage,"releasing held input does not activate the overlapping exit");stage=9;phase="fresh_action";frames=0;return false;
    }
    if(stage==9&&jump_to_next_stage){
        SX_CHECK(quest_next_level()==3&&quest_entry_count(quest_current())==3,"final explicit action queues native sequence completion");stage=11;phase="final_queued";
    }
report:
    {const char* path=getenv("BSL_SEQUENCE_EXIT_REPORT");FILE* report=path?fopen(path,"w"):NULL;
    if(report){fprintf(report,"{\"ok\":%s,\"checks\":%d,\"failures\":%d,\"phase\":\"%s\",\"cursor\":%d,\"health\":%d,\"items\":%d}\n",failures?"false":"true",checks,failures,stage==99?"complete":phase,quest_next_level(),player.health,player.items);fclose(report);}}
    if(stage==99){engine_quit();return true;}
#undef SX_CHECK
    return false;
}
