// Isolated fixture table, empty for normal builds. Signed: Atlas.
#pragma once
typedef struct {const char* scene;double x,y;int kind;const char* recipe;const char* label;} BslAudioStation;
typedef struct {const char* scene;double x,y,w,h;unsigned material;} BslAudioMaterial;
typedef struct {const char* scene;unsigned id;double x,y,threshold;} BslAudioListener;
static const unsigned bsl_audio_stations_count=0;
static const BslAudioStation bsl_audio_stations[]={{0}};
static const unsigned bsl_audio_materials_count=0;
static const BslAudioMaterial bsl_audio_materials[]={{0}};
static const unsigned bsl_audio_listeners_count=0;
static const BslAudioListener bsl_audio_listeners[]={{0}};
