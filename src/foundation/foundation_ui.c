#include "../editor_shell/shell.h"
#include <stdlib.h>
/* Render-only C2 skin/layout adapter; services own dialogue/input/time. Signed: Atlas. */
#include "foundation_host.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../core/engine.h"
#include "../core/asset.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <surgescript.h>
#include "../core/font.h"
#include "../core/image.h"
#include "../core/video.h"
#include "../core/timer.h"
#include "../core/logfile.h"
#include "../scenes/level.h"
#include "../entities/camera.h"
typedef struct { const char* path; int left,top,right,bottom; float scale; int edges_tile,fill_tile; } BslSkinDef;
typedef struct { int x,y,w,h,skin,left,top,right,bottom,auto_height; } BslWindowDef;
typedef struct { const char* name; BslWindowDef windows[4]; } BslLayoutDef;
typedef struct { BslWindowDef windows[3];float font_scale,skin_scale;int body_role,title_role,footer_role;float anchor[2],offset[2],margin[4],font_reference[2]; } BslOwnedDialogDef;
typedef struct { const char *type,*animation; int frame; const char* path; int x,y,w,h; } C2Asset;
typedef struct { int uid; const char *type,*tag,*text; float x,y,w,h,hx,hy; int role,align,valign,wrap,visible,fit,frame,max_frame; float opacity; const char *animation,*color; } C2Element;
typedef struct { int element,kind,parent,animation,skin,bottom_anchor,pages,action,binding,clip_children; float slide_x,slide_y,fade_start,skin_scale; } C2DrawNode;
#include "c2_ui_catalog.h"
#include "ui_theme.h"
typedef struct { int x,y,w,h; } Rect;
typedef struct { int x,y,w,h; } Clip;
static font_t *status_font,*title_font,*body_font;
static font_t* c2_fonts[C2_FONT_BANDS][C2_FONT_ROLES];
static ALLEGRO_FONT* c2_metrics[C2_FONT_BANDS][C2_FONT_ROLES];
static image_t* library_images[sizeof(c2_assets)/sizeof(c2_assets[0])];
static image_t* library_sources[sizeof(c2_assets)/sizeof(c2_assets[0])];
typedef struct { double position; bool target; } UiAnim;
static UiAnim animations[7];
static double previous_clock=-1;
static bool reduced_motion=false,prefs_ready=false;
static Rect last_target[7],last_visible[7];
static image_t* patches[8][9];
static image_t* skin_sources[8];
static image_t *cursor_image,*slot_image;
static int layout_index=0;
static bool actor_dialogue,actor_compact;static double actor_world_x,actor_world_y;static Rect actor_view;
static int imin(int a,int b){return a<b?a:b;}
static int imax(int a,int b){return a>b?a:b;}
static float sx(void){return video_get_screen_size().x/bsl_ui_reference[0];}
static float sy(void){return video_get_screen_size().y/bsl_ui_reference[1];}
static int c2_font_band(float scale){int band=0;for(int i=1;i<C2_FONT_BANDS;++i)if(fabsf(c2_font_scales[i]-scale)<fabsf(c2_font_scales[band]-scale))band=i;return band;}
static font_t* c2_font_at(int role,float scale){
 if(role<0||role>=C2_FONT_ROLES)role=bsl_ui_body_role;
 int band=c2_font_band(scale);
 if(!c2_fonts[band][role]){char name[48];snprintf(name,sizeof(name),"BSL C2 %d %d",band,role);c2_fonts[band][role]=font_create(name);}
 return c2_fonts[band][role];
}
static float c2_current_scale(void){return fminf(video_get_screen_size().x/c2_reference_size[0],video_get_screen_size().y/c2_reference_size[1]);}
static const BslOwnedDialogDef* owned_profile(void){return actor_dialogue&&bsl_ui_owned_count>0?&bsl_ui_owned[actor_compact&&bsl_ui_owned_count>1?1:0]:NULL;}
static Rect owned_view(void){v2d_t size=video_get_screen_size();return actor_view.w>0&&actor_view.h>0?actor_view:(Rect){0,0,(int)size.x,(int)size.y};}
static float owned_scale(void){
 const BslOwnedDialogDef* p=owned_profile();if(!p)return fminf(sx(),sy());Rect v=owned_view();float scale=fminf((float)v.w/bsl_ui_reference[0],(float)v.h/bsl_ui_reference[1]);int w=0,h=0;
 for(int i=0;i<3;++i){w=imax(w,p->windows[i].x+p->windows[i].w);h=imax(h,p->windows[i].y+p->windows[i].h);}
 float available_w=fmaxf(1,v.w-(p->margin[0]+p->margin[2])*scale),available_h=fmaxf(1,v.h-(p->margin[1]+p->margin[3])*scale);
 return fminf(scale,fminf(available_w/w,available_h/h));
}
static float dialogue_text_scale(void){
 const BslOwnedDialogDef* p=owned_profile();if(!p)return c2_current_scale();
 int w=0,h=0;for(int i=0;i<3;++i){w=imax(w,p->windows[i].x+p->windows[i].w);h=imax(h,p->windows[i].y+p->windows[i].h);}
 float authored=fminf(w/p->font_reference[0],h/p->font_reference[1]);
 return c2_current_scale()*p->font_scale*authored*owned_scale()/fminf(sx(),sy());
}
static int dialogue_role(int role){const BslOwnedDialogDef* p=owned_profile();return !p?role:role==bsl_ui_title_role?p->title_role:role==bsl_ui_footer_role?p->footer_role:p->body_role;}
static font_t* dialogue_font(int role){return c2_font_at(dialogue_role(role),dialogue_text_scale());}
static const BslWindowDef* window_def(int role){const BslOwnedDialogDef* p=owned_profile();return p&&role>0?&p->windows[role-1]:&bsl_ui_layouts[layout_index].windows[role];}
void bsl_ui_actor_dialogue_context(bool owned,double x,double y,bool compact,int vx,int vy,int vw,int vh){
 actor_dialogue=owned;actor_compact=compact;actor_view=(Rect){vx,vy,vw,vh};if(isfinite(x)&&isfinite(y)){actor_world_x=x;actor_world_y=y;}
}
static Rect owned_bounds(void){
 const BslOwnedDialogDef* p=owned_profile();if(!p)return (Rect){0,0,0,0};float scale=owned_scale();Rect view=owned_view();v2d_t camera=camera_get_position(),screen=video_get_screen_size();int w=0,h=0;
 for(int i=0;i<3;++i){w=imax(w,p->windows[i].x+p->windows[i].w);h=imax(h,p->windows[i].y+p->windows[i].h);}
 w=(int)lroundf(w*scale);h=(int)lroundf(h*scale);
 int x=(int)lround(actor_world_x-camera.x+screen.x*.5+p->offset[0]*scale-w*p->anchor[0]),y=(int)lround(actor_world_y-camera.y+screen.y*.5+p->offset[1]*scale-h*p->anchor[1]);
 int left=view.x+(int)ceilf(p->margin[0]*scale),top=view.y+(int)ceilf(p->margin[1]*scale),right=view.x+view.w-(int)ceilf(p->margin[2]*scale),bottom=view.y+view.h-(int)ceilf(p->margin[3]*scale);
 return (Rect){imax(left,imin(x,right-w)),imax(top,imin(y,bottom-h)),w,h};
}
void bsl_ui_actor_dialogue_probe(BslOwnedUiProbe* out){if(out){Rect r=owned_bounds();*out=(BslOwnedUiProbe){owned_profile()!=NULL,actor_compact,r.x,r.y,r.w,r.h,0,0,0,0};out->text_scale=dialogue_text_scale();int band=c2_font_band(out->text_scale);out->body_pixels=c2_font_sizes[band][dialogue_role(bsl_ui_body_role)];out->title_pixels=c2_font_sizes[band][dialogue_role(bsl_ui_title_role)];out->footer_pixels=c2_font_sizes[band][dialogue_role(bsl_ui_footer_role)];}}

