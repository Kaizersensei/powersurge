// Derived cosmetic effect registry. Signed: Atlas.
#pragma once
#include "particles.hpp"
inline bsl::ParticleCatalog compiled_particles(){bsl::ParticleCatalog c;c.capacity=8192;
{bsl::ParticleEffect f;f.id="\145\146\146\145\143\164\072\167\145\141\164\150\145\162\137\162\141\151\156";f.capacity=2048;
{bsl::ParticleEmitter e;e.texture="\151\155\141\147\145\163\057\145\146\146\145\143\164\163\057\143\062\057\127\105\101\124\110\105\122\137\122\141\151\156\061\056\160\156\147";
e.sx=99;e.sy=114;e.sw=5;e.sh=9;e.rate=80;e.burst=24;e.life_min=1.2;e.life_max=2;e.vx_min=-15;e.vx_max=15;e.vy_min=650;e.vy_max=850;e.ax=0;e.ay=0;e.wind=1;e.width=4;e.height=12;e.end_scale=0.8;e.fade=0.3;e.color={0.7,0.85,1,0.65};f.emitters.push_back(e);}
c.effects.push_back(f);}
{bsl::ParticleEffect f;f.id="\145\146\146\145\143\164\072\167\145\141\164\150\145\162\137\163\156\157\167";f.capacity=2048;
{bsl::ParticleEmitter e;e.texture="\151\155\141\147\145\163\057\145\146\146\145\143\164\163\057\143\062\057\127\105\101\124\110\105\122\137\123\156\157\167\061\056\160\156\147";
e.sx=314;e.sy=110;e.sw=11;e.sh=12;e.rate=22;e.burst=24;e.life_min=3;e.life_max=5;e.vx_min=-12;e.vx_max=12;e.vy_min=50;e.vy_max=90;e.ax=0;e.ay=0;e.wind=1;e.width=7;e.height=9;e.end_scale=0.8;e.fade=0.3;e.color={1,1,1,0.8};f.emitters.push_back(e);}
c.effects.push_back(f);}
{bsl::ParticleEffect f;f.id="\145\146\146\145\143\164\072\167\145\141\164\150\145\162\137\141\163\150";f.capacity=2048;
{bsl::ParticleEmitter e;e.texture="\151\155\141\147\145\163\057\145\146\146\145\143\164\163\057\143\062\057\127\105\101\124\110\105\122\137\101\163\150\145\156\061\056\160\156\147";
e.sx=187;e.sy=131;e.sw=14;e.sh=18;e.rate=15;e.burst=24;e.life_min=3;e.life_max=5;e.vx_min=-20;e.vx_max=20;e.vy_min=35;e.vy_max=65;e.ax=0;e.ay=0;e.wind=1;e.width=8;e.height=7;e.end_scale=0.8;e.fade=0.3;e.color={1,1,1,0.6};f.emitters.push_back(e);}
c.effects.push_back(f);}
c.weather={"\145\146\146\145\143\164\072\167\145\141\164\150\145\162\137\162\141\151\156","\145\146\146\145\143\164\072\167\145\141\164\150\145\162\137\163\156\157\167","\145\146\146\145\143\164\072\167\145\141\164\150\145\162\137\141\163\150"};return c;}
