/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Observer.hxx"
#include "Filter.hxx"
#include "Prepared.hxx"
#include "util/ConstBuffer.hxx"

#include <cassert>

class FilterObserver::PreparedProxy final : public PreparedFilter {
	FilterObserver &observer;

	std::unique_ptr<PreparedFilter> prepared_filter;
	Proxy *child = nullptr;

public:
	PreparedProxy(FilterObserver &_observer,
		      std::unique_ptr<PreparedFilter> _prepared_filter) noexcept
		:observer(_observer),
		 prepared_filter(std::move(_prepared_filter)) {}

	~PreparedProxy() noexcept override {
		assert(child == nullptr);
		assert(observer.proxy == this);

		observer.proxy = nullptr;
	}

	PreparedProxy(const PreparedProxy &) = delete;
	PreparedProxy &operator=(const PreparedProxy &) = delete;

	void Clear([[maybe_unused]] Proxy *_child) noexcept {
		assert(child == _child);
		child = nullptr;
	}

	Filter *Get() noexcept;

	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

class FilterObserver::Proxy final : public Filter {
	PreparedProxy &parent;

	std::unique_ptr<Filter> filter;

public:
	Proxy(PreparedProxy &_parent, std::unique_ptr<Filter> _filter) noexcept
		:Filter(_filter->GetOutAudioFormat()),
		 parent(_parent), filter(std::move(_filter)) {}

	~Proxy() noexcept override {
		parent.Clear(this);
	}

	Proxy(const Proxy &) = delete;
	Proxy &operator=(const Proxy &) = delete;

	Filter *Get() noexcept {
		return filter.get();
	}

	void Reset() noexcept override {
		filter->Reset();
	}

	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override {
		return filter->FilterPCM(src);
	}

	ConstBuffer<void> Flush() override {
		return filter->Flush();
	}
};

Filter *
FilterObserver::PreparedProxy::Get() noexcept
{
	return child != nullptr
		? child->Get()
		: nullptr;
}

std::unique_ptr<Filter>
FilterObserver::PreparedProxy::Open(AudioFormat &af)
{
	assert(child == nullptr);

	auto c = std::make_unique<Proxy>(*this, prepared_filter->Open(af));
	child = c.get();
	return c;
}

std::unique_ptr<PreparedFilter>
FilterObserver::Set(std::unique_ptr<PreparedFilter> pf)
{
	assert(proxy == nullptr);

	auto p = std::make_unique<PreparedProxy>(*this, std::move(pf));
	proxy = p.get();
	return p;
}

Filter *
FilterObserver::Get() noexcept
{
	return proxy != nullptr
		? proxy->Get()
		: nullptr;
}
