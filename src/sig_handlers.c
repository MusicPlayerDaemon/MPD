/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
 * (c) 2004 Nick Welch (mack@incise.org)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sig_handlers.h"
#include "player.h"
#include "playlist.h"
#include "directory.h"
#include "command.h"
#include "signal_check.h"
#include "log.h"
#include "player.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>

int handlePendingSignals() {
        if(signal_is_pending(SIGINT) || signal_is_pending(SIGTERM)) {
                DEBUG("got SIGINT or SIGTERM, exiting\n");
		return COMMAND_RETURN_KILL;
        }

	if(signal_is_pending(SIGHUP)) {
                DEBUG("got SIGHUP, rereading DB\n");
		signal_clear(SIGHUP);
		if(!isUpdatingDB()) {
                        readDirectoryDB();
		        incrPlaylistVersion();
                }
                if(myfprintfCloseAndOpenLogFile()<0) return COMMAND_RETURN_KILL;
                playerCycleLogFiles();
	}

	return 0;
}

void chldSigHandler(int signal) {
	int status;
	int pid;
	DEBUG("got SIGCHLD\n");
	while(0 != (pid = wait3(&status,WNOHANG,NULL))) {
		if(pid<0) {
			if(errno==EINTR) continue;
			else break;
		}
		player_sigChldHandler(pid,status);
		directory_sigChldHandler(pid,status);
	}
}

void initSigHandlers() {
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	while(sigaction(SIGPIPE,&sa,NULL)<0 && errno==EINTR);
	sa.sa_handler = chldSigHandler;
	while(sigaction(SIGCHLD,&sa,NULL)<0 && errno==EINTR);
        signal_handle(SIGUSR1);
        signal_handle(SIGINT);
        signal_handle(SIGTERM);
        signal_handle(SIGHUP);
}

void finishSigHandlers() {
        signal_unhandle(SIGINT);
        signal_unhandle(SIGUSR1);
        signal_unhandle(SIGTERM);
        signal_unhandle(SIGHUP);
}

void ignoreSignals() {
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	while(sigaction(SIGPIPE,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGCHLD,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGUSR1,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGINT,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGTERM,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGHUP,&sa,NULL)<0 && errno==EINTR);
}

void blockSignals() {
	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset,SIGCHLD);
	sigaddset(&sset,SIGUSR1);
	sigaddset(&sset,SIGHUP);
	sigaddset(&sset,SIGINT);
	sigaddset(&sset,SIGTERM);
	while(sigprocmask(SIG_BLOCK,&sset,NULL)<0 && errno==EINTR);
}

void unblockSignals() {
	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset,SIGCHLD);
	sigaddset(&sset,SIGUSR1);
	sigaddset(&sset,SIGHUP);
	sigaddset(&sset,SIGINT);
	sigaddset(&sset,SIGTERM);
	while(sigprocmask(SIG_UNBLOCK,&sset,NULL)<0 && errno==EINTR);
}
/* vim:set shiftwidth=4 tabstop=8 expandtab: */
