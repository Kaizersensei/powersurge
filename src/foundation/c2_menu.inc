/* Catalog render tree: code supplies data bindings and drawing algorithms only. Signed: Atlas. */
static float c2_sx(void){return video_get_screen_size().x/c2_reference_size[0];}
static float c2_sy(void){return video_get_screen_size().y/c2_reference_size[1];}
static float c2_scale(void){return fminf(c2_sx(),c2_sy());}
static Rect c2_rect(const C2Element* e){return (Rect){(int)lroundf((e->x-e->w*e->hx)*c2_sx()),(int)lroundf((e->y-e->h*e->hy)*c2_sy()),(int)lroundf(e->w*c2_sx()),(int)lroundf(e->h*c2_sy())};}
static image_t* c2_image(const C2Element* e,int frame){
 for(unsigned i=0;i<sizeof(c2_assets)/sizeof(c2_assets[0]);++i)if(!strcmp(c2_assets[i].type,e->type)&&(!*e->animation||!strcmp(c2_assets[i].animation,e->animation))&&c2_assets[i].frame==frame){if(!library_images[i]){const C2Asset* a=&c2_assets[i];if(a->w>0){library_sources[i]=image_load(a->path);library_images[i]=image_create_shared(library_sources[i],a->x,a->y,a->w,a->h);}else library_images[i]=image_load(a->path);}return library_images[i];}
 return NULL;
}
static void c2_sprite(const C2Element* e,Rect r,float alpha,int frame){
 if(!e->visible||r.w<=0||r.h<=0||alpha<=0)return;
 image_t* image=c2_image(e,frame);if(!image)return;
 float xs=(float)r.w/image_width(image),ys=(float)r.h/image_height(image);
 if(e->fit){float scale=fminf(xs,ys);r.x+=(r.w-(int)(image_width(image)*scale))/2;r.y+=(r.h-(int)(image_height(image)*scale))/2;xs=ys=scale;}
 Clip old=clip(r);image_draw_scaled_trans(image,r.x,r.y,v2d_new(xs,ys),alpha,IF_NONE);unclip(old);
}
static void c2_wind(const C2Element* e,Rect r,float alpha,const BslWeatherProbe* weather){
 if(!weather->enabled||!e->visible||r.w<=0||r.h<=0)return;
 float strength=fminf(1,fmaxf(0,weather->wind/c2_wind_max_strength));int count=imax(1,(int)ceilf(strength*c2_wind_max_arrows));
 int frame=e->frame;if(strength>0&&!reduced_motion)frame+=(int)fmod(floor(fmax(0,previous_clock)*(c2_wind_fps[0]+strength*(c2_wind_fps[1]-c2_wind_fps[0]))),e->max_frame-e->frame+1);
 image_t* image=c2_image(e,frame);if(!image)return;float iw=image_width(image),ih=image_height(image),gap=c2_wind_gap_em*r.h;
 float angle=weather->direction*acosf(-1)/180,total=count*iw+(count-1)*gap,cs=fabsf(cosf(angle)),sn=fabsf(sinf(angle));
 float scale=fminf(r.w/(total*cs+ih*sn),r.h/(total*sn+ih*cs))*(c2_wind_scale[0]+strength*(c2_wind_scale[1]-c2_wind_scale[0]));
 Clip old=clip(r);for(int i=0;i<count;i++){float offset=(i-(count-1)*.5f)*(iw+gap)*scale;image_draw_scaled_rotated_trans(image,(int)lroundf(r.x+r.w*.5f+cosf(angle)*offset),(int)lroundf(r.y+r.h*.5f+sinf(angle)*offset),(int)(iw/2),(int)(ih/2),v2d_new(scale,scale),-angle,alpha*(strength>0?1:c2_wind_calm_opacity),IF_NONE);}unclip(old);
}
static void c2_text(const C2Element* e,const char* value,Rect r,float scale,float alpha){if(e->visible&&alpha>0){char rgba[9];snprintf(rgba,sizeof(rgba),"%s%02x",e->color,(int)lroundf(fminf(1,alpha)*255));styled_text(e->role,value,r,e->align,e->valign,e->wrap,rgba,scale);}}
int bsl_ui_inventory_columns(void){return c2_inventory_columns;}
int bsl_ui_inventory_default_slots(void){return c2_inventory_slots;}
int bsl_ui_menu_option_count(void){int count=0;for(unsigned i=0;i<sizeof(c2_nodes)/sizeof(*c2_nodes);i++)if(c2_nodes[i].action)count++;return count;}
int bsl_ui_menu_option_page(int row){for(unsigned i=0;i<sizeof(c2_nodes)/sizeof(*c2_nodes);i++)if(c2_nodes[i].action&&row--==0)return c2_nodes[i].action;return 0;}
const char* bsl_ui_menu_page_title(int page){for(unsigned i=0;i<sizeof(c2_nodes)/sizeof(*c2_nodes);i++)if(c2_nodes[i].action==page&&page)return c2_elements[c2_nodes[i].element].text;return c2_default_page_title;}
static Rect c2_intersection(Rect a,Rect b){int x=imax(a.x,b.x),y=imax(a.y,b.y);return (Rect){x,y,imax(0,imin(a.x+a.w,b.x+b.w)-x),imax(0,imin(a.y+a.h,b.y+b.h)-y)};}
static Rect c2_cell_rect(const C2Element* e,const C2Element* base,Rect cell){return (Rect){cell.x+(int)lroundf((e->x-e->w*e->hx)*cell.w/base->w),cell.y+(int)lroundf((e->y-e->h*e->hy)*cell.h/base->h),(int)lroundf(e->w*cell.w/base->w),(int)lroundf(e->h*cell.h/base->h)};}
static void c2_inventory(const BslMenuView* view,Rect area,float alpha){
 int cols=c2_inventory_columns,rows=(view->slot_limit+cols-1)/cols,gap=(int)lroundf(c2_inventory_gap*c2_scale());
 int cell=imax(1,imin((area.w-gap*(cols-1))/cols,(area.h-gap*(rows-1))/rows));
 int w=cols*cell+(cols-1)*gap,h=rows*cell+(rows-1)*gap,left=area.x+(area.w-w)/2,top=area.y+(area.h-h)/2;
 const C2Element *base=&c2_elements[c2_inventory_cell],*name=&c2_elements[c2_inventory_name],*quantity=&c2_elements[c2_inventory_quantity];
 Clip old=clip(area);
 for(int i=0;i<rows*cols;++i){Rect r={left+(i%cols)*(cell+gap),top+(i/cols)*(cell+gap),cell,cell};
  c2_sprite(base,r,alpha*base->opacity*(i<view->slot_limit?1:c2_inventory_locked_opacity),base->frame);
  if(i>=view->slot_limit||i>=view->item_count)continue;
  const BslInventoryCell* item=&view->items[i];
  if(alpha>.95f){Rect hit=c2_intersection(r,area);ps_ui_menu_region(PS_MENU_SLOT,i,hit.x,hit.y,hit.w,hit.h);}
  if(i==view->selected)for(int edge=0;edge<(int)c2_inventory_outline_width;edge++)image_rect(r.x+edge,r.y+edge,r.x+r.w-1-edge,r.y+r.h-1-edge,color_rgb(c2_inventory_outline[0],c2_inventory_outline[1],c2_inventory_outline[2]));
  char number[32],label[1024];snprintf(number,sizeof(number),"%d",item->quantity);UiValue values[]={{"item",item->name},{"quantity",number},{"equipped",item->equipped?c2_inventory_equipped_marker:""},{"assigned",item->assigned?c2_inventory_assigned_marker:""}};
  float scale=fminf(c2_scale(),cell/base->w);ui_format(label,sizeof(label),name->text,values,4);c2_text(name,label,c2_cell_rect(name,base,r),scale,alpha*name->opacity);ui_format(label,sizeof(label),quantity->text,values,4);c2_text(quantity,label,c2_cell_rect(quantity,base,r),scale,alpha*quantity->opacity);
 }unclip(old);
}
static void c2_selection(Rect r,float alpha){
 int left=(int)lroundf(c2_selection_padding[0]*c2_sx()),top=(int)lroundf(c2_selection_padding[1]*c2_sy()),right=(int)lroundf(c2_selection_padding[2]*c2_sx()),bottom=(int)lroundf(c2_selection_padding[3]*c2_sy());
 image_rectfill(r.x-left,r.y-top,r.x+r.w+right-1,r.y+r.h+bottom-1,color_rgb(c2_selection_color[0],c2_selection_color[1],c2_selection_color[2]));
 const C2Element* e=&c2_elements[c2_selection_cursor];Rect cursor=c2_rect(e);cursor.x+=r.x+(int)lroundf(r.w*c2_selection_anchor[0]);cursor.y+=r.y+(int)lroundf(r.h*c2_selection_anchor[1]);c2_sprite(e,cursor,alpha*e->opacity,e->frame);
}
void bsl_ui_menu(const BslMenuView* view){
 ensure();if(animations[4].position<=0)return;BslWeatherProbe weather;bsl_host_weather_probe(&weather);
 typedef struct {Rect target,rect,clip;float alpha;bool visible;} DrawState;
 DrawState states[sizeof(c2_nodes)/sizeof(*c2_nodes)];memset(states,0,sizeof(states));v2d_t screen=video_get_screen_size();Rect viewport={0,0,(int)screen.x,(int)screen.y};
 char hp[32],maxhp[32],used[32],capacity[32],clock[64],category[512];snprintf(hp,sizeof(hp),"%d",view->hp);snprintf(maxhp,sizeof(maxhp),"%d",view->max_hp);snprintf(used,sizeof(used),"%d",view->item_count);snprintf(capacity,sizeof(capacity),"%d",view->slot_limit);
 unsigned long seconds=(unsigned long)fmax(0,view->calendar);snprintf(clock,sizeof(clock),"%02lu:%02lu",seconds/60,seconds%60);
 const char* details=strchr(view->body,'\n');size_t n=details?(size_t)(details-view->body):strlen(view->body);if(n>=sizeof(category))n=sizeof(category)-1;memcpy(category,view->body,n);category[n]=0;
 UiValue values[]={{"name",view->name},{"hp",hp},{"max_hp",maxhp},{"title",view->title},{"body",view->body},{"used",used},{"capacity",capacity},{"category",category},{"inventory_details",details?details+1:""},{"month",view->calendar_month?view->calendar_month:c2_default_month},{"time",view->calendar_time?view->calendar_time:clock},{"day",view->calendar_day?view->calendar_day:""},{"year",view->calendar_year?view->calendar_year:""},{"location",weather.enabled?weather.location:c2_default_location},{"weather",weather.enabled?weather.description:c2_default_weather}};
 int option=0;
 for(unsigned i=0;i<sizeof(c2_nodes)/sizeof(*c2_nodes);i++){
  const C2DrawNode* node=&c2_nodes[i];const C2Element* e=&c2_elements[node->element];DrawState* state=&states[i];int option_index=node->action?option++:-1;
  state->target=c2_rect(e);state->rect=state->target;state->alpha=e->opacity;state->clip=viewport;state->visible=e->visible&&e->opacity>0&&(!node->pages||(node->pages&(1<<view->page)));
  if(node->parent>=0){const DrawState* parent=&states[node->parent];state->visible=state->visible&&parent->visible;state->alpha*=parent->alpha;state->rect.x+=parent->rect.x-parent->target.x;state->rect.y+=parent->rect.y-parent->target.y+(node->bottom_anchor?parent->rect.h-parent->target.h:0);state->clip=parent->clip;}
  if(node->animation>=0){double p=animations[node->animation].position;state->visible=state->visible&&p>0;double t=eased(p);state->alpha*=fmax(0,(t-node->fade_start)/(1-node->fade_start));state->rect.x+=(int)lroundf(node->slide_x*c2_sx()*(1-t));state->rect.y+=(int)lroundf(node->slide_y*c2_sy()*(1-t));}
  if(node->binding==1&&(!view->assigned||!strcmp(view->assigned,"none")))state->visible=false;
  if(!state->visible)continue;
  Clip old=clip(state->clip);Rect r=state->rect;
  switch(node->kind){
   case 0:{int frame=e->frame;if(node->binding==2){float health=view->max_hp>0?fminf(1,fmaxf(0,(float)view->hp/view->max_hp)):0;frame+=(int)lroundf((1-health)*(e->max_frame-e->frame));}if(node->binding==3&&weather.enabled)frame=weather.icon;if(node->binding==4&&weather.enabled)frame=weather.daytime;if(frame>=0)c2_sprite(e,r,state->alpha,frame);break;}
   case 1:{if(option_index==view->home_row)c2_selection(r,state->alpha);char label[65536];ui_format(label,sizeof(label),e->text,values,sizeof(values)/sizeof(*values));c2_text(e,label,r,c2_scale(),state->alpha);break;}
   case 2:if(node->animation>=0)r=animated_skin_alpha(node->animation,node->skin,r,c2_scale()*node->skin_scale,state->alpha);else skin_frame_alpha(node->skin,r,c2_scale()*node->skin_scale,state->alpha);break;
   case 3:{float scale=c2_scale()*node->skin_scale;int left=(int)(bsl_ui_skins[node->skin].left*scale),right=(int)(bsl_ui_skins[node->skin].right*scale);patch_alpha(patches[node->skin][6],(Rect){r.x,r.y,left,r.h},0,0,scale,state->alpha);patch_alpha(patches[node->skin][7],(Rect){r.x+left,r.y,r.w-left-right,r.h},1,0,scale,state->alpha);patch_alpha(patches[node->skin][8],(Rect){r.x+r.w-right,r.y,right,r.h},0,0,scale,state->alpha);break;}
   case 5:c2_inventory(view,r,state->alpha);break;
   case 6:c2_wind(e,r,state->alpha,&weather);break;
  }
  if(state->alpha>.95f){Rect hit=c2_intersection(r,state->clip);
   if(node->action)ps_ui_menu_region(PS_MENU_LINK,node->action,hit.x,hit.y,hit.w,hit.h);
   if(!strcmp(e->tag,"Inventory_Category"))ps_ui_menu_region(PS_MENU_CATEGORIES,0,hit.x,hit.y,hit.w,hit.h);
   if(!strcmp(e->tag,"Inventory_Details")||(!strcmp(e->tag,"Detail_Content")&&view->page!=1&&view->page!=2))ps_ui_menu_region(PS_MENU_CONTROLS,0,hit.x,hit.y,hit.w,hit.h);
  }
  state->rect=r;if(node->clip_children)state->clip=c2_intersection(state->clip,r);unclip(old);
 }
}
