// Selected SoLoud/miniaudio host setup around the existing owned-voice ABI. Signed: Mech.
#pragma once
#include "../host/spatial_audio.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// Asset reads/releases happen only on the caller's control thread, never the audio callback.
typedef struct BslAudioAssets {void* context;const unsigned char* (*read)(void*,const char*,size_t*);void (*release)(void*,const unsigned char*);} BslAudioAssets;
typedef struct BslSoloudInfo {unsigned sample_rate,buffer_frames,cached_samples,voices;uint64_t decoded_bytes;bool offline;const char* backend;} BslSoloudInfo;
bool bsl_soloud_init(BslAudioAssets assets,unsigned sample_rate,bool offline,double ceiling,double release_seconds);
void bsl_soloud_shutdown(void);
bool bsl_soloud_preload(const char* path);
bool bsl_soloud_unload(const char* path);
bool bsl_soloud_info(BslSoloudInfo* out);
// Last post-limiter block. Diagnostic snapshots, not gameplay timing. Signed: Mech.
typedef struct BslAudioLevels {double rms_left,rms_right,peak_left,peak_right;uint64_t frames;} BslAudioLevels;
bool bsl_soloud_levels(BslAudioLevels* out);
typedef struct BslAudioSourceInfo {unsigned channels,sample_rate;double duration;} BslAudioSourceInfo;
bool bsl_soloud_source_info(const char* path,bool streaming,BslAudioSourceInfo* out);

// Native legacy routes share the output but retain separate pause/category semantics. Signed: Mech.
enum BslAudioScope {BSL_AUDIO_OWNED=0,BSL_AUDIO_BANK_A=1,BSL_AUDIO_BANK_B=2,BSL_AUDIO_MUSIC=3};
bool bsl_soloud_stream_preload(const char* path);
bool bsl_soloud_stream_unload(const char* path);
double bsl_soloud_source_duration(const char* path,bool streaming);
uint64_t bsl_soloud_play_scoped(const char* path,uint64_t owner,bool looping,unsigned scope,bool streaming);
bool bsl_soloud_control(uint64_t voice,double gain,double rate,double balance,bool playing);
bool bsl_soloud_repeat_from(uint64_t voice,double seconds);
void bsl_soloud_active_bank(unsigned bank);
unsigned bsl_soloud_current_bank(void);
void bsl_soloud_mix(double master,double sound,double music,bool muted);
void bsl_soloud_muffle(double sound_cutoff,double music_cutoff);
void bsl_soloud_suspend(bool suspended);
void bsl_soloud_collect(void);
// Presentation-only scope monitor: bits1<<scope; muted scopes keep their timelines.
bool bsl_soloud_monitor(unsigned scope_mask);
unsigned bsl_soloud_monitor_mask(void);
// Offline only; refuses device mode so two consumers cannot advance the same mixer.
bool bsl_soloud_render(float* stereo,unsigned frames);
#ifdef __cplusplus
}
#endif
