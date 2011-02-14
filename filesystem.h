#include <stdio.h>

#include "qtypes.h"

void FS_InitFilesystem(void);
void FS_ShutdownFilesystem(void);

int FS_FileOpenRead(char *path, FILE **hndl);
void FS_CreatePath(char *path);
qboolean FS_WriteFile(char *filename, void *data, int len);
void FS_SetGamedir(char *dir);
int FS_FOpenFile(char *filename, FILE **file);
void *FS_LoadZFile(char *path);
void *FS_LoadMallocFile(char *path);

void FS_Init(void);

