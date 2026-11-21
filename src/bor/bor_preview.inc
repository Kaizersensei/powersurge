/* Read-only authored animation sampling for menu visual Actors. Signed: Atlas. */
#include "../presentation/selection_catalog.h"
bool bsl_bor_selection_frame(const char* model,const char* phase,double age,BslSelectionFrame* out){
 if(!out)return false;
 memset(out,0,sizeof *out);
 if(!model||!phase||!isfinite(age)||age<0||age>1e9)return false;
 if(strcmp(phase,"idle")&&strcmp(phase,"selectin")&&strcmp(phase,"select")&&strcmp(phase,"pick")&&strcmp(phase,"selectout"))return false;
 const BorModel* definition=bor_find_model(bor_models,(int)(sizeof bor_models/sizeof *bor_models),model);
 if(!definition)return false;
 const BorAnimation* animation=bor_animation(definition,phase);
 if(!animation){animation=bor_animation(definition,"idle");out->fallback=true;}
 if(!animation||!animation->frames||animation->count<=0)return false;
 double total=0,prefix=0;
 int loop_from=animation->loop_from;
 if(loop_from<0||loop_from>=animation->count)loop_from=0;
 for(int i=0;i<animation->count;i++){
  const double duration=animation->frames[i].duration;
  if(!isfinite(duration)||duration<=0)return false;
  total+=duration;if(i<loop_from)prefix+=duration;
 }
 if(!isfinite(total)||total<=0)return false;
 double position=age;
 if(position>=total){
  if(animation->loop){const double cycle=total-prefix;if(cycle<=0)return false;position=prefix+fmod(position-total,cycle);}
  else position=total;
 }
 int frame=0;
 while(frame+1<animation->count&&position>=animation->frames[frame].duration){position-=animation->frames[frame].duration;frame++;}
 const BorFrame* f=animation->frames+frame;
 int written=snprintf(out->sprite,sizeof out->sprite,"%s",definition->sprite);
 if(f->sprite>=256)written=snprintf(out->sprite,sizeof out->sprite,"%s@%d",definition->sprite,f->sprite/256);
 if(f->cw>0)written=snprintf(out->sprite,sizeof out->sprite,"%s#%d",definition->sprite,f->sprite);
 if(written<0||written>=(int)sizeof out->sprite||f->sprite<0)return false;
 out->animation=f->cw>0?0:f->sprite%256;out->origin_x=f->ox;out->origin_y=f->oy;
 out->frame=(unsigned)frame;out->count=(unsigned)animation->count;out->duration=total;out->loop=animation->loop!=0;
 return true;
}
