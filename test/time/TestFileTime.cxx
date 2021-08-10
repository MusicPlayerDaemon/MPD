/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
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
