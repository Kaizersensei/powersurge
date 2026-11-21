/* Silent Theora decoding; soundtracks use the existing SoLoud owner/limiter.
 * All draw/pump operations run on the display thread. No gameplay clock/state.
 * Signed: Atlas.
 */
#include "video_presentation.h"
#include "../core/asset.h"
#include "../core/engine.h"
#include "../core/video.h"
#include "../core/image.h"
#include "../core/shader.h"
#include "../core/color.h"
#include "../core/logfile.h"
#include "soloud_backend.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_video.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_SIZE 256
#define PRESENTATION_OWNER UINT64_C(0x50524553454e5401)

typedef struct {
    bool loaded, enabled, logo_enabled, skippable, progress;
    char start[PATH_SIZE], loop[PATH_SIZE], logo[PATH_SIZE], audio[PATH_SIZE];
    double minimum, timeout, skip_delay, interval, first_frame_timeout, show_after, minimum_visible;
    float x,y,width,height,bar_x,bar_y,bar_width,bar_height;
    unsigned char red,green,blue,bar_bg[3],bar_fg[3];
} Settings;
static Settings settings;
static _Thread_local bool presentation_thread_owner; /* Reject worker probes before shared reads. Signed: President. */
static ALLEGRO_VIDEO* movie;
static ALLEGRO_BITMAP* loading_cover; /* Retained between loading sequences. Signed: President. */
static ALLEGRO_EVENT_QUEUE* events;
static ALLEGRO_DISPLAY* display;
static uint64_t soundtrack;
static bool active, pumping, intro, finished, skip_requested, fallback, halted;
static bool addon_started, soundtrack_retained, clip_has_frame, loading_open_attempted;
static double load_started, last_pump, last_draw, last_frame_position=-1, last_trace;
static double clip_started, actual_load_seconds, display_seconds, max_pump_gap, first_shown=-1, visible_seconds;
static unsigned loads, starts, loops, frames, distinct_frames, pumps, fallbacks, logos, skips;
static unsigned audio_starts, audio_failures, rejected_audio, closes, waiting_frames, suppressed_loads, shown_loads;
static char phase[24]="idle";
static char error_text[128]="";

