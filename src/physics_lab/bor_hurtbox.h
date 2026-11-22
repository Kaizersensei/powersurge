/* Converts an already-resolved BOR hurtbox, not an actor or collision policy. Signed: Mech. */
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BslHurtboxPose {float x,y,angle,half_width,half_height;} BslHurtboxPose;
/* corners: TL, TR, BR, BL in world pixels; origin and pixel scale are explicit.
 * Returns false without changing out for non-rectangular, invalid or tiny inputs. */
bool bsl_hurtbox_pose_from_corners(const double corners[8],double origin_x,double origin_y,double pixels_per_meter,BslHurtboxPose* out);
#ifdef __cplusplus
}
#endif
