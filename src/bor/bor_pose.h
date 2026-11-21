/* Shared surface-pose smoothing; terrain/contact angles remain immediate. Signed: Atlas. */
#ifndef BSL_BOR_POSE_H
#define BSL_BOR_POSE_H
#include <math.h>
/* Seconds to halve the remaining angular error. Zero requests immediate alignment. */
#define BOR_SURFACE_ROTATION_HALF_LIFE 0.06
static inline double bor_surface_angle_step(double current,double target,double dt){
 const double pi=3.141592653589793;
 double weight=BOR_SURFACE_ROTATION_HALF_LIFE>0?-expm1(-0.6931471805599453*dt/BOR_SURFACE_ROTATION_HALF_LIFE):1;
 return remainder(current+remainder(target-current,2*pi)*weight,2*pi);
}
#endif
