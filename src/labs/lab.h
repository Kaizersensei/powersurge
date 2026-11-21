/* Native disposable laboratory boundary. Signed: President. */
#ifndef POWERSURGE_LAB_H
#define POWERSURGE_LAB_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
int ps_lab_prepare(int* argc, char*** argv);
bool ps_lab_active(void);
bool ps_lab_automatic(void);
const char* ps_lab_preferences(void);
void ps_lab_tick(void);
int ps_lab_finish(void);
int ps_lab_exit_code(void);
bool ps_lab_spawn(const char* id);
#ifdef __cplusplus
}
#endif
#endif
