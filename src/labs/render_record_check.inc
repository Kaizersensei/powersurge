// Shared-record save -> real terrain consumer -> isolated test -> source restore. Signed: President.
namespace {
int render_phase=0,render_checks=0,render_settle=0;bool render_keep=false;fs::path render_source;std::string render_legacy,render_geometry,render_document;PsTerrainMaterialProbe render_before{};auto render_began=std::chrono::steady_clock::now();
void render_check_begin(){render_phase=render_checks=render_settle=0;render_source.clear();render_legacy.clear();render_geometry.clear();render_document.clear();render_before={};render_began=std::chrono::steady_clock::now();render_check=true;}
void render_check_tick(){
 if(!render_check)return;
 auto check=[&](bool okay,const char* why){if(!okay)throw std::runtime_error(why);++render_checks;};
 try {
  if(std::chrono::steady_clock::now()-render_began>std::chrono::seconds(120))throw std::runtime_error("Rendering record journey timed out");
  if(owned_pending()||owned_test.resume_ui||bsl_host_boot_busy()||level_time()<.05f)return;
  PsTerrainMaterialProbe material{};ps_terrain_material_probe("material:stone",7,11,&material);if(!material.present)return;
  if(render_phase==0){
   render_source=run.folder/"content";render_keep=run.keep;render_legacy=read(render_source/"terrain/cell-materials.json");auto* level=ps::authoring::world_snapshot().levels.by_scene(bsl_engine_level_file());check(level!=nullptr,"Current level is not registered");render_document=level->terrain.document;render_geometry=read(render_source/fs::u8path(render_document));render_before=material;check(material.tint[0]==255&&material.rgba[0]>=2,"Unexpected source material fixture");
   char error[512]="";int count=ps_render_record_check(render_source.u8string().c_str(),run.folder.u8string().c_str(),error,sizeof error);check(count>0,error);render_checks+=count;
   check(ps_hub_native_render_edit(0)&&ps_hub_native_render_edit(1),"Shared material edit/save failed");
   ps_terrain_material_probe("material:stone",7,11,&material);check(material.tint[0]==255,"Saving source replaced the active material cache");check(ps::authoring::render_snapshot().libraries.at("material-library:terrain").materials[0]["tint"][0]==255,"Saving source replaced immutable render definitions");
   check(read(render_source/"terrain/cell-materials.json")==render_legacy&&read(render_source/fs::u8path(render_document))==render_geometry,"Material edit changed legacy data or geometry");
   auto project=render_source.u8string();PsLabTestRequest request{1,PS_TEST_LAUNCH,0,generation,project.c_str(),true};PsLabTestReply reply{};check(ps_lab_test_request(&request,&reply)==PS_TEST_ACCEPTED,reply.message);render_phase++;return;
  }
  if(render_phase==1){
   check(owned_test.source&&run.folder/"content"!=render_source,"Material test reused the source root");check(material.tint[0]==128,"Terrain consumer ignored the indexed material library");check(material.rgba[0]==render_before.rgba[0]*128/255&&material.rgba[1]==render_before.rgba[1]&&material.rgba[2]==render_before.rgba[2],"Real decoded terrain texel did not use the authored tint");
   auto& snapshot=ps::authoring::render_snapshot();check(snapshot.lights.count("light:sun")==1&&snapshot.lights.count("light:bevel-default")==1&&snapshot.level_profiles.at("level:cell-lab")=="lighting:outdoor","Lighting definitions absent from session snapshot");check(read(run.folder/"content"/fs::u8path(render_document))==render_geometry,"Test altered terrain coverage source");
   PsLabTestInfo info{};ps_lab_test_info(&info);PsLabTestRequest request{1,PS_TEST_STOP,info.session,generation,info.source,true};PsLabTestReply reply{};check(ps_lab_test_request(&request,&reply)==PS_TEST_ACCEPTED,reply.message);render_phase++;return;
  }
  if(render_phase==2){
   check(!owned_test.source&&run.folder/"content"==render_source,"Stop did not return to source");check(ps_hub_native_render_edit(2),"Cannot restore the source fixture through its shared document");auto source=render_source.u8string();PsLabTestReply reply{};check(ps_lab_edit_level(1,generation,source.c_str(),"level:cell-lab",&reply)==PS_TEST_ACCEPTED,reply.message);render_phase++;return;
  }
  if(render_phase==3){check(material.tint[0]==render_before.tint[0]&&material.rgba[0]==render_before.rgba[0],"Restored source did not restore real texels");check(read(render_source/"terrain/cell-materials.json")==render_legacy&&read(render_source/fs::u8path(render_document))==render_geometry,"Round trip changed compatibility data or geometry");check(ps_hub_native_render_edit(3)&&ps_hub_native_render_edit(4),"Terrain authoring surface did not reopen");render_phase++;return;}
  if(render_phase==4){int count=ps_hub_native_render_edit(5);if(!count)return;check(count>0,"Malformed source left an unbalanced UI stack");render_checks+=count;render_phase++;return;}
  if(render_phase==5){if(++render_settle<5)return;ps::ui::native_capture((run.folder/"render-records.png").u8string().c_str());if(!render_keep){run.keep=false;fs::remove(run.folder/".keep");}run.result=ps::authoring::Json{{"passed",true},{"checks",render_checks},{"authoritative_material_round_trip",true},{"immutable_render_snapshot",true},{"lighting_renderer",false}}.dump();run.completed=true;run.code=0;render_check=false;std::printf("Rendering records and terrain binding checks passed: %d\n",render_checks);if(!run.embedded)engine_quit();else bsl_shell_note("test","Rendering contracts and terrain color binding passed. Lighting rendering remains pending.",0);render_phase++;}
 }catch(const std::exception& e){run.error=e.what();run.code=1;run.completed=true;run.result=ps::authoring::Json{{"passed",false},{"checks",render_checks},{"error",e.what()}}.dump();render_check=false;std::fprintf(stderr,"Rendering record journey failed: %s\n",e.what());engine_quit();}
}
}
