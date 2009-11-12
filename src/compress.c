/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Imported from AudioCompress by J. Shagam <fluffy@beesbuzz.biz>
 */

#include "config.h"
#include "compress.h"

#include <glib.h>

#include <stdint.h>
#include <string.h>

#ifdef USE_X
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *display;
static Window window;
static Visual *visual;
static int screen;
static GC blackGC, whiteGC, blueGC, yellowGC, dkyellowGC, redGC;
#endif

static int *peaks;
static int gainCurrent, gainTarget;

static struct {
	int show_mon;
	int anticlip;
	int target;
	int gainmax;
	int gainsmooth;
	unsigned buckets;
} prefs;

#ifdef USE_X
static int mon_init;
#endif

void CompressCfg(int show_mon, int anticlip, int target, int gainmax,
		 int gainsmooth, unsigned buckets)
{
	static unsigned lastsize;

	prefs.show_mon = show_mon;
	prefs.anticlip = anticlip;
	prefs.target = target;
	prefs.gainmax = gainmax;
	prefs.gainsmooth = gainsmooth;
	prefs.buckets = buckets;

	/* Allocate the peak structure */
	peaks = g_realloc(peaks, sizeof(int)*prefs.buckets);

	if (prefs.buckets > lastsize)
		memset(peaks + lastsize, 0, sizeof(int)*(prefs.buckets
							 - lastsize));
	lastsize = prefs.buckets;

#ifdef USE_X
	/* Configure the monitor window if needed */
	if (show_mon && !mon_init)
	{
		display = XOpenDisplay(getenv("DISPLAY"));

		/* We really shouldn't try to init X if there's no X */
		if (!display)
		{
			fprintf(stderr,
				"X not detected; disabling monitor window\n");
			show_mon = prefs.show_mon = 0;
		}
	}

	if (show_mon && !mon_init)
	{
		XGCValues gcv;
		XColor col;
	
		gainCurrent = gainTarget = (1 << GAINSHIFT);



		screen = DefaultScreen(display);
		visual = DefaultVisual(display, screen);
		window = XCreateSimpleWindow(display,
					     RootWindow(display, screen),
					     0, 0, prefs.buckets, 128 + 8, 0,
					     BlackPixel(display, screen),
					     WhitePixel(display, screen));
		XStoreName(display, window, "AudioCompress monitor");
	
		gcv.foreground = BlackPixel(display, screen);
		blackGC = XCreateGC(display, window, GCForeground, &gcv);
		gcv.foreground = WhitePixel(display, screen);
		whiteGC = XCreateGC(display, window, GCForeground, &gcv);
		col.red = 0;
		col.green = 0;
		col.blue = 65535;
		XAllocColor(display, DefaultColormap(display, screen), &col);
		gcv.foreground = col.pixel;
		blueGC = XCreateGC(display, window, GCForeground, &gcv);
		col.red = 65535;
		col.green = 65535;
		col.blue = 0;
		XAllocColor(display, DefaultColormap(display, screen), &col);
		gcv.foreground = col.pixel;
		yellowGC = XCreateGC(display, window, GCForeground, &gcv);
		col.red = 32767;
		col.green = 32767;
		col.blue = 0;
		XAllocColor(display, DefaultColormap(display, screen), &col);
		gcv.foreground = col.pixel;
		dkyellowGC = XCreateGC(display, window, GCForeground, &gcv);
		col.red = 65535;
		col.green = 0;
		col.blue = 0;
		XAllocColor(display, DefaultColormap(display, screen), &col);
		gcv.foreground = col.pixel;
		redGC = XCreateGC(display, window, GCForeground, &gcv);
		mon_init = 1;
	}

	if (mon_init)
	{
		if (show_mon)
			XMapWindow(display, window);
		else
			XUnmapWindow(display, window);
		XResizeWindow(display, window, prefs.buckets, 128 + 8);
		XFlush(display);
	}
#endif
}

void CompressFree(void)
{
#ifdef USE_X
	if (mon_init)
	{
		XFreeGC(display, blackGC);
		XFreeGC(display, whiteGC);
		XFreeGC(display, blueGC);
		XFreeGC(display, yellowGC);
		XFreeGC(display, dkyellowGC);
		XFreeGC(display, redGC);
		XDestroyWindow(display, window);
		XCloseDisplay(display);
	}
#endif

	g_free(peaks);
}

