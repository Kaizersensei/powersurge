/* PowerSurge-owned UI boundary; presentation emits intentions, never outcomes. Signed: President. */
#ifndef PS_UI_WIDGETS_H
#define PS_UI_WIDGETS_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum PsUiAction { PS_UI_SELECT, PS_UI_CONFIRM, PS_UI_CANCEL } PsUiAction;
typedef struct PsUiCommand { uint64_t owner,epoch; PsUiAction action; int index; } PsUiCommand;
typedef struct PsUiConversation {
 uint64_t owner,epoch; const char* message; const char* const* choices;
 int count,selected; const char* information; double clock; bool open,reduced_motion;
} PsUiConversation;
void ps_ui_init(void);
void ps_ui_shutdown(void);
void ps_ui_detach(void);
void ps_ui_input(void);
void ps_ui_frame(bool active);
bool ps_ui_enabled(void);
bool ps_ui_conversation_visible(void);
void ps_ui_conversation(const PsUiConversation* model);
void ps_ui_meter(uint64_t owner,double value,double minimum,double maximum,double fixture_mp);
void ps_ui_navigate(uint64_t owner,uint64_t epoch,int selected,int count,int delta,bool confirm,bool cancel);
float ps_ui_preview_mp(void);
bool ps_ui_command(uint64_t owner,uint64_t epoch,PsUiCommand* out);
/* Shared semantic entry for widgets, controller adapters and native scenarios. */
bool ps_ui_request(uint64_t owner,uint64_t epoch,PsUiAction action,int index);
typedef struct PsUiProbe {uint64_t owner,epoch;int choices,selected;bool ready,open,enabled;double hp_fraction,mp_fraction;int width,height;} PsUiProbe;
void ps_ui_probe(PsUiProbe* out);
void ps_ui_tools(void);
typedef enum PsUiMenuAction { PS_MENU_SELECT,PS_MENU_ACTIVATE,PS_MENU_ASSIGN,PS_MENU_PAGE,PS_MENU_CATEGORY,PS_MENU_BACK,PS_MENU_RESUME } PsUiMenuAction;
typedef enum PsUiMenuRegion { PS_MENU_SLOT,PS_MENU_LINK,PS_MENU_CONTROLS,PS_MENU_CATEGORIES } PsUiMenuRegion;
typedef struct PsUiMenuItem {const char* definition;const char* instance;int quantity;} PsUiMenuItem;
typedef struct PsUiMenuModel {uint64_t owner,epoch;bool open;int page,selected,count;const PsUiMenuItem* items;} PsUiMenuModel;
typedef struct PsUiMenuCommand {uint64_t owner,epoch;PsUiMenuAction action;int index;char definition[513],instance[513];} PsUiMenuCommand;
bool ps_ui_menu_active(void);
void ps_ui_menu_clear(void);
void ps_ui_menu_begin(const PsUiMenuModel* model);
void ps_ui_menu_region(PsUiMenuRegion kind,int index,float x,float y,float width,float height);
void ps_ui_menu_end(void);
bool ps_ui_menu_request(PsUiMenuAction action,int index);
bool ps_ui_menu_command(uint64_t owner,uint64_t epoch,PsUiMenuCommand* out);
int ps_ui_check(char* error,unsigned capacity);
#ifdef __cplusplus
}
#endif
#endif
