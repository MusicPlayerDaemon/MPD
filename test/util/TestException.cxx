// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "util/Exception.hxx"

#include <gtest/gtest.h>

TEST(ExceptionTest, RuntimeError)
{
	ASSERT_EQ(GetFullMessage(std::make_exception_ptr(std::runtime_error("Foo"))), "Foo");
}

TEST(ExceptionTest, DerivedError)
{
	class DerivedError : public std::runtime_error {
	public:
		explicit DerivedError(const char *_msg)
			:std::runtime_error(_msg) {}
	};

	ASSERT_EQ(GetFullMessage(std::make_exception_ptr(DerivedError("Foo"))), "Foo");
}

TEST(ExceptionTest, FindNestedDirect)
{
	struct Foo {};
	struct Bar {};
	struct Derived : Foo {};

	try {
		throw Foo{};
	} catch (...) {
		EXPECT_NE(FindNested<Foo>(std::current_exception()),
			  nullptr);
	}

	try {
		throw Bar{};
	} catch (...) {
		EXPECT_EQ(FindNested<Foo>(std::current_exception()),
			  nullptr);
	}

	try {
		throw Derived{};
	} catch (...) {
		EXPECT_NE(FindNested<Foo>(std::current_exception()),
			  nullptr);
	}
}

TEST(ExceptionTest, FindNestedIndirect)
{
	struct Foo {};
	struct Bar {};
	struct Derived : Foo {};
	struct Outer {};

	try {
		throw Foo{};
	} catch (...) {
		try {
			std::throw_with_nested(Outer{});
		} catch (...) {
			EXPECT_NE(FindNested<Foo>(std::current_exception()),
				  nullptr);
		}
	}

	try {
		throw Bar{};
	} catch (...) {
		try {
			std::throw_with_nested(Outer{});
		} catch (...) {
			EXPECT_EQ(FindNested<Foo>(std::current_exception()),
				  nullptr);
		}
	}

	try {
		throw Derived{};
	} catch (...) {
		try {
			std::throw_with_nested(Outer{});
		} catch (...) {
			EXPECT_NE(FindNested<Foo>(std::current_exception()),
				  nullptr);
		}
	}
}

template<typename T>
static bool
CheckFindRetrowNested(std::exception_ptr e) noexcept
{
	try {
		FindRetrowNested<T>(e);
	} catch (const T &) {
		return true;
	}

	return false;
}

TEST(ExceptionTest, FindRetrowNestedDirect)
{
	struct Foo {};
	struct Bar {};
	struct Derived : Foo {};

	try {
		throw Foo{};
	} catch (...) {
		EXPECT_TRUE(CheckFindRetrowNested<Foo>(std::current_exception()));
	}

	try {
		throw Bar{};
	} catch (...) {
		EXPECT_FALSE(CheckFindRetrowNested<Foo>(std::current_exception()));
	}

	try {
		throw Derived{};
	} catch (...) {
		EXPECT_TRUE(CheckFindRetrowNested<Foo>(std::current_exception()));
	}
}

TEST(ExceptionTest, FindRetrowNestedIndirect)
{
	struct Foo {};
	struct Bar {};
	struct Derived : Foo {};
	struct Outer {};

	try {
		throw Foo{};
	} catch (...) {
		try {
			std::throw_with_nested(Outer{});
		} catch (...) {
			EXPECT_TRUE(CheckFindRetrowNested<Foo>(std::current_exception()));
		}
	}

	try {
		throw Bar{};
	} catch (...) {
		try {
			std::throw_with_nested(Outer{});
		} catch (...) {
			EXPECT_FALSE(CheckFindRetrowNested<Foo>(std::current_exception()));
		}
	}

	try {
		throw Derived{};
	} catch (...) {
		try {
			std::throw_with_nested(Outer{});
		} catch (...) {
			EXPECT_TRUE(CheckFindRetrowNested<Foo>(std::current_exception()));
		}
	}
}

TEST(ExceptionTest, FindRetrowNestedIndirectRuntimeError)
{
	struct Foo {};
	struct Bar {};
	struct Derived : Foo {};

	try {
		throw Foo{};
	} catch (...) {
		try {
			std::throw_with_nested(std::runtime_error("X"));
		} catch (...) {
			EXPECT_TRUE(CheckFindRetrowNested<Foo>(std::current_exception()));
		}
	}

	try {
		throw Bar{};
	} catch (...) {
		try {
			std::throw_with_nested(std::runtime_error("X"));
		} catch (...) {
			EXPECT_FALSE(CheckFindRetrowNested<Foo>(std::current_exception()));
		}
	}

	try {
		throw Derived{};
	} catch (...) {
		try {
			std::throw_with_nested(std::runtime_error("X"));
		} catch (...) {
			EXPECT_TRUE(CheckFindRetrowNested<Foo>(std::current_exception()));
		}
	}
}
