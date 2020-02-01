/*
 * Unit tests for class RewindInputStream.
 */

#include "input/RewindInputStream.hxx"
#include "input/InputStream.hxx"
#include "thread/Mutex.hxx"

#include <gtest/gtest.h>

#include <string.h>

class StringInputStream final : public InputStream {
	const char *data;
	size_t remaining;

public:
	StringInputStream(const char *_uri,
			  Mutex &_mutex,
			  const char *_data)
		:InputStream(_uri, _mutex),
		 data(_data), remaining(strlen(data)) {
		SetReady();
	}

	/* virtual methods from InputStream */
	bool IsEOF() const noexcept override {
		return remaining == 0;
	}

	size_t Read(std::unique_lock<Mutex> &,
		    void *ptr, size_t read_size) override {
		size_t nbytes = std::min(remaining, read_size);
		memcpy(ptr, data, nbytes);
		data += nbytes;
		remaining -= nbytes;
		offset += nbytes;
		return nbytes;
	}
};

TEST(RewindInputStream, Basic)
{
	Mutex mutex;

	auto *sis =
		new StringInputStream("foo://", mutex,
				      "foo bar");
	EXPECT_TRUE(sis->IsReady());

	auto ris = input_rewind_open(InputStreamPtr(sis));
	EXPECT_TRUE(ris.get() != sis);
	EXPECT_TRUE(ris != nullptr);

	std::unique_lock<Mutex> lock(mutex);

	ris->Update();
	EXPECT_TRUE(ris->IsReady());
	EXPECT_FALSE(ris->KnownSize());
	EXPECT_EQ(offset_type(0), ris->GetOffset());

	char buffer[16];
	size_t nbytes = ris->Read(lock, buffer, 2);
	EXPECT_EQ(size_t(2), nbytes);
	EXPECT_EQ('f', buffer[0]);
	EXPECT_EQ('o', buffer[1]);
	EXPECT_EQ(offset_type(2), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	nbytes = ris->Read(lock, buffer, 2);
	EXPECT_EQ(size_t(2), nbytes);
	EXPECT_EQ('o', buffer[0]);
	EXPECT_EQ(' ', buffer[1]);
	EXPECT_EQ(offset_type(4), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	ris->Seek(lock, 1);
	EXPECT_EQ(offset_type(1), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	nbytes = ris->Read(lock, buffer, 2);
	EXPECT_EQ(size_t(2), nbytes);
	EXPECT_EQ('o', buffer[0]);
	EXPECT_EQ('o', buffer[1]);
	EXPECT_EQ(offset_type(3), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	ris->Seek(lock, 0);
	EXPECT_EQ(offset_type(0), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	nbytes = ris->Read(lock, buffer, 2);
	EXPECT_EQ(size_t(2), nbytes);
	EXPECT_EQ('f', buffer[0]);
	EXPECT_EQ('o', buffer[1]);
	EXPECT_EQ(offset_type(2), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	nbytes = ris->Read(lock, buffer, sizeof(buffer));
	EXPECT_EQ(size_t(2), nbytes);
	EXPECT_EQ('o', buffer[0]);
	EXPECT_EQ(' ', buffer[1]);
	EXPECT_EQ(offset_type(4), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	nbytes = ris->Read(lock, buffer, sizeof(buffer));
	EXPECT_EQ(size_t(3), nbytes);
	EXPECT_EQ('b', buffer[0]);
	EXPECT_EQ('a', buffer[1]);
	EXPECT_EQ('r', buffer[2]);
	EXPECT_EQ(offset_type(7), ris->GetOffset());
	EXPECT_TRUE(ris->IsEOF());

	ris->Seek(lock, 3);
	EXPECT_EQ(offset_type(3), ris->GetOffset());
	EXPECT_FALSE(ris->IsEOF());

	nbytes = ris->Read(lock, buffer, sizeof(buffer));
	EXPECT_EQ(size_t(4), nbytes);
	EXPECT_EQ(' ', buffer[0]);
	EXPECT_EQ('b', buffer[1]);
	EXPECT_EQ('a', buffer[2]);
	EXPECT_EQ('r', buffer[3]);
	EXPECT_EQ(offset_type(7), ris->GetOffset());
	EXPECT_TRUE(ris->IsEOF());
}
