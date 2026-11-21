// File-owned material definitions, indexed in memory only. Signed: President.
#pragma once
#include "terrain.hpp"
namespace cell {
struct Material {
 std::string id,name,texture,shader,physical_profile,acoustic_profile;
 std::array<int,3> tint{};
 std::map<std::string,std::string> feedback;
 std::vector<std::string> reactions;
};
std::vector<Material> decode_materials(const std::string& text);
}
