// Shared GL state, role decoder and light uniforms. Signed: President.
#pragma once
#include "surface.hpp"
extern "C" {
#include "../core/shader.h"
}
namespace ps::lighting::detail {
struct State {
 ALLEGRO_STATE state;const image_t* target=image_drawing_target();const shader_t* managed=shader_get_active();ALLEGRO_SHADER* actual=al_get_current_shader();bool held=al_is_bitmap_drawing_held();
 State(){al_store_state(&state,ALLEGRO_STATE_ALL);al_hold_bitmap_drawing(false);}
 ~State(){image_set_drawing_target(const_cast<image_t*>(target));al_restore_state(&state);shader_set_active(managed);al_use_shader(actual);al_hold_bitmap_drawing(held);}
};
cell::MaterialTexture normal_data(const std::string&);
void bind_light(shader_t*,const Settings&);
Settings material_settings(const authoring::RenderRegistry&,const authoring::Json&);
Settings light_settings(const authoring::RenderRegistry&,const std::string& light,const std::string& profile);
}
