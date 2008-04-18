/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "filesystem.h"

// in memory
struct packfile
{
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

struct pack
{
	char filename[MAX_OSPATH];
	FILE *handle;
	int numfiles;
	struct packfile *files;
};

struct dpackheader
{
	char id[4];
	int dirofs;
	int dirlen;
};

struct dpackfile
{
	char name[56];
	int filepos, filelen;
};

// on disk
struct searchpath
{
	char filename[MAX_OSPATH];
	struct pack *pack;	// only one of filename / pack will be used
	struct searchpath *next;
};

static struct searchpath *com_searchpaths;
static struct searchpath *com_base_searchpaths;	// without gamedirs

static int FS_FileLength(FILE * f)
{
	int pos, end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

int FS_FileOpenRead(char *path, FILE ** hndl)
{
	FILE *f;

	if (!(f = fopen(path, "rb")))
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return FS_FileLength(f);
}

//The filename will be prefixed by com_basedir
qboolean FS_WriteFile(char *filename, void *data, int len)
{
	FILE *f;
	char name[MAX_OSPATH];

	Q_snprintfz(name, sizeof(name), "%s/%s", com_basedir, filename);

	if (!(f = fopen(name, "wb")))
	{
		FS_CreatePath(name);
		if (!(f = fopen(name, "wb")))
			return false;
	}
	Sys_Printf("FS_WriteFile: %s\n", name);
	fwrite(data, 1, len, f);
	fclose(f);
	return true;
}

//Only used for CopyFile and download


void FS_CreatePath(char *path)
{
	char *s, save;

	if (!*path)
		return;

	for (s = path + 1; *s; s++)
	{
#ifdef _WIN32
		if (*s == '/' || *s == '\\')
		{
#else
		if (*s == '/')
		{
#endif
			save = *s;
			*s = 0;
			Sys_mkdir(path);
			*s = save;
		}
	}
}

//Finds the file in the search path.
//Sets com_filesize, com_netpath and one of handle or file
int file_from_pak;		// global indicating file came from pack file ZOID
int file_from_gamedir;

int FS_FOpenFile(char *filename, FILE ** file)
{
	struct searchpath *search;
	struct pack *pak;
	int i;
	int ret;

	int searchbegin, searchend;

	*file = NULL;
	file_from_pak = 0;
	file_from_gamedir = 1;
	com_filesize = -1;
	com_netpath[0] = 0;

	// search through the path, one element at a time
	for (search = com_searchpaths; search; search = search->next)
	{
		if (search == com_base_searchpaths && com_searchpaths != com_base_searchpaths)
			file_from_gamedir = 0;

		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;

			searchbegin = 0;
			searchend = pak->numfiles - 1;
			for (i = pak->numfiles / 2;; i = searchbegin + ((searchend - searchbegin) / 2))
			{
				ret = strcmp(filename, pak->files[i].name);
				if (ret == 0)
				{
					if (developer.value)
						Sys_Printf("PackFile: %s : %s\n", pak->filename, filename);
					// open a new file on the pakfile
					if (!(*file = fopen(pak->filename, "rb")))
						Sys_Error("Couldn't reopen %s", pak->filename);
					fseek(*file, pak->files[i].filepos, SEEK_SET);
					com_filesize = pak->files[i].filelen;

					file_from_pak = 1;
					Q_snprintfz(com_netpath, sizeof(com_netpath), "%s#%i", pak->filename, i);
					return com_filesize;
				}
				else
				{
					if (searchbegin == searchend)
						break;

					if (ret < 0)
						searchend = i;
					else
						searchbegin = i + 1;
				}
			}
		}
		else
		{
			Q_snprintfz(com_netpath, sizeof(com_netpath), "%s/%s", search->filename, filename);

			if (!(*file = fopen(com_netpath, "rb")))
				continue;

			if (developer.value)
				Sys_Printf("FindFile: %s\n", com_netpath);

			com_filesize = FS_FileLength(*file);
			return com_filesize;
		}
	}

	if (developer.value)
		Sys_Printf("FindFile: can't find %s\n", filename);

	return -1;
}

//Filename are relative to the quake directory.
//Always appends a 0 byte to the loaded data.
static cache_user_t *loadcache;
static byte *loadbuf;
static int loadsize;
byte *FS_LoadFile(char *path, int usehunk)
{
	FILE *h;
	byte *buf;
	char base[32];
	int len;

	buf = NULL;		// quiet compiler warning

	// look for it in the filesystem or pack files
	len = com_filesize = FS_FOpenFile(path, &h);
	if (!h)
		return NULL;

	// extract the filename base name for hunk tag
	COM_FileBase(path, base);

	if (usehunk == 1)
	{
		buf = Hunk_AllocName(len + 1, base);
	}
	else if (usehunk == 2)
	{
		buf = Hunk_TempAlloc(len + 1);
	}
	else if (usehunk == 0)
	{
		buf = Z_Malloc(len + 1);
	}
	else if (usehunk == 3)
	{
		buf = Cache_Alloc(loadcache, len + 1, base);
	}
	else if (usehunk == 4)
	{
		if (len + 1 > loadsize)
			buf = Hunk_TempAlloc(len + 1);
		else
			buf = loadbuf;
	}
	else
	{
		Sys_Error("FS_LoadFile: bad usehunk");
	}

	if (!buf)
		Sys_Error("FS_LoadFile: not enough space for %s", path);

	((byte *) buf)[len] = 0;
#ifndef SERVERONLY
	Draw_BeginDisc();
#endif
	fread(buf, 1, len, h);
	fclose(h);
#ifndef SERVERONLY
	Draw_EndDisc();
#endif

	return buf;
}

void *FS_LoadHunkFile(char *path)
{
	return FS_LoadFile(path, 1);
}

void *FS_LoadTempFile(char *path)
{
	return FS_LoadFile(path, 2);
}

void FS_LoadCacheFile(char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	FS_LoadFile(path, 3);
}

// uses temp hunk if larger than bufsize
void *FS_LoadStackFile(char *path, void *buffer, int bufsize)
{
	byte *buf;

	loadbuf = (byte *) buffer;
	loadsize = bufsize;
	buf = FS_LoadFile(path, 4);

	return buf;
}

static int packfile_name_compare(const void *pack1, const void *pack2)
{
	return strcmp(((const struct packfile *)pack1)->name, ((const struct packfile *)pack2)->name);
}

/*
Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
*/
static struct pack *FS_LoadPackFile(char *packfile)
{

	struct dpackheader header;
	int i;
	struct packfile *newfiles;
	struct pack *pack;
	FILE *packhandle;
	struct dpackfile *info;

	if (FS_FileOpenRead(packfile, &packhandle) == -1)
		return NULL;

	fread(&header, 1, sizeof(header), packhandle);
	if (header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error("%s is not a packfile", packfile);
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	pack = Q_Malloc(sizeof(*pack));
	strcpy(pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = header.dirlen / sizeof(struct dpackfile);

	pack->files = newfiles = Q_Malloc(pack->numfiles * sizeof(struct packfile));
	info = Q_Malloc(header.dirlen);

	fseek(packhandle, header.dirofs, SEEK_SET);
	fread(info, 1, header.dirlen, packhandle);

	// parse the directory
	for (i = 0; i < pack->numfiles; i++)
	{
		strcpy(newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	free(info);

	/* Sort the entries by name to make it easier to search */
	qsort(newfiles, pack->numfiles, sizeof(*newfiles), packfile_name_compare);

	return pack;
}

static void FS_FreePackFile(struct pack *pack)
{
	fclose(pack->handle);
	free(pack);
}

//Sets com_gamedir, adds the directory to the head of the path, then loads and adds pak1.pak pak2.pak ... 
void FS_AddGameDirectory(char *dir)
{
	int i;
	struct searchpath *search;
	struct pack *pak;
	char pakfile[MAX_OSPATH], *p;

	if ((p = strrchr(dir, '/')) != NULL)
		strcpy(com_gamedirfile, ++p);
	else
		strcpy(com_gamedirfile, p);
	strcpy(com_gamedir, dir);

	// add the directory to the search path
	search = Hunk_Alloc(sizeof(*search));
	strcpy(search->filename, dir);
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0;; i++)
	{
		Q_snprintfz(pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		if (!(pak = FS_LoadPackFile(pakfile)))
			break;
		search = Hunk_Alloc(sizeof(*search));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

//Sets the gamedir and path to a different directory.
void FS_SetGamedir(char *dir)
{
	struct searchpath *search, *next;
	int i;
	struct pack *pak;
	char pakfile[MAX_OSPATH];

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":"))
	{
		Com_Printf("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp(com_gamedirfile, dir))
		return;		// still the same
	Q_strncpyz(com_gamedirfile, dir, sizeof(com_gamedirfile));

	// free up any current game dir info
	while (com_searchpaths != com_base_searchpaths)
	{
		if (com_searchpaths->pack)
		{
			fclose(com_searchpaths->pack->handle);
			free(com_searchpaths->pack->files);
			free(com_searchpaths->pack);
		}
		next = com_searchpaths->next;
		free(com_searchpaths);
		com_searchpaths = next;
	}

	// flush all data, so it will be forced to reload
	Cache_Flush();

	Q_snprintfz(com_gamedir, sizeof(com_gamedir), "%s/%s", com_basedir, dir);

	if (!strcmp(dir, "id1") || !strcmp(dir, "qw") || !strcmp(dir, "fuhquake"))
		return;

	// add the directory to the search path
	search = Q_Malloc(sizeof(*search));
	strcpy(search->filename, com_gamedir);
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0;; i++)
	{
		Q_snprintfz(pakfile, sizeof(pakfile), "%s/pak%i.pak", com_gamedir, i);
		if (!(pak = FS_LoadPackFile(pakfile)))
			break;
		search = Q_Malloc(sizeof(*search));
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

void FS_InitFilesystem(void)
{
	int i;

	// -basedir <path>
	// Overrides the system supplied base directory (under id1)
	if ((i = COM_CheckParm("-basedir")) && i < com_argc - 1)
		Q_strncpyz(com_basedir, com_argv[i + 1], sizeof(com_basedir));
	else
		getcwd(com_basedir, sizeof(com_basedir) - 1);

	for (i = 0; i < strlen(com_basedir); i++)
		if (com_basedir[i] == '\\')
			com_basedir[i] = '/';

	i = strlen(com_basedir) - 1;
	if (i >= 0 && com_basedir[i] == '/')
		com_basedir[i] = 0;

	// start up with id1 by default
	FS_AddGameDirectory(va("%s/id1", com_basedir));
	FS_AddGameDirectory(va("%s/fodquake", com_basedir));
	FS_AddGameDirectory(va("%s/qw", com_basedir));

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	// the user might want to override default game directory
	if (!(i = COM_CheckParm("-game")))
		i = COM_CheckParm("+gamedir");
	if (i && i < com_argc - 1)
		FS_SetGamedir(com_argv[i + 1]);
}

void FS_ShutdownFilesystem(void)
{
	struct searchpath *s;
	struct searchpath *next;

	s = com_searchpaths;
	while(s)
	{
		next = s->next;

		if (s->pack)
		{
			FS_FreePackFile(s->pack);
		}

#warning "We can't free the structures because this file uses a mix of allocation routines :/"
#if 0
		free(s);
#endif

		s = next;
	}
}

static void FS_Path_f(void)
{
	struct searchpath *s;

	Com_Printf("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next)
	{
		if (s == com_base_searchpaths)
			Com_Printf("----------\n");
		if (s->pack)
			Com_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf("%s\n", s->filename);
	}
}

void FS_Init()
{
	Cmd_AddCommand("path", FS_Path_f);
}

