/* Native disposable laboratory boundary. Signed: President. */
#ifndef POWERSURGE_LAB_H
#define POWERSURGE_LAB_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
enum PsLabTestAction { PS_TEST_LAUNCH=1, PS_TEST_RELOAD=2, PS_TEST_STOP=3 };
enum PsLabTestStatus { PS_TEST_ACCEPTED=0, PS_TEST_INVALID=1, PS_TEST_UNAVAILABLE=2, PS_TEST_DIRTY=3, PS_TEST_CONFIRM=4, PS_TEST_FAILED=5 };
typedef struct PsLabTestRequest { int version,action; unsigned long long session,generation; const char* project; bool discard; } PsLabTestRequest;
typedef struct PsLabTestReply { int status; char message[512]; } PsLabTestReply;
typedef struct PsLabTestInfo { bool active,pending,source_retained; unsigned reloads; unsigned long long session,generation; char source[8192],level[512],message[512],report[8192],snapshot[32]; } PsLabTestInfo;
int ps_lab_test_request(const PsLabTestRequest*, PsLabTestReply*);
int ps_lab_test_sequence(const PsLabTestRequest*, const char* sequence, PsLabTestReply*);
void ps_lab_test_info(PsLabTestInfo*);
int ps_lab_edit_level(int version,unsigned long long generation,const char* project,const char* level,PsLabTestReply*);
bool ps_lab_retain_project(const char* project,const char* reason);
int ps_lab_prepare(int* argc, char*** argv);
bool ps_lab_active(void);
bool ps_lab_automatic(void);
const char* ps_lab_preferences(void);
const char* ps_lab_snapshot_source(void); /* explicit authoring lineage, empty outside a staged test */
void ps_lab_tick(void);
int ps_lab_finish(void);
int ps_lab_exit_code(void);
struct commandline_t;
bool ps_lab_spawn(const char* id); /* confirmed same-instance request; retained ABI name */
bool ps_lab_apply_switch(struct commandline_t* cmd);
unsigned long long ps_lab_generation(void);
void ps_lab_mark_saved(void);
void ps_lab_retain_authoring(const char* reason);
bool ps_lab_test_project(const char* root);
#ifdef __cplusplus
}
#endif
#endif
