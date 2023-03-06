// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "time/FileTime.hxx"

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <tchar.h>

TEST(Time, FileTimeToChrono)
{
	WIN32_FILE_ATTRIBUTE_DATA data;

	ASSERT_TRUE(GetFileAttributesEx(_T("."), GetFileExInfoStandard,
					&data));
	const auto tp = FileTimeToChrono(data.ftLastWriteTime);

	struct stat st;
	ASSERT_EQ(stat(".", &st), 0);

	ASSERT_EQ(std::chrono::system_clock::to_time_t(tp), st.st_mtime);

	const auto ft2 = ChronoToFileTime(std::chrono::system_clock::from_time_t(st.st_mtime));
	const auto tp2 = FileTimeToChrono(ft2);
	ASSERT_EQ(std::chrono::system_clock::to_time_t(tp2), st.st_mtime);
}