static void styled_text(int role,const char* value,Rect r,int align,int valign,bool wrap,const char* color,float scale){
 if(r.w<=0||r.h<=0)return;
 int band=c2_font_band(scale);font_t* font=c2_font_at(role,scale);
 if(!c2_metrics[band][role])c2_metrics[band][role]=al_load_ttf_font(asset_path(c2_font_path),-c2_font_sizes[band][role],0);
 int x=0,y=0,w=0,h=0;if(c2_metrics[band][role])al_get_text_dimensions(c2_metrics[band][role],"Ag",&x,&y,&w,&h);
 font_set_align(font,align==1?FONTALIGN_CENTER:align==2?FONTALIGN_RIGHT:FONTALIGN_LEFT);font_set_width(font,wrap?r.w:0);font_set_text(font,"<color=%s>%s</color>",color,value?value:"");
 int block=wrap?(int)ceilf(font_get_textsize(font).y):h,leading=wrap?0:y;
 int py=r.y+(valign==1?(r.h-block)/2:valign==2?r.h-block:0)-leading;
 Clip old;al_get_clipping_rectangle(&old.x,&old.y,&old.w,&old.h);
 int left=imax(old.x,r.x),top=imax(old.y,r.y),right=imin(old.x+old.w,r.x+r.w),bottom=imin(old.y+old.h,r.y+r.h);al_set_clipping_rectangle(left,top,imax(0,right-left),imax(0,bottom-top));
 font_set_position(font,v2d_new(r.x+(align==1?r.w/2:align==2?r.w:0),py));font_render(font,v2d_multiply(video_get_screen_size(),.5));font_set_align(font,FONTALIGN_LEFT);al_set_clipping_rectangle(old.x,old.y,old.w,old.h);
}
typedef struct {const char *key,*value;} UiValue;
static void ui_format(char* out,size_t capacity,const char* source,const UiValue* values,int count){
 size_t used=0;if(!capacity)return;
 while(source&&*source&&used+1<capacity){const char* end=*source=='{'?strchr(source,'}'):NULL;const char* value=NULL;
  if(end)for(int i=0;i<count;i++)if((size_t)(end-source-1)==strlen(values[i].key)&&!strncmp(source+1,values[i].key,end-source-1)){value=values[i].value?values[i].value:"";break;}
  if(value){size_t n=strlen(value);if(n>capacity-used-1)n=capacity-used-1;memcpy(out+used,value,n);used+=n;source=end+1;}else out[used++]=*source++;
 }out[used]=0;
}
static void ensure(void){
 title_font=dialogue_font(bsl_ui_title_role);body_font=dialogue_font(bsl_ui_body_role);
 if(status_font)return;
 status_font=font_create(bsl_ui_status_font);
 if(!prefs_ready){reduced_motion=bsl_ui_default_reduced_motion!=0;prefs_ready=true;}
 cursor_image=image_load(bsl_ui_cursor_path);slot_image=image_load(bsl_ui_slot_path);
 for(unsigned k=0;k<sizeof(bsl_ui_skins)/sizeof(bsl_ui_skins[0]);++k){
  const BslSkinDef* s=&bsl_ui_skins[k];const image_t* source=skin_sources[k]=image_load(s->path);
  int x[4]={0,s->left,image_width(source)-s->right,image_width(source)};
  int y[4]={0,s->top,image_height(source)-s->bottom,image_height(source)};
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)patches[k][row*3+col]=image_create_shared(source,x[col],y[row],x[col+1]-x[col],y[row+1]-y[row]);
 }
}
static double eased(double t){return t*t*(3-2*t);}
void bsl_ui_reset_presentation(void){actor_dialogue=false;memset(animations,0,sizeof(animations));memset(last_target,0,sizeof(last_target));memset(last_visible,0,sizeof(last_visible));previous_clock=-1;}
void bsl_ui_update(double clock,bool menu,int page,bool dialogue,bool details){
 ensure();double dt=previous_clock<0?0:fmax(0,fmin(.1,clock-previous_clock));previous_clock=clock;
 bool targets[7]={menu&&page!=0,dialogue,dialogue,dialogue&&details,menu,menu,menu};
 for(int i=0;i<7;++i){animations[i].target=targets[i];double goal=targets[i]?1:0;if(reduced_motion)animations[i].position=goal;
  else {double step=dt/(targets[i]?bsl_ui_open_seconds:bsl_ui_close_seconds);animations[i].position=targets[i]?fmin(1,animations[i].position+step):fmax(0,animations[i].position-step);}
 }
}
bool bsl_ui_menu_visible(void){return animations[4].position>0;}
bool bsl_ui_dialogue_visible(void){return animations[1].position>0||animations[2].position>0||animations[3].position>0;}
bool bsl_ui_reduced_motion(void){return reduced_motion;}
void bsl_ui_toggle_motion(void){reduced_motion=!reduced_motion;}
void bsl_ui_probe(BslUiProbe* p){if(p)*p=(BslUiProbe){animations[4].position,animations[1].position,last_visible[1].y,last_visible[1].y+last_visible[1].h,last_target[1].y+last_target[1].h};}
static Clip clip(Rect r){
 Clip old;al_get_clipping_rectangle(&old.x,&old.y,&old.w,&old.h);
 int x=imax(old.x,r.x),y=imax(old.y,r.y),right=imin(old.x+old.w,r.x+r.w),bottom=imin(old.y+old.h,r.y+r.h);
 al_set_clipping_rectangle(x,y,imax(0,right-x),imax(0,bottom-y));return old;
}
static void unclip(Clip old){al_set_clipping_rectangle(old.x,old.y,old.w,old.h);}
static void patch_alpha(const image_t* image,Rect r,int tile_x,int tile_y,float scale,float alpha){
 if(r.w<=0||r.h<=0)return;
 const int sw=image_width(image),sh=image_height(image);
 int step_x=tile_x?imax(1,(int)lroundf(sw*scale)):r.w,step_y=tile_y?imax(1,(int)lroundf(sh*scale)):r.h;
 Clip old=clip(r);
 for(int y=0;y<r.h;y+=step_y)for(int x=0;x<r.w;x+=step_x)image_draw_scaled_trans(image,r.x+x,r.y+y,v2d_new((float)step_x/sw,(float)step_y/sh),alpha,IF_NONE);
 unclip(old);
}
static Rect window_rect(int role){
 const BslWindowDef* w=window_def(role);
 if(owned_profile()&&role>0){Rect bounds=owned_bounds();float scale=owned_scale();return (Rect){bounds.x+(int)lroundf(w->x*scale),bounds.y+(int)lroundf(w->y*scale),imax(1,(int)lroundf(w->w*scale)),imax(1,(int)lroundf(w->h*scale))};}
 return (Rect){(int)lroundf(w->x*sx()),(int)lroundf(w->y*sy()),imax(1,(int)lroundf(w->w*sx())),imax(1,(int)lroundf(w->h*sy()))};
}
static Rect inner(int role,Rect r){
 const BslWindowDef* w=window_def(role);
 float xscale=owned_profile()&&role>0?owned_scale():sx(),yscale=owned_profile()&&role>0?owned_scale():sy();
 int left=(int)lroundf(w->left*xscale),top=(int)lroundf(w->top*yscale),right=(int)lroundf(w->right*xscale),bottom=(int)lroundf(w->bottom*yscale);
 return (Rect){r.x+left,r.y+top,imax(0,r.w-left-right),imax(0,r.h-top-bottom)};
}
static void skin_frame_alpha(int skin,Rect r,float scale,float alpha){
 const BslSkinDef* s=&bsl_ui_skins[skin];
 float bx=s->left*scale,br=s->right*scale,bt=s->top*scale,bb=s->bottom*scale;
 if(bx+br>r.w){float f=r.w/(bx+br);bx*=f;br*=f;}if(bt+bb>r.h){float f=r.h/(bt+bb);bt*=f;bb*=f;}
 int x[4]={r.x,r.x+(int)lroundf(bx),r.x+r.w-(int)lroundf(br),r.x+r.w};
 int y[4]={r.y,r.y+(int)lroundf(bt),r.y+r.h-(int)lroundf(bb),r.y+r.h};
 for(int row=0;row<3;++row)for(int col=0;col<3;++col){
  int center=row==1&&col==1;patch_alpha(patches[skin][row*3+col],(Rect){x[col],y[row],x[col+1]-x[col],y[row+1]-y[row]},col==1&&(center?s->fill_tile:s->edges_tile),row==1&&(center?s->fill_tile:s->edges_tile),scale,alpha);
 }
}
static Rect animated_skin_alpha(int role,int skin,Rect target,float scale,float alpha){
 last_target[role]=target;Rect r=target;double p=animations[role].position;
 if(p<=0){r.h=0;last_visible[role]=r;return r;}
 const BslSkinDef* s=&bsl_ui_skins[skin];int closed=imin(target.h,(int)lroundf((s->top+s->bottom)*scale));
 r.h=closed+(int)lround((target.h-closed)*eased(p));last_visible[role]=r;skin_frame_alpha(skin,r,scale,alpha);return r;
}
static Rect animated_skin(int role,int skin,Rect target,float scale){return animated_skin_alpha(role,skin,target,scale,1);}
static Rect frame(int role,Rect target){const BslWindowDef* w=window_def(role);return animated_skin(role,w->skin,target,bsl_ui_skins[w->skin].scale*(owned_profile()&&role>0?owned_scale()*owned_profile()->skin_scale:fminf(sx(),sy())));}
static void text(font_t* font,const char* value,Rect r,int dark){
 if(r.w<=0||r.h<=0)return;
 Clip old=clip(r);int align=bsl_ui_body_alignment;
 font_set_align(font,align==1?FONTALIGN_CENTER:align==2?FONTALIGN_RIGHT:FONTALIGN_LEFT);font_set_width(font,bsl_ui_body_wrapping?r.w:0);
 font_set_text(font,"<color=%s>%s</color>",dark?bsl_ui_text_color:bsl_ui_status_text_color,value?value:"");
 int height=(int)ceilf(font_get_textsize(font).y),y=r.y+(bsl_ui_body_vertical_alignment==1?(r.h-height)/2:bsl_ui_body_vertical_alignment==2?r.h-height:0);
 font_set_position(font,v2d_new(r.x+(align==1?r.w/2:align==2?r.w:0),y));font_render(font,v2d_multiply(video_get_screen_size(),.5));font_set_align(font,FONTALIGN_LEFT);unclip(old);
}
static int measure(font_t* font,const char* value,int width){font_set_width(font,imax(0,width));font_set_text(font,"%s",value?value:"");return imax(bsl_ui_minimum_line_height,(int)ceilf(font_get_textsize(font).y)+bsl_ui_measure_padding);}
static int text_height(const char* value,int width){return measure(body_font,value,bsl_ui_body_wrapping?width:0);}

