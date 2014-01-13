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

#include "util/ByteReverse.hxx"
#include "util/Macros.hxx"
#include "Compiler.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include <string.h>
#include <stdlib.h>

class ByteReverseTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(ByteReverseTest);
	CPPUNIT_TEST(TestByteReverse2);
	CPPUNIT_TEST(TestByteReverse3);
	CPPUNIT_TEST(TestByteReverse4);
	CPPUNIT_TEST(TestByteReverse5);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestByteReverse2();
	void TestByteReverse3();
	void TestByteReverse4();
	void TestByteReverse5();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ByteReverseTest);

void
ByteReverseTest::TestByteReverse2()
{
	static const char src[] = "123456";
	static const char result[] = "214365";
	static uint8_t dest[ARRAY_SIZE(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + ARRAY_SIZE(src) - 1), 2);
	CPPUNIT_ASSERT(strcmp(result, (const char *)dest) == 0);
}

void
ByteReverseTest::TestByteReverse3()
{
	static const char src[] = "123456";
	static const char result[] = "321654";
	static uint8_t dest[ARRAY_SIZE(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + ARRAY_SIZE(src) - 1), 3);
	CPPUNIT_ASSERT(strcmp(result, (const char *)dest) == 0);
}

void
ByteReverseTest::TestByteReverse4()
{
	static const char src[] = "12345678";
	static const char result[] = "43218765";
	static uint8_t dest[ARRAY_SIZE(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + ARRAY_SIZE(src) - 1), 4);
	CPPUNIT_ASSERT(strcmp(result, (const char *)dest) == 0);
}

void
ByteReverseTest::TestByteReverse5()
{
	static const char src[] = "1234567890";
	static const char result[] = "5432109876";
	static uint8_t dest[ARRAY_SIZE(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + ARRAY_SIZE(src) - 1), 5);
	CPPUNIT_ASSERT(strcmp(result, (const char *)dest) == 0);
}

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
