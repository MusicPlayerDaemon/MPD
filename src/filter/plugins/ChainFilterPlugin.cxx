/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "ChainFilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "AudioFormat.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringBuffer.hxx"
#include "util/RuntimeError.hxx"

#include <memory>
#include <list>

#include <assert.h>

class ChainFilter final : public Filter {
	struct Child {
		const char *name;
		std::unique_ptr<Filter> filter;

		Child(const char *_name,
		      std::unique_ptr<Filter> _filter) noexcept
			:name(_name), filter(std::move(_filter)) {}
	};

	std::list<Child> children;

	/**
	 * The child which will be flushed in the next Flush() call.
	 */
	std::list<Child>::iterator flushing = children.end();

public:
	explicit ChainFilter(AudioFormat _audio_format)
		:Filter(_audio_format) {}

	void Append(const char *name,
		    std::unique_ptr<Filter> filter) noexcept {
		assert(out_audio_format.IsValid());
		out_audio_format = filter->GetOutAudioFormat();
		assert(out_audio_format.IsValid());

		children.emplace_back(name, std::move(filter));

		RewindFlush();
	}

	/* virtual methods from class Filter */
	void Reset() noexcept override;
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;
	ConstBuffer<void> Flush() override;

private:
	void RewindFlush() {
		flushing = children.begin();
	}

};

class PreparedChainFilter final : public PreparedFilter {
	struct Child {
		const char *name;
		std::unique_ptr<PreparedFilter> filter;

		Child(const char *_name,
		      std::unique_ptr<PreparedFilter> _filter)
			:name(_name), filter(std::move(_filter)) {}

		Child(const Child &) = delete;
		Child &operator=(const Child &) = delete;

		std::unique_ptr<Filter> Open(const AudioFormat &prev_audio_format);
	};

	std::list<Child> children;

public:
	void Append(const char *name,
		    std::unique_ptr<PreparedFilter> filter) noexcept {
		children.emplace_back(name, std::move(filter));
	}

	/* virtual methods from class PreparedFilter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

std::unique_ptr<Filter>
PreparedChainFilter::Child::Open(const AudioFormat &prev_audio_format)
{
	AudioFormat conv_audio_format = prev_audio_format;
	auto new_filter = filter->Open(conv_audio_format);

	if (conv_audio_format != prev_audio_format)
		throw FormatRuntimeError("Audio format not supported by filter '%s': %s",
					 name,
					 ToString(prev_audio_format).c_str());

	return new_filter;
}

std::unique_ptr<Filter>
PreparedChainFilter::Open(AudioFormat &in_audio_format)
{
	auto chain = std::make_unique<ChainFilter>(in_audio_format);

	for (auto &child : children) {
		AudioFormat audio_format = chain->GetOutAudioFormat();
		chain->Append(child.name, child.Open(audio_format));
	}

	return chain;
}

void
ChainFilter::Reset() noexcept
{
	RewindFlush();

	for (auto &child : children)
		child.filter->Reset();
}

template<typename I>
static ConstBuffer<void>
ApplyFilterChain(I begin, I end, ConstBuffer<void> src)
{
	for (auto i = begin; i != end; ++i)
		/* feed the output of the previous filter as input
		   into the current one */
		src = i->filter->FilterPCM(src);

	return src;
}

ConstBuffer<void>
ChainFilter::FilterPCM(ConstBuffer<void> src)
{
	RewindFlush();

	/* return the output of the last filter */
	return ApplyFilterChain(children.begin(), children.end(), src);
}

ConstBuffer<void>
ChainFilter::Flush()
{
	for (auto end = children.end(); flushing != end; ++flushing) {
		auto data = flushing->filter->Flush();
		if (!data.IsNull())
			return ApplyFilterChain(std::next(flushing), end,
						data);
	}

	return nullptr;
}

std::unique_ptr<PreparedFilter>
filter_chain_new() noexcept
{
	return std::make_unique<PreparedChainFilter>();
}

void
filter_chain_append(PreparedFilter &_chain, const char *name,
		    std::unique_ptr<PreparedFilter> filter) noexcept
{
	PreparedChainFilter &chain = (PreparedChainFilter &)_chain;

	chain.Append(name, std::move(filter));
}
