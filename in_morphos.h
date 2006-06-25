#define MAXIMSGS 32

extern char keyconv[];
extern struct EmulLibEntry myinputhandler;

void *Sys_Input_Init(struct Screen *screen, struct Window *window);
void Sys_Input_Shutdown(void *inputdata);
void Sys_Input_GetEvents(void *inputdata);
void Sys_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey);

