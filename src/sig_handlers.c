/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

struct sigaction original_termSa;
struct sigaction original_hupSa;

void termSigHandler(int signal) {
	if(signal==SIGTERM) {
		savePlaylistState();
		playerKill();
		exit(EXIT_SUCCESS);
	}
}

void usr1SigHandler(int signal) {
}

void hupSigHandler(int signal) {
	readDirectoryDB();
}

void chldSigHandler(int signal) {
	int status;
	int pid = wait3(&status,WNOHANG,NULL);
	if(pid>0) {
		player_sigChldHandler(pid,status);
		directory_sigChldHandler(pid,status);
	}
}

void initSigHandlers() {
	struct sigaction sa;

	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE,&sa,NULL);
	sa.sa_handler = usr1SigHandler;
	sigaction(SIGUSR1,&sa,NULL);
	sa.sa_handler = chldSigHandler;
	sigaction(SIGCHLD,&sa,NULL);
	sa.sa_handler = hupSigHandler;
	sigaction(SIGHUP,&sa,&original_hupSa);
	sa.sa_handler = termSigHandler;
	sigaction(SIGTERM,&sa,&original_termSa);
}

void finishSigHandlers() {
	sigaction(SIGHUP,&original_termSa,NULL);
	sigaction(SIGTERM,&original_termSa,NULL);
}

void blockSignals() {
	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset,SIGCHLD);
	sigaddset(&sset,SIGUSR1);
	sigaddset(&sset,SIGHUP);
	sigprocmask(SIG_BLOCK,&sset,NULL);
}

void unblockSignals() {
	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset,SIGCHLD);
	sigaddset(&sset,SIGUSR1);
	sigaddset(&sset,SIGHUP);
	sigprocmask(SIG_UNBLOCK,&sset,NULL);
}

void blockTermSignal() {
	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset,SIGTERM);
	sigaddset(&sset,SIGHUP);
	sigprocmask(SIG_BLOCK,&sset,NULL);
}

void unblockTermSignal() {
	sigset_t sset;

	sigemptyset(&sset);
	sigaddset(&sset,SIGTERM);
	sigaddset(&sset,SIGHUP);
	sigprocmask(SIG_UNBLOCK,&sset,NULL);
}
