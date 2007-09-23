/*
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

#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "sleep.h"

#ifdef _WIN32
#include <windows.h>

#define usleep(x) Sleep(x/1000)
#endif

static unsigned int sleep_granularity;

#define NUMSAMPLES 50

static int get_sleep_granularity()
{
	struct timeval basetv;
	struct timeval tv[NUMSAMPLES];
	int i;
	unsigned long long accum;
	unsigned int avg;
	unsigned int min;
	unsigned int max;
	unsigned int maxdist;
	unsigned int samples;
	double stddev;
	double tmpf;

	samples = NUMSAMPLES;

	for(i=0;i<NUMSAMPLES;i++)
	{
		usleep(1);
		gettimeofday(&basetv, 0);
		usleep(1);
		gettimeofday(&tv[i], 0);

		tv[i].tv_sec -= basetv.tv_sec;
		tv[i].tv_usec -= basetv.tv_usec;

		tv[i].tv_usec+= tv[i].tv_sec * 1000000;
	}

	do
	{
		max = 0;
		min = 1000000;
		accum = 0;
		for(i=0;i<samples;i++)
		{
			accum+= tv[i].tv_usec;
			stddev+= ((double)tv[i].tv_usec)*((double)tv[i].tv_usec);
		
			if (tv[i].tv_usec < min)
				min = tv[i].tv_usec;
			if (tv[i].tv_usec > max)
				max = tv[i].tv_usec;
		}

		avg = accum/samples;

		stddev = 0;
		for(i=0;i<samples;i++)
		{
			tmpf = ((double)tv[i].tv_usec) - avg;
			tmpf*= tmpf;
			stddev+= tmpf;
		}
		stddev = sqrt(stddev/samples);

		maxdist = 0;
		for(i=0;i<samples;i++)
		{
			if (abs(tv[i].tv_usec-avg) > maxdist)
				maxdist = abs(tv[i].tv_usec-avg);
		}

#if 0
		printf("avg: %06d min: %06d max: %06d maxdist: %06d stddev: %.2f\n", avg, min, max, maxdist, (float)stddev);
#endif

		for(i=0;i<samples;i++)
		{
			if (abs(tv[i].tv_usec-avg) == maxdist)
			{
				memcpy(&tv[i], &tv[i+1], samples-i-1);
				samples--;
				break;
			}
		}
	} while(stddev > (((double)avg)*0.01) && avg > 500); 

	if (samples < NUMSAMPLES/5)
	{
#if 0
		printf("System timing too unstable, assuming 8ms granularity\n");
#endif
		avg = 8000;
	}

	return avg;
}

void Sleep_Init()
{
	sleep_granularity = get_sleep_granularity();

	Com_Printf("System sleep granularity: %d us\n", sleep_granularity);
	
#ifdef __linux__
	/* Linux sucks horse cocks */
	sleep_granularity+= sleep_granularity;
#endif

	sleep_granularity+= sleep_granularity*2/100;
}

void Sleep_Sleep(unsigned int sleeptime)
{
	unsigned int real_sleep_time;
	struct timeval endtime;
	struct timeval curtime;

	gettimeofday(&endtime, 0);

	endtime.tv_sec+= sleeptime/1000000;
	endtime.tv_usec+= sleeptime%1000000;
	if (endtime.tv_usec > 1000000)
	{
		endtime.tv_sec++;
		endtime.tv_usec-= 1000000;
	}

	if (sleeptime >= sleep_granularity)
	{
		real_sleep_time = sleeptime - sleep_granularity;

		usleep(real_sleep_time);
	}

	do
	{
		gettimeofday(&curtime, 0);
	} while(!(curtime.tv_sec > endtime.tv_sec || (curtime.tv_sec == endtime.tv_sec && curtime.tv_usec >= endtime.tv_usec)));

	curtime.tv_sec-= endtime.tv_sec ;
	curtime.tv_usec-= endtime.tv_usec ;
	if (curtime.tv_sec)
		curtime.tv_usec+= 1000000;

#if 0
	if (curtime.tv_usec > 100)
	{
		printf("Asked to sleep %d us, usleept %d us\n", sleeptime, real_sleep_time);
		printf("Overslept %d us\n", curtime.tv_usec);
	}
#endif
}

