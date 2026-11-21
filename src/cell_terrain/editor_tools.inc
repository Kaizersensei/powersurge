// Data-authored controls attached to the loaded level. Signed: President.
#include "editor_layout.hpp"
struct ToolRect {int x,y,w,h;bool contains(float px,float py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}};
struct ToolLayout {ToolRect panel;std::vector<ToolRect> buttons;int status_y;};
bool tools_visible=true,pointer_capture=false;float pointer_x=-1,pointer_y=-1;
struct LinePreview {bool valid=false;cell::Stroke stroke;std::string preset;std::vector<cell::Edge> lines;uint64_t builds=0;} preview;
v2d_t tool_position(int x,int y){return v2d_new(static_cast<float>(x),static_cast<float>(y));}
color_t tool_color(const int* rgb){return color_rgb(rgb[0],rgb[1],rgb[2]);}
void tool_header(int width){
 const auto& shape=chosen();const char* modes[]={"Linear","Over","Under"};
 font_set_width(label,std::max(1,width));
 font_set_text(label,"<color=%02x%02x%02x>TERRAIN %s %s | Shape %d: %s | %s | brush %.2f m | route %d\nBlend %s (%d cells) | %s curve %.1f | Material edits affect the whole selected shape</color>",
  ui_text[0],ui_text[1],ui_text[2],terrain_level.c_str(),dirty?"*":"",shape.id,material(shape.material)->name.c_str(),shape.preset.c_str(),brush/double(cell::base),shape.route,shape.blend?"on":"off",shape.blend_cells,modes[mode],curve);
}
void tool_status(int width){
 font_set_width(label,std::max(1,width));
 font_set_text(label,"Shift: line | MMB: sample | G: grid | F1: hide tools | F6: objects | Ctrl+Shift+R: reload materials\nWorld %.2f, %.2f m | %s",cursorx/cell::base,cursory/cell::base,message.c_str());
}
ToolLayout tool_layout(int width){
 ToolLayout result;int available=std::max(1,width-2*ui_x),w=std::min(ui_max_width,available);
 int padding=std::min(ui_padding,w/4),header=ui_header_height;
 if(label){tool_header(w-2*padding);header=std::max(header,static_cast<int>(std::ceil(font_get_textsize(label).y))+ui_gap);}
 int x=ui_x+padding,y=ui_y+padding+header,right=ui_x+w-padding;
 for(const auto& b:tool_buttons){
  int needed=b.width;
  if(label){font_set_width(label,0);font_set_text(label,"%s",b.label);needed=std::max(needed,static_cast<int>(std::ceil(font_get_textsize(label).x))+2*ui_text_x);}
  int bw=std::min(needed,std::max(1,w-2*padding));
  if(x>ui_x+padding&&x+bw>right){x=ui_x+padding;y+=ui_button_height+ui_gap;}
  result.buttons.push_back({x,y,bw,ui_button_height});x+=bw+ui_gap;
 }
 result.status_y=y+ui_button_height+ui_gap;
 int status=ui_status_height;if(label){tool_status(w-2*padding);status=std::max(status,static_cast<int>(std::ceil(font_get_textsize(label).y)));}
 result.panel={ui_x,ui_y,w,result.status_y-ui_y+status+padding};return result;
}
void editor_command(const std::string& command){
 if(command=="undo"){history(true);return;}
 if(command=="redo"){history(false);return;}
 if(command=="save"){save();return;}
 if(command=="reload"){reload_base();return;}
 if(command=="reload_materials"){reload_materials();return;}
 if(command=="mode"){mode=(mode+1)%3;return;}
 if(command=="curve_less"||command=="curve_more"){curve=std::clamp(curve+(command=="curve_more"?.1:-.1),0.,1.);return;}
 commit();
 if(command=="new_shape"){new_shape(chosen().material);pending=false;return;}
 start_edit();
 if(command=="material_prev"||command=="material_next"){
  int n=material_index(chosen().material),count=static_cast<int>(std::size(cell_materials));
  chosen().material=cell_materials[(n+(command=="material_next"?1:count-1))%count].id;
  message="Material changed for every region belonging to the selected shape";
 }else if(command=="preset"){chosen().preset=chosen().preset=="square"?"round":"square";field.rebuild();}
 else if(command=="blend")chosen().blend=!chosen().blend;
 else if(command=="blend_less"||command=="blend_more")chosen().blend_cells=std::clamp(chosen().blend_cells+(command=="blend_more"?1:-1),0,4);
 else {drag=false;throw std::runtime_error("Unknown terrain tool command");}
 rebuild();commit();
}
bool tool_pointer(float x,float y,unsigned buttons,bool press){
 pointer_x=x;pointer_y=y;
 bool inside=tools_visible&&tool&&tool_layout(video_get_screen_size().x).panel.contains(x,y);
 if(inside&&drag)commit();
 if(inside&&buttons)pointer_capture=true;
 if(inside&&press){
  auto layout=tool_layout(video_get_screen_size().x);
  for(size_t i=0;i<layout.buttons.size();i++)if(layout.buttons[i].contains(x,y)){editor_command(tool_buttons[i].command);break;}
 }
 bool blocked=inside||pointer_capture;
 if(!buttons)pointer_capture=false;
 return blocked;
}
void update_preview(double x,double y){
 if(!pending||!tool){preview.valid=false;return;}
 cell::Stroke stamp;stamp.shape=selected;stamp.x=ax;stamp.y=ay;stamp.u=x;stamp.v=y;stamp.size=brush;stamp.mode=mode;stamp.curve=curve;
 const auto& old=preview.stroke;
 if(preview.valid&&preview.preset==chosen().preset&&old.shape==stamp.shape&&old.x==stamp.x&&old.y==stamp.y&&old.u==stamp.u&&old.v==stamp.v&&old.size==stamp.size&&old.mode==stamp.mode&&old.curve==stamp.curve)return;
 // The same paint kernel supplies committed geometry. No live field, masks or textures change.
 try{
  cell::Field stencil;stencil.doc.shapes.push_back(chosen());stencil.paint(stamp);
  auto lines=stencil.outline();preview.lines=std::move(lines);preview.stroke=stamp;preview.preset=chosen().preset;preview.valid=true;preview.builds++;
 }catch(const std::exception& e){preview.valid=false;message=std::string("Line preview: ")+e.what();}
}
void draw_preview(float dx,float dy){
 if(!pending||!preview.valid||!tool)return;
 color_t c=tool_color(ui_preview);
 for(const auto& e:preview.lines)image_line(origin+e.x-static_cast<int>(dx),origin+e.y-static_cast<int>(dy),origin+e.u-static_cast<int>(dx),origin+e.v-static_cast<int>(dy),c);
}
void draw_tools(){
 if(!tool||!tools_visible)return;
 auto layout=tool_layout(video_get_screen_size().x);auto r=layout.panel;
 image_rectfill(r.x,r.y,r.x+r.w,r.y+r.h,tool_color(ui_background));image_rect(r.x,r.y,r.x+r.w,r.y+r.h,tool_color(ui_border));
 v2d_t camera=v2d_new(video_get_screen_size().x*.5f,video_get_screen_size().y*.5f);
 const auto& shape=chosen();
 tool_header(r.w-2*ui_padding);font_set_position(label,tool_position(r.x+ui_padding,r.y+ui_padding));
 font_render(label,camera);
 for(size_t i=0;i<layout.buttons.size();i++){
  auto b=layout.buttons[i];bool lit=std::string(tool_buttons[i].command)=="blend"&&shape.blend;
  auto color=b.contains(pointer_x,pointer_y)?ui_hover:lit?ui_active:ui_button;
  image_rectfill(b.x,b.y,b.x+b.w,b.y+b.h,tool_color(color));image_rect(b.x,b.y,b.x+b.w,b.y+b.h,tool_color(ui_border));
  font_set_width(label,std::max(1,b.w-2*ui_text_x));font_set_text(label,"<color=%02x%02x%02x>%s</color>",ui_text[0],ui_text[1],ui_text[2],tool_buttons[i].label);
  int inset=std::max(ui_text_y,(b.h-static_cast<int>(std::ceil(font_get_textsize(label).y)))/2);
  font_set_position(label,tool_position(b.x+ui_text_x,b.y+inset));font_render(label,camera);
 }
 tool_status(r.w-2*ui_padding);font_set_position(label,tool_position(r.x+ui_padding,layout.status_y));font_render(label,camera);
}
// Own-process capture only: actual attached editor, with no injected desktop input.
void tool_capture(){
 static int frames=0;
 if(!std::getenv("BSL_CELL_UI_CAPTURE")||++frames!=85)return;
 bsl_cell_editor_open();message="Line preview: not yet committed";pending=true;ax=-96;ay=-64;mode=1;curve=.5;update_preview(352,-64);
}
