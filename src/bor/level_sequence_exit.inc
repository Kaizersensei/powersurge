/* Local placed helper: native sequence traversal, no RPG record. GPL-3.0-or-later. Signed: Atlas. */
#include "../core/quest.h"
#define BSL_SEQUENCE_EXIT_LIMIT 4096
static struct {
    uint64_t id,actor,pending;
    bool primed,inside,armed,fired;
} bsl_sequence_exits[BSL_SEQUENCE_EXIT_LIMIT];
static int bsl_sequence_exit_count;

static void bsl_sequence_exit_reset(void) { bsl_sequence_exit_count=0; }
static void bsl_sequence_exit_suspend(void)
{
    for(int i=0;i<bsl_sequence_exit_count;i++) {
        bsl_sequence_exits[i].pending=0;bsl_sequence_exits[i].primed=false;bsl_sequence_exits[i].armed=false;
    }
}
void bsl_level_sequence_exit_reprime(void) { bsl_sequence_exit_suspend(); }
static void bsl_sequence_exit_note(surgescript_object_t* object)
{
    if(strcmp(surgescript_object_name(object),"BSL Sequence Exit")||!entity_info_exists(object)||!entity_info_is_persistent(object))return;
    uint64_t id=entity_info_id(object);if(!id)return;
    int at=0;while(at<bsl_sequence_exit_count&&bsl_sequence_exits[at].id<id)at++;
    if(at<bsl_sequence_exit_count&&bsl_sequence_exits[at].id==id)return;
    if(bsl_sequence_exit_count==BSL_SEQUENCE_EXIT_LIMIT){logfile_message("Sequence exit helper capacity reached; placement remains authored but inactive");return;}
    memmove(&bsl_sequence_exits[at+1],&bsl_sequence_exits[at],(bsl_sequence_exit_count-at)*sizeof bsl_sequence_exits[0]);
    memset(&bsl_sequence_exits[at],0,sizeof bsl_sequence_exits[at]);bsl_sequence_exits[at].id=id;bsl_sequence_exit_count++;
}
static bool bsl_sequence_exit_view(surgescript_object_t* object,BslSequenceExitView* out)
{
    if(!object||!out||surgescript_object_is_killed(object)||strcmp(surgescript_object_name(object),"BSL Sequence Exit"))return false;
    static const char* getters[]={"get_width","get_height","get_originX","get_originY","get_requireAction","get_enabled"};
    double numbers[6];bool valid=true;surgescript_var_t* value=surgescript_var_create();
    for(int i=0;i<6&&valid;i++) {
        if(!surgescript_object_has_function(object,getters[i])){valid=false;break;}
        surgescript_object_call_function(object,getters[i],NULL,0,value);
        valid=i<4?surgescript_var_is_number(value):surgescript_var_is_bool(value);
        numbers[i]=i<4?surgescript_var_get_number(value):(surgescript_var_get_bool(value)?1:0);
        valid=valid&&isfinite(numbers[i]);
    }
    surgescript_var_destroy(value);if(!valid)return false;
    v2d_t at=scripting_util_world_position(object);
    if(!isfinite(at.x)||!isfinite(at.y)||fabs(at.x)>1e7||fabs(at.y)>1e7||numbers[0]<1||numbers[0]>1024||numbers[1]<1||numbers[1]>1024||fabs(numbers[2])>1e7||fabs(numbers[3])>1e7)return false;
    *out=(BslSequenceExitView){at.x,at.y,numbers[0],numbers[1],numbers[2],numbers[3],numbers[4]!=0,numbers[5]!=0};return true;
}
static const char* bsl_sequence_exit_context(void)
{
    const quest_t* sequence=quest_current();
    const char* path=sequence?quest_file(sequence):NULL;
    if(!path||strncmp(path,"quests/sequences/",17)||strstr(path,".."))return "No active authored sequence";
    int next=quest_next_level(),count=quest_entry_count(sequence);
    if(next<1||next>count||!quest_entry_is_level(sequence,next-1)||strcmp(quest_entry_path(sequence,next-1),file))return "Not the current sequence member";
    if(next<count&&(!quest_entry_is_level(sequence,next)||!asset_exists(quest_entry_path(sequence,next))))return "Next sequence level is unavailable";
    return NULL;
}
void bsl_level_sequence_exit_tick(void)
{
    if(editor_is_enabled()||jump_to_next_stage)return;
    double x=0,y=0;uint64_t actor=bsl_host_interactor(&x,&y);
    bool held=bsl_bor_sequence_exit_action_held(),fresh=bsl_bor_teleport_action();
    int candidate=-1;
    for(int i=0;i<bsl_sequence_exit_count;i++) {
        if(!held)bsl_sequence_exits[i].armed=true;
        if(!actor){bsl_sequence_exits[i].pending=0;bsl_sequence_exits[i].primed=false;bsl_sequence_exits[i].armed=false;continue;}
        if(bsl_sequence_exits[i].actor!=actor){bsl_sequence_exits[i].actor=actor;bsl_sequence_exits[i].pending=0;bsl_sequence_exits[i].primed=false;}
        surgescript_object_t* object=level_get_entity_by_id(x64_to_str(bsl_sequence_exits[i].id,NULL,0));BslSequenceExitView view;
        if(!bsl_sequence_exit_view(object,&view)){bsl_sequence_exits[i].pending=0;bsl_sequence_exits[i].primed=false;continue;}
        bool inside=x>=view.x-view.origin_x&&x<=view.x-view.origin_x+view.width&&y>=view.y-view.origin_y&&y<=view.y-view.origin_y+view.height;
        bool entered=bsl_sequence_exits[i].primed&&inside&&!bsl_sequence_exits[i].inside;
        bsl_sequence_exits[i].inside=inside;bsl_sequence_exits[i].primed=true;
        if(!inside||!view.enabled){bsl_sequence_exits[i].pending=0;continue;}
        if(bsl_sequence_exits[i].fired)continue;
        if((view.require_action&&fresh&&bsl_sequence_exits[i].armed)||(!view.require_action&&entered)) {
            bsl_sequence_exits[i].pending=actor;bsl_sequence_exits[i].armed=false;
            const char* why=bsl_sequence_exit_context();
            if(why){video_showmessage("Sequence exit: %s",why);bsl_sequence_exits[i].pending=0;}
        }
        if(bsl_sequence_exits[i].pending==actor&&bsl_bor_sequence_exit_ready(actor)&&candidate<0)candidate=i;
    }
    if(candidate<0)return;
    if(bsl_sequence_exit_context()){bsl_sequence_exits[candidate].pending=0;return;}
    if(bsl_host_prepare_sequence_exit(actor)!=0)return;
    bsl_sequence_exits[candidate].fired=true;
    logfile_message("Sequence exit %s advances %s at cursor %d",x64_to_str(bsl_sequence_exits[candidate].id,NULL,0),file,quest_next_level());
    for(int i=0;i<bsl_sequence_exit_count;i++)bsl_sequence_exits[i].pending=0;
    level_jump_to_next_stage();
}
