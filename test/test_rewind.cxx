/*
 * Unit tests for class RewindInputStream.
 */

#include "config.h"
#include "input/plugins/RewindInputPlugin.hxx"
#include "input/InputStream.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/Error.hxx"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string>

#include <string.h>
#include <stdlib.h>

class StringInputStream final : public InputStream {
	const char *data;
	size_t remaining;

public:
	StringInputStream(const char *_uri,
			  Mutex &_mutex, Cond &_cond,
			  const char *_data)
		:InputStream(_uri, _mutex, _cond),
		 data(_data), remaining(strlen(data)) {
		SetReady();
	}

	/* virtual methods from InputStream */
	bool IsEOF() override {
		return remaining == 0;
	}

	size_t Read(void *ptr, size_t read_size,
		    gcc_unused Error &error) override {
		size_t nbytes = std::min(remaining, read_size);
		memcpy(ptr, data, nbytes);
		data += nbytes;
		remaining -= nbytes;
		offset += nbytes;
		return nbytes;
	}
};

class RewindTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(RewindTest);
	CPPUNIT_TEST(TestRewind);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestRewind() {
		Mutex mutex;
		Cond cond;

		StringInputStream *sis =
			new StringInputStream("foo://", mutex, cond,
					      "foo bar");
		CPPUNIT_ASSERT(sis->IsReady());

		InputStream *ris = input_rewind_open(sis);
		CPPUNIT_ASSERT(ris != sis);
		CPPUNIT_ASSERT(ris != nullptr);

		const ScopeLock protect(mutex);

		ris->Update();
		CPPUNIT_ASSERT(ris->IsReady());
		CPPUNIT_ASSERT(!ris->KnownSize());
		CPPUNIT_ASSERT_EQUAL(offset_type(0), ris->GetOffset());

		Error error;
		char buffer[16];
		size_t nbytes = ris->Read(buffer, 2, error);
		CPPUNIT_ASSERT_EQUAL(size_t(2), nbytes);
		CPPUNIT_ASSERT_EQUAL('f', buffer[0]);
		CPPUNIT_ASSERT_EQUAL('o', buffer[1]);
		CPPUNIT_ASSERT_EQUAL(offset_type(2), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		nbytes = ris->Read(buffer, 2, error);
		CPPUNIT_ASSERT_EQUAL(size_t(2), nbytes);
		CPPUNIT_ASSERT_EQUAL('o', buffer[0]);
		CPPUNIT_ASSERT_EQUAL(' ', buffer[1]);
		CPPUNIT_ASSERT_EQUAL(offset_type(4), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		CPPUNIT_ASSERT(ris->Seek(1, error));
		CPPUNIT_ASSERT_EQUAL(offset_type(1), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		nbytes = ris->Read(buffer, 2, error);
		CPPUNIT_ASSERT_EQUAL(size_t(2), nbytes);
		CPPUNIT_ASSERT_EQUAL('o', buffer[0]);
		CPPUNIT_ASSERT_EQUAL('o', buffer[1]);
		CPPUNIT_ASSERT_EQUAL(offset_type(3), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		CPPUNIT_ASSERT(ris->Seek(0, error));
		CPPUNIT_ASSERT_EQUAL(offset_type(0), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		nbytes = ris->Read(buffer, 2, error);
		CPPUNIT_ASSERT_EQUAL(size_t(2), nbytes);
		CPPUNIT_ASSERT_EQUAL('f', buffer[0]);
		CPPUNIT_ASSERT_EQUAL('o', buffer[1]);
		CPPUNIT_ASSERT_EQUAL(offset_type(2), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		nbytes = ris->Read(buffer, sizeof(buffer), error);
		CPPUNIT_ASSERT_EQUAL(size_t(2), nbytes);
		CPPUNIT_ASSERT_EQUAL('o', buffer[0]);
		CPPUNIT_ASSERT_EQUAL(' ', buffer[1]);
		CPPUNIT_ASSERT_EQUAL(offset_type(4), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		nbytes = ris->Read(buffer, sizeof(buffer), error);
		CPPUNIT_ASSERT_EQUAL(size_t(3), nbytes);
		CPPUNIT_ASSERT_EQUAL('b', buffer[0]);
		CPPUNIT_ASSERT_EQUAL('a', buffer[1]);
		CPPUNIT_ASSERT_EQUAL('r', buffer[2]);
		CPPUNIT_ASSERT_EQUAL(offset_type(7), ris->GetOffset());
		CPPUNIT_ASSERT(ris->IsEOF());

		CPPUNIT_ASSERT(ris->Seek(3, error));
		CPPUNIT_ASSERT_EQUAL(offset_type(3), ris->GetOffset());
		CPPUNIT_ASSERT(!ris->IsEOF());

		nbytes = ris->Read(buffer, sizeof(buffer), error);
		CPPUNIT_ASSERT_EQUAL(size_t(4), nbytes);
		CPPUNIT_ASSERT_EQUAL(' ', buffer[0]);
		CPPUNIT_ASSERT_EQUAL('b', buffer[1]);
		CPPUNIT_ASSERT_EQUAL('a', buffer[2]);
		CPPUNIT_ASSERT_EQUAL('r', buffer[3]);
		CPPUNIT_ASSERT_EQUAL(offset_type(7), ris->GetOffset());
		CPPUNIT_ASSERT(ris->IsEOF());
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(RewindTest);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
