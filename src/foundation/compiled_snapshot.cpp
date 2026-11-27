// One immutable saved-project snapshot per engine session. Signed: President.
#include "foundation_records.hpp"
#include "world_snapshot.h"
#include "../authoring/world_records.hpp"
#include "../authoring/render_records.hpp"
#include <memory>
#include <cstdio>
extern "C" {
#include "../core/asset.h"
#include "../core/logfile.h"
#include <allegro5/allegro.h>
}
namespace {
std::unique_ptr<bsl::Catalog> snapshot;
std::unique_ptr<ps::authoring::WorldRegistry> world;
std::unique_ptr<ps::authoring::RenderRegistry> rendering;
std::vector<ps::authoring::Json> records;bool loaded=false,indexed=false;
void load(){if(loaded)return;std::vector<ps::authoring::Json> next;
 indexed=asset_is_init()&&asset_exists("database/project.json");
 if(indexed)next=ps::authoring::read_records([](const std::string& path){std::string name="database/"+path;auto* f=al_fopen(asset_path(name.c_str()),"rb");if(!f)throw std::runtime_error("Missing database file: "+path);auto size=al_fsize(f);if(size<0||size>1024*1024){al_fclose(f);throw std::runtime_error("Oversized database file: "+path);}std::string bytes(size_t(size),'\0');bool okay=al_fread(f,bytes.data(),bytes.size())==bytes.size();al_fclose(f);if(!okay)throw std::runtime_error("Cannot read database file: "+path);return bytes;});
 records=std::move(next);loaded=true;
}
}
namespace ps::authoring {
const RenderRegistry& render_snapshot(){if(!asset_is_init()){static const auto empty=compile_render({});return empty;}load();if(!rendering)rendering=std::make_unique<RenderRegistry>(compile_render(records));return *rendering;}
const WorldRegistry& world_snapshot(){if(!asset_is_init()){static const auto fallback=compile_world({});return fallback;}load();if(!world)world=std::make_unique<WorldRegistry>(compile_world(records));return *world;}
}
namespace bsl {
const Catalog& database_snapshot(){if(!asset_is_init()){static const auto fallback=compiled_catalog();return fallback;}if(!snapshot){load();snapshot=std::make_unique<Catalog>(indexed?ps::authoring::compile(records):compiled_catalog());}return *snapshot;}
Catalog load_compiled_snapshot(){return database_snapshot();}
}
extern "C" void bsl_database_snapshot_clear(){snapshot.reset();world.reset();rendering.reset();records.clear();loaded=indexed=false;}
extern "C" int bsl_sequence_probe(const char* route,char* name,int capacity){try{if(!route)throw std::runtime_error("Sequence route missing");auto* s=ps::authoring::world_snapshot().route(route);if(!s)return 0;if(name){if(capacity<=int(s->name.size()))throw std::runtime_error("Sequence name buffer too small");snprintf(name,capacity,"%s",s->name.c_str());}return int(s->levels.size());}catch(const std::exception& e){logfile_message("Sequence registry: %s",e.what());return -1;}}
extern "C" int bsl_sequence_entry(const char* route,int index,char* scene,int capacity){try{auto& registry=ps::authoring::world_snapshot();auto* s=route?registry.route(route):nullptr;if(!s||index<0||index>=int(s->levels.size())||!scene)return 0;auto* level=registry.levels.by_id(s->levels[index]);if(!level||capacity<=int(level->scene.size()))return 0;snprintf(scene,capacity,"%s",level->scene.c_str());return 1;}catch(const std::exception& e){logfile_message("Sequence registry: %s",e.what());return 0;}}
