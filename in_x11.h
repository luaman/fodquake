void *X11_Input_Init(Display *x_disp, Window x_win, int x_shmeventtype, void (*eventcallback)(void *eventcallbackdata, int type), void *eventcallbackdata);
void X11_Input_Shutdown(void *inputdata);
void X11_Input_GetEvents(void *inputdata);
void X11_Input_WaitForShmMsg(void *inputdata);
void X11_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey);
void X11_Input_GetConfigNotify(void *inputdata, int *config_notify, int *config_notify_width, int *config_notify_height);
void X11_Input_GrabMouse(void *inputdata, int dograb);

