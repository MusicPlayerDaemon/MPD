/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "ChainFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/FilterRegistry.hxx"
#include "AudioFormat.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "util/ConstBuffer.hxx"

#include <list>

#include <assert.h>

class ChainFilter final : public Filter {
	struct Child {
		const char *name;
		Filter *filter;

		Child(const char *_name, Filter *_filter)
			:name(_name), filter(_filter) {}
		~Child() {
			delete filter;
		}

		Child(const Child &) = delete;
		Child &operator=(const Child &) = delete;
	};

	std::list<Child> children;

public:
	void Append(const char *name, Filter *filter) {
		children.emplace_back(name, filter);
	}

	/* virtual methods from class Filter */
	AudioFormat Open(AudioFormat &af, Error &error) override;
	void Close() override;
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src,
					    Error &error) override;

private:
	/**
	 * Close all filters in the chain until #until is reached.
	 * #until itself is not closed.
	 */
	void CloseUntil(const Filter *until);
};

static constexpr Domain chain_filter_domain("chain_filter");

static Filter *
chain_filter_init(gcc_unused const config_param &param,
		  gcc_unused Error &error)
{
	return new ChainFilter();
}

void
ChainFilter::CloseUntil(const Filter *until)
{
	for (auto &child : children) {
		if (child.filter == until)
			/* don't close this filter */
			return;

		/* close this filter */
		child.filter->Close();
	}

	/* this assertion fails if #until does not exist (anymore) */
	assert(false);
	gcc_unreachable();
}

static AudioFormat
chain_open_child(const char *name, Filter *filter,
		 const AudioFormat &prev_audio_format,
		 Error &error)
{
	AudioFormat conv_audio_format = prev_audio_format;
	const AudioFormat next_audio_format =
		filter->Open(conv_audio_format, error);
	if (!next_audio_format.IsDefined())
		return next_audio_format;

	if (conv_audio_format != prev_audio_format) {
		struct audio_format_string s;

		filter->Close();

		error.Format(chain_filter_domain,
			     "Audio format not supported by filter '%s': %s",
			     name,
			     audio_format_to_string(prev_audio_format, &s));
		return AudioFormat::Undefined();
	}

	return next_audio_format;
}

AudioFormat
ChainFilter::Open(AudioFormat &in_audio_format, Error &error)
{
	AudioFormat audio_format = in_audio_format;

	for (auto &child : children) {
		audio_format = chain_open_child(child.name, child.filter,
						audio_format, error);
		if (!audio_format.IsDefined()) {
			/* rollback, close all children */
			CloseUntil(child.filter);
			break;
		}
	}

	/* return the output format of the last filter */
	return audio_format;
}

void
ChainFilter::Close()
{
	for (auto &child : children)
		child.filter->Close();
}

ConstBuffer<void>
ChainFilter::FilterPCM(ConstBuffer<void> src, Error &error)
{
	for (auto &child : children) {
		/* feed the output of the previous filter as input
		   into the current one */
		src = child.filter->FilterPCM(src, error);
		if (src.IsNull())
			return nullptr;
	}

	/* return the output of the last filter */
	return src;
}

const struct filter_plugin chain_filter_plugin = {
	"chain",
	chain_filter_init,
};

Filter *
filter_chain_new(void)
{
	return new ChainFilter();
}

void
filter_chain_append(Filter &_chain, const char *name, Filter *filter)
{
	ChainFilter &chain = (ChainFilter &)_chain;

	chain.Append(name, filter);
}