static void report(void)
{
    const char* path=getenv("BSL_PRESENTATION_TRACE");
    if(!path||!*path) return;
    FILE* out=fopen(path,"w");
    if(!out) return;
    fprintf(out,"{\"enabled\":%s,\"active\":%s,\"phase\":\"%s\",\"loads\":%u,\"starts\":%u,\"loops\":%u,\"frames\":%u,\"distinct_frames\":%u,\"pumps\":%u,\"fallbacks\":%u,\"logos\":%u,\"skips\":%u,\"soundtrack_starts\":%u,\"soundtrack_failures\":%u,\"rejected_audio\":%u,\"closed_clips\":%u,\"waiting_frames\":%u,\"suppressed_loads\":%u,\"shown_loads\":%u,\"show_after_seconds\":%.6f,\"minimum_visible_seconds\":%.6f,\"visible_seconds\":%.6f,\"video_open\":%s,\"soundtrack_open\":%s,\"allegro_audio_installed\":%s,\"actual_load_seconds\":%.6f,\"presentation_seconds\":%.6f,\"minimum_seconds\":%.6f,\"max_pump_gap_seconds\":%.6f,\"error\":\"%s\"}\n",
        settings.enabled?"true":"false",active?"true":"false",phase,loads,starts,loops,frames,distinct_frames,pumps,fallbacks,logos,skips,audio_starts,audio_failures,rejected_audio,closes,waiting_frames,suppressed_loads,shown_loads,settings.show_after,settings.minimum_visible,visible_seconds,movie?"true":"false",soundtrack?"true":"false",al_is_audio_installed()?"true":"false",actual_load_seconds,display_seconds,settings.minimum,max_pump_gap,error_text);
    fclose(out);
    last_trace=al_get_time();
}
static double number(const ALLEGRO_CONFIG* config,const char* section,const char* name,double def,double low,double high)
{
    const char* s=al_get_config_value(config,section,name);
    if(!s||!*s) return def;
    char* end=NULL;const double n=strtod(s,&end);
    return end&&!*end&&isfinite(n)&&n>=low&&n<=high?n:def;
}
static bool boolean(const ALLEGRO_CONFIG* config,const char* section,const char* name,bool def)
{
    const char* s=al_get_config_value(config,section,name);
    return !s?def:!strcmp(s,"true")||!strcmp(s,"1");
}
static bool media_path(const ALLEGRO_CONFIG* config,const char* section,const char* key,char* out)
{
    const char* s=al_get_config_value(config,section,key);
    out[0]=0;
    if(!s||!*s) return true;
    if(strlen(s)>=PATH_SIZE||s[0]=='/'||strstr(s,"..")||strchr(s,'\\')||strchr(s,':')||strchr(s,'"')||strchr(s,'\n')||strchr(s,'\r')) return false;
    strcpy(out,s);return true;
}
static void load_settings(void)
{
    if(settings.loaded) return;
    settings.loaded=true;
    if(!asset_exists("presentation/presentation.ini")) return;
    ALLEGRO_CONFIG* c=al_load_config_file(asset_path("presentation/presentation.ini"));
    if(!c) return;
    settings.enabled=boolean(c,"presentation","enabled",true);
    settings.logo_enabled=boolean(c,"logo","enabled",false);
    settings.skippable=boolean(c,"logo","skippable",true);
    settings.progress=boolean(c,"loading","show_progress",true);
    settings.minimum=number(c,"loading","minimum_seconds",0,0,30);
    settings.show_after=number(c,"loading","show_after_seconds",.25,0,5);
    settings.minimum_visible=number(c,"loading","minimum_visible_seconds",.25,0,5);
    settings.timeout=number(c,"logo","timeout_seconds",20,.1,120);
    settings.skip_delay=number(c,"logo","skip_delay_seconds",.25,0,10);
    settings.interval=1/number(c,"presentation","frame_hz",60,1,120);
    settings.first_frame_timeout=number(c,"presentation","first_frame_timeout_seconds",3,.1,30);
    settings.x=(float)number(c,"presentation","x",0,0,1);
    settings.y=(float)number(c,"presentation","y",0,0,1);
    settings.width=(float)number(c,"presentation","width",1,.01,1);
    settings.height=(float)number(c,"presentation","height",1,.01,1);
    settings.red=(unsigned char)number(c,"presentation","background_r",0,0,255);
    settings.green=(unsigned char)number(c,"presentation","background_g",0,0,255);
    settings.blue=(unsigned char)number(c,"presentation","background_b",0,0,255);
    settings.bar_x=(float)number(c,"loading","progress_x",.1,0,1);
    settings.bar_y=(float)number(c,"loading","progress_y",.94,0,1);
    settings.bar_width=(float)number(c,"loading","progress_width",.8,.001,1);
    settings.bar_height=(float)number(c,"loading","progress_height",.01,.001,1);
    const char* channels[3]={"r","g","b"};
    for(unsigned i=0;i<3;i++){
        char key[32];snprintf(key,sizeof key,"progress_background_%s",channels[i]);
        settings.bar_bg[i]=(unsigned char)number(c,"loading",key,40,0,255);
        snprintf(key,sizeof key,"progress_foreground_%s",channels[i]);
        settings.bar_fg[i]=(unsigned char)number(c,"loading",key,230,0,255);
    }
    bool paths=media_path(c,"loading","start",settings.start)&&media_path(c,"loading","loop",settings.loop)&&media_path(c,"logo","video",settings.logo)&&media_path(c,"logo","audio",settings.audio);
    if(!paths){settings.enabled=false;strcpy(error_text,"Unsafe presentation media path");}
    al_destroy_config(c);
}
static bool open_events(void)
{
    if(events) return true;
    display=al_get_current_display();
    if(!display) return false;
    events=al_create_event_queue();
    if(!events) return false;
    al_register_event_source(events,al_get_display_event_source(display));
    if(al_is_keyboard_installed()) al_register_event_source(events,al_get_keyboard_event_source());
    if(al_is_mouse_installed()) al_register_event_source(events,al_get_mouse_event_source());
    if(al_is_joystick_installed()) al_register_event_source(events,al_get_joystick_event_source());
    return true;
}
static void close_movie(void)
{
    if(!movie) return;
    if(events) al_unregister_event_source(events,al_get_video_event_source(movie));
    al_close_video(movie);movie=NULL;closes++;
}
static void close_audio(void)
{
    if(soundtrack){bsl_voice_destroy(soundtrack);soundtrack=0;}
    if(soundtrack_retained){bsl_soloud_stream_unload(settings.audio);soundtrack_retained=false;}
}
static void close_events(void)
{
    if(events){al_destroy_event_queue(events);events=NULL;}
    display=NULL;
}
static bool open_movie(const char* path)
{
    close_movie();finished=false;last_frame_position=-1;clip_has_frame=false;
    if(!*path||!asset_exists(path)) return false;
    if(!addon_started){if(!al_init_video_addon()) return false;addon_started=true;}
    if(!open_events()) return false;
    ALLEGRO_STATE old;
    al_store_state(&old,ALLEGRO_STATE_NEW_BITMAP_PARAMETERS);
    al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP|ALLEGRO_MIN_LINEAR|ALLEGRO_MAG_LINEAR);
    movie=al_open_video(asset_path(path));
    al_restore_state(&old);
    if(!movie){strcpy(error_text,"Video open failed");return false;}
    if(al_get_video_audio_rate(movie)>0){rejected_audio++;strcpy(error_text,"Embedded audio refused; use silent video and soundtrack sidecar");close_movie();return false;}
    const float w=al_get_video_scaled_width(movie),h=al_get_video_scaled_height(movie);
    const double fps=al_get_video_fps(movie);
    if(!isfinite(w)||!isfinite(h)||w<=0||h<=0||w>4096||h>4096||!isfinite(fps)||fps<=0||fps>240){strcpy(error_text,"Unsupported video dimensions or frame rate");close_movie();return false;}
    al_register_event_source(events,al_get_video_event_source(movie));
    al_start_video(movie,NULL);
    clip_started=al_get_time();return true;
}
static void poll_events(void)
{
    if(!events) return;
    ALLEGRO_EVENT e;
    while(al_get_next_event(events,&e)){
        if(e.type==ALLEGRO_EVENT_DISPLAY_CLOSE) engine_quit();
        else if(e.type==ALLEGRO_EVENT_DISPLAY_RESIZE) al_acknowledge_resize(e.display.source);
        else if(e.type==ALLEGRO_EVENT_DISPLAY_HALT_DRAWING){halted=true;al_acknowledge_drawing_halt(e.display.source);}
        else if(e.type==ALLEGRO_EVENT_DISPLAY_RESUME_DRAWING){halted=false;al_acknowledge_drawing_resume(e.display.source);}
        else if(e.type==ALLEGRO_EVENT_VIDEO_FINISHED) finished=true;
        else if(e.type==ALLEGRO_EVENT_KEY_DOWN||e.type==ALLEGRO_EVENT_MOUSE_BUTTON_DOWN||e.type==ALLEGRO_EVENT_JOYSTICK_BUTTON_DOWN) skip_requested=true;
        if(ALLEGRO_EVENT_TYPE_IS_USER(e.type)) al_unref_user_event(&e.user);
    }
}
/* The engine rotates its backbuffers on every video_render. Preserve the
 * default-target meaning instead of restoring yesterday's concrete buffer. */
