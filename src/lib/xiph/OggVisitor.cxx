// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "OggVisitor.hxx"

#include <stdexcept>
#include <utility>

void
OggVisitor::EndStream()
{
	if (!has_stream)
		return;

	has_stream = false;
	OnOggEnd();
}

inline bool
OggVisitor::ReadNextPage()
{
	ogg_page page;
	if (!sync.ExpectPage(page))
		return false;

	const auto page_serialno = ogg_page_serialno(&page);
	if (page_serialno != stream.GetSerialNo()) {
		EndStream();
		stream.Reinitialize(page_serialno);
	}

	stream.PageIn(page);
	return true;
}

inline void
OggVisitor::HandlePacket(const ogg_packet &packet)
{
	const bool _post_seek = std::exchange(post_seek, false);

	if (packet.b_o_s) {
		if (_post_seek)
			/* ignore the BOS packet after seeking */
			return;

		EndStream();
		has_stream = true;
		OnOggBeginning(packet);
		return;
	}

	if (!has_stream)
		/* fail if BOS is missing */
		throw std::runtime_error("BOS packet expected");

	OnOggPacket(packet);

	if (packet.e_o_s) {
		EndStream();
		return;
	}
}

inline void
OggVisitor::HandlePackets()
{
	ogg_packet packet;
	while (stream.PacketOut(packet) == 1)
		HandlePacket(packet);
}

void
OggVisitor::Visit()
{
	do {
		HandlePackets();
	} while (ReadNextPage());
}

void
OggVisitor::PostSeek(uint64_t offset)
{
	sync.Reset();
	sync.SetOffset(offset);

	/* reset the stream to clear any previous partial packet
	   data */
	stream.Reset();

	/* find the next Ogg page and feed it into the stream */
	sync.ExpectPageSeekIn(stream);

	post_seek = true;
}

ogg_int64_t
OggVisitor::ReadGranulepos() noexcept
{
	ogg_packet packet;
	while (stream.PacketOut(packet) == 1)
		if (packet.granulepos >= 0)
			return packet.granulepos;

	return -1;
}
