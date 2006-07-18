/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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
#include "playerData.h"
#include "playlist.h"
#include "directory.h"
#include "command.h"
#include "signal_check.h"
#include "log.h"
#include "player.h"
#include "decode.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

extern volatile int masterPid;
extern volatile int mainPid;

int masterHandlePendingSignals() {
        if(signal_is_pending(SIGINT) || signal_is_pending(SIGTERM)) {
                DEBUG("master process got SIGINT or SIGTERM, exiting\n");
		return COMMAND_RETURN_KILL;
        }

	if(signal_is_pending(SIGHUP)) {
		signal_clear(SIGHUP);
		/* Forward it to the main process, which will update the DB */
		kill(mainPid, SIGHUP); 
	}


	return 0;
}

int handlePendingSignals() {
	/* this SIGUSR1 signal comes before the KILL signals, because there if the process is 
	 * looping, waiting for this signal, it will not respond to the KILL signal. This might be
	 * better implemented by using bit-wise defines and or'ing of the COMMAND_FOO as return.
	 */
       	if (signal_is_pending(SIGUSR1)) {
		signal_clear(SIGUSR1);
		DEBUG("The master process is ready to receive signals\n");
		return COMMAND_MASTER_READY;
	}
	
	if(signal_is_pending(SIGINT) || signal_is_pending(SIGTERM)) {
                DEBUG("main process got SIGINT or SIGTERM, exiting\n");
		return COMMAND_RETURN_KILL;
        }

	if(signal_is_pending(SIGHUP)) {
                DEBUG("got SIGHUP, rereading DB\n");
		signal_clear(SIGHUP);
		if(!isUpdatingDB()) {
                        readDirectoryDB();
		        playlistVersionChange();
                }
                if(myfprintfCloseAndOpenLogFile()<0) return COMMAND_RETURN_KILL;
                playerCycleLogFiles();
	}

	return 0;
}

void chldSigHandler(int signal) {
	int status;
	int pid;
	DEBUG("main process got SIGCHLD\n");
	while(0 != (pid = wait3(&status,WNOHANG,NULL))) {
		if(pid<0) {
			if(errno==EINTR) continue;
			else break;
		}
		directory_sigChldHandler(pid,status);
	}
}

void masterChldSigHandler(int signal) {
	int status;
	int pid;
	DEBUG("master process got SIGCHLD\n");
	while(0 != (pid = wait3(&status,WNOHANG,NULL))) {
		if(pid<0) {
			if(errno==EINTR) continue;
			else break;
		}
		DEBUG("PID: %d\n",pid);
		if (pid == mainPid) kill(getpid(), SIGTERM); 
		player_sigChldHandler(pid,status);
	}
}

int playerInitReal();

void masterSigUsr2Handler(int signal) {
	DEBUG("Master process got SIGUSR2 starting a new player process\n");
	if (getPlayerPid() <= 0)
		playerInitReal();
}

void masterInitSigHandlers() {
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	while(sigaction(SIGPIPE,&sa,NULL)<0 && errno==EINTR);
	sa.sa_handler = masterChldSigHandler;
	while(sigaction(SIGCHLD,&sa,NULL)<0 && errno==EINTR);
	sa.sa_handler = masterSigUsr2Handler;
	while(sigaction(SIGUSR2,&sa,NULL)<0 && errno==EINTR);
	signal_handle(SIGUSR1);
        signal_handle(SIGINT);
        signal_handle(SIGTERM);
        signal_handle(SIGHUP);
}

void initSigHandlers() {
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	while(sigaction(SIGPIPE,&sa,NULL)<0 && errno==EINTR);
	sa.sa_handler = chldSigHandler;
	while(sigaction(SIGCHLD,&sa,NULL)<0 && errno==EINTR);
        signal_handle(SIGUSR2);
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

void setSigHandlersForDecoder() {
	struct sigaction sa;

	finishSigHandlers();

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	while(sigaction(SIGHUP,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGINT,&sa,NULL)<0 && errno==EINTR);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = decodeSigHandler;
	while(sigaction(SIGCHLD,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGTERM,&sa,NULL)<0 && errno==EINTR);
}

void ignoreSignals() {
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	sa.sa_sigaction = NULL;
	while(sigaction(SIGPIPE,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGCHLD,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGUSR1,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGUSR2,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGINT,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGTERM,&sa,NULL)<0 && errno==EINTR);
	while(sigaction(SIGHUP,&sa,NULL)<0 && errno==EINTR);
}

void waitOnSignals() {
	sigset_t sset;

	sigfillset(&sset);
	sigdelset(&sset,SIGCHLD);
	sigdelset(&sset,SIGUSR1);
	sigdelset(&sset,SIGUSR2);
	sigdelset(&sset,SIGHUP);
	sigdelset(&sset,SIGINT);
	sigdelset(&sset,SIGTERM);
	sigsuspend(&sset);
}

void blockSignals() {
	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset,SIGCHLD);
	sigaddset(&sset,SIGUSR1);
	sigaddset(&sset,SIGUSR2);
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
	sigaddset(&sset,SIGUSR2);
	sigaddset(&sset,SIGHUP);
	sigaddset(&sset,SIGINT);
	sigaddset(&sset,SIGTERM);
	while(sigprocmask(SIG_UNBLOCK,&sset,NULL)<0 && errno==EINTR);
}
