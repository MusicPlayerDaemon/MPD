// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Client.hxx"
#include "Config.hxx"
#include "Domain.hxx"
#include "command/AllCommands.hxx"
#include "Log.hxx"
#include "util/StringAPI.hxx"
#include "util/CharUtil.hxx"

#define CLIENT_LIST_MODE_BEGIN "command_list_begin"
#define CLIENT_LIST_OK_MODE_BEGIN "command_list_ok_begin"
#define CLIENT_LIST_MODE_END "command_list_end"

inline CommandResult
Client::ProcessCommandList(bool list_ok,
			   std::list<std::string> &&list) noexcept
{
	unsigned n = 0;

	for (auto &&i : list) {
		char *cmd = &*i.begin();

		FmtDebug(client_domain, "process command {:?}", cmd);
		auto ret = command_process(*this, n++, cmd);
		FmtDebug(client_domain, "command returned {}", unsigned(ret));
		if (IsExpired())
			return CommandResult::CLOSE;
		else if (ret != CommandResult::OK)
			return ret;
		else if (list_ok)
			Write("list_OK\n");
	}

	return CommandResult::OK;
}

CommandResult
Client::ProcessLine(char *line) noexcept
{
	assert(!background_command);

	if (!IsLowerAlphaASCII(*line)) {
		/* all valid MPD commands begin with a lower case
		   letter; this could be a badly routed HTTP
		   request */
		FmtWarning(client_domain,
			   "[{}] malformed command {:?}",
			   num, line);
		return CommandResult::CLOSE;
	}

	if (StringIsEqual(line, "noidle")) {
		if (idle_waiting) {
			/* send empty idle response and leave idle mode */
			idle_waiting = false;
			WriteOK();
		}

		/* do nothing if the client wasn't idling: the client
		   has already received the full idle response from
		   IdleNotify(), which he can now evaluate */

		return CommandResult::OK;
	} else if (idle_waiting) {
		/* during idle mode, clients must not send anything
		   except "noidle" */
		FmtWarning(client_domain,
			   "[{}] command {:?} during idle",
			   num, line);
		return CommandResult::CLOSE;
	}

	if (cmd_list.IsActive()) {
		if (StringIsEqual(line, CLIENT_LIST_MODE_END)) {
			const unsigned id = num;

			FmtDebug(client_domain,
				 "[{}] process command list",
				 id);

			const bool ok_mode = cmd_list.IsOKMode();
			auto list = cmd_list.Commit();
			cmd_list.Reset();

			auto ret = ProcessCommandList(ok_mode,
						      std::move(list));
			FmtDebug(client_domain,
				 "[{}] process command "
				 "list returned {}", id, unsigned(ret));

			if (ret == CommandResult::OK)
				WriteOK();

			return ret;
		} else {
			if (!cmd_list.Add(line)) {
				FmtWarning(client_domain,
					   "[{}] command list size "
					   "is larger than the max ({})",
					   num, client_max_command_list_size);
				return CommandResult::CLOSE;
			}

			return CommandResult::OK;
		}
	} else {
		if (StringIsEqual(line, CLIENT_LIST_MODE_BEGIN)) {
			cmd_list.Begin(false);
			return CommandResult::OK;
		} else if (StringIsEqual(line, CLIENT_LIST_OK_MODE_BEGIN)) {
			cmd_list.Begin(true);
			return CommandResult::OK;
		} else {
			const unsigned id = num;

			FmtDebug(client_domain,
				 "[{}] process command {:?}",
				 id, line);
			auto ret = command_process(*this, 0, line);
			FmtDebug(client_domain,
				 "[{}] command returned {}",
				 id, unsigned(ret));

			if (IsExpired())
				return CommandResult::CLOSE;

			if (ret == CommandResult::OK)
				WriteOK();

			return ret;
		}
	}
}