static int title_height(void){return measure(title_font,"Ag",4096);}
static int footer_height(Rect area){
 int measured=measure(dialogue_font(bsl_ui_footer_role),bsl_ui_choice_footer,area.w);
 int room=imax(0,area.h-text_height("Ag",imax(1,area.w-bsl_ui_choice_cursor_width))-bsl_ui_row_gap);
 return imin(measured,room);
}
static Rect fitted(int role,int content){
 Rect r=window_rect(role);const BslWindowDef* w=window_def(role);
 if(w->auto_height){int needed=(int)lroundf((w->top+w->bottom)*(owned_profile()&&role>0?owned_scale():sy()))+content;r.h=imin(r.h,imax(1,needed));}return r;
}
static void caption(int role,Rect r,const char* title){
 const BslWindowDef* w=window_def(role);int h=(int)lroundf(bsl_ui_skins[w->skin].top*bsl_ui_skins[w->skin].scale*(owned_profile()&&role>0?owned_scale()*owned_profile()->skin_scale:fminf(sx(),sy())));
 styled_text(dialogue_role(bsl_ui_title_role),title,(Rect){r.x+bsl_ui_caption_inset,r.y,imax(0,r.w-2*bsl_ui_caption_inset),h},bsl_ui_title_alignment,bsl_ui_title_vertical_alignment,bsl_ui_title_wrapping,bsl_ui_text_color,dialogue_text_scale());
}
static void text_window(int role,const char* title,const char* body){
 Rect max=window_rect(role),area=inner(role,max);Rect r=frame(role,fitted(role,text_height(body,area.w)));if(r.h<=0)return;
 caption(role,r,title);area=inner(role,r);text(body_font,body,area,1);
}
void bsl_ui_cycle_layout(void){layout_index=(layout_index+1)%(int)(sizeof(bsl_ui_layouts)/sizeof(bsl_ui_layouts[0]));}
int bsl_ui_layout_index(void){return layout_index;}
void bsl_ui_choice_view(const char* const* labels,int count,int selected,int* first,int* last){
 ensure();if(count<1||count>64){*first=*last=0;return;}selected=imax(0,imin(selected,count-1));
 Rect area=inner(2,window_rect(2));const int height=imax(1,area.h-footer_height(area)),width=imax(1,area.w-bsl_ui_choice_cursor_width);
 *first=imax(0,imin(*first,count-1));if(selected<*first)*first=selected;
 int used=0;for(int i=*first;i<=selected;++i)used+=text_height(labels[i],width)+bsl_ui_row_gap;
 while(*first<selected&&used>height){used-=text_height(labels[*first],width)+bsl_ui_row_gap;++*first;}
 *last=*first;used=0;
 while(*last<count){int h=text_height(labels[*last],width)+bsl_ui_row_gap;if(*last>*first&&used+h>height)break;used+=h;++*last;}
 if(*last==count)while(*first>0){int h=text_height(labels[*first-1],width)+bsl_ui_row_gap;if(used+h>height)break;used+=h;--*first;}
}
void bsl_ui_conversation(const char* message,const char* const* choices,int count,int selected,int first,const char* details){
 ensure();text_window(1,bsl_ui_message_title,message);
 if(details&&*details)text_window(3,bsl_ui_details_title,details);
 int last=0;bsl_ui_choice_view(choices,count,selected,&first,&last);Rect max=inner(2,window_rect(2));int used=0;
 for(int i=first;i<last;++i)used+=text_height(choices[i],imax(1,max.w-bsl_ui_choice_cursor_width))+bsl_ui_row_gap;
 int fh=footer_height(max);Rect r=frame(2,fitted(2,used+fh));if(r.h<=0)return;Rect area=inner(2,r);char title[96];char a[24],b[24],c[24];snprintf(a,sizeof(a),"%d",first+1);snprintf(b,sizeof(b),"%d",last);snprintf(c,sizeof(c),"%d",count);UiValue values[]={{"first",a},{"last",b},{"count",c}};ui_format(title,sizeof(title),bsl_ui_choices_title,values,3);caption(2,r,title);
 int y=area.y;Rect list={area.x,y,area.w,imax(0,area.h-fh)};Clip old=clip(list);
 for(int i=first;i<last;++i){int height=text_height(choices[i],imax(1,area.w-bsl_ui_choice_cursor_width));
  if(i==selected){image_rectfill(area.x,y,area.x+area.w-1,y+height+bsl_ui_row_gap-1,color_rgb(bsl_ui_selection_color[0],bsl_ui_selection_color[1],bsl_ui_selection_color[2]));image_draw_scaled(cursor_image,area.x,y+imax(0,(height-bsl_ui_choice_cursor_height)/2),v2d_new(bsl_ui_choice_cursor_scale[0],bsl_ui_choice_cursor_scale[1]),IF_NONE);}
  text(body_font,choices[i],(Rect){area.x+bsl_ui_choice_cursor_width,y,imax(0,area.w-bsl_ui_choice_cursor_width),height},1);y+=height+bsl_ui_row_gap;
 }
 unclip(old);text(dialogue_font(bsl_ui_footer_role),bsl_ui_choice_footer,(Rect){area.x,area.y+area.h-fh,area.w,fh},1);
}
double bsl_engine_delta(void){return timer_get_delta();}
bool bsl_engine_editor(void){return level_editmode();}
void bsl_engine_restart(void){level_restart();}
bool bsl_engine_has_level(const char* path){return path&&asset_exists(path);}
void bsl_engine_load_level(const char* path){level_change(path);}
void bsl_engine_quit(void){engine_quit();}
const char* bsl_engine_data_dir(void){return asset_writedir();}
const char* bsl_engine_level_file(void){return level_file();}
const char* bsl_engine_level_path(void){return asset_path(level_file());}
void bsl_engine_error(const char* message){logfile_message("FOUNDATION: %s",message);}
void bsl_ui_release(void){
 if(status_font)font_destroy(status_font);
 for(int band=0;band<C2_FONT_BANDS;++band)for(int role=0;role<C2_FONT_ROLES;++role){if(c2_fonts[band][role]){font_destroy(c2_fonts[band][role]);c2_fonts[band][role]=NULL;}if(c2_metrics[band][role]){al_destroy_font(c2_metrics[band][role]);c2_metrics[band][role]=NULL;}}
 for(unsigned i=0;i<sizeof(c2_assets)/sizeof(c2_assets[0]);++i)if(library_images[i]){image_unload(library_images[i]);if(library_sources[i]){image_destroy(library_images[i]);image_unload(library_sources[i]);library_sources[i]=NULL;}library_images[i]=NULL;}
 bsl_ui_reset_presentation();
 status_font=title_font=body_font=NULL;
 for(unsigned k=0;k<sizeof(bsl_ui_skins)/sizeof(bsl_ui_skins[0]);++k){
  /* Keep the original parent reference until every shared bitmap is destroyed. */
  for(int i=0;i<9;++i)if(patches[k][i]){image_unload(patches[k][i]);image_destroy(patches[k][i]);patches[k][i]=NULL;}
  if(skin_sources[k]){image_unload(skin_sources[k]);skin_sources[k]=NULL;}
 }
 if(cursor_image)image_unload(cursor_image);
 if(slot_image)image_unload(slot_image);
 cursor_image=slot_image=NULL;
}
void bsl_ui_loot_marker(double x,double y,const char* label){
 ensure();v2d_t screen=video_get_screen_size(),camera=camera_get_position();double sx=x-camera.x+screen.x*.5,sy=y-camera.y+screen.y*.5;
 Rect label_rect={(int)sx+bsl_ui_loot_rect[0],(int)sy+bsl_ui_loot_rect[1],bsl_ui_loot_rect[2],bsl_ui_loot_rect[3]};
 if(label_rect.x+label_rect.w<0||label_rect.x>screen.x||label_rect.y+label_rect.h<0||label_rect.y>screen.y)return;
 image_rectfill((int)sx+bsl_ui_loot_marker_rect[0],(int)sy+bsl_ui_loot_marker_rect[1],(int)sx+bsl_ui_loot_marker_rect[0]+bsl_ui_loot_marker_rect[2],(int)sy+bsl_ui_loot_marker_rect[1]+bsl_ui_loot_marker_rect[3],color_rgb(bsl_ui_loot_marker_color[0],bsl_ui_loot_marker_color[1],bsl_ui_loot_marker_color[2]));text(status_font,label,label_rect,0);
}
void bsl_ui_draw(const char* status,const char* title,const char* body,double clock){
 (void)clock;ensure();if(bsl_ui_menu_visible())return;v2d_t screen=video_get_screen_size();
 bsl_shell_status("world",status);
 if(!title||!*title)return;
 Rect r=frame(0,window_rect(0));if(r.h<=0)return;Rect area=inner(0,r);text(title_font,title,(Rect){area.x,area.y,imax(0,area.w-bsl_ui_legacy_title_reserved),title_height()},0);
 image_draw(slot_image,r.x+r.w+bsl_ui_legacy_slot_offset[0],r.y+bsl_ui_legacy_slot_offset[1],IF_NONE);int th=title_height();area.y+=th;area.h=imax(0,area.h-th);text(body_font,body,area,0);
}
#include "c2_menu.inc"

