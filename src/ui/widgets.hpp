#pragma once
#include "widgets.h"
#include "imgui.h"
#include <string>
#include <vector>
struct BslShellField;
struct ALLEGRO_CONFIG;
namespace ps::ui {
bool wheel_selection(int& selected,int count);
bool combo(const char* label,int* selected,const char* const items[],int count);
bool combo(const char* label,int* selected,const std::vector<std::string>& items);
// Returns 1 for a validated edit, 2 for reset, 0 otherwise. Caller retains authority.
int instance_field(const BslShellField& field,double& value);
const char* shortcut(const char* command);
bool pressed(const char* command);
void command_help();
// Owned native scenario events; never sends desktop input.
void native_key(ImGuiKey key,bool down);
void native_capture(const char* path);
void rendered();
bool selected_visible();
const char* native_choice_bounds();
bool native_click_choice(int index,bool down);
void native_wheel(float delta);
float native_scroll();
bool native_menu_click(PsUiMenuAction action,int index,bool down);
int native_menu_item(const char* definition,int* quantity);
int native_menu_selected();
uint64_t native_menu_epoch();
float appearance_number(ALLEGRO_CONFIG* config,const char* key,float fallback);
}
