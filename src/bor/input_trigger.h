/* Normalized Allegro gamepad trigger -> mapped digital action. Signed: Atlas. */
#ifndef BSL_INPUT_TRIGGER_H
#define BSL_INPUT_TRIGGER_H
#include <stdbool.h>
static inline bool bsl_trigger_down(float pressure,bool previous){
 return pressure>=0&&pressure<=1&&(previous?pressure>.35f:pressure>=.55f);
}
#endif
