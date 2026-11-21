/* Private native authoring seam. GPL-3.0-or-later. Signed: Atlas. */
#ifndef BSL_BOR_EDITOR_H
#define BSL_BOR_EDITOR_H
#include <stdbool.h>
#include <surgescript.h>
void bsl_bor_editor_origin(const surgescript_object_t* entity, float x, float y);
/* Inert outside the opt-in isolated inspector test. */
bool bsl_bor_editor_origin_test(const surgescript_object_t* entity, float x, float y, bool reset);
#endif
