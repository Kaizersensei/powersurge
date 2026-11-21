/* Owned native loading/video presentation. Signed: Atlas. */
#ifndef BSL_VIDEO_PRESENTATION_H
#define BSL_VIDEO_PRESENTATION_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void bsl_presentation_loading_begin(void);
bool bsl_presentation_loading_frame(double progress);
void bsl_presentation_loading_pump(void);
void bsl_presentation_loading_end(void);
void bsl_presentation_play_logo(void);
void bsl_presentation_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
