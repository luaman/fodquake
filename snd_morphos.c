#include <exec/exec.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#define USE_INLINE_STDARG
#include <proto/ahi.h>

#include "quakedef.h"
#include "sound.h"

struct AHIChannelInfo
{
	struct AHIEffChannelInfo aeci;
	ULONG offset;
};

static struct MsgPort *msgport;
static struct AHIRequest *ahireq;
static int ahiopen;
static struct AHIAudioCtrl *audioctrl;
static void *samplebuffer;
static struct Hook EffectHook;
static struct AHIChannelInfo aci;
static unsigned int readpos;

ULONG EffectFunc()
{
	struct AHIEffChannelInfo *aeci = (struct AHIEffChannelInfo *)REG_A1;

	readpos = aeci->ahieci_Offset[0];

	return 0;
}

static struct EmulLibEntry EffectFunc_Gate =
{
	TRAP_LIB, 0, (void (*)(void))EffectFunc
};

void SNDDMA_Shutdown()
{
	struct Library *AHIBase;

	if (ahireq)
	{
		AHIBase = (struct Library *)ahireq->ahir_Std.io_Device;

		aci.aeci.ahie_Effect = AHIET_CHANNELINFO|AHIET_CANCEL;
		AHI_SetEffect(&aci.aeci, audioctrl);
		AHI_ControlAudio(audioctrl,
		                 AHIC_Play, FALSE,
		                 TAG_END);

		AHI_FreeAudio(audioctrl);

		audioctrl = 0;

		CloseDevice((struct IORequest *)ahireq);
		DeleteIORequest((struct IORequest *)ahireq);

		ahireq = 0;

		DeleteMsgPort(msgport);
		msgport = 0;

		FreeVec(samplebuffer);
		samplebuffer = 0;
	}
}

int SNDDMA_GetDMAPos()
{
	shm->samplepos = readpos*shm->channels;

	return shm->samplepos;
}

void SNDDMA_Submit()
{
}

qboolean SNDDMA_Init()
{
	ULONG channels;
	ULONG speed;
	ULONG bits;

	ULONG r;

	char name[64];

	struct Library *AHIBase;

	struct AHISampleInfo sample;

	shm = &sn;

	bzero(shm, sizeof(*shm));

	if (s_khz.value == 44)
		speed = 44100;
	else if (s_khz.value == 22)
		speed = 22050;
	else
		speed = 11025;

	msgport = CreateMsgPort();
	if (msgport)
	{
		ahireq = (struct AHIRequest *)CreateIORequest(msgport, sizeof(struct AHIRequest));
		if (ahireq)
		{
			ahiopen = !OpenDevice("ahi.device", AHI_NO_UNIT, (struct IORequest *)ahireq, 0);
			if (ahiopen)
			{
				AHIBase = (struct Library *)ahireq->ahir_Std.io_Device;

				audioctrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
				                               AHIA_MixFreq, speed,
				                               AHIA_Channels, 1,
				                               AHIA_Sounds, 1,
				                               TAG_END);

				if (audioctrl)
				{
					AHI_GetAudioAttrs(AHI_INVALID_ID, audioctrl,
					                  AHIDB_BufferLen, sizeof(name),
					                  AHIDB_Name, (ULONG)name,
					                  AHIDB_MaxChannels, (ULONG)&channels,
					                  AHIDB_Bits, (ULONG)&bits,
					                  TAG_END);

					AHI_ControlAudio(audioctrl,
					                 AHIC_MixFreq_Query, (ULONG)&speed,
					                 TAG_END);

					if (bits == 8 || bits == 16)
					{
						if (channels > 2)
							channels = 2;

						shm->speed = speed;
						shm->samplebits = bits;
						shm->channels = channels;
						shm->samples = 16384*(speed/11025);
						shm->submission_chunk = 1;

						samplebuffer = AllocVec(16384*(speed/11025)*(bits/8)*channels, MEMF_CLEAR);
						if (samplebuffer)
						{
							shm->buffer = samplebuffer;

							if (channels == 1)
							{
								if (bits == 8)
									sample.ahisi_Type = AHIST_M8S;
								else
									sample.ahisi_Type = AHIST_M16S;
							}
							else
							{
								if (bits == 8)
									sample.ahisi_Type = AHIST_S8S;
								else
									sample.ahisi_Type = AHIST_S16S;
							}

							sample.ahisi_Address = samplebuffer;
							sample.ahisi_Length = (16384*(speed/11025)*(bits/8))/AHI_SampleFrameSize(sample.ahisi_Type);

							r = AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, audioctrl);
							if (r == 0)
							{
								r = AHI_ControlAudio(audioctrl,
								                     AHIC_Play, TRUE,
								                     TAG_END);

								if (r == 0)
								{
									AHI_Play(audioctrl,
									         AHIP_BeginChannel, 0,
									         AHIP_Freq, speed,
									         AHIP_Vol, 0x10000,
									         AHIP_Pan, 0x8000,
									         AHIP_Sound, 0,
									         AHIP_EndChannel, NULL,
									         TAG_END);

									aci.aeci.ahie_Effect = AHIET_CHANNELINFO;
									aci.aeci.ahieci_Func = &EffectHook;
									aci.aeci.ahieci_Channels = 1;

									EffectHook.h_Entry = (void *)&EffectFunc_Gate;

									AHI_SetEffect(&aci, audioctrl);

									Com_Printf("Using AHI mode \"%s\" for audio output\n", name);
									Com_Printf("Channels: %d bits: %d frequency: %d\n", channels, bits, speed);

									return 1;
								}
							}
						}
						FreeVec(samplebuffer);
					}
					AHI_FreeAudio(audioctrl);
				}
				else
					Com_Printf("Failed to allocate AHI audio\n");

				CloseDevice((struct IORequest *)ahireq);
			}
			DeleteIORequest((struct IORequest *)ahireq);
		}
		DeleteMsgPort(msgport);
	}

	return 0;
}
