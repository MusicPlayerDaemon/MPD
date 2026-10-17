// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/upnp/Callback.hxx"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace {

class CountingCallback final : public UpnpCallback {
public:
	unsigned n = 0;

	~CountingCallback() noexcept {
		UnregisterUpnpCallback();
	}

protected:
	int Invoke(Upnp_EventType, const void *) noexcept override {
		++n;
		return 42;
	}
};

class BlockingCallback final : public UpnpCallback {
	std::atomic_bool &entered, &release;

public:
	BlockingCallback(std::atomic_bool &_entered,
			 std::atomic_bool &_release) noexcept
		:entered(_entered), release(_release) {}

	~BlockingCallback() noexcept {
		UnregisterUpnpCallback();
	}

protected:
	int Invoke(Upnp_EventType, const void *) noexcept override {
		entered = true;
		while (!release)
			std::this_thread::yield();
		return UPNP_E_SUCCESS;
	}
};

} // anonymous namespace

TEST(UpnpCallback, Dispatch)
{
	CountingCallback a, b;
	EXPECT_NE(a.GetUpnpCookie(), nullptr);
	EXPECT_NE(a.GetUpnpCookie(), b.GetUpnpCookie());

	EXPECT_EQ(UpnpCallback::Dispatch(a.GetUpnpCookie(),
					 UPNP_DISCOVERY_SEARCH_RESULT, nullptr),
		  42);
	EXPECT_EQ(a.n, 1U);
	EXPECT_EQ(b.n, 0U);
}

TEST(UpnpCallback, Cookie)
{
	CountingCallback a;

	/* the first call registers the object, and the cookie
	   remains the same */
	void *const cookie = a.GetUpnpCookie();
	EXPECT_EQ(a.GetUpnpCookie(), cookie);
}

TEST(UpnpCallback, NeverRegistered)
{
	/* an object whose cookie was never requested is not
	   registered, and can be destroyed */
	CountingCallback a;
}

TEST(UpnpCallback, Unregistered)
{
	void *cookie;

	{
		CountingCallback a;
		cookie = a.GetUpnpCookie();
	}

	/* a new object must not receive callbacks for the old
	   cookie, even if it is allocated at the same address */
	CountingCallback b;
	EXPECT_NE(b.GetUpnpCookie(), cookie);

	EXPECT_EQ(UpnpCallback::Dispatch(cookie,
					 UPNP_DISCOVERY_SEARCH_RESULT, nullptr),
		  UPNP_E_SUCCESS);
	EXPECT_EQ(b.n, 0U);
}

TEST(UpnpCallback, UnregisterWaits)
{
	std::atomic_bool entered{false}, release{false};
	auto *c = new BlockingCallback(entered, release);
	void *const cookie = c->GetUpnpCookie();

	std::thread dispatcher([cookie]{
		UpnpCallback::Dispatch(cookie,
				       UPNP_DISCOVERY_SEARCH_RESULT, nullptr);
	});

	while (!entered)
		std::this_thread::yield();

	std::atomic_bool deleted{false};
	std::thread deleter([c, &deleted]{
		delete c;
		deleted = true;
	});

	/* the destructor must wait for the running Invoke() call */
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_FALSE(deleted);

	release = true;
	deleter.join();
	dispatcher.join();
	EXPECT_TRUE(deleted);
}
