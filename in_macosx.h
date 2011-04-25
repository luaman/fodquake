
struct input_data *Sys_Input_Init(void);
void Sys_Input_Shutdown(struct input_data *input);
int Sys_Input_GetKeyEvent(struct input_data *input, keynum_t *keynum, qboolean *down);
void Sys_Input_GetMouseMovement(struct input_data *input, int *mouse_x, int *mouse_y);
void Sys_Input_GrabMouse(struct input_data *input, int dograb);

