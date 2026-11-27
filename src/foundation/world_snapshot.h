// Main-thread immutable session sequence adapter. Signed: President.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* Positive entry count: registered; 0: legacy route; -1: invalid database. Copies outputs. */
int bsl_sequence_probe(const char* route,char* name,int capacity);
int bsl_sequence_entry(const char* route,int index,char* scene,int capacity);
#ifdef __cplusplus
}
#endif
