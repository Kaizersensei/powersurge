/* Existing services exercised with generic character bodies and a learned traversal capability. Signed: Mech. */
static HostActor* journey_character(const char* definition){
 BslCharacterDefinition d={0};if(bsl_host_character_definition(definition,&d)!=0)return NULL;
 for(int i=0;i<64;i++){if(slots[i]&&slots[i]->lifetime==d.actor)return slots[i];}
 return NULL;
}
static void registry_lifecycle_test_tick(void){
 const char* mode=getenv("BSL_JOURNEY_LIFECYCLE");if(!mode)return;
 static int stage,wait,checks,failures;static uint64_t previous_actor;
 if(++wait<60)return;
 HostActor* p=hero();HostActor* primary=journey_character("character:michio");HostActor* alternate=journey_character("character:michio_alternate");
 BslGameplayProbe bag={0};BslCheckpointProbe cp={0};BslProgressProbe save={0};BslStatProbe xp={0};BslTeleportProbe travel={0};BslEquipmentProbe gear={0};
 bsl_host_gameplay_probe(&bag);bsl_host_checkpoint_probe(&cp);bsl_host_progress_probe(&save);bsl_host_stat_probe("stat:experience",&xp);bsl_host_teleport_probe(&travel);bsl_host_equipment_probe("stat:drive",&gear);
#define LIFE_CHECK(expr,message) do{bool ok=(expr);++checks;if(!ok)++failures;logfile_message("JOURNEY %s: %s",ok?"PASS":"FAIL",message);if(!ok)logfile_message("JOURNEY observed mode=%s stage=%d scene=%s actor=%llu hp=%d bag=%d xp=%llu checkpoint=%s rewinds=%u arrivals=%u",mode,stage,bsl_engine_level_file(),(unsigned long long)(p?p->lifetime:0),p?p->a.health:0,bag.items,(unsigned long long)xp.total,cp.chosen,cp.rewinds,travel.arrivals);}while(0)
 if(!p||!primary||!alternate){LIFE_CHECK(false,"all registered character bodies are available after scene assembly");stage=99;}
 if(stage==0&&p){
  checkpoint_park_enemies();LIFE_CHECK(!strcmp(surgescript_object_name(p->entity),"BSL Registry Character"),"lifecycle journey runs through the generic actor class");
  if(!strcmp(mode,"read")){
   LIFE_CHECK(!save.pending&&save.loads==1&&!bsl_host_boot_busy()&&!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev"),"fresh-process Continue routes to the saved annex rest point");
   LIFE_CHECK(!strcmp(bag.character,"player/1/character:michio_alternate")&&bag.health==100&&bag.items==2&&xp.total==0,"saved character selection, bag and independent character values reconstruct together");
   LIFE_CHECK(!strcmp(cp.chosen,"checkpoint:annex")&&fabs(p->a.x-120)<3&&p->a.grounded,"registered rest point reconstructs a supported standing actor");
   LIFE_CHECK(bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump")&&!bsl_host_capability_allowed(alternate->lifetime,"traversal.double_jump"),"fresh-process learning retains the primary unlock without granting it to the alternate");
   LIFE_CHECK(gear.equipped==1&&gear.base==2&&gear.effective==6,"equipped instance restores and derives the alternate stat contribution once");
   LIFE_CHECK(bsl_host_progress_flag("switch:training_reward")&&bsl_host_progress_variable("variable:training_choices")==1,"selected event switch and variable reconstruct before continued gameplay");
   LIFE_CHECK(bsl_host_select_character("character:michio")==0,"continued player can switch to the progressed character");p=hero();
   LIFE_CHECK(p==primary&&p->a.health==100,"inactive primary retains its saved health and independent identity");
   p->a.health=50;LIFE_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==55,"continued inventory effect targets the new live controlled actor");
   LIFE_CHECK(bsl_host_anchor_start(0xca0021,"event:objective_reward",p->lifetime)==0,"continued common event can replay the authored objective safely");
   stage=21;wait=0;
  }else{
   LIFE_CHECK(bag.health==83&&bag.items==3&&xp.total==0&&!bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"fresh run starts from authored values with locked double jump");
   LIFE_CHECK(bsl_host_progress_save()!=0,"durable save refuses before a permanent activation");
   checkpoint_place(p,230,p->a.y);stage=1;wait=0;
  }
 }else if(stage==1){
  p->a.health=70;LIFE_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"temporary checkpoint captures pre-unlock player state");
  LIFE_CHECK(bsl_host_anchor_start(0xca0001,"event:objective_reward",p->lifetime)==0,"authored common event starts the first XP source");stage=2;wait=0;
 }else if(stage==2){
  LIFE_CHECK(xp.total==2500&&bsl_host_progress_flag("switch:training_reward"),"native event runner commits objective XP and its progress switch");
  HostActor* red=equipment_actor("red_grunt");int base=0;
  LIFE_CHECK(red&&checkpoint_strike(p,red,true,1,&base),"real BOR combat supplies the second XP source");
  bsl_host_stat_probe("stat:experience",&xp);LIFE_CHECK(xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"combat and event progression unlock the actual traversal permission");
  checkpoint_place(p,230,p->a.y);p->a.health=70;LIFE_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==75,"post-checkpoint item use changes live HP and bag");
  HostActor* enemy=equipment_actor("Green_Grunt");LIFE_CHECK(enemy&&checkpoint_defeat(p,enemy),"an actual enemy attack defeats the generic controlled character");stage=3;wait=0;
 }else if(stage==3){
  LIFE_CHECK(cp.rewinds==1&&!strcmp(cp.restored,"checkpoint:halfway"),"defeat selects the temporary activation");
  LIFE_CHECK(bag.health==70&&bag.items==3&&xp.total==0&&!bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"pre-unlock checkpoint rewinds health, inventory, XP and learned double jump coherently");
  LIFE_CHECK(!bsl_host_progress_flag("switch:training_reward")&&bsl_host_progress_variable("variable:training_choices")==0,"objective state and reward availability rewind with learning");
  {double vy=p->a.vy;int used=p->jumps_used;LIFE_CHECK(!player_jump(p,0,true,false)&&p->a.vy==vy&&p->jumps_used==used,"rewound double jump is mechanically locked without spending an impulse");}
  checkpoint_park_enemies();LIFE_CHECK(bsl_host_anchor_start(0xca0002,"event:objective_reward",p->lifetime)==0,"rolled-back objective can be earned again through its event");stage=4;wait=0;
 }else if(stage==4){
  HostActor* red=equipment_actor("red_grunt");int base=0;LIFE_CHECK(red&&red->a.health>0&&checkpoint_strike(p,red,true,1,&base),"rewound enemy and its combat reward become available together");
  bsl_host_stat_probe("stat:experience",&xp);LIFE_CHECK(xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"re-earned outcomes restore the same character capability once");
  checkpoint_place(p,120,p->a.y);p->a.health=70;LIFE_CHECK(bsl_host_use_item("item:strawberry")==0,"one consumable commits before the permanent snapshot");
  LIFE_CHECK(bsl_host_equipment_set(p->lifetime,"gameplay/sword/player1",true)==0,"player-owned equipped instance joins the checkpoint bundle");stage=5;wait=0;
 }else if(stage==5){
  LIFE_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&p->a.health==100,"permanent activation recovers and captures earned progress");
  LIFE_CHECK(bsl_host_progress_save()==0,"explicit save publishes the activated checkpoint through the existing verified temporary-slot writer");
  p->a.health=31;LIFE_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==36,"post-save live inventory and health can diverge from the activation");
  LIFE_CHECK(bsl_host_anchor_start(0xca0003,"event:door_unlock",p->lifetime)==0,"door usability is enabled by the registered event");stage=6;wait=0;
 }else if(stage==6){
  checkpoint_place(p,260,p->a.y);previous_actor=p->lifetime;
  LIFE_CHECK(bsl_host_teleport_request(p->lifetime,"door:local",true)==0,"same-level teleporter queues coordinate travel");stage=7;wait=0;
 }else if(stage==7){
  LIFE_CHECK(p->lifetime==previous_actor&&travel.arrivals==1&&!travel.busy,"local teleport does not reload or replace the actor");
  LIFE_CHECK(bag.items==1&&bag.health==36&&xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"local relocation retains current resources and the learned capability");
  checkpoint_place(p,200,p->a.y);LIFE_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"unlocked door starts real cross-level travel");stage=8;wait=0;
 }else if(stage==8){
  checkpoint_park_enemies();LIFE_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev")&&p->lifetime!=previous_actor&&!bsl_host_controlled(previous_actor),"cross-level travel constructs fresh generic actors and retires the old authority");
  LIFE_CHECK(bag.items==1&&bag.health==36&&xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"cross-level travel preserves current health, bag, XP and traversal access");
  LIFE_CHECK(!cp.temporary&&!strcmp(cp.chosen,"checkpoint:rest"),"unload clears temporary memory and keeps the earlier permanent checkpoint eligible");
  LIFE_CHECK(bsl_host_progress_variable("variable:arrivals")==1&&travel.completion_context,"completion event receives its travel context and runs once");
  checkpoint_place(p,200,p->a.y);LIFE_CHECK(bsl_host_teleport_request(p->lifetime,"door:return",true)==0,"origin-aware return uses the captured departure");stage=9;wait=0;
 }else if(stage==9){
  checkpoint_park_enemies();LIFE_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&bag.items==1&&bag.health==36&&xp.total==5000,"return travel preserves live progress rather than rewinding it");
  checkpoint_place(p,200,p->a.y);LIFE_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"repeat travel retains usable registered destinations");stage=10;wait=0;
 }else if(stage==10){
  checkpoint_park_enemies();LIFE_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev"),"repeat outbound travel reaches the annex");
  HostActor* enemy=equipment_actor("Green_Grunt");LIFE_CHECK(enemy&&checkpoint_defeat(p,enemy),"a real hit defeats the player in the destination level");stage=11;wait=0;
 }else if(stage==11){
  checkpoint_park_enemies();LIFE_CHECK(!strcmp(bsl_engine_level_file(),"levels/checkpoint-services.lev")&&!strcmp(cp.restored,"checkpoint:rest")&&cp.rewinds==2,"destination defeat resolves the eligible permanent checkpoint in the previous level");
  LIFE_CHECK(bag.health==100&&bag.items==2&&xp.total==5000&&bsl_host_capability_allowed(p->lifetime,"traversal.double_jump"),"permanent rewind restores its activation-era resources and retained earned jump");
  LIFE_CHECK(!bsl_host_progress_flag("switch:door_open")&&bsl_host_progress_variable("variable:arrivals")==0,"travel-related switches and variables rewind with the same bundle");
  LIFE_CHECK(bsl_host_anchor_start(0xca0004,"event:door_unlock",p->lifetime)==0,"reconstructed services accept a new unlock event");stage=12;wait=0;
 }else if(stage==12){
  checkpoint_place(p,200,p->a.y);LIFE_CHECK(bsl_host_teleport_request(p->lifetime,"door:annex",true)==0,"post-rewind travel uses the rebound actor and services");stage=13;wait=0;
 }else if(stage==13){
  checkpoint_park_enemies();LIFE_CHECK(!strcmp(bsl_engine_level_file(),"levels/teleport-annex.lev"),"post-rewind destination constructs successfully");
  LIFE_CHECK(bsl_host_select_character("character:michio_alternate")==0,"alternate becomes controlled before capturing a different selected character");p=hero();checkpoint_place(p,120,p->a.y);stage=14;wait=0;
 }else if(stage==14){
  LIFE_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:annex")==0&&p->a.health==100,"annex rest point captures the alternate with recovered health");
  LIFE_CHECK(bsl_host_progress_save()==0,"second explicit save replaces the slot while retaining its previous-good backup");
  LIFE_CHECK(!bsl_host_capability_allowed(p->lifetime,"traversal.double_jump")&&bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump"),"save captures independently locked and unlocked character capabilities");
  p->a.health=11;LIFE_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==16,"later live consumption is excluded from the saved activation");
  LIFE_CHECK(bsl_host_reward(p->lifetime,"source:lab_exploration","journey/post-save")==0&&bsl_host_select_character("character:michio")==0,"later XP and selected-character changes remain outside the saved snapshot");stage=99;
 }else if(stage==21){
  LIFE_CHECK(xp.total==5000&&bsl_host_progress_variable("variable:training_choices")==1&&bag.items==1,"replayed loaded objective does not duplicate rewards or overwrite the live bag");
  LIFE_CHECK(player_jump(p,0,false,false),"continued character can begin an ordinary jump");
  {double vx=p->a.vx;LIFE_CHECK(player_jump(p,0,true,false)&&p->jumps_used==2&&!strcmp(p->a.animation->name,"doublejump")&&p->a.vx==vx,"saved learned capability executes the actual second jump in a fresh process");}
  LIFE_CHECK(bsl_host_checkpoint_request()==0,"continued checkpoint can rewind the changed character and item use");stage=22;wait=0;
 }else if(stage==22){
  LIFE_CHECK(!strcmp(cp.restored,"checkpoint:annex")&&!strcmp(bag.character,"player/1/character:michio_alternate")&&bag.health==100&&bag.items==2,"loaded permanent activation restores its selected actor and player bag coherently");
  LIFE_CHECK(!bsl_host_capability_allowed(p->lifetime,"traversal.double_jump")&&bsl_host_capability_allowed(primary->lifetime,"traversal.double_jump"),"loaded checkpoint retains separate learned permissions after another rewind");
  LIFE_CHECK(p->a.state==BOR_FREE&&p->jumps_used==0&&p->a.partner<0&&physicsactor_hlock_timer(p->body)==0,"recovery discards transient traversal, attachments and input locks");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_JOURNEY_LIFECYCLE_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef LIFE_CHECK
}
