/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "util/ConstBuffer.hxx"
#include "util/RuntimeError.hxx"

#include <memory>
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
	explicit ChainFilter(AudioFormat _audio_format)
		:Filter(_audio_format) {}

	void Append(const char *name, Filter *filter) {
		assert(out_audio_format.IsValid());
		out_audio_format = filter->GetOutAudioFormat();
		assert(out_audio_format.IsValid());

		children.emplace_back(name, filter);
	}

	/* virtual methods from class Filter */
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;
};

class PreparedChainFilter final : public PreparedFilter {
	struct Child {
		const char *name;
		PreparedFilter *filter;

		Child(const char *_name, PreparedFilter *_filter)
			:name(_name), filter(_filter) {}
		~Child() {
			delete filter;
		}

		Child(const Child &) = delete;
		Child &operator=(const Child &) = delete;

		Filter *Open(const AudioFormat &prev_audio_format);
	};

	std::list<Child> children;

public:
	void Append(const char *name, PreparedFilter *filter) {
		children.emplace_back(name, filter);
	}

	/* virtual methods from class PreparedFilter */
	Filter *Open(AudioFormat &af) override;
};

static PreparedFilter *
chain_filter_init(gcc_unused const ConfigBlock &block)
{
	return new PreparedChainFilter();
}

Filter *
PreparedChainFilter::Child::Open(const AudioFormat &prev_audio_format)
{
	AudioFormat conv_audio_format = prev_audio_format;
	Filter *new_filter = filter->Open(conv_audio_format);

	if (conv_audio_format != prev_audio_format) {
		delete new_filter;

		struct audio_format_string s;
		throw FormatRuntimeError("Audio format not supported by filter '%s': %s",
					 name,
					 audio_format_to_string(prev_audio_format, &s));
	}

	return new_filter;
}

Filter *
PreparedChainFilter::Open(AudioFormat &in_audio_format)
{
	std::unique_ptr<ChainFilter> chain(new ChainFilter(in_audio_format));

	for (auto &child : children) {
		AudioFormat audio_format = chain->GetOutAudioFormat();
		auto *filter = child.Open(audio_format);
		chain->Append(child.name, filter);
	}

	return chain.release();
}

ConstBuffer<void>
ChainFilter::FilterPCM(ConstBuffer<void> src)
{
	for (auto &child : children) {
		/* feed the output of the previous filter as input
		   into the current one */
		src = child.filter->FilterPCM(src);
	}

	/* return the output of the last filter */
	return src;
}

const FilterPlugin chain_filter_plugin = {
	"chain",
	chain_filter_init,
};

PreparedFilter *
filter_chain_new(void)
{
	return new PreparedChainFilter();
}

void
filter_chain_append(PreparedFilter &_chain, const char *name,
		    PreparedFilter *filter)
{
	PreparedChainFilter &chain = (PreparedChainFilter &)_chain;

	chain.Append(name, filter);
}
