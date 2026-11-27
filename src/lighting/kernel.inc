// GLSL120 / ES100 builtin. Normal data is never gamma decoded. Signed: President.
static const char kernel[]=FRAGMENT_SHADER_GLSL_PREFIX("highp") R"GLSL(
uniform sampler2D tex;
uniform sampler2D normal_tex;
uniform vec2 color_storage,normal_storage,uv_scale,uv_offset;
uniform float rotation,normal_strength,roughness,specular,emission_energy;
uniform float light_energy,ambient_energy;
uniform vec3 light_direction,light_color,ambient_color;
uniform bool directx;
uniform int mode;
)GLSL"
#include "shading.inc"
R"GLSL(
void main(){
 float c=cos(rotation),s=sin(rotation);
 // Owned Allegro render-target textures use bottom-up GL coordinates.
 vec2 uv=vec2(v_texcoord.x/color_storage.x,1.0-v_texcoord.y/color_storage.y)-.5;
 vec2 q=fract(vec2(c*uv.x+s*uv.y,-s*uv.x+c*uv.y)*uv_scale+.5+uv_offset);
 vec3 base=texture2D(tex,vec2(q.x,1.0-q.y)*color_storage).rgb;
 if(mode==0){gl_FragColor=vec4(base,1.0);return;}
 vec3 n=texture2D(normal_tex,vec2(q.x,1.0-q.y)*normal_storage).rgb*2.0-1.0;
 n.y*=directx?-1.0:1.0;
 vec2 xy=vec2(n.x,-n.y)*sign(uv_scale)*normal_strength;
 n=normal_strength==0.0?vec3(0.0,0.0,1.0):normalize(vec3(c*xy.x-s*xy.y,s*xy.x+c*xy.y,n.z));
 if(mode==2){gl_FragColor=vec4(n*.5+.5,1.0);return;}
 gl_FragColor=vec4(shade_surface(base,n,roughness,specular,emission_energy),1.0);
}
)GLSL";
