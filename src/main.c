/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (warren.dukes@gmail.com)
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
#include "audioOutput.h"
#include "inputStream.h"
#include "tag.h"
#include "tagTracker.h"
#include "dbUtils.h"
#include "../config.h"
#include "utils.h"

#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
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

volatile int masterPid = 0;
volatile int mainPid = 0;

typedef struct _Options {
	int kill;
        int daemon;
        int stdOutput;
        int createDB;
	int updateDB;
} Options;

/* 
 * from git-1.3.0, needed for solaris
 */
#ifndef HAVE_SETENV
static int setenv(const char *name, const char *value, int replace)
{
	int out;
	size_t namelen, valuelen;
	char *envstr;

	if (!name || !value) return -1;
	if (!replace) {
		char *oldval = NULL;
		oldval = getenv(name);
		if (oldval) return 0;
	}

	namelen = strlen(name);
	valuelen = strlen(value);
	envstr = malloc((namelen + valuelen + 2));
	if (!envstr) return -1;

	memcpy(envstr, name, namelen);
	envstr[namelen] = '=';
	memcpy(envstr + namelen + 1, value, valuelen);
	envstr[namelen + valuelen + 1] = 0;

	out = putenv(envstr);
	/* putenv(3) makes the argument string part of the environment,
	 * and changing that string modifies the environment --- which
	 * means we do not own that storage anymore.  Do not free
	 * envstr.
	 */

	return out;
}
#endif /* HAVE_SETENV */

static void usage(char * argv[]) {
        ERROR("usage:\n");
        ERROR("   %s [options] <conf file>\n",argv[0]);
        ERROR("   %s [options]   (searches for ~%s then %s)\n",
                        argv[0],USER_CONFIG_FILE_LOCATION,
                        SYSTEM_CONFIG_FILE_LOCATION);
        ERROR("\n");
        ERROR("options:\n");
        ERROR("   --help             this usage statement\n");
        ERROR("   --kill             kill the currently running mpd session\n");
        ERROR("   --create-db        force (re)creation of database and exit\n");
        ERROR("   --no-create-db     don't create database, even if it doesn't exist\n");
        ERROR("   --no-daemon        don't detach from console\n");
        ERROR("   --stdout           print messages to stdout and stderr\n");
        /*ERROR("   --update-db        create database and exit\n");*/
        ERROR("   --verbose          verbose logging\n");
        ERROR("   --version          prints version information\n");
}

