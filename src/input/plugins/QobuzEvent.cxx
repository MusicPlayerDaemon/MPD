/*
 * Copyright 2015-2018 Cary Audio
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

#include "config.h"
#include "QobuzEvent.hxx"
#include "external/jaijson/Deserializer.hxx"
#include "external/jaijson/Serializer.hxx"

void
serialize(jaijson::Writer &w, const QobuzEvent &m)
{
	w.StartObject();

	serialize(w, "user_id", m.user_id);
	serialize(w, "date", m.date);
	if (m.duration > 0) {
		serialize(w, "duration", m.duration);
	}
	serialize(w, "online", m.online);
	serialize(w, "intent", m.intent);
	serialize(w, "sample", m.sample);
	serialize(w, "device_id", m.device_id);
	serialize(w, "track_id", m.track_id);
	serialize(w, "purchase", m.purchase);
	serialize(w, "local", m.local);
	serialize(w, "credential_id", m.credential_id);
	serialize(w, "format_id", m.format_id);

	w.EndObject();
}
