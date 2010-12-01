/*
 
 Copyright (C) 2001-2002       A Nourai
 Copyright (C) 2006            Jacek Piszczek (Mac OSX port)
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 
 See the included (GNU.txt) GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>

#include "quakedef.h"
#include "sound.h"

// Jacek:
// coreaudio is poorly documented so I'm not 100% sure the code below
// is correct :(

struct coreaudio_private
{
	AudioUnit OutputUnit;
	unsigned int readpos;
	void *buffer;
};

static OSStatus AudioRender(void *inRefCon, AudioUnitRenderActionFlags * ioActionFlags, const AudioTimeStamp * inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList * ioData)
{
	struct coreaudio_private *p;

	p = inRefCon;

	memcpy(ioData->mBuffers[0].mData, p->buffer + (p->readpos * 4), ioData->mBuffers[0].mDataByteSize);

	if (p->readpos >= 128 * 1023)
	{
		p->readpos = 0;
	}
	else
	{
		p->readpos += inNumberFrames;
	}

	return noErr;
}

static int coreaudio_getdmapos(struct SoundCard *sc)
{
	struct coreaudio_private *p;

	p = sc->driverprivate;

	return p->readpos * 2;
}

static void coreaudio_submit(struct SoundCard *sc, unsigned int count)
{
}

static void coreaudio_shutdown(struct SoundCard *sc)
{
	struct coreaudio_private *p;

	p = sc->driverprivate;

	// stop playback
	AudioOutputUnitStop(p->OutputUnit);

	// release the unit
	AudioUnitUninitialize(p->OutputUnit);

	// free the unit
	CloseComponent(p->OutputUnit);

	// free the buffer memory
	free(p->buffer);

	free(p);
}

static qboolean coreaudio_init(struct SoundCard *sc, const char *device, int rate, int channels, int bits)
{
	struct coreaudio_private *p;
	ComponentResult err;
	ComponentDescription desc;
	Component comp;
	AudioStreamBasicDescription streamFormat;
	AURenderCallbackStruct input;
	int speed;

	p = malloc(sizeof(*p));
	if (p)
	{
		// Open the default output unit
		desc.componentType = kAudioUnitType_Output;
		desc.componentSubType = kAudioUnitSubType_DefaultOutput;
		desc.componentManufacturer = kAudioUnitManufacturer_Apple;
		desc.componentFlags = 0;
		desc.componentFlagsMask = 0;

		comp = FindNextComponent(NULL, &desc);
		if (comp == NULL)
			return FALSE;

		err = OpenAComponent(comp, &p->OutputUnit);
		if (err == 0)
		{
			// Set up a callback function to generate output to the output unit
			input.inputProc = AudioRender;
			input.inputProcRefCon = NULL;

			err = AudioUnitSetProperty(p->OutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
			if (err == 0)
			{
				// describe our audio data
				streamFormat.mSampleRate = rate;
				streamFormat.mFormatID = kAudioFormatLinearPCM;
				streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
				//| kAudioFormatFlagIsNonInterleaved;
				streamFormat.mBytesPerPacket = 4;
				streamFormat.mFramesPerPacket = 1;
				streamFormat.mBytesPerFrame = 4;
				streamFormat.mChannelsPerFrame = 2;
				streamFormat.mBitsPerChannel = 16;

				err = AudioUnitSetProperty(p->OutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(AudioStreamBasicDescription));
				if (err == 0)
				{
					p->buffer = malloc(128 * 4 * 1025);
					if (p->buffer)
					{
						// Initialize unit
						err = AudioUnitInitialize(p->OutputUnit);
						if (err == 0)
						{
							// start playing :)
							err = AudioOutputUnitStart(p->OutputUnit);
							if (err == 0)
							{
								sc->driverprivate = p;

								sc->GetDMAPos = coreaudio_getdmapos;
								sc->Submit = coreaudio_submit;
								sc->Shutdown = coreaudio_shutdown;

								sc->channels = 2;
								sc->samples = 256 * 1024;
								sc->samplepos = 0;
								sc->samplebits = 16;
								sc->speed = rate;
								sc->buffer = p->buffer;

								return TRUE;
							}

							AudioUnitUninitialize(p->OutputUnit);
						}

						free(p->buffer);
					}
				}
			}

			CloseComponent(p->OutputUnit);
		}

		free(p);
	}

	return FALSE;
}

SoundInitFunc CoreAudio_Init = coreaudio_init;

