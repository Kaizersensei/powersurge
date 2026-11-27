// Reserved background pass and disposable native proof. Signed: President.
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void ps_sky_render(double camera_x,double camera_y);
void ps_sky_detach(void);
int ps_sky_check(const char* owned_root,char* error,unsigned capacity);
bool ps_sky_active(void);
#ifdef __cplusplus
}
#endif
