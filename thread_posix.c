/*
Copyright (C) 2008-2009 Mark Olsen

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

#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>

#include "sys_thread.h"

struct SysThread
{
	pthread_t thread;
};

struct SysMutex
{
	pthread_mutex_t mutex;
};

struct SysThread *Sys_Thread_CreateThread(void (*entrypoint)(void *), void *argument)
{
	struct SysThread *thread;
	int r;

	thread = malloc(sizeof(*thread));
	if (thread)
	{
		r = pthread_create(&thread->thread, 0, entrypoint, argument);
		if (r == 0)
		{
			return thread;
		}

		free(thread);
	}

	return 0;
}

void Sys_Thread_DeleteThread(struct SysThread *thread)
{
	pthread_join(thread->thread, 0);
	free(thread);
}

int Sys_Thread_SetThreadPriority(struct SysThread *thread, enum SysThreadPriority priority)
{
	fprintf(stderr, "%s: stub\n", __func__);

	return 0;
}

struct SysMutex *Sys_Thread_CreateMutex(void)
{
	struct SysMutex *mutex;
	int r;

	mutex = malloc(sizeof(*mutex));
	if (mutex)
	{
		r = pthread_mutex_init(&mutex->mutex, 0);
		if (r == 0)
		{
			return mutex;
		}

		free(mutex);
	}

	return 0;
}

void Sys_Thread_DeleteMutex(struct SysMutex *mutex)
{
	pthread_mutex_destroy(&mutex->mutex);
	free(mutex);
}

void Sys_Thread_LockMutex(struct SysMutex *mutex)
{
	pthread_mutex_lock(&mutex->mutex);
}

void Sys_Thread_UnlockMutex(struct SysMutex *mutex)
{
	pthread_mutex_unlock(&mutex->mutex);
}

