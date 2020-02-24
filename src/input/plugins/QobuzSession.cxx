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
#include "QobuzSession.hxx"
#include "external/jaijson/Deserializer.hxx"
#include "external/jaijson/Serializer.hxx"

bool
deserialize(const jaijson::Value &d, QobuzSession &m)
{
	deserialize(d, "app_id", m.app_id);
	deserialize(d, "app_secret", m.app_secret);
	deserialize(d, "user_auth_token", m.user_auth_token);
	deserialize(d, "format_id", m.format_id);
	deserialize(d, "user_id", m.user_id);
	deserialize(d, "device_id", m.device_id);
	deserialize(d, "credential_id", m.credential_id);
	m.user_purchases_track_ids.clear();
	deserialize(d, "user_purchases_track_ids", m.user_purchases_track_ids);

	return !m.user_auth_token.empty();
}
