/* Derived from authoritative UI layout JSON. Signed: Atlas. */
#pragma once
static const int bsl_ui_reference[2] = {640,480};
static const int bsl_ui_row_gap = 3;
static const char* bsl_ui_cursor_path = "\151\155\141\147\145\163\057\165\151\057\143\062\057\143\165\162\163\157\162\056\160\156\147";
static const char* bsl_ui_slot_path = "\151\155\141\147\145\163\057\165\151\057\143\062\057\163\154\157\164\056\160\156\147";
static const BslSkinDef bsl_ui_skins[] = {
{"\151\155\141\147\145\163\057\165\151\057\143\062\057\163\143\162\157\154\154\056\160\156\147",32,64,32,64,0.5,1,1},
{"\151\155\141\147\145\163\057\165\151\057\143\062\057\167\157\157\144\056\160\156\147",8,8,8,8,1.0,1,0},
};
static const BslLayoutDef bsl_ui_layouts[] = {
{"\123\160\154\151\164",{{72,104,496,312,1,16,24,16,20,0},{24,136,300,116,0,20,34,20,34,1},{336,124,280,290,0,20,34,20,34,1},{24,270,300,138,0,20,34,20,34,1}}},
{"\127\151\144\145",{{24,98,592,324,1,16,24,16,20,0},{24,104,592,108,0,20,34,20,34,1},{320,218,296,200,0,20,34,20,34,1},{24,234,280,168,0,20,34,20,34,1}}},
};
static const double bsl_ui_open_seconds = 0.28;
static const double bsl_ui_close_seconds = 0.22;
static const int bsl_ui_default_reduced_motion = 0;
static const int bsl_ui_body_role = 3;
static const int bsl_ui_title_role = 1;
static const int bsl_ui_footer_role = 4;
static const char* bsl_ui_text_color = "\060\060\060\060\060\060";
static const float bsl_ui_selection_color[] = {224,184,112};
static const int bsl_ui_caption_inset = 8;
static const int bsl_ui_choice_cursor_width = 26;
static const int bsl_ui_choice_cursor_height = 6;
static const float bsl_ui_choice_cursor_scale[] = {0.35,0.35};
static const int bsl_ui_measure_padding = 4;
static const int bsl_ui_minimum_line_height = 8;
static const char* bsl_ui_choice_footer = "\125\160\057\104\157\167\156\072\040\163\145\154\145\143\164\012\105\156\164\145\162\057\110\072\040\143\150\157\157\163\145\040\040\040\112\057\105\163\143\072\040\143\141\156\143\145\154";
static const char* bsl_ui_message_title = "\115\145\163\163\141\147\145";
static const char* bsl_ui_details_title = "\123\145\154\145\143\164\145\144\040\143\150\157\151\143\145";
static const char* bsl_ui_choices_title = "\103\150\157\151\143\145\163\040\173\146\151\162\163\164\175\055\173\154\141\163\164\175\040\057\040\173\143\157\165\156\164\175";
static const char* bsl_ui_status_font = "\110\125\104";
static const int bsl_ui_status_height = 84;
static const float bsl_ui_status_padding[] = {12,6,12,2};
static const float bsl_ui_status_color[] = {14,24,35};
static const float bsl_ui_loot_rect[] = {-56,-48,160,44};
static const float bsl_ui_legacy_slot_offset[] = {-44,10};
static const int bsl_ui_legacy_title_reserved = 42;
static const char* bsl_ui_status_text_color = "\146\146\146\146\146\146";
static const float bsl_ui_loot_marker_color[] = {238,202,96};
static const float bsl_ui_loot_marker_rect[] = {-3,-6,6,6};
static const int bsl_ui_body_alignment = 0;
static const int bsl_ui_body_vertical_alignment = 0;
static const int bsl_ui_body_wrapping = 1;
static const int bsl_ui_title_alignment = 1;
static const int bsl_ui_title_vertical_alignment = 1;
static const int bsl_ui_title_wrapping = 0;
static const int bsl_ui_owned_count = 2;
static const BslOwnedDialogDef bsl_ui_owned[] = {
{{{0,0,240,128,0,10,22,10,16,1},{244,0,240,260,0,10,22,10,16,1},{0,132,240,128,0,10,22,10,16,1}},1.0,0.8,3,3,4,{0.5,1.0},{0,-10},{8,8,8,8},{592,290}},
{{{0,0,180,144,0,10,22,10,16,1},{184,0,180,276,0,10,22,10,16,1},{0,148,180,128,0,10,22,10,16,1}},0.75,0.6,3,3,4,{0.5,1.0},{0,-10},{8,8,8,8},{592,290}},
};