static void version(void) {
        LOG("mpd (MPD: Music Player Daemon) %s\n",VERSION);
        LOG("\n");
        LOG("Copyright (C) 2003-2006 Warren Dukes <warren.dukes@gmail.com>\n");
        LOG("This is free software; see the source for copying conditions.  There is NO\n");
        LOG("warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
        LOG("\n");
        LOG("Supported formats:\n");

        initInputPlugins();
        printAllInputPluginSuffixes(stdout);

        LOG("\n");
        LOG("Supported outputs:\n");
        loadAudioDrivers();
        printAllOutputPluginTypes(stdout);
}

static void parseOptions(int argc, char ** argv, Options * options) {
        int argcLeft = argc;

        options->daemon = 1;
        options->stdOutput = 0;
        options->createDB = 0;
        options->updateDB = 0;
	options->kill = 0;

        if(argc>1) {
                int i = 1;
                while(i<argc) {
                        if(strncmp(argv[i],"--",2)==0) {
                                if(strcmp(argv[i],"--help")==0) {
                                        usage(argv);
                                        exit(EXIT_SUCCESS);
                                }
                                else if(strcmp(argv[i],"--kill")==0) {
                                        options->kill++;
                                        argcLeft--;
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

static void closeAllFDs(void) {
        int i;
	int fds = getdtablesize();

        for(i = 3; i < fds; i++) close(i);
}

static void changeToUser(void) {
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
                        WARNING("cannot init supplementary groups "
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

static void openLogFiles(Options * options, FILE ** out, FILE ** err) {
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

static void openDB(Options * options, char * argv0) {
        if(options->createDB>0 || readDirectoryDB()<0) {
                if(options->createDB<0) {
                        ERROR("can't open db file and using \"--no-create-db\""
                                        " command line option\n");
			ERROR("try running \"%s --create-db\"\n", argv0);
                        exit(EXIT_FAILURE);
                }
                flushWarningLog();
                if(checkDirectoryDB()<0) exit(EXIT_FAILURE);
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

static void startMainProcess(void) {
	int pid;
	fflush(0);
	pid = fork();
        if(pid>0) {
		initInputStream(); 
	        initReplayGainState();
		readAudioDevicesState();

		/* free stuff we don't need */
		freeAllListenSockets();
		
		mainPid = pid;
		masterInitSigHandlers();
		kill(mainPid, SIGUSR1);
		while (masterHandlePendingSignals()!=COMMAND_RETURN_KILL)
			waitOnSignals();
		/* we're killed */
		playerKill();
		
		finishAudioConfig();
		finishAudioDriver();
	
		finishPaths();

		kill(mainPid, SIGTERM);
		waitpid(mainPid,NULL,0);
		finishConf();
		myfprintfCloseLogFile();
		exit(EXIT_SUCCESS);

	} else if(pid<0) {
        	ERROR("problems fork'ing main process!\n");
                exit(EXIT_FAILURE);
	}

	DEBUG("main process started!\n");
}

static void daemonize(Options * options) {
	FILE * fp = NULL;
	ConfigParam * pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);
	
	if (pidFileParam) {
		/* do this before daemon'izing so we can fail gracefully if we can't
		 * write to the pid file */
		DEBUG("opening pid file\n");
		fp = fopen(pidFileParam->value, "w+");
		if(!fp) {
			ERROR("could not open %s \"%s\" (at line %i) for writing: %s\n",
					CONF_PID_FILE, pidFileParam->value,
					pidFileParam->line, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

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

		DEBUG("daemonized!\n");
        }

	if (pidFileParam) {
		DEBUG("writing pid file\n");
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
		masterPid = getpid();
	}
}

static void setupLogOutput(Options * options, FILE * out, FILE * err) {
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
        }
        flushWarningLog();

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

static void cleanUpPidFile(void) {
	ConfigParam * pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);

	if (!pidFileParam) return;

	DEBUG("cleaning up pid file\n");
	
	unlink(pidFileParam->value);
}

static void killFromPidFile(char * cmd, int killOption) {
	/*char buf[32];
	struct stat st_cmd;
	struct stat st_exe;*/
	FILE * fp;
	ConfigParam * pidFileParam = parseConfigFilePath(CONF_PID_FILE, 0);
	int pid;

	if (!pidFileParam) {
		ERROR("no pid_file specified in the config file\n");
		exit(EXIT_FAILURE);
	}

	fp = fopen(pidFileParam->value,"r");
	if(!fp) {
		ERROR("unable to open %s \"%s\": %s\n", 
				CONF_PID_FILE, pidFileParam->value,
				strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(fscanf(fp, "%i",  &pid) != 1) {
		ERROR("unable to read the pid from file \"%s\"\n",
				pidFileParam->value);
		exit(EXIT_FAILURE);
	}
	fclose(fp);

	/*memset(buf, 0, 32);
	snprintf(buf, 31, "/proc/%i/exe", pid);

	if(killOption == 1) {
		if(stat(cmd, &st_cmd)) {
			ERROR("unable to stat file \"%s\"\n", cmd);
			ERROR("execute \"%s --kill -kill\" to kill pid %i\n", 
					cmd, pid);
			exit(EXIT_FAILURE);
		}
		if(stat(buf, &st_exe)) {
			ERROR("unable to kill proccess %i (%s: %s)\n", pid, buf,
					strerror(errno));
			ERROR("execute \"%s --kill -kill\" to kill pid %i\n", 
					cmd, pid);
			exit(EXIT_FAILURE);
		}

		if(st_exe.st_dev != st_cmd.st_dev || st_exe.st_ino != st_cmd.st_ino) {
			ERROR("%s doesn't appear to be running as pid %i\n",
					cmd, pid);
			ERROR("execute \"%s --kill -kill\" to kill pid %i\n", 
					cmd, pid);
			exit(EXIT_FAILURE);
		}
	}*/

	if(kill(pid, SIGTERM)) {
		ERROR("unable to kill proccess %i: %s\n", pid, strerror(errno));
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

int main(int argc, char * argv[]) {
        FILE * out = NULL;
        FILE * err = NULL;
        Options options;

        closeAllFDs();

        initConf();

        parseOptions(argc, argv, &options);

	if(options.kill) killFromPidFile(argv[0], options.kill);
        

        initStats();
	initTagConfig();
        initLog();

        if(options.createDB <= 0 && !options.updateDB) listenOnPort();

        changeToUser();
	
        openLogFiles(&options, &out, &err);

        initPlayerData();
	
	daemonize(&options);
       
        initInputPlugins();
	initPaths();
	initAudioConfig();
        initAudioDriver();

        initSigHandlers();
        setupLogOutput(&options, out, err);
	startMainProcess();
	/* This is the main process which has
	 * been forked from the master process.
	 */
	
	initPermissions();

        initPlaylist();

        openDB(&options, argv[0]);

        initCommands();
        initVolume();
        initInterfaces();

	printMemorySavedByTagTracker();
	printSavedMemoryFromFilenames();

	/* wait for the master process to get ready so we can start 
	 * playing if readPlaylistState thinks we should*/
	while (COMMAND_MASTER_READY != handlePendingSignals()) 
		my_usleep(1);

	openVolumeDevice();
        readPlaylistState();


        while(COMMAND_RETURN_KILL!=doIOForInterfaces()) {
		if(COMMAND_RETURN_KILL==handlePendingSignals()) break;
                syncPlayerAndPlaylist();
                closeOldInterfaces();
		readDirectoryDBIfUpdateIsFinished();
        }

        savePlaylistState();
        saveAudioDevicesState();

        freeAllInterfaces();
        closeAllListenSockets();

        /* This slows shutdown immensely, and doesn't really accomplish
         * anything.  Uncomment when we rewrite tagTracker to use a tree. */
        /*closeMp3Directory();*/

        finishPlaylist();
        freePlayerData();
        finishAudioDriver();
        finishAudioConfig();
        finishVolume();
	finishPaths();
	finishPermissions();
        finishCommands();
        finishInputPlugins();
	cleanUpPidFile();
	finishConf();
	
	myfprintfCloseLogFile();
        return EXIT_SUCCESS;
}
