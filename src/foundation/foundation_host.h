/* Surge boundary for the native data services. Signed: Atlas. */
#ifndef BSL_FOUNDATION_HOST_H
#define BSL_FOUNDATION_HOST_H
#include <stdbool.h>
#include <stdint.h>
typedef struct BorActor BorActor;
#ifdef __cplusplus
extern "C" {
#endif
/* Opt-in single-player ownership. Legacy candidates retain their original routing. */
bool bsl_gameplay_enabled(void);
bool bsl_host_controlled(uint64_t lifetime);
/* Read-only registry metadata, independent of a world actor. Signed: Mech. */
typedef struct BslCharacterDefinition {char id[129],name[129],model[129];int health;uint64_t actor;bool selected;} BslCharacterDefinition;
int bsl_host_character_definition(const char* definition,BslCharacterDefinition* out);
bool bsl_host_character_at(unsigned index,BslCharacterDefinition* out);

int bsl_host_bind_character(uint64_t lifetime,const char* definition);
int bsl_host_select_character(const char* definition);
int bsl_host_choose_initial_character(const char* definition);
int bsl_host_use_item(const char* definition);
/* Trusted UI adapter; stable player owns the instance, currently controlled character supplies the profile. */
int bsl_host_equipment_set(uint64_t actor,const char* instance,bool equipped);
typedef struct BslEquipmentProbe {bool enabled,present;unsigned equipped;int64_t base,effective;} BslEquipmentProbe;
void bsl_host_equipment_probe(const char* stat,BslEquipmentProbe* out);

typedef struct BslGameplayProbe { uint64_t actor; int health,items; char player[129],character[384]; } BslGameplayProbe;
void bsl_host_gameplay_probe(BslGameplayProbe* out);
/* Action snapshots and accepted melee outcomes. Stat units and optional XP scales are authored. */
int bsl_host_action_begin(uint64_t source,uint64_t action,const char* tag);
void bsl_host_combat_result(uint64_t source,uint64_t action,uint64_t target,uint64_t episode,int actual,bool defeated,int bonus);
typedef struct BslStatProbe {bool present;int64_t units;uint64_t scale,total,into,next,stage;int last_damage,last_bonus;} BslStatProbe;
void bsl_host_stat_probe(const char* stat,BslStatProbe* out);
/* Registered trusted-script reward; caller supplies a stable session receipt, not a selected character ID. */
int bsl_host_reward(uint64_t actor,const char* source,const char* receipt);
int bsl_host_ability_use(uint64_t actor,unsigned slot);
/* Traversal asks for an earned capability; it does not execute an attack animation. Signed: Mech. */
bool bsl_host_capability_allowed(uint64_t actor,const char* capability);
typedef struct BslLearningProbe {bool present,learned;unsigned receipts,refused;int last_reward_code;char ability[129],animation[129];} BslLearningProbe;
void bsl_host_learning_probe(BslLearningProbe* out);
int bsl_host_checkpoint_activate(uint64_t actor,const char* definition);
int bsl_host_checkpoint_request(void);
typedef struct BslCheckpointProbe {bool enabled,temporary,save_offer,requested;unsigned captures,rewinds,failures;char chosen[129],restored[129];} BslCheckpointProbe;
void bsl_host_checkpoint_probe(BslCheckpointProbe* out);
/* Separate durable selected progress slot; explicit actions, never automatic on checkpoint activation. */
int bsl_host_progress_save(void);
int bsl_host_progress_load(void);
int bsl_host_progress_recover(void);
typedef struct BslProgressProbe {bool enabled,pending,reward;int choices;unsigned saves,loads,failures;} BslProgressProbe;
void bsl_host_progress_probe(BslProgressProbe* out);
/* Read-only data-authored calendar diagnostics; no parallel mutable date. */
/* Canonical read-only simulation clock; available even without a calendar. Signed: Mech. */
typedef struct BslWorldTimeProbe {bool available,paused;uint64_t generation;double simulation,ui;} BslWorldTimeProbe;
void bsl_host_world_time_probe(BslWorldTimeProbe* out);
typedef struct BslCalendarProbe {bool enabled,paused;double elapsed,simulation,ui,factor;int64_t year;int month,day,hour,minute,second;char phase[129],month_name[129];} BslCalendarProbe;
void bsl_host_calendar_probe(BslCalendarProbe* out);
/* Test-only policy exercise, disabled without BSL_CALENDAR_TEST. */
int bsl_host_calendar_test_policy(int mode);
void bsl_bor_calendar_test_tick(void);
/* Generic door service; actor coordinates use the feet reference. */
int bsl_host_teleport_request(uint64_t actor,const char* door,bool fresh_action);
typedef struct BslTeleportProbe {bool enabled,busy,completion_context;unsigned arrivals,failures;double source_x,source_y,target_x,target_y;char source[201],target[201],door[129];} BslTeleportProbe;
void bsl_host_teleport_probe(BslTeleportProbe* out);
bool bsl_bor_teleport_site(uint64_t placement,double* x,double* y);
/* Definition defaults are independent of the currently enabled gameplay services. */
bool bsl_host_teleport_default(uint64_t placement,const char* field,double* value);
typedef struct BslDoorLive {double x,y,width,height,origin_x,origin_y;bool require_action,preserve_velocity,enabled;int facing;} BslDoorLive;
bool bsl_bor_teleport_live(uint64_t placement,BslDoorLive* out);
bool bsl_bor_teleport(uint64_t actor,double x,double y,double vx,double vy,int facing);
bool bsl_bor_teleport_action(void);
/* Fresh destination boot; independent from ordinary in-place checkpoint recovery. */
void bsl_host_level_begin(const char* level);
bool bsl_host_boot_step(void);
bool bsl_host_boot_busy(void);
typedef struct BslBootProbe {bool enabled,busy,observed_reward,mutations_refused;unsigned arrivals,failures,observations,observed_actors;int observed_items;uint64_t observed_xp;char level[201];} BslBootProbe;
void bsl_host_boot_probe(BslBootProbe* out);
bool bsl_host_progress_flag(const char* id);
int bsl_host_progress_variable(const char* id);
void bsl_host_boot_observe(void);
/* Fixed authored stack pickups: -1 means no pickup context; other codes consume the fresh trigger edge. */
int bsl_host_pickup_try(uint64_t actor);
bool bsl_host_pickup_available(uint64_t placement);
const char* bsl_host_pickup_label(uint64_t placement);
bool bsl_bor_pickup_site(uint64_t placement,double* x,double* y);
bool bsl_bor_pickup_ready(uint64_t actor);
/* Transient enemy stack loot; diagnostics expose no mutation/award authority. */
typedef struct BslLootProbe {bool enabled;unsigned outcomes,pending,collected,empty,refused;uint32_t random;uint64_t epoch;} BslLootProbe;
typedef struct BslLootView {uint64_t victim;int quantity;double x,y,range;char item[129];} BslLootView;
void bsl_host_loot_probe(BslLootProbe* out);
bool bsl_host_loot_view(unsigned index,BslLootView* out);
void bsl_ui_loot_marker(double x,double y,const char* label);




bool bsl_bor_checkpoint_site(uint64_t placement,bool permanent,double* x,double* y);
bool bsl_bor_checkpoint_spot(uint64_t actor,double x,double y,bool require_support,double* safe_x,double* safe_y);
typedef struct BslActorRewind {uint64_t lifetime;int health;double x,y;} BslActorRewind;
/* Native-only temporary body ownership; no live roster/input binding until rewind commits. Signed: Mech. */
typedef struct BslCharacterReconstruction BslCharacterReconstruction;
BslCharacterReconstruction* bsl_bor_checkpoint_character_prepare(const char* definition,double x,double y);
uint64_t bsl_bor_checkpoint_character_actor(const BslCharacterReconstruction* prepared);
void bsl_bor_checkpoint_character_publish(BslCharacterReconstruction* prepared);
void bsl_bor_checkpoint_character_discard(BslCharacterReconstruction* prepared);
typedef struct BslRewindPlan BslRewindPlan;
BslRewindPlan* bsl_bor_rewind_prepare(const BslActorRewind* actors,unsigned count,uint64_t selected);
void bsl_bor_rewind_discard(BslRewindPlan* plan);
void bsl_bor_rewind_apply(BslRewindPlan* plan,const BslActorRewind* actors,unsigned count,uint64_t selected,double x,double y,int facing);
bool bsl_bor_ability(uint64_t actor,const char* animation,const char* tag);
/* Explicit test-only reconstruction, disabled outside BSL_PROGRESSION_TEST. */
int bsl_host_stat_test_seed(const char* stat,uint64_t xp);
typedef struct BslPlayerAssemblyProbe { bool enabled,required,ready,failed,retry_pending; unsigned waiting,retries; char character[129],message[512]; } BslPlayerAssemblyProbe;
void bsl_host_player_required(bool required);
void bsl_host_player_assembly_probe(BslPlayerAssemblyProbe* out);
bool bsl_host_player_retry(void);
bool bsl_host_player_take_retry(void);
bool bsl_host_player_assembly_waiting(void);
void bsl_host_enable(void);
void bsl_host_shutdown(void);
/* Independent Level Select tests start a fresh disposable run, never story travel. */
bool bsl_host_begin_lab_run(void);
void bsl_host_reset(void);
void bsl_host_bind(uint64_t lifetime,BorActor* actor);
void bsl_host_unbind(uint64_t lifetime);
void bsl_host_hit(uint64_t lifetime);
void bsl_host_use_assigned(uint64_t lifetime,uint64_t action);
bool bsl_host_input_locked(uint64_t lifetime);
bool bsl_host_preupdate(void);
bool bsl_host_captures_commands(void);
void bsl_host_render(void);
void bsl_host_simulation_tick(void);
typedef struct BslCommerceProbe {bool enabled;int balance,stock;} BslCommerceProbe;
void bsl_host_commerce_probe(BslCommerceProbe* out);
/* Read-only quest diagnostics over authoritative switches/inventory/reward claims. */
typedef struct BslQuestProbe {bool enabled;int status,met,total;} BslQuestProbe;
void bsl_host_quest_probe(const char* quest,BslQuestProbe* out);
/* Reusable actor-owned dialogue; the main game menu remains single-player. Signed: Mech. */
typedef struct BslConversationProbe {bool open,actor_owned;int state,choice,run_status;uint64_t initiator,speaker;char definition[129],owner[64],player[129],character[257],run[64];} BslConversationProbe;
/* Optional authored actor bindings; runtime actor identity is never saved. Signed: Mech. */
/* Stable placement diagnostics; no world/actor snapshot is persisted. Signed: Mech. */
enum BslActorPlacementStatus {BSL_ACTOR_PLACEMENT_PENDING=0,BSL_ACTOR_PLACEMENT_BOUND=1,BSL_ACTOR_PLACEMENT_MISSING=2,BSL_ACTOR_PLACEMENT_WRONG_OBJECT=3,BSL_ACTOR_PLACEMENT_TRANSIENT=4,BSL_ACTOR_PLACEMENT_NO_CONTROLLER=5,BSL_ACTOR_PLACEMENT_REFUSED=6};
typedef struct BslActorPlacementProbe {uint64_t actor;unsigned attempts;int status;char id[129],placement[17],interaction[129],object[128],level[257];} BslActorPlacementProbe;
unsigned bsl_host_actor_placement_count(void);
bool bsl_host_actor_placement_probe(unsigned index,BslActorPlacementProbe* out);
int bsl_host_actor_placement_reset(uint64_t actor);
int bsl_level_actor_placement_resolve(const char* placement,const char* expected,uint64_t* actor);
uint64_t bsl_bor_entity_actor(uint32_t handle);
/* Generic actor definitions are independent of the player roster. Signed: Mech. */
typedef struct BslActorTemplate {char id[129],name[129],model[129],interaction[129];bool passive;} BslActorTemplate;
bool bsl_host_actor_template_definition(const char* id,BslActorTemplate* out);
bool bsl_host_actor_template_at(unsigned index,BslActorTemplate* out);
int bsl_host_actor_template_bind(uint64_t actor,const char* id);
const char* bsl_host_actor_template_for(uint64_t actor);
const char* bsl_bor_actor_template_id(uint64_t actor);
bool bsl_bor_actor_template_record(uint64_t actor,const char* id,bool passive);
int bsl_host_actor_interaction_bind(uint64_t actor,const char* definition);
const char* bsl_host_actor_interaction_definition(uint64_t actor);
uint64_t bsl_host_actor_interaction_nearest(uint64_t initiator,double* distance_squared);
int bsl_host_interaction_dispatch(uint64_t initiator,uint64_t anchor,const char* event,double distance_squared);
int bsl_host_conversation_open(uint64_t initiator,uint64_t speaker,const char* definition,double range,double calendar_factor);
int bsl_host_conversation_choose(uint64_t initiator,int choice);
int bsl_host_conversation_cancel(uint64_t participant);
void bsl_host_conversation_probe(BslConversationProbe* out);
bool bsl_host_conversation_locked(uint64_t actor);
bool bsl_bor_conversation_actor_live(uint64_t actor);
bool bsl_bor_conversation_actor_ready(uint64_t actor);
/* Placement event adapter: catalog discovery works without activating the lab. */
int bsl_host_event_count(void);
const char* bsl_host_event_id(int index);
bool bsl_host_event_exists(const char* definition);
uint64_t bsl_host_interactor(double* x,double* y);
uint64_t bsl_host_interaction_take(double* x,double* y);
void bsl_host_interaction_reset(void);
int bsl_host_anchor_start(uint64_t placement,const char* definition,uint64_t player_lifetime);
void bsl_host_anchor_cancel(uint64_t placement);
typedef struct BslAnchorRun { int status; unsigned next; char owner[64],player[64]; int error; } BslAnchorRun;
void bsl_host_anchor_probe(uint64_t placement,BslAnchorRun* out);
void bsl_level_events_tick(void);
/* Local sequence helper: no database definition is required. */
void bsl_level_sequence_exit_tick(void);
void bsl_level_sequence_exit_reprime(void);
int bsl_host_prepare_sequence_exit(uint64_t actor);
bool bsl_bor_sequence_exit_ready(uint64_t actor);
bool bsl_bor_sequence_exit_action_held(void);

bool bsl_bor_switch_ready(uint64_t lifetime);
void bsl_bor_capture_input(void);
bool bsl_bor_input_held(void);
typedef struct BslWorldProbe { uint64_t tick; double platform_x,platform_y; int actor_count; } BslWorldProbe;
void bsl_bor_probe(BslWorldProbe* probe);
void bsl_ui_draw(const char* status,const char* title,const char* body,double clock);
void bsl_ui_release(void);
typedef struct BslInventoryCell { const char* name; int quantity; bool equipped,assigned; } BslInventoryCell;
typedef struct BslMenuView { int page, row, home_row, hp, max_hp; double calendar; const char* name; const char* assigned; const char* title; const char* body; const char* calendar_month; const char* calendar_day; const char* calendar_year; const char* calendar_time; const BslInventoryCell* items; int item_count,selected,slot_limit; } BslMenuView;
int bsl_ui_menu_option_count(void);
int bsl_ui_menu_option_page(int row);
const char* bsl_ui_menu_page_title(int page);
int bsl_ui_inventory_columns(void);
int bsl_host_inventory_resize(uint64_t actor,int slots,int rows);
int bsl_ui_inventory_default_slots(void);
typedef struct BslLevelProbe {bool enabled,registered,editable,indoor;double wind_factor,precipitation_factor;char id[129],scene[257],parent[129],name[129],country[129],locality[129],weather_region[129],hemisphere[8],weather_preset[129];} BslLevelProbe;
void bsl_host_level_probe(BslLevelProbe* out);
bool bsl_host_level_editable(void);
unsigned bsl_host_level_count(void);
bool bsl_host_level_definition(const char* id,BslLevelProbe* out);
/* Level-owned terrain documents; empty/unregistered scenes use native bricks unchanged. */
typedef struct BslLevelTerrain {char id[129],document[257],materials[257];} BslLevelTerrain;
bool bsl_host_level_terrain(const char* scene,BslLevelTerrain* out);


/* Owned weather ambience: separate voices, shared SFX/master mixer. */
typedef struct BslAmbientProbe {bool paused;unsigned voices,playing,starts,stops,failures,position;uint64_t first_voice;double gain;} BslAmbientProbe;
void bsl_host_ambient_probe(BslAmbientProbe* out);
void bsl_ambient_tick(double simulation_delta,bool paused);
void bsl_ambient_clear(void);
void bsl_ambient_release(void);
void bsl_bor_weather_test_tick(void);

/* Cosmetic effects: loaded-level ownership, same-thread calls, no save/gameplay state. */
typedef struct BslParticleSpawn {uint64_t owner,seed;double x,y,width,height,multiplier;bool looping;} BslParticleSpawn;
typedef struct BslParticleProbe {unsigned instances,particles,batches,vertices;uint64_t dropped,weather_handle;double update_ms,submit_ms;char weather_effect[129];} BslParticleProbe;
uint64_t bsl_host_particle_start(const char* effect,const BslParticleSpawn* spawn);
bool bsl_host_particle_stop(uint64_t handle,bool cancel);
void bsl_host_particle_cancel_owner(uint64_t owner);
void bsl_host_particle_probe(BslParticleProbe* out);
void bsl_particles_tick(double simulation_delta);
void bsl_particles_render(void);
void bsl_particles_clear(void);
void bsl_particles_release(void);

typedef struct BslWeatherProbe {bool enabled,indoor;int icon,daytime;double wind,direction,precipitation,dust_rate,particle_speed,wind_speed;char region[129],location[129],preset[129],description[129],kind[16],season[129],hemisphere[8];} BslWeatherProbe;
void bsl_host_weather_probe(BslWeatherProbe* out);
/* Same-thread engine ABI. Temporary overrides expire on level load, rewind or reload. */
typedef struct BslWeatherOverride {const char* preset;const char* hemisphere;double wind_factor,precipitation_factor;bool wind_override,precipitation_override;double wind,direction,precipitation;} BslWeatherOverride;
int bsl_host_weather_override(const BslWeatherOverride* request);
int bsl_host_weather_reset(void);


typedef struct BslUiProbe { double menu,dialogue; int message_top,message_bottom,message_target_bottom; } BslUiProbe;
void bsl_ui_update(double clock,bool menu,int page,bool dialogue,bool details);
void bsl_ui_reset_presentation(void);
bool bsl_ui_menu_visible(void);
bool bsl_ui_dialogue_visible(void);
void bsl_ui_menu(const BslMenuView* view);
void bsl_ui_probe(BslUiProbe* probe);
bool bsl_ui_reduced_motion(void);
void bsl_ui_toggle_motion(void);
int bsl_bor_event_spawn_character(const char* definition,double x,double y);
int bsl_bor_event_spawn_actor(const char* definition,double x,double y);
void bsl_engine_quit(void);
const char* bsl_engine_data_dir(void);
const char* bsl_engine_level_file(void);
const char* bsl_engine_level_path(void);
void bsl_ui_cycle_layout(void);
int bsl_ui_layout_index(void);
void bsl_ui_choice_view(const char* const* labels,int count,int selected,int* first,int* last);
typedef struct BslOwnedUiProbe {bool owned,compact;int x,y,w,h;float text_scale;int body_pixels,title_pixels,footer_pixels;} BslOwnedUiProbe;
void bsl_ui_actor_dialogue_context(bool owned,double speaker_x,double speaker_y,bool compact,int viewport_x,int viewport_y,int viewport_width,int viewport_height);
void bsl_ui_actor_dialogue_probe(BslOwnedUiProbe* out);
void bsl_ui_conversation(const char* message,const char* const* choices,int count,int selected,int first,const char* details);
double bsl_engine_delta(void);
bool bsl_engine_editor(void);
void bsl_engine_restart(void);
bool bsl_engine_has_level(const char* path);
void bsl_engine_load_level(const char* path);
void bsl_engine_error(const char* message);
#ifdef __cplusplus
}
#endif
#endif
