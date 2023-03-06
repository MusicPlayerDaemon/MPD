// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Observer.hxx"
#include "Filter.hxx"
#include "Prepared.hxx"

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

	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override {
		return filter->FilterPCM(src);
	}

	std::span<const std::byte> Flush() override {
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
