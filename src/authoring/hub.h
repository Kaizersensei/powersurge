/* Integrated database workspace. Presentation only; documents own drafts. Signed: President. */
#ifndef PS_AUTHORING_HUB_H
#define PS_AUTHORING_HUB_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Synchronous versioned request; strings are borrowed only for this call. */
typedef struct PsHubRequest { int version; const char* project; const char* record; const char* level; const char* placement; const char* object; } PsHubRequest;
enum PsHubStatus { PS_HUB_OPENED=0, PS_HUB_INVALID=1, PS_HUB_PROJECT_MISMATCH=2, PS_HUB_UNAVAILABLE=3, PS_HUB_NOT_FOUND=4, PS_HUB_FAILED=5 };
typedef struct PsHubReply { int status; char message[512]; } PsHubReply;
int ps_hub_request(const PsHubRequest*, PsHubReply*);
bool ps_hub_has_drafts(void);
bool ps_hub_release_project(void);
bool ps_hub_save_all(void);
void ps_hub_test_prompt(int action);
void ps_hub_resume_authoring(bool editor);
void ps_hub_suspend_for_transition(void);
void ps_hub_toggle(void);
bool ps_hub_open(void);
void ps_hub_frame(void);
void ps_hub_shutdown(void);
void bsl_database_snapshot_clear(void);
#ifdef __cplusplus
}
#endif
#endif
