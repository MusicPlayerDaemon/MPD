/*
 * Copyright 2007-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
