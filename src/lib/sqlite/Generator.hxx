// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "co/Generator.hxx"

struct sqlite3_stmt;

namespace Sqlite {

class Row;

Co::Generator<Row>
GenerateRows(sqlite3_stmt &stmt);

} // namespace Sqlite
