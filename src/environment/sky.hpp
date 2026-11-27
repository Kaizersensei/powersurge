// Actual runtime renderer also used by native GPU readback. Signed: President.
#pragma once
#include "sky_profile.hpp"
extern "C" {
#include "../core/image.h"
}
namespace ps::environment {
bool draw_sky(image_t*,const SkyFrame&,const SkyProfile&,std::string& diagnostic);
}
