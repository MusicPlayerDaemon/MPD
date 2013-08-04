/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "NullOutputPlugin.hxx"
#include "OutputAPI.hxx"
#include "Timer.hxx"

#include <assert.h>

struct NullOutput {
	struct audio_output base;

	bool sync;

	Timer *timer;

	bool Initialize(const config_param &param, GError **error_r) {
		return ao_base_init(&base, &null_output_plugin, param,
				    error_r);
	}

	void Deinitialize() {
		ao_base_finish(&base);
	}
};

static struct audio_output *
null_init(const config_param &param, GError **error_r)
{
	NullOutput *nd = new NullOutput();

	if (!nd->Initialize(param, error_r)) {
		delete nd;
		return nullptr;
	}

	nd->sync = param.GetBlockValue("sync", true);

	return &nd->base;
}

static void
null_finish(struct audio_output *ao)
{
	NullOutput *nd = (NullOutput *)ao;

	nd->Deinitialize();
	delete nd;
}

static bool
null_open(struct audio_output *ao, AudioFormat &audio_format,
	  gcc_unused GError **error)
{
	NullOutput *nd = (NullOutput *)ao;

	if (nd->sync)
		nd->timer = new Timer(audio_format);

	return true;
}

static void
null_close(struct audio_output *ao)
{
	NullOutput *nd = (NullOutput *)ao;

	if (nd->sync)
		delete nd->timer;
}

static unsigned
null_delay(struct audio_output *ao)
{
	NullOutput *nd = (NullOutput *)ao;

	return nd->sync && nd->timer->IsStarted()
		? nd->timer->GetDelay()
		: 0;
}

static size_t
null_play(struct audio_output *ao, gcc_unused const void *chunk, size_t size,
	  gcc_unused GError **error)
{
	NullOutput *nd = (NullOutput *)ao;
	Timer *timer = nd->timer;

	if (!nd->sync)
		return size;

	if (!timer->IsStarted())
		timer->Start();
	timer->Add(size);

	return size;
}

static void
null_cancel(struct audio_output *ao)
{
	NullOutput *nd = (NullOutput *)ao;

	if (!nd->sync)
		return;

	nd->timer->Reset();
}

const struct audio_output_plugin null_output_plugin = {
	"null",
	nullptr,
	null_init,
	null_finish,
	nullptr,
	nullptr,
	null_open,
	null_close,
	null_delay,
	nullptr,
	null_play,
	nullptr,
	null_cancel,
	nullptr,
	nullptr,
};
