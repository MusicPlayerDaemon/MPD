// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Callback.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "thread/ScopeUnlock.hxx"

#include <algorithm> // for std::find_if()
#include <cassert>

static Mutex registry_mutex;

/**
 * Signalled when UpnpCallback::n_invoking drops to zero.
 */
static Cond registry_cond;

/**
 * Protected by #registry_mutex.
 */
static std::uintptr_t next_id = 1;

struct UpnpCallbackRegistryHook
	: IntrusiveListMemberHookTraits<&UpnpCallback::registry_siblings> {};

/**
 * All registered #UpnpCallback instances.  Protected by
 * #registry_mutex.
 */
static IntrusiveList<UpnpCallback, UpnpCallbackRegistryHook> registry;

#ifndef NDEBUG

UpnpCallback::~UpnpCallback() noexcept
{
	const std::lock_guard protect{registry_mutex};
	assert(n_invoking == 0);
	assert(!registry_siblings.is_linked());
}

#endif

void *
UpnpCallback::GetUpnpCookie() noexcept
{
	const std::lock_guard protect{registry_mutex};

	/* must not be called after UnregisterUpnpCallback() (it would
	   register this object again) */
	assert(!unregistered);

	if (!registry_siblings.is_linked()) {
		/* register on the first call, i.e. before libupnp
		   gets the cookie; this way, an object whose
		   (derived) constructor throws is never registered */
		cookie_id = next_id++;
		registry.push_back(*this);
	}

	return reinterpret_cast<void *>(cookie_id);
}

void
UpnpCallback::UnregisterUpnpCallback() noexcept
{
	std::unique_lock lock{registry_mutex};

#ifndef NDEBUG
	unregistered = true;
#endif

	if (!registry_siblings.is_linked()) {
		/* not registered because GetUpnpCookie() was never
		   called, so Dispatch() cannot be running */
		assert(n_invoking == 0);
		return;
	}

	registry.erase(registry.iterator_to(*this));

	/* wait for Dispatch() to complete */
	registry_cond.wait(lock, [this]{ return n_invoking == 0; });
}

int
UpnpCallback::Dispatch(void *cookie,
		       Upnp_EventType et, const void *evp) noexcept
{
	std::unique_lock lock{registry_mutex};

	/* linear search: there are only very few registered
	   objects */
	const auto id = reinterpret_cast<std::uintptr_t>(cookie);
	const auto i = std::find_if(registry.begin(), registry.end(),
				    [id](const UpnpCallback &c){
					    return c.cookie_id == id;
				    });
	if (i == registry.end())
		/* already unregistered (or an unknown cookie) */
		return UPNP_E_SUCCESS;

	UpnpCallback &callback = *i;
	++callback.n_invoking;

	int result;

	{
		const ScopeUnlock unlock{lock};
		result = callback.Invoke(et, evp);
	}

	if (--callback.n_invoking == 0)
		registry_cond.notify_all();

	return result;
}
