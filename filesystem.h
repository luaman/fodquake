#include <stdio.h>

void FS_InitFilesystem(void);
int FS_FileOpenRead(char *path, FILE **hndl);
void FS_CreatePath(char *path);
qboolean FS_WriteFile(char *filename, void *data, int len);
void FS_SetGamedir(char *dir);
int FS_FOpenFile(char *filename, FILE **file);
void *FS_LoadStackFile(char *path, void *buffer, int bufsize);
void *FS_LoadTempFile(char *path);
void *FS_LoadHunkFile(char *path);
void FS_LoadCacheFile(char *path, struct cache_user_s *cu);

void FS_Init(void);
