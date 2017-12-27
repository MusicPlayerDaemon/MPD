/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Observer.hxx"
#include "FilterInternal.hxx"
#include "util/ConstBuffer.hxx"

#include <assert.h>

class FilterObserver::PreparedProxy final : public PreparedFilter {
	FilterObserver &observer;

	std::unique_ptr<PreparedFilter> prepared_filter;
	Proxy *child = nullptr;

public:
	PreparedProxy(FilterObserver &_observer,
		      std::unique_ptr<PreparedFilter> _prepared_filter)
		:observer(_observer),
		 prepared_filter(std::move(_prepared_filter)) {}

	~PreparedProxy() {
		assert(child == nullptr);
		assert(observer.proxy == this);

		observer.proxy = nullptr;
	}

	void Clear(gcc_unused Proxy *_child) {
		assert(child == _child);
		child = nullptr;
	}

	Filter *Get();

	Filter *Open(AudioFormat &af) override;
};

class FilterObserver::Proxy final : public Filter {
	PreparedProxy &parent;

	Filter *const filter;

public:
	Proxy(PreparedProxy &_parent, Filter *_filter)
		:Filter(_filter->GetOutAudioFormat()),
		 parent(_parent), filter(_filter) {}

	~Proxy() {
		parent.Clear(this);
		delete filter;
	}

	Filter *Get() {
		return filter;
	}

	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override {
		return filter->FilterPCM(src);
	}
};

Filter *
FilterObserver::PreparedProxy::Get()
{
	return child != nullptr
		? child->Get()
		: nullptr;
}

Filter *
FilterObserver::PreparedProxy::Open(AudioFormat &af)
{
	assert(child == nullptr);

	Filter *f = prepared_filter->Open(af);
	return child = new Proxy(*this, f);
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
FilterObserver::Get()
{
	return proxy != nullptr
		? proxy->Get()
		: nullptr;
}
