void *Sys_Input_Init(Display *x_disp, Window x_win, int x_shmeventtype);
void Sys_Input_Shutdown(void *inputdata);
void Sys_Input_GetEvents(void *inputdata);
void Sys_Input_WaitForShmMsg(void *inputdata);
void Sys_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey);


