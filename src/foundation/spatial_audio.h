// Shared native / SurgeScript acoustic and owned-voice boundary. Signed: Atlas.
#ifndef BSL_SPATIAL_AUDIO_H
#define BSL_SPATIAL_AUDIO_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BslVoiceProbe {bool present,playing;unsigned position;double gain,rate,balance,cutoff;} BslVoiceProbe;
uint64_t bsl_voice_create(const char* path,uint64_t owner,bool looping);
/* Cached calls fail without loading when a sample is not already retained. Signed: Mech. */
uint64_t bsl_voice_create_cached(const char* path,uint64_t owner,bool looping);
typedef struct BslCachedVoiceProbe {uint64_t acquired,released,decoded;unsigned active;} BslCachedVoiceProbe;
void bsl_voice_cached_probe(BslCachedVoiceProbe* out);
bool bsl_voice_set(uint64_t voice,double gain,double rate,double balance,double cutoff,bool playing,double dt);
bool bsl_voice_probe(uint64_t voice,BslVoiceProbe* out);
double bsl_voice_duration(uint64_t voice);
void bsl_voice_destroy(uint64_t voice);
void bsl_voice_cancel_owner(uint64_t owner);
void bsl_voice_release(void);
/* World pause preserves each voice's own requested playback state. Signed: Mech. */
void bsl_voice_pause(bool paused);
bool bsl_voice_paused(void);
typedef struct BslOwnedVoiceSnapshot {uint64_t id,owner;BslVoiceProbe voice;} BslOwnedVoiceSnapshot;
unsigned bsl_voice_snapshot(BslOwnedVoiceSnapshot* out,unsigned capacity);
void bsl_master_probe(float* reduction,float* peak);
/* Gameplay bands, arrival and thresholds are independent of output mixing/camera. */
typedef struct BslAcousticResult {bool resolved;uint64_t event,revision;double low,mid,high,x,y,length;unsigned walls,bends;} BslAcousticResult;
typedef struct BslAcousticProbe {unsigned sources,voices,chunks,expansions,deferred,saturated;uint64_t revision,cache_hits,invalidations,node_limit;double update_ms,terrain_ms,search_ms,playback_ms,hearing_ms;bool geometry_changed;} BslAcousticProbe;
uint64_t bsl_acoustic_emit(uint64_t owner,double x,double y,uint32_t category,double low,double mid,double high,double range,double lifetime,bool noise);
bool bsl_acoustic_hear(uint64_t listener,double x,double y,uint32_t categories,double threshold,BslAcousticResult* out);
bool bsl_acoustic_sample(uint64_t event,double x,double y,BslAcousticResult* out);
void bsl_acoustic_move(uint64_t event,double x,double y);
void bsl_acoustic_remove(uint64_t event);
uint64_t bsl_spatial_play(const char* recipe,uint64_t owner,double x,double y);
uint64_t bsl_spatial_play_cached(const char* recipe,uint64_t owner,double x,double y);
/* Native sample adapter: refuses missing/ambiguous, looping or motion-driven recipes. */
uint64_t bsl_spatial_play_cached_path(const char* path,uint64_t owner,double x,double y);
void bsl_spatial_move(uint64_t handle,double x,double y,double speed,bool airborne);
void bsl_spatial_stop(uint64_t handle);
bool bsl_spatial_configure(uint64_t handle,double gain,double rate);
uint64_t bsl_spatial_voice_id(uint64_t handle);
bool bsl_spatial_probe(uint64_t handle,BslVoiceProbe* out);
/* Current resolved propagation only; dirty/stale paths return false. Signed: Mech. */
bool bsl_spatial_result(uint64_t handle,BslAcousticResult* out);
void bsl_acoustic_tick(double dt,bool paused);
void bsl_acoustic_render(void);
void bsl_acoustic_reaction(uint64_t owner,int kind,double x,double head_y);
void bsl_acoustic_reaction_move(uint64_t owner,double x,double head_y);
typedef struct BslReactionProbe {int kind,queued;double age,alpha,scale,rise;} BslReactionProbe;
bool bsl_acoustic_reaction_probe(uint64_t owner,BslReactionProbe* out);
void bsl_acoustic_clear(void);
void bsl_acoustic_cancel_owner(uint64_t owner);
void bsl_acoustic_probe(BslAcousticProbe* out);
void bsl_acoustic_debug(unsigned mask);
void bsl_acoustic_geometry_dirty(void);
/* Registered runtime blocker: remove with enabled=false. Material0 is transparent. */
void bsl_acoustic_blocker(uint64_t id,double x,double y,double width,double height,unsigned material,bool enabled);
/* Geometry adapter calls these on the game thread only. */
void bsl_acoustic_cell(int x,int y,unsigned material);
void bsl_acoustic_scan_terrain(void);
#ifdef __cplusplus
}
#endif
#endif