void CompressDo(void *data, unsigned int length)
{
	int16_t *audio = (int16_t *)data, *ap;
	int peak;
	unsigned int i, pos;
	int gr, gf, gn;
	static int pn = -1;
#ifdef STATS
	static int clip;
#endif
	static int clipped;

	if (!peaks)
		return;

	if (pn == -1)
	{
		for (i = 0; i < prefs.buckets; i++)
			peaks[i] = 0;
	}
	pn = (pn + 1)%prefs.buckets;

#ifdef DEBUG
	fprintf(stderr, "modifyNative16(0x%08x, %d)\n",(unsigned int)data,
		length);
#endif

	/* Determine peak's value and position */
	peak = 1;
	pos = 0;

#ifdef DEBUG
	fprintf(stderr, "finding peak(b=%d)\n", pn);
#endif

	ap = audio;
	for (i = 0; i < length/2; i++)
	{
		int val = *ap;
		if (val > peak)
		{
			peak = val;
			pos = i;
		} else if (-val > peak)
		{
			peak = -val;
			pos = i;
		}
		ap++;
	}
	peaks[pn] = peak;

	/* Only draw if needed, of course */
#ifdef USE_X
	if (prefs.show_mon)
	{
		/* current amplitude */
		XDrawLine(display, window, whiteGC,
			  pn, 0,
			  pn,
			  127 -
			  (peaks[pn]*gainCurrent >> (GAINSHIFT + 8)));

		/* amplification */
		XDrawLine(display, window, yellowGC,
			  pn,
			  127 - (peaks[pn]*gainCurrent
				 >> (GAINSHIFT + 8)),
			  pn, 127);

		/* peak */
		XDrawLine(display, window, blackGC,
			  pn, 127 - (peaks[pn] >> 8), pn, 127);

		/* clip indicator */
		if (clipped)
			XDrawLine(display, window, redGC,
				  (pn + prefs.buckets - 1)%prefs.buckets,
				  126 - clipped/(length*512),
				  (pn + prefs.buckets - 1)%prefs.buckets,
				  127);
		clipped = 0;

		/* target line */
		/* XDrawPoint(display, window, redGC, */
		/*         pn, 127 - TARGET/256); */
		/* amplification edge */
		XDrawLine(display, window, dkyellowGC,
			  pn,
			  127 - (peaks[pn]*gainCurrent
				 >> (GAINSHIFT + 8)),
			  pn - 1,
			  127 -
			  (peaks[(pn + prefs.buckets
				  - 1)%prefs.buckets]*gainCurrent
			   >> (GAINSHIFT + 8)));
	}
#endif

	for (i = 0; i < prefs.buckets; i++)
	{
		if (peaks[i] > peak)
		{
			peak = peaks[i];
			pos = 0;
		}
	}

	/* Determine target gain */
	gn = (1 << GAINSHIFT)*prefs.target/peak;

	if (gn <(1 << GAINSHIFT))
		gn = 1 << GAINSHIFT;

	gainTarget = (gainTarget *((1 << prefs.gainsmooth) - 1) + gn)
				      >> prefs.gainsmooth;

	/* Give it an extra insignifigant nudge to counteract possible
	** rounding error
	*/

	if (gn < gainTarget)
		gainTarget--;
	else if (gn > gainTarget)
		gainTarget++;

	if (gainTarget > prefs.gainmax << GAINSHIFT)
		gainTarget = prefs.gainmax << GAINSHIFT;


#ifdef USE_X
	if (prefs.show_mon)
	{
		int x;

		/* peak*gain */
		XDrawPoint(display, window, redGC,
			   pn,
			   127 - (peak*gainCurrent
				  >> (GAINSHIFT + 8)));

		/* gain indicator */
		XFillRectangle(display, window, whiteGC, 0, 128,
			       prefs.buckets, 8);
		x = (gainTarget - (1 << GAINSHIFT))*prefs.buckets
			/ ((prefs.gainmax - 1) << GAINSHIFT);
		XDrawLine(display, window, redGC, x,
			  128, x, 128 + 8);

		x = (gn - (1 << GAINSHIFT))*prefs.buckets
			/ ((prefs.gainmax - 1) << GAINSHIFT);

		XDrawLine(display, window, blackGC,
			  x, 132 - 1,
			  x, 132 + 1);

		/* blue peak line */
		XDrawLine(display, window, blueGC,
			  0, 127 - (peak >> 8), prefs.buckets,
			  127 - (peak >> 8));
		XFlush(display);
		XDrawLine(display, window, whiteGC,
			  0, 127 - (peak >> 8), prefs.buckets,
			  127 - (peak >> 8));
	}
#endif

	/* See if a peak is going to clip */
	gn = (1 << GAINSHIFT)*32768/peak;

	if (gn < gainTarget)
	{
		gainTarget = gn;

		if (prefs.anticlip)
			pos = 0;

	} else
	{
		/* We're ramping up, so draw it out over the whole frame */
		pos = length;
	}

	/* Determine gain rate necessary to make target */
	if (!pos)
		pos = 1;

	gr = ((gainTarget - gainCurrent) << 16)/(int)pos;

	/* Do the shiznit */
	gf = gainCurrent << 16;

#ifdef STATS
	fprintf(stderr, "\rgain = %2.2f%+.2e ",
		gainCurrent*1.0/(1 << GAINSHIFT),
		(gainTarget - gainCurrent)*1.0/(1 << GAINSHIFT));
#endif

	ap = audio;
	for (i = 0; i < length/2; i++)
	{
		int sample;

		/* Interpolate the gain */
		gainCurrent = gf >> 16;
		if (i < pos)
			gf += gr;
		else if (i == pos)
			gf = gainTarget << 16;

		/* Amplify */
		sample = (*ap)*gainCurrent >> GAINSHIFT;
		if (sample < -32768)
		{
#ifdef STATS
			clip++;
#endif
			clipped += -32768 - sample;
			sample = -32768;
		} else if (sample > 32767)
		{
#ifdef STATS
			clip++;
#endif
			clipped += sample - 32767;
			sample = 32767;
		}
		*ap++ = sample;
	}
#ifdef STATS
	fprintf(stderr, "clip %d b%-3d ", clip, pn);
#endif

#ifdef DEBUG
	fprintf(stderr, "\ndone\n");
#endif
}

