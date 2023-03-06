// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "LogCallback.hxx"
#include "Domain.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

extern "C" {
#include <libavutil/log.h>
}

[[gnu::const]]
static LogLevel
FfmpegImportLogLevel(int level) noexcept
{
	if (level <= AV_LOG_FATAL)
		return LogLevel::ERROR;

	if (level <= AV_LOG_WARNING)
		return LogLevel::WARNING;

	if (level <= AV_LOG_INFO)
		return LogLevel::INFO;

	return LogLevel::DEBUG;
}

void
FfmpegLogCallback(void *ptr, int level, const char *fmt, std::va_list vl)
{
	const AVClass * cls = nullptr;

	if (ptr != nullptr)
		cls = *(const AVClass *const*)ptr;

	if (cls != nullptr) {
		const auto domain =
			FmtBuffer<64>("{}/{}",
				      ffmpeg_domain.GetName(),
				      cls->item_name(ptr));
		const Domain d(domain);

		char msg[1024];
		vsnprintf(msg, sizeof(msg), fmt, vl);

		Log(FfmpegImportLogLevel(level), d, msg);
	}
}
