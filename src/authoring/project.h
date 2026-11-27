/* Durable project commands; borrowed strings, main thread only. Signed: President. */
#ifndef PS_PROJECT_H
#define PS_PROJECT_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
enum PsProjectAction { PS_PROJECT_NEW=1, PS_PROJECT_OPEN=2, PS_PROJECT_COPY=3, PS_PROJECT_SAVE=4 };
typedef struct PsProjectRequest {int version,action;unsigned long long generation;const char* path;const char* name;} PsProjectRequest;
typedef struct PsProjectReply {int status;char message[1024];} PsProjectReply;
typedef struct PsProjectInfo {bool persistent,pending;unsigned long long generation;char root[8192],name[256],identity[256];} PsProjectInfo;
int ps_project_request(const PsProjectRequest*,PsProjectReply*);
void ps_project_info(PsProjectInfo*);
int ps_project_recent(int index,char* path,unsigned capacity);
void ps_project_prompt(int action);
void ps_project_ui(void);
bool ps_project_ui_open(void);
void ps_project_ui_reset(void);
#ifdef __cplusplus
}
#endif
#endif