typedef struct {
    ALLEGRO_STATE state;
    image_t* image;
    ALLEGRO_BITMAP* bitmap;
    const shader_t* shader;
    int clip_x,clip_y,clip_w,clip_h;
    bool default_target,held;
} DrawState;
static void save_draw_state(DrawState* out)
{
    out->image=image_drawing_target();out->bitmap=al_get_target_bitmap();
    out->default_target=out->image==video_get_backbuffer()&&out->bitmap==IMAGE2BITMAP(video_get_backbuffer());
    out->shader=shader_get_active();out->held=al_is_bitmap_drawing_held();
    al_get_clipping_rectangle(&out->clip_x,&out->clip_y,&out->clip_w,&out->clip_h);
    al_store_state(&out->state,ALLEGRO_STATE_TRANSFORM|ALLEGRO_STATE_PROJECTION_TRANSFORM|ALLEGRO_STATE_BLENDER|ALLEGRO_STATE_NEW_BITMAP_PARAMETERS);
    if(out->held) al_hold_bitmap_drawing(false);
}
static void restore_draw_state(const DrawState* state)
{
    image_set_drawing_target(state->default_target?video_get_backbuffer():state->image);
    if(!state->default_target) al_set_target_bitmap(state->bitmap);
    al_restore_state(&state->state);
    shader_set_active(state->shader);
    al_set_clipping_rectangle(state->clip_x,state->clip_y,state->clip_w,state->clip_h);
    if(state->held) al_hold_bitmap_drawing(true);
}
static bool draw_frame(ALLEGRO_BITMAP* frame,float fw,float fh,double progress)
{
    if(halted) return true;
    if(active&&first_shown<0){first_shown=al_get_time();shown_loads++;}
    DrawState previous;save_draw_state(&previous);
    const float sw=(float)VIDEO_SCREEN_W,sh=(float)VIDEO_SCREEN_H;
    float rw=fminf(settings.width,1-settings.x)*sw,rh=fminf(settings.height,1-settings.y)*sh;
    const float scale=fminf(rw/fw,rh/fh);
    const float dw=fw*scale,dh=fh*scale;
    image_set_drawing_target(video_get_backbuffer());
    shader_set_active(shader_get_default());
    ALLEGRO_TRANSFORM identity,projection;
    al_identity_transform(&identity);al_use_transform(&identity);
    al_identity_transform(&projection);al_orthographic_transform(&projection,0,0,-1,sw,sh,1);al_use_projection_transform(&projection);
    al_set_blender(ALLEGRO_ADD,ALLEGRO_ONE,ALLEGRO_INVERSE_ALPHA);
    al_set_clipping_rectangle(0,0,(int)sw,(int)sh);
    image_clear(color_rgb(settings.red,settings.green,settings.blue));
    if(frame) al_draw_scaled_bitmap(frame,0,0,al_get_bitmap_width(frame),al_get_bitmap_height(frame),settings.x*sw+(rw-dw)*.5f,settings.y*sh+(rh-dh)*.5f,dw,dh,0);
    if(active&&settings.progress&&isfinite(progress)){
        const float p=(float)fmin(1,fmax(0,progress));
        const int x=(int)(settings.bar_x*sw),y=(int)(settings.bar_y*sh),w=(int)(settings.bar_width*sw),h=(int)fmaxf(1,settings.bar_height*sh);
        image_rectfill(x,y,x+w,y+h,color_rgb(settings.bar_bg[0],settings.bar_bg[1],settings.bar_bg[2]));
        image_rectfill(x,y,x+(int)(w*p),y+h,color_rgb(settings.bar_fg[0],settings.bar_fg[1],settings.bar_fg[2]));
    }
    video_render(NULL);restore_draw_state(&previous);frames++;last_draw=al_get_time();
    if(last_draw-last_trace>=1) report();
    return true;
}
static bool draw_movie(double progress)
{
    if(!movie||halted) return false;
    ALLEGRO_BITMAP* frame=al_get_video_frame(movie);
    if(!frame) return false;
    clip_has_frame=true;
    const double position=al_get_video_position(movie,ALLEGRO_VIDEO_POSITION_VIDEO_DECODE);
    if(position!=last_frame_position){last_frame_position=position;distinct_frames++;}
    return draw_frame(frame,al_get_video_scaled_width(movie),al_get_video_scaled_height(movie),progress);
}
static void retain_loading_cover(void)
{
    if(!movie||!clip_has_frame) return;
    ALLEGRO_BITMAP* frame=al_get_video_frame(movie);
    if(!frame) return;
    DrawState previous;save_draw_state(&previous);
    al_set_new_bitmap_flags(ALLEGRO_VIDEO_BITMAP|ALLEGRO_MIN_LINEAR|ALLEGRO_MAG_LINEAR);
    ALLEGRO_BITMAP* copy=al_clone_bitmap(frame);
    if(copy){if(loading_cover)al_destroy_bitmap(loading_cover);loading_cover=copy;}
    restore_draw_state(&previous);
}
void bsl_presentation_loading_begin(void)
{
    presentation_thread_owner=true;
    load_settings();
    if(!settings.enabled||active) return;
    active=true;intro=true;fallback=false;skip_requested=false;halted=false;
    loading_open_attempted=false;first_shown=-1;visible_seconds=0;
    loads++;load_started=al_get_time();last_pump=load_started;last_draw=-1;
    strcpy(phase,"loading-pending");open_events();report();
}
bool bsl_presentation_loading_frame(double progress)
{
    if(!active||!settings.enabled) return false;
    poll_events();
    if(engine_must_quit()) return true; /* Do not reveal legacy presentation during teardown. */
    if(!loading_open_attempted){
        /* Keep the last presented scene on screen for quick transitions. No decoder
         * or artificial hold is needed unless loading is still busy. Signed: President. */
        if(al_get_time()-load_started<settings.show_after) return true;
        loading_open_attempted=true;strcpy(phase,"loading-start");
        if(open_movie(settings.start)){starts++;}
        else if(open_movie(settings.loop)){intro=false;loops++;strcpy(phase,"loading-loop");}
        else {fallback=true;fallbacks++;strcpy(phase,"loading-fallback");}
        report();
    }
    if(movie&&(finished||!al_is_video_playing(movie))){
        if(open_movie(settings.loop)){intro=false;loops++;strcpy(phase,"loading-loop");report();}
        else {fallback=true;fallbacks++;close_movie();strcpy(phase,"loading-fallback");report();}
    }
    if(!fallback&&draw_movie(progress)) return true;
    if(movie&&!clip_has_frame&&al_get_time()-clip_started>=settings.first_frame_timeout){
        strcpy(error_text,"First video frame timed out");fallback=true;fallbacks++;
        close_movie();strcpy(phase,"loading-fallback");report();
    }
    /* Decoder startup is not media failure. Keep the owned presentation visible,
     * including on very short loads, rather than flashing Surge's stock screen.
     * An unavailable clip uses this same static cover/background. Signed: President. */
    waiting_frames++;
    return draw_frame(loading_cover,loading_cover?(float)al_get_bitmap_width(loading_cover):1,
        loading_cover?(float)al_get_bitmap_height(loading_cover):1,progress);
}
void bsl_presentation_loading_pump(void)
{
    if(!presentation_thread_owner) return;
    /* Asset helpers may also be called off the display thread. Signed: President. */
    if(!active||pumping||al_get_current_display()!=display) return;
    const double now=al_get_time();
    if(now-last_pump<settings.interval) return;
    const double gap=now-last_pump;
    if(gap>max_pump_gap) max_pump_gap=gap;
    last_pump=now;pumps++;pumping=true;
    DrawState previous;save_draw_state(&previous);
    video_display_loading_screen_ex(NAN);
    restore_draw_state(&previous);
    pumping=false;
}
void bsl_presentation_loading_end(void)
{
    if(!active) return;
    actual_load_seconds=al_get_time()-load_started;
    if(first_shown<0) suppressed_loads++;
    else {
        while(!engine_must_quit()&&(al_get_time()-first_shown<settings.minimum_visible||al_get_time()-load_started<settings.minimum)){
            bsl_presentation_loading_pump();al_rest(.001);
        }
        visible_seconds=al_get_time()-first_shown;
    }
    display_seconds=al_get_time()-load_started;
    retain_loading_cover();
    close_movie();close_events();active=false;strcpy(phase,"idle");report();
}
void bsl_presentation_play_logo(void)
{
    presentation_thread_owner=true;
    load_settings();
    if(!settings.enabled||!settings.logo_enabled||!*settings.logo||engine_must_quit()) return;
    bsl_presentation_loading_end();skip_requested=false;halted=false;strcpy(phase,"logo");
    /* Retain soundtrack before starting either timeline; no decoding in the audio callback. */
    if(*settings.audio&&asset_exists(settings.audio)){
        soundtrack_retained=bsl_soloud_stream_preload(settings.audio);
        if(!soundtrack_retained) audio_failures++;
    }
    if(!open_movie(settings.logo)){fallbacks++;strcpy(phase,"logo-failed");close_audio();close_events();report();return;}
    logos++;
    if(soundtrack_retained){
        soundtrack=bsl_soloud_play_scoped(settings.audio,PRESENTATION_OWNER,false,BSL_AUDIO_MUSIC,true);
        if(soundtrack) audio_starts++;else audio_failures++;
    }
    report();
    while(!engine_must_quit()){
        poll_events();
        const double elapsed=al_get_time()-clip_started;
        if(finished||!al_is_video_playing(movie)||elapsed>=settings.timeout) break;
        if(settings.skippable&&skip_requested&&elapsed>=settings.skip_delay){skips++;break;}
        if(al_get_time()-last_draw>=settings.interval) draw_movie(NAN);
        if(!clip_has_frame&&elapsed>=settings.first_frame_timeout){strcpy(error_text,"First video frame timed out");fallbacks++;break;}
        al_rest(.001);
    }
    close_audio();close_movie();close_events();strcpy(phase,"idle");report();
}
void bsl_presentation_shutdown(void)
{
    close_audio();close_movie();close_events();
    if(loading_cover){al_destroy_bitmap(loading_cover);loading_cover=NULL;}
    active=false;strcpy(phase,"shutdown");report();
    if(addon_started){al_shutdown_video_addon();addon_started=false;}
    memset(&settings,0,sizeof settings);
    presentation_thread_owner=false;
}
