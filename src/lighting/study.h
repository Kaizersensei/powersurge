// Authored-level study adapter, no second renderer. Signed: President.
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void ps_lighting_study_frame(bool editing);
void ps_lighting_study_render(void);
void ps_lighting_study_detach(void);
bool ps_lighting_study_visible(void);
bool ps_lighting_study_captures(void);
int ps_lighting_study_check(char*,unsigned);
int ps_lighting_study_ui_check(int);
#ifdef __cplusplus
}
#endif
