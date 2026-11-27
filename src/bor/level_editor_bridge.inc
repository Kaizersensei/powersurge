/* Integrated definition navigation; no subprocess or temporary request files. GPL-3.0-or-later. Signed: President. */
#include "../authoring/hub.h"
static char bsl_editor_status[512] = "Shared definitions open in the database; instance edits stay here.";
static void bsl_editor_poll(void) {}
static void bsl_editor_launch(surgescript_object_t* object,const char* definition)
{
    char placement[17]="";
    if(object)str_cpy(placement,x64_to_str(entity_info_id(object),NULL,0),sizeof placement);
    PsHubRequest request={1,asset_gamedir(),definition?definition:"",file,placement,object?surgescript_object_name(object):""};
    PsHubReply reply={0};ps_hub_request(&request,&reply);str_cpy(bsl_editor_status,reply.message,sizeof bsl_editor_status);
}
static bool bsl_editor_bridge_click(surgescript_object_t* object,int x,int y)
{
    if(y<294||y>=315)return false;
    if(x>=10&&x<155)bsl_editor_launch(object,"");
    else if(x>=165&&x<320&&bsl_event_is_anchor(object)){
        const char* id=bsl_event_definition(entity_info_id(object));
        if(*id)bsl_editor_launch(object,id);else str_cpy(bsl_editor_status,"Bind a common event before opening its definition.",sizeof bsl_editor_status);
    }
    return true;
}
static void bsl_editor_bridge_render(surgescript_object_t* object,int x)
{
    image_rect(x+10,294,x+154,314,color_rgb(122,156,181));bsl_inspect_text(x+15,298,"Open database");
    bool available=bsl_event_is_anchor(object)&&*bsl_event_definition(entity_info_id(object));
    image_rect(x+165,294,x+319,314,color_rgb(available?122:65,available?156:65,available?181:65));bsl_inspect_text(x+170,298,"Edit shared event");
    char first[48];snprintf(first,sizeof first,"%.46s",bsl_editor_status);bsl_inspect_text(x+10,322,first);
    if(strlen(bsl_editor_status)>46){snprintf(first,sizeof first,"%.46s",bsl_editor_status+46);bsl_inspect_text(x+10,337,first);}
    bsl_inspect_text(x+10,358,"Ctrl+Z/Y undo/redo | Ctrl+S save level");
    bsl_inspect_text(x+10,376,available?"Shared event; local instance parameters.":"Other definition surfaces remain unbound.");
}
