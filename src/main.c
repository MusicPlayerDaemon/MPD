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

#include "interface.h"
#include "command.h"
#include "playlist.h"
#include "directory.h"
#include "player.h"
#include "listen.h"
#include "conf.h"
#include "path.h"
#include "playerData.h"
#include "stats.h"
#include "sig_handlers.h"
#include "audio.h"
#include "volume.h"
#include "log.h"
#include "permission.h"
#include "replayGain.h"
#include "inputPlugin.h"
#include "inputStream.h"
#include "tag.h"
#include "tagTracker.h"
#include "dbUtils.h"
#include "../config.h"

#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#define SYSTEM_CONFIG_FILE_LOCATION	"/etc/mpd.conf"
#define USER_CONFIG_FILE_LOCATION	"/.mpdconf"

typedef struct _Options {
        int daemon;
        int stdOutput;
        int createDB;
	int updateDB;
} Options;

void usage(char * argv[]) {
        ERROR("usage:\n");
        ERROR("   %s [options] <port> <music dir> <playlist dir> <log file> <error file>\n",argv[0]);
        ERROR("   %s [options] <conf file>\n",argv[0]);
        ERROR("   %s [options]   (searches for ~%s then %s)\n",
                        argv[0],USER_CONFIG_FILE_LOCATION,
                        SYSTEM_CONFIG_FILE_LOCATION);
        ERROR("\n");
        ERROR("options:\n");
        ERROR("   --help             this usage statement\n");
        ERROR("   --no-daemon        don't detach from console\n");
        ERROR("   --stdout           print msgs to stdout and stderr\n");
        ERROR("   --create-db        force (re)creation database and exit\n");
        /*ERROR("   --update-db        create database and exit\n");*/
        ERROR("   --no-create-db     don't create database\n");
        ERROR("   --verbose          verbose logging\n");
        ERROR("   --version          prints version information\n");
}

