/* Immutable selector metadata and visual-only BOR samples. Signed: Atlas. */
#ifndef BSL_SELECTION_CATALOG_H
#define BSL_SELECTION_CATALOG_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BslSelectionCharacter {char id[129],name[129],model[129];int health;} BslSelectionCharacter;
unsigned bsl_selection_character_count(void);
bool bsl_selection_character_at(unsigned index,BslSelectionCharacter* out);
bool bsl_selection_character_definition(const char* id,BslSelectionCharacter* out);
bool bsl_selection_begin(const char* id,unsigned participants,unsigned device);
const char* bsl_selection_last_error(void);
int bsl_selection_last_error_code(void);
/* Validates the complete initial roster before the existing inactive-run reset. */
int bsl_host_begin_selected_run(const char* definition);
typedef struct BslSelectionFrame {
 char sprite[160];int animation,origin_x,origin_y;unsigned frame,count;
 double duration;bool loop,fallback;
} BslSelectionFrame;
/* No actor/controller construction, progression, physics, scripts or sound callbacks. */
bool bsl_bor_selection_frame(const char* model,const char* phase,double age,BslSelectionFrame* out);
#ifdef __cplusplus
}
#endif
#endif
