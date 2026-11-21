// Authored native tool-strip view. Signed: President.
static constexpr int ui_x=16;
static constexpr int ui_y=76;
static constexpr int ui_max_width=1130;
static constexpr int ui_padding=8;
static constexpr int ui_gap=6;
static constexpr int ui_button_height=30;
static constexpr int ui_header_height=48;
static constexpr int ui_status_height=60;
static const char* ui_font_role="BSL Presentation hint 11";
static constexpr int ui_text_x=7;
static constexpr int ui_text_y=4;
static const int ui_background[]={15,27,34};
static const int ui_border[]={94,134,147};
static const int ui_button[]={34,52,62};
static const int ui_hover[]={53,81,94};
static const int ui_active[]={75,95,54};
static const int ui_text[]={235,231,207};
static const int ui_preview[]={255,215,90};
struct ToolButton {const char* command;const char* label;int width;};
static const ToolButton tool_buttons[]={
{"material_prev","Material <",96},
{"material_next","Material >",96},
{"new_shape","New shape",100},
{"preset","Square / Round",126},
{"blend","Blend on/off",110},
{"blend_less","Width -",80},
{"blend_more","Width +",80},
{"mode","Linear / Over / Under",168},
{"curve_less","Curve -",80},
{"curve_more","Curve +",80},
{"undo","Undo",70},
{"redo","Redo",70},
{"save","Save",70},
{"reload","Reload",70},
{"reload_materials","Reload materials",152},
};