void version() {
        LOG("mpd (MPD: Music Player Daemon) %s\n",VERSION);
        LOG("\n");
        LOG("Copyright (C) 2003-2004 Warren Dukes <shank@mercury.chem.pitt.edu>\n");
        LOG("This is free software; see the source for copying conditions.  There is NO\n");
        LOG("warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
        LOG("\n");
        LOG("Supported formats:\n");

        initInputPlugins();
        printAllInputPluginSuffixes(stdout);
}

void parseOptions(int argc, char ** argv, Options * options) {
        int argcLeft = argc;

        options->daemon = 1;
        options->stdOutput = 0;
        options->createDB = 0;
        options->updateDB = 0;

        if(argc>1) {
                int i = 1;
                while(i<argc) {
                        if(strncmp(argv[i],"--",2)==0) {
                                if(strcmp(argv[i],"--help")==0) {
                                        usage(argv);
                                        exit(EXIT_SUCCESS);
                                }
                                else if(strcmp(argv[i],"--no-daemon")==0) {
                                        options->daemon = 0;
                                        argcLeft--;
                                }
                                else if(strcmp(argv[i],"--stdout")==0) {
                                        options->stdOutput = 1;
                                        argcLeft--;
                                }
                                else if(strcmp(argv[i],"--create-db")==0) {
                                        options->stdOutput = 1;
                                        options->createDB = 1;
                                        argcLeft--;
                                }
                                /*else if(strcmp(argv[i],"--update-db")==0) {
                                        options->stdOutput = 1;
                                        options->updateDB = 1;
                                        argcLeft--;
                                }*/
                                else if(strcmp(argv[i],"--no-create-db")==0) {
                                        options->createDB = -1;
                                        argcLeft--;
                                }
                                else if(strcmp(argv[i],"--verbose")==0) {
                                        logLevel = LOG_LEVEL_DEBUG;
                                        argcLeft--;
                                }
                                else if(strcmp(argv[i],"--version")==0) {
                                        version();
                                        exit(EXIT_SUCCESS);
                                }
                                else {
                                        myfprintf(stderr,"unknown command line option: %s\n",argv[i]);
                                        exit(EXIT_FAILURE);
                                }
                        }
                        else break;
                        i++;
                }
        }

        if(argcLeft<=2) {
                if(argcLeft==2) {
			readConf(argv[argc-1]);
			return;
		}
                else if(argcLeft==1) {
			struct stat st;
                        char * homedir = getenv("HOME");
                        char userfile[MAXPATHLEN+1] = "";
                        if(homedir && (strlen(homedir)+
                        		strlen(USER_CONFIG_FILE_LOCATION)) <
                                        	MAXPATHLEN) {
                                strcpy(userfile,homedir);
                                strcat(userfile,USER_CONFIG_FILE_LOCATION);
                        }
                        if(strlen(userfile) && (0 == stat(userfile,&st))) {
                                readConf(userfile);
				return;
                        }
                        else if(0 == stat(SYSTEM_CONFIG_FILE_LOCATION,&st)) {
                                readConf(SYSTEM_CONFIG_FILE_LOCATION);
				return;
                        }
                }
        }

        usage(argv);
        exit(EXIT_FAILURE);
}

void closeAllFDs() {
        int i;
	int fds = getdtablesize();

        for(i = 3; i < fds; i++) close(i);
}

void changeToUser() {
	ConfigParam * param = getConfigParam(CONF_USER);
	
        if (param && strlen(param->value)) {
                /* get uid */
                struct passwd * userpwd;
                if ((userpwd = getpwnam(param->value)) == NULL) {
                        ERROR("no such user \"%s\" at line %i\n", param->value,
					param->line);
                        exit(EXIT_FAILURE);
                }

                if(setgid(userpwd->pw_gid) == -1) {
                        ERROR("cannot setgid for user \"%s\" at line %i: %s\n", 					param->value, param->line,
                                        strerror(errno));
                        exit(EXIT_FAILURE);
                }

#ifdef _BSD_SOURCE
                /* init suplementary groups 
                 * (must be done before we change our uid)
                 */
                if (initgroups(param->value, userpwd->pw_gid) == -1) {
                        WARNING("cannot init suplementary groups "
                                        "of user \"%s\" at line %i: %s\n", 
					param->value, param->line, 
                                        strerror(errno));
                }
#endif

                /* set uid */
                if (setuid(userpwd->pw_uid) == -1) {
                        ERROR("cannot change to uid of user "
                                        "\"%s\" at line %i: %s\n", 
					param->value, param->line,
                                        strerror(errno));
                        exit(EXIT_FAILURE);
                }

		/* this is needed by libs such as arts */
		if(userpwd->pw_dir) {
			setenv("HOME", userpwd->pw_dir, 1);
		}
        }
}

void openLogFiles(Options * options, FILE ** out, FILE ** err) {
	ConfigParam * logParam = parseConfigFilePath(CONF_LOG_FILE, 1);
	ConfigParam * errorParam = parseConfigFilePath(CONF_ERROR_FILE, 1);
	
        mode_t prev;

        if(options->stdOutput) {
                flushWarningLog();
                return;
        }

        /* be sure to create log files w/ rw permissions*/
        prev = umask(0066);

        if(NULL==(*out=fopen(logParam->value,"a"))) {
                ERROR("problem opening log file \"%s\" (config line %i) for "
				"writing\n", logParam->value, logParam->line);
                exit(EXIT_FAILURE);
        }

        if(NULL==(*err=fopen(errorParam->value,"a"))) {
                ERROR("problem opening error file \"%s\" (config line %i) for "
				"writing\n", errorParam->value, 
				errorParam->line);
                exit(EXIT_FAILURE);
        }

        umask(prev);
}

void openDB(Options * options, char * argv0) {
        if(options->createDB>0 || readDirectoryDB()<0) {
                if(options->createDB<0) {
                        ERROR("can't open db file and using \"--no-create-db\""
                                        " command line option\n");
			ERROR("try running \"%s --create-db\"\n", argv0);
                        exit(EXIT_FAILURE);
                }
                flushWarningLog();
                initMp3Directory();
                if(writeDirectoryDB()<0) exit(EXIT_FAILURE);
		if(options->createDB) exit(EXIT_SUCCESS);
        }
	if(options->updateDB) {
                flushWarningLog();
		updateMp3Directory();
		exit(EXIT_SUCCESS);
	}
}

void daemonize(Options * options) {
        if(options->daemon) {
                int pid;

                fflush(NULL);
                pid = fork();
                if(pid>0) _exit(EXIT_SUCCESS);
                else if(pid<0) {
                        ERROR("problems fork'ing for daemon!\n");
                        exit(EXIT_FAILURE);
                }

                if(chdir("/")<0) {
                        ERROR("problems changing to root directory\n");
                        exit(EXIT_FAILURE);
                }

                if(setsid()<0) {
                        ERROR("problems setsid'ing\n");
                        exit(EXIT_FAILURE);
                }

                fflush(NULL);
                pid = fork();
                if(pid>0) _exit(EXIT_SUCCESS);
                else if(pid<0) {
                        ERROR("problems fork'ing for daemon!\n");
                        exit(EXIT_FAILURE);
                }
        }
}

void setupLogOutput(Options * options, FILE * out, FILE * err) {
        if(!options->stdOutput) {
                fflush(NULL);

                if(dup2(fileno(out),STDOUT_FILENO)<0) {
                        myfprintf(err,"problems dup2 stdout : %s\n",
                                        strerror(errno));
                        exit(EXIT_FAILURE);
                }

                if(dup2(fileno(err),STDERR_FILENO)<0) {
                        myfprintf(err,"problems dup2 stderr : %s\n",
                                        strerror(errno));
                        exit(EXIT_FAILURE);
                }

                myfprintfStdLogMode(out, err);
                flushWarningLog();
        }

        /* lets redirect stdin to dev null as a work around for libao bug */
        {
                int fd = open("/dev/null",O_RDONLY);
                if(fd<0) {
                        ERROR("not able to open /dev/null to redirect stdin: "
                                        "%s\n",strerror(errno));
                        exit(EXIT_FAILURE);
                }
                if(dup2(fd,STDIN_FILENO)<0) {
                        ERROR("problems dup2's stdin for redirection: "
                                        "%s\n",strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }
}

int main(int argc, char * argv[]) {
        FILE * out;
        FILE * err;
        Options options;

        closeAllFDs();

        initConf();

        parseOptions(argc, argv, &options);

        initStats();
	initTagConfig();
        initLog();

        if(options.createDB <= 0 && !options.updateDB) listenOnPort();

        changeToUser();

        openLogFiles(&options, &out, &err);

	initPaths();
	initPermissions();
        initReplayGainState();

        initPlaylist();
        initInputPlugins();

        openDB(&options, argv[0]);

        initCommands();
        initPlayerData();
        initAudioConfig();
        initAudioDriver();
        initVolume();
        initInterfaces();
	initInputStream(); 

	printMemorySavedByTagTracker();
	printSavedMemoryFromFilenames();
	/*printSavedMemoryFromDirectoryNames();*/
	
        daemonize(&options);

        setupLogOutput(&options, out, err);

        openVolumeDevice();
        initSigHandlers();
        readPlaylistState();

        while(COMMAND_RETURN_KILL!=doIOForInterfaces()) {
                syncPlayerAndPlaylist();
                closeOldInterfaces();
		if(COMMAND_RETURN_KILL==handlePendingSignals()) break;
		readDirectoryDBIfUpdateIsFinished();
        }

        savePlaylistState();
        playerKill();

        freeAllInterfaces();
	closeAllListenSockets();
        closeMp3Directory();
        finishPlaylist();
        freePlayerData();
        finishAudioDriver();
        finishAudioConfig();
        finishVolume();
	finishPaths();
	finishPermissions();
        finishCommands();
        finishInputPlugins();

        return EXIT_SUCCESS;
}
