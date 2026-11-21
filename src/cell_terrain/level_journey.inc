// Connected registered-level verification only; no behavior when unset. Signed: President.
void level_journey_tick(){
 const char* report=std::getenv("BSL_CELL_LEVEL_JOURNEY");if(!report)return;
 struct Observation {std::string scene,id,document,materials;uint64_t actor=0,xp=0;int hp=0,items=0;bool cells=false;};
 static std::vector<Observation> observations;static unsigned phase=0,settled=0,checks=0;static bool done=false;static double started=al_get_time();
 static BslGameplayProbe initial{};static BslStatProbe initial_stat{};static BslWorldTimeProbe initial_time{};
 if(done)return;
 auto finish=[&](bool passed,const char* error){
  std::ofstream out(std::filesystem::u8path(report));out<<"{\"passed\":"<<(passed?"true":"false")<<",\"checks\":"<<checks<<",\"error\":\""<<error<<"\",\"observations\":[";
  for(size_t i=0;i<observations.size();i++){const auto& o=observations[i];if(i)out<<",";out<<"{\"scene\":\""<<o.scene<<"\",\"id\":\""<<o.id<<"\",\"document\":\""<<o.document<<"\",\"materials\":\""<<o.materials<<"\",\"actor\":"<<o.actor<<",\"xp\":"<<o.xp<<",\"hp\":"<<o.hp<<",\"items\":"<<o.items<<",\"cells\":"<<(o.cells?"true":"false")<<"}";}
  out<<"],\"signed\":\"President\"}";done=true;
 };
 try{
  auto check=[&](bool ok,const char* error){if(!ok)throw std::runtime_error(error);checks++;};
  if(al_get_time()-started>30)throw std::runtime_error("connected level journey timed out");
  if(bsl_host_boot_busy()||bsl_cell_editor_active()){settled=0;return;}
  BslGameplayProbe game{};bsl_host_gameplay_probe(&game);if(!game.actor||game.health<=0){settled=0;return;}
  if(++settled<35)return;
  settled=0;
  BslLevelProbe level{};bsl_host_level_probe(&level);BslWorldTimeProbe clock{};bsl_host_world_time_probe(&clock);
  const char* ids[]={"level:cell-lab","level:cell-annex","level:cell-plain","level:cell-lab"};
  check(level.registered&&std::string(level.id)==ids[phase],"registered level did not match travel destination");
  if(phase==0){
   check(bsl_host_reward(game.actor,"source:lab_exploration","terrain/level-journey")==0,"authored progression reward refused");
   bsl_host_stat_probe("stat:experience",&initial_stat);check(initial_stat.present&&initial_stat.total>=2500,"reward did not produce non-default progress");
   initial=game;initial_time=clock;
   BslLevelTerrain none{};check(!bsl_host_level_terrain("levels/unregistered.lev",&none),"unregistered scene inherited cell terrain");
  }else{
   BslStatProbe stat{};bsl_host_stat_probe("stat:experience",&stat);
   check(stat.present&&stat.total==initial_stat.total&&stat.units==initial_stat.units,"progression changed across terrain travel");
   check(game.health==initial.health&&game.items==initial.items&&std::string(game.player)==initial.player&&std::string(game.character)==initial.character,"player or inventory changed across terrain travel");
   check(clock.generation==initial_time.generation&&clock.simulation>=initial_time.simulation,"session or simulation clock reset across terrain travel");
   check(game.actor!=observations.back().actor,"previous scene body remained bound");
  }
  if(phase==2){
   bsl_cell_native_brick_probe();
   check(!active&&resources.empty()&&textures.empty()&&field.blocks.empty()&&undo_stack.empty(),"cell resources leaked into ordinary brick scene");
   check(material_path.empty()&&terrain_level.empty(),"previous cell attachment remained active");
  }else{
   check(active&&terrain_level==ids[phase]&&!resources.empty(),"registered terrain attachment missing");
   if(phase==1){check(path=="terrain/cell-annex.json"&&material_path=="terrain/cell-annex-materials.json","annex did not use its own documents");
    check(field.doc.shapes.size()==1&&field.sample(0,40)!=nullptr&&material("material:moss")->name=="Annex moss","annex geometry or material set retained the previous level");
   }else check(path=="terrain/cell-lab.json"&&material_path=="terrain/cell-materials.json"&&field.doc.shapes.size()==3,"primary terrain base was not reconstructed");
  }
  BslStatProbe stat{};bsl_host_stat_probe("stat:experience",&stat);
  observations.push_back({level.scene,level.id,active?path:"",material_path,game.actor,stat.total,game.health,game.items,active});
  image_t* image=video_take_snapshot();if(image){auto* previous=al_get_new_file_interface();al_set_standard_file_interface();auto file=std::filesystem::u8path(report).parent_path()/("level-"+std::to_string(phase)+".png");al_save_bitmap(file.u8string().c_str(),IMAGE2BITMAP(image));al_set_new_file_interface(previous);image_destroy(image);}
  if(phase==3){BslTeleportProbe travel{};bsl_host_teleport_probe(&travel);check(travel.arrivals==3&&travel.failures==0,"door journey did not complete cleanly");finish(true,"");return;}
  const char* doors[]={"door:cell-annex","door:cell-plain","door:cell-lab"};
  check(bsl_host_teleport_request(game.actor,doors[phase],true)==0,"registered door refused travel");phase++;
 }catch(const std::exception& error){finish(false,error.what());}
}
