
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "quakedef.h"
#include "sound.h"

static const int rates[] = { 11025, 22050, 44100, 8000 };

#define NUMRATES (sizeof(rates)/sizeof(*rates))

struct oss_private
{
	int fd;
};

static int oss_getdmapos(struct SoundCard *sc)
{
	struct count_info count;
	struct oss_private *p = sc->driverprivate;
	
	if (ioctl(p->fd, SNDCTL_DSP_GETOPTR, &count) != -1)
		sc->samplepos = count.ptr/(sc->samplebits/8);
	else
		sc->samplepos = 0;

	return sc->samplepos;
}

static void oss_submit(struct SoundCard *sc)
{
}

static void oss_shutdown(struct SoundCard *sc)
{
	struct oss_private *p = sc->driverprivate;

	munmap(sc->buffer, sc->samples*(sc->samplebits/8));
	close(p->fd);

	free(p);
}

static qboolean oss_init(struct SoundCard *sc, int rate, int channels, int bits)
{
	int i;
	struct oss_private *p;

	int capabilities;
	int dspformats;
	struct audio_buf_info info;

	p = malloc(sizeof(*p));
	if (p)
	{
		p->fd = open("/dev/dsp", O_RDWR|O_NONBLOCK);
		if (p->fd != -1)
		{
			if ((ioctl(p->fd, SNDCTL_DSP_RESET, 0) >= 0)
			&& (ioctl(p->fd, SNDCTL_DSP_GETCAPS, &capabilities) != -1 && (capabilities&(DSP_CAP_TRIGGER|DSP_CAP_MMAP)) == (DSP_CAP_TRIGGER|DSP_CAP_MMAP))
			&& (ioctl(p->fd, SNDCTL_DSP_GETOSPACE, &info) != -1))
			{
				ioctl(p->fd, SNDCTL_DSP_GETFMTS, &dspformats);

				if ((!(dspformats&AFMT_S16_LE) && (dspformats&AFMT_U8)) || bits == 8)
					sc->samplebits = 8;
				else if ((dspformats&AFMT_S16_LE))
					sc->samplebits = 16;
				else
					sc->samplebits = 0;

				if (sc->samplebits)
				{
					i = 0;
					do
					{
						if (ioctl(p->fd, SNDCTL_DSP_SPEED, &rate) == 0)
							break;

						rate = rates[i];
					} while(i++ != NUMRATES);

					if (i != NUMRATES+1)
					{
						if (channels == 1)
						{
							sc->channels = 1;
							i = 0;
						}
						else
						{
							sc->channels = 2;
							i = 1;
						}

						if (ioctl(p->fd, SNDCTL_DSP_STEREO, &i) >= 0)
						{
							if (sc->samplebits == 16)
								i = AFMT_S16_LE;
							else
								i = AFMT_S8;

							if (ioctl(p->fd, SNDCTL_DSP_SETFMT, &i) >= 0)
							{
								if ((sc->buffer = mmap(0, info.fragstotal * info.fragsize, PROT_WRITE, MAP_SHARED, p->fd, 0)) != 0)
								{
									i = 0;
									if (ioctl(p->fd, SNDCTL_DSP_SETTRIGGER, &i) >= 0)
									{
										i = PCM_ENABLE_OUTPUT;
										if (ioctl(p->fd, SNDCTL_DSP_SETTRIGGER, &i) >= 0)
										{

											printf("bits: %d speed: %d\n", sc->samplebits, rate);
											sc->samples = info.fragstotal * info.fragsize / (sc->samplebits/8);
											sc->samplepos = 0;
											sc->submission_chunk = 1;
											sc->speed = rate;

											sc->driverprivate = p;

											sc->GetDMAPos = oss_getdmapos;
											sc->Submit = oss_submit;
											sc->Shutdown = oss_shutdown;

											return 1;
										}
									}

									munmap(sc->buffer, info.fragstotal * info.fragsize);
								}
							}
						}
					}
				}
			}

			close(p->fd);
		}

		free(p);
	}

	return 0;
}

SoundInitFunc OSS_Init = oss_init;

