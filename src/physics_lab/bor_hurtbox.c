/* Transform BOR's resolved world corners into a Box2D box. Signed: Mech. */
#include "bor_hurtbox.h"
#include <math.h>
bool bsl_hurtbox_pose_from_corners(const double c[8],double ox,double oy,double ppm,BslHurtboxPose* out){
 if(!c||!out||!isfinite(ox)||!isfinite(oy)||!isfinite(ppm)||ppm<=0)return false;
 for(int i=0;i<8;i++)if(!isfinite(c[i]))return false;
 double ax=c[2]-c[0],ay=c[3]-c[1],bx=c[6]-c[0],by=c[7]-c[1];
 double a=hypot(ax,ay),b=hypot(bx,by),tol=1e-6*fmax(1.0,fmax(a,b));
 if(a/ppm<.04||b/ppm<.04||fabs(ax*bx+ay*by)>tol*fmax(a,b)||
    hypot(c[4]-(c[2]+c[6]-c[0]),c[5]-(c[3]+c[7]-c[1]))>tol||
    ax*by-ay*bx<=0)return false;
 double x=((c[0]+c[4])*.5-ox)/ppm,y=((c[1]+c[5])*.5-oy)/ppm;
 double angle=atan2(ay,ax),hw=a/(2*ppm),hh=b/(2*ppm);
 if(!isfinite(x)||!isfinite(y)||fabs(x)>10000||fabs(y)>10000||hw>10000||hh>10000)return false;
 *out=(BslHurtboxPose){(float)x,(float)y,(float)angle,(float)hw,(float)hh};return true;
}
