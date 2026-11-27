R"GLSL(
vec3 linearize(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 encode(vec3 c){c=clamp(c,0.0,1.0);return mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-.055,step(vec3(.0031308),c));}
vec3 shade_surface(vec3 base,vec3 n,float roughness,float specular,float emission_energy){
 float d=max(0.0,dot(n,light_direction));
 vec3 h=normalize(light_direction+vec3(0.0,0.0,1.0));
 float shine=pow(max(0.0,dot(n,h)),exp2(1.0+7.0*(1.0-roughness)))*specular*step(.00001,d);
 vec3 lit=linearize(base)*(linearize(ambient_color)*ambient_energy+linearize(light_color)*light_energy*d+vec3(emission_energy));
 lit+=linearize(light_color)*light_energy*shine;
 return encode(lit);
}
)GLSL"