static surgescript_var_t* begin_session(surgescript_object_t* object,const surgescript_var_t** args,int count){bsl_host_enable();return NULL;}
static surgescript_var_t* end_session(surgescript_object_t* object,const surgescript_var_t** args,int count){bsl_host_shutdown();return NULL;}
static surgescript_var_t* idle_session(surgescript_object_t* object,const surgescript_var_t** args,int count){return NULL;}
static surgescript_var_t* captured_input(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_bool(surgescript_var_create(),bsl_host_captures_commands());}
static surgescript_var_t* cancel_anchor_owner(surgescript_object_t* object,const surgescript_var_t** args,int count){
 surgescript_objectmanager_t* manager=surgescript_object_manager(object);
 surgescript_object_t* parent=surgescript_objectmanager_get(manager,surgescript_object_parent(object));
 if(parent&&strcmp(surgescript_object_name(parent),"BSL Event Anchor")==0){
  const char* id=level_get_entity_id(parent);
  if(id&&*id)bsl_host_anchor_cancel(strtoull(id,NULL,16));
 }
 return NULL;
}
static surgescript_var_t* progress_flag(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_bool(surgescript_var_create(),surgescript_var_is_string(args[0])&&bsl_host_progress_flag(surgescript_var_fast_get_string(args[0])));}
static surgescript_var_t* progress_variable(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_number(surgescript_var_create(),surgescript_var_is_string(args[0])?bsl_host_progress_variable(surgescript_var_fast_get_string(args[0])):0);}
static surgescript_var_t* progress_observe(surgescript_object_t* object,const surgescript_var_t** args,int count){bsl_host_boot_observe();return NULL;}
static uint64_t pickup_placement(surgescript_object_t* object){surgescript_object_t* parent=surgescript_objectmanager_get(surgescript_object_manager(object),surgescript_object_parent(object));if(!parent||strcmp(surgescript_object_name(parent),"BSL Pickup"))return 0;const char* id=level_get_entity_id(parent);return id?strtoull(id,NULL,16):0;}
static surgescript_var_t* pickup_available(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_bool(surgescript_var_create(),bsl_host_pickup_available(pickup_placement(object)));}
static surgescript_var_t* pickup_label(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_string(surgescript_var_create(),bsl_host_pickup_label(pickup_placement(object)));}
/* Read-only startup view plus one explicit retry command for the bootstrap owner. Signed: Mech. */
static surgescript_var_t* assembly_required(surgescript_object_t* object,const surgescript_var_t** args,int count){BslPlayerAssemblyProbe p={0};bsl_host_player_assembly_probe(&p);return surgescript_var_set_bool(surgescript_var_create(),p.required);}
static surgescript_var_t* assembly_set_required(surgescript_object_t* object,const surgescript_var_t** args,int count){if(surgescript_var_is_bool(args[0]))bsl_host_player_required(surgescript_var_get_bool(args[0]));return NULL;}
static surgescript_var_t* assembly_ready(surgescript_object_t* object,const surgescript_var_t** args,int count){BslPlayerAssemblyProbe p={0};bsl_host_player_assembly_probe(&p);return surgescript_var_set_bool(surgescript_var_create(),p.ready);}
static surgescript_var_t* assembly_failed(surgescript_object_t* object,const surgescript_var_t** args,int count){BslPlayerAssemblyProbe p={0};bsl_host_player_assembly_probe(&p);return surgescript_var_set_bool(surgescript_var_create(),p.failed);}
static surgescript_var_t* assembly_message(surgescript_object_t* object,const surgescript_var_t** args,int count){BslPlayerAssemblyProbe p={0};bsl_host_player_assembly_probe(&p);return surgescript_var_set_string(surgescript_var_create(),p.message);}
static surgescript_var_t* assembly_retry(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_bool(surgescript_var_create(),bsl_host_player_retry());}
static surgescript_var_t* assembly_take_retry(surgescript_object_t* object,const surgescript_var_t** args,int count){return surgescript_var_set_bool(surgescript_var_create(),bsl_host_player_take_retry());}
/* Shared script clock facade. Getters observe host state; they do not retain private clocks. Signed: Mech. */
static surgescript_var_t* world_available(surgescript_object_t* object,const surgescript_var_t** args,int count){BslWorldTimeProbe p={0};bsl_host_world_time_probe(&p);return surgescript_var_set_bool(surgescript_var_create(),p.available);}
static surgescript_var_t* world_paused(surgescript_object_t* object,const surgescript_var_t** args,int count){BslWorldTimeProbe p={0};bsl_host_world_time_probe(&p);return surgescript_var_set_bool(surgescript_var_create(),p.paused);}
static surgescript_var_t* world_simulation(surgescript_object_t* object,const surgescript_var_t** args,int count){BslWorldTimeProbe p={0};bsl_host_world_time_probe(&p);return surgescript_var_set_number(surgescript_var_create(),p.simulation);}
static surgescript_var_t* world_ui(surgescript_object_t* object,const surgescript_var_t** args,int count){BslWorldTimeProbe p={0};bsl_host_world_time_probe(&p);return surgescript_var_set_number(surgescript_var_create(),p.ui);}
static surgescript_var_t* world_generation(surgescript_object_t* object,const surgescript_var_t** args,int count){BslWorldTimeProbe p={0};bsl_host_world_time_probe(&p);return surgescript_var_set_number(surgescript_var_create(),(double)p.generation);}
static surgescript_var_t* world_calendar_available(surgescript_object_t* object,const surgescript_var_t** args,int count){BslCalendarProbe p={0};bsl_host_calendar_probe(&p);return surgescript_var_set_bool(surgescript_var_create(),p.enabled);}
static surgescript_var_t* world_calendar_hour(surgescript_object_t* object,const surgescript_var_t** args,int count){BslCalendarProbe p={0};bsl_host_calendar_probe(&p);surgescript_var_t* value=surgescript_var_create();return p.enabled?surgescript_var_set_number(value,p.hour+p.minute/60.0+p.second/3600.0):surgescript_var_set_null(value);}
static surgescript_var_t* world_calendar_factor(surgescript_object_t* object,const surgescript_var_t** args,int count){BslCalendarProbe p={0};bsl_host_calendar_probe(&p);surgescript_var_t* value=surgescript_var_create();return p.enabled?surgescript_var_set_number(value,p.factor):surgescript_var_set_null(value);}
void scripting_register_foundation(surgescript_vm_t* vm){
 surgescript_vm_bind(vm,"BSLWorldView","state:main",idle_session,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_available",world_available,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_paused",world_paused,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_simulationTime",world_simulation,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_uiTime",world_ui,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_generation",world_generation,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_calendarAvailable",world_calendar_available,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_calendarHour",world_calendar_hour,0);
 surgescript_vm_bind(vm,"BSLWorldView","get_calendarFactor",world_calendar_factor,0);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","state:main",idle_session,0);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","get_required",assembly_required,0);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","set_required",assembly_set_required,1);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","get_ready",assembly_ready,0);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","get_failed",assembly_failed,0);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","get_message",assembly_message,0);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","retry",assembly_retry,0);
 surgescript_vm_bind(vm,"BSLPlayerAssembly","takeRetry",assembly_take_retry,0);

 surgescript_vm_bind(vm,"BSLPickupView","state:main",idle_session,0);
 surgescript_vm_bind(vm,"BSLPickupView","get_available",pickup_available,0);
 surgescript_vm_bind(vm,"BSLPickupView","get_label",pickup_label,0);
 surgescript_vm_bind(vm,"BSLProgressView","state:main",idle_session,0);
 surgescript_vm_bind(vm,"BSLProgressView","flag",progress_flag,1);
 surgescript_vm_bind(vm,"BSLProgressView","variable",progress_variable,1);
 surgescript_vm_bind(vm,"BSLProgressView","observe",progress_observe,0);
 surgescript_vm_bind(vm,"BSLAnchorOwner","state:main",idle_session,0);
 surgescript_vm_bind(vm,"BSLAnchorOwner","cancel",cancel_anchor_owner,0);
 surgescript_vm_bind(vm,"FoundationInputGate","state:main",idle_session,0);
 surgescript_vm_bind(vm,"FoundationInputGate","get_captured",captured_input,0);
 surgescript_vm_bind(vm,"FoundationSession","constructor",begin_session,0);
 surgescript_vm_bind(vm,"FoundationSession","destructor",end_session,0);
 surgescript_vm_bind(vm,"FoundationSession","state:main",idle_session,0);
}
