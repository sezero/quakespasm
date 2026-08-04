/*
Director_* — the only new engine entry points this patch adds. Kept to
three calls so the diff against vanilla stays small and easy to read:
Director_Init (once, from Host_Init), Director_NewLevel (once per level
transition, from the top of SV_SpawnServer), Director_Frame (once per
frame, from _Host_Frame, to poll the background LLM call).
*/
#ifndef _DIRECTOR_HOOK_H_
#define _DIRECTOR_HOOK_H_

void Director_Init (void);
void Director_NewLevel (const char *server);
void Director_Frame (void);

#endif /* _DIRECTOR_HOOK_H_ */
