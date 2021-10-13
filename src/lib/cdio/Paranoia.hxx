/*
 * Copyright 2019 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef CDIO_PARANOIA_HXX
#define CDIO_PARANOIA_HXX

#include "util/ConstBuffer.hxx"

#include <cdio/version.h>
#include <cdio/paranoia/paranoia.h>

#include <stdexcept>
#include <utility>

#include <cstdio>

class CdromDrive {
	cdrom_drive_t *drv = nullptr;

public:
	CdromDrive() = default;

	explicit CdromDrive(CdIo_t *cdio)
		:drv(cdio_cddap_identify_cdio(cdio, 1, nullptr))
	{
		if (drv == nullptr)
			throw std::runtime_error("Failed to identify audio CD");

		cdda_verbose_set(drv, CDDA_MESSAGE_FORGETIT,
				 CDDA_MESSAGE_FORGETIT);
	}

	~CdromDrive() noexcept {
		if (drv != nullptr)
			cdio_cddap_close_no_free_cdio(drv);
	}

	CdromDrive(CdromDrive &&src) noexcept
		:drv(std::exchange(src.drv, nullptr)) {}

	CdromDrive &operator=(CdromDrive &&src) noexcept {
		using std::swap;
		swap(drv, src.drv);
		return *this;
	}

	auto get() const noexcept {
		return drv;
	}

	void Open() {
		if (cdio_cddap_open(drv) != 0)
			throw std::runtime_error("Failed to open disc");
	}

	auto GetDiscSectorRange() const {
		auto first = cdio_cddap_disc_firstsector(drv);
		auto last = cdio_cddap_disc_lastsector(drv);
		if (first < 0 || last < 0)
			throw std::runtime_error("Failed to get disc audio sectors");
		return std::pair(first, last);
	}

	[[gnu::pure]]
	bool IsAudioTrack(track_t i) const noexcept {
		return cdio_cddap_track_audiop(drv, i);
	}

	auto GetTrackSectorRange(track_t i) const {
		auto first = cdio_cddap_track_firstsector(drv, i);
		auto last = cdio_cddap_track_lastsector(drv, i);
		if (first < 0 || last < 0)
			throw std::runtime_error("Invalid track number");
		return std::pair(first, last);
	}

	[[gnu::pure]]
	unsigned GetTrackCount() const noexcept {
		return cdio_cddap_tracks(drv);
	}

	unsigned GetTrackChannels(track_t i) const {
		auto value = cdio_cddap_track_channels(drv, i);
		if (value < 0)
			throw std::runtime_error("cdio_cddap_track_channels() failed");
		return unsigned(value);
	}
};

class CdromParanoia {
	cdrom_paranoia_t *paranoia = nullptr;

public:
	CdromParanoia() = default;

	explicit CdromParanoia(cdrom_drive_t *drv) noexcept
		:paranoia(cdio_paranoia_init(drv)) {}

	~CdromParanoia() noexcept {
		if (paranoia != nullptr)
			cdio_paranoia_free(paranoia);
	}

	CdromParanoia(CdromParanoia &&src) noexcept
		:paranoia(std::exchange(src.paranoia, nullptr)) {}

	CdromParanoia &operator=(CdromParanoia &&src) noexcept {
		using std::swap;
		swap(paranoia, src.paranoia);
		return *this;
	}

	auto get() const noexcept {
		return paranoia;
	}

	void SetMode(int mode_flags) noexcept {
		paranoia_modeset(paranoia, mode_flags);
	}

	void Seek(int32_t seek, int whence=SEEK_SET) {
		if (cdio_paranoia_seek(paranoia, seek, whence) < 0)
			throw std::runtime_error("Failed to seek disc");
	}

	ConstBuffer<int16_t> Read() {
		const int16_t *data = cdio_paranoia_read(paranoia, nullptr);
		if (data == nullptr)
			throw std::runtime_error("Read from audio CD failed");

		return {data, CDIO_CD_FRAMESIZE_RAW / sizeof(int16_t)};
	}
};

#endif
