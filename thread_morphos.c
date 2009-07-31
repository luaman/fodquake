#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dostags.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "sys_thread.h"

struct SysThread
{
	struct MsgPort *msgport;
	struct Message msg;
	struct Process *process;
};

struct SysMutex
{
	struct SignalSemaphore sem;
};

struct SysThread *Sys_Thread_CreateThread(void (*entrypoint)(void *), void *argument)
{
	struct SysThread *thread;

	thread = AllocVec(sizeof(*thread), MEMF_ANY);
	if (thread)
	{
		thread->msgport = CreateMsgPort();
		if (thread->msgport)
		{
			thread->msg.mn_Node.ln_Type = NT_MESSAGE;
			thread->msg.mn_ReplyPort = thread->msgport;
			thread->msg.mn_Length = sizeof(thread->msg);

			thread->process = CreateNewProcTags(NP_Entry, entrypoint,
			                                    NP_CodeType, CODETYPE_PPC,
			                                    NP_PPC_Arg1, argument,
			                                    NP_Name, "Fodquake Thread",
			                                    NP_StartupMsg, &thread->msg,
			                                    TAG_DONE);
			if (thread->process)
			{
				return thread;
			}

			DeleteMsgPort(thread->msgport);
		}
		FreeVec(thread);
	}

	return 0;
}

void Sys_Thread_DeleteThread(struct SysThread *thread)
{
	SetTaskPri(&thread->process->pr_Task, 0);

	while(!GetMsg(thread->msgport))
		WaitPort(thread->msgport);

	DeleteMsgPort(thread->msgport);
	FreeVec(thread);
}

int Sys_Thread_SetThreadPriority(struct SysThread *thread, enum SysThreadPriority priority)
{
	int pri;

	switch(priority)
	{
		case SYSTHREAD_PRIORITY_LOW:
			pri = -1;
			break;
		case SYSTHREAD_PRIORITY_HIGH:
			pri = 4;
			break;
		default:
			pri = 0;
			break;
	}

	SetTaskPri(&thread->process->pr_Task, pri);

	return 0;
}

struct SysMutex *Sys_Thread_CreateMutex(void)
{
	struct SysMutex *mutex;

	mutex = AllocVec(sizeof(*mutex), MEMF_ANY);
	if (mutex)
	{
		InitSemaphore(&mutex->sem);

		return mutex;
	}

	return 0;
}

void Sys_Thread_DeleteMutex(struct SysMutex *mutex)
{
	FreeVec(mutex);
}

void Sys_Thread_LockMutex(struct SysMutex *mutex)
{
	ObtainSemaphore(&mutex->sem);
}

void Sys_Thread_UnlockMutex(struct SysMutex *mutex)
{
	ReleaseSemaphore(&mutex->sem);
}

