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

#include "conf.h"

#include "log.h"

#include "utils.h"
#include "buffer2array.h"
#include "audio.h"
#include "volume.h"

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#define MAX_STRING_SIZE	MAXPATHLEN+80

#define CONF_COMMENT	'#'

#define CONF_NUMBER_OF_PARAMS		35
#define CONF_NUMBER_OF_PATHS		6
#define CONF_NUMBER_OF_REQUIRED		5
#define CONF_NUMBER_OF_ALLOW_CATS	1

#define CONF_CONNECTION_TIMEOUT_DEFAULT			"60"
#define CONF_MAX_CONNECTIONS_DEFAULT			"5"
#define CONF_MAX_PLAYLIST_LENGTH_DEFAULT		"16384"
#define CONF_BUFFER_BEFORE_PLAY_DEFAULT			"25%"
#define CONF_MAX_COMMAND_LIST_SIZE_DEFAULT		"2048"
#define CONF_MAX_OUTPUT_BUFFER_SIZE_DEFAULT		"2048"
#define CONF_AO_DRIVER_DEFAULT				AUDIO_AO_DRIVER_DEFAULT
#define CONF_AO_DRIVER_OPTIONS_DEFAULT			""
#define CONF_SAVE_ABSOLUTE_PATHS_IN_PLAYLISTS_DEFAULT	"no"
#define CONF_BIND_TO_ADDRESS_DEFAULT			"any"
#define CONF_USER_DEFAULT				""
#define CONF_LOG_LEVEL_DEFAULT				"default"
#define CONF_AUDIO_WRITE_SIZE_DEFAULT			"1024"
#define CONF_BUFFER_SIZE_DEFAULT			"2048"
#ifndef NO_OSS_MIXER
#define CONF_MIXER_TYPE_DEFAULT				VOLUME_MIXER_OSS
#define CONF_MIXER_DEVICE_DEFAULT			""
#else
#ifdef HAVE_ALSA
#define CONF_MIXER_TYPE_DEFAULT				VOLUME_MIXER_ALSA
#define CONF_MIXER_DEVICE_DEFAULT			""
#else
#define CONF_MIXER_TYPE_DEFAULT				VOLUME_MIXER_SOFTWARE
#define CONF_MIXER_DEVICE_DEFAULT			""
#endif
#endif

static char * conf_params[CONF_NUMBER_OF_PARAMS];

void initConf() {
	int i;

	for(i=0;i<CONF_NUMBER_OF_PARAMS;i++) conf_params[i] = NULL;

	/* we don't specify these on the command line */
	conf_params[CONF_CONNECTION_TIMEOUT] = strdup(CONF_CONNECTION_TIMEOUT_DEFAULT);
	conf_params[CONF_MIXER_DEVICE] = strdup(CONF_MIXER_DEVICE_DEFAULT);
	conf_params[CONF_MAX_CONNECTIONS] = strdup(CONF_MAX_CONNECTIONS_DEFAULT);
	conf_params[CONF_MAX_PLAYLIST_LENGTH] = strdup(CONF_MAX_PLAYLIST_LENGTH_DEFAULT);
	conf_params[CONF_BUFFER_BEFORE_PLAY] = strdup(CONF_BUFFER_BEFORE_PLAY_DEFAULT);
	conf_params[CONF_MAX_COMMAND_LIST_SIZE] = strdup(CONF_MAX_COMMAND_LIST_SIZE_DEFAULT);
	conf_params[CONF_MAX_OUTPUT_BUFFER_SIZE] = strdup(CONF_MAX_OUTPUT_BUFFER_SIZE_DEFAULT);
	conf_params[CONF_AO_DRIVER] = strdup(CONF_AO_DRIVER_DEFAULT);
	conf_params[CONF_AO_DRIVER_OPTIONS] = strdup(CONF_AO_DRIVER_OPTIONS_DEFAULT);
	conf_params[CONF_SAVE_ABSOLUTE_PATHS_IN_PLAYLISTS] = strdup(CONF_SAVE_ABSOLUTE_PATHS_IN_PLAYLISTS_DEFAULT);
	conf_params[CONF_BIND_TO_ADDRESS] = strdup(CONF_BIND_TO_ADDRESS_DEFAULT);
	conf_params[CONF_MIXER_TYPE] = strdup(CONF_MIXER_TYPE_DEFAULT);
	conf_params[CONF_USER] = strdup(CONF_USER_DEFAULT);
	conf_params[CONF_LOG_LEVEL] = strdup(CONF_LOG_LEVEL_DEFAULT);
	conf_params[CONF_AUDIO_WRITE_SIZE] = strdup(CONF_AUDIO_WRITE_SIZE_DEFAULT);
	conf_params[CONF_BUFFER_SIZE] = strdup(CONF_BUFFER_SIZE_DEFAULT);
}

char ** readConf(char * file) {
	char * conf_strings[CONF_NUMBER_OF_PARAMS] = {
		"port",
		"music_directory",
		"playlist_directory",
		"log_file",
		"error_file",
		"connection_timeout",
		"mixer_device",
		"max_connections",
		"max_playlist_length",
		"buffer_before_play",
		"max_command_list_size",
		"max_output_buffer_size",
		"ao_driver",
		"ao_driver_options",
		"save_absolute_paths_in_playlists",
		"bind_to_address",
		"mixer_type",
		"state_file",
		"user",
		"db_file",
		"log_level",
		"mixer_control",
		"audio_write_size",
		"filesystem_charset",
		"password",
		"default_permissions",
		"audio_buffer_size",
                "replaygain",
                "audio_output_format",
                "http_proxy_host",
                "http_proxy_port",
		"http_proxy_user",
		"http_proxy_password",
		"replaygain_preamp",
		"id3v1_encoding"
	};

	int conf_absolutePaths[CONF_NUMBER_OF_PATHS] = {
		CONF_MUSIC_DIRECTORY,
		CONF_PLAYLIST_DIRECTORY,
		CONF_LOG_FILE,
		CONF_ERROR_FILE,
		CONF_STATE_FILE,
		CONF_DB_FILE
	};

	int conf_required[CONF_NUMBER_OF_REQUIRED] = {
		CONF_MUSIC_DIRECTORY,
		CONF_PLAYLIST_DIRECTORY,
		CONF_LOG_FILE,
		CONF_ERROR_FILE,
		CONF_PORT
	};

	short conf_allowCat[CONF_NUMBER_OF_ALLOW_CATS] = {
		CONF_PASSWORD
	};

	FILE * fp;
	char string[MAX_STRING_SIZE+1];
	char ** array;
	int i;
	int numberOfArgs;
	short allowCat[CONF_NUMBER_OF_PARAMS];
	int count = 0;

	for(i=0;i<CONF_NUMBER_OF_PARAMS;i++) allowCat[i] = 0;

	for(i=0;i<CONF_NUMBER_OF_ALLOW_CATS;i++) allowCat[conf_allowCat[i]] = 1;

	if(!(fp=fopen(file,"r"))) {
		ERROR("problems opening file %s for reading\n",file);
		exit(EXIT_FAILURE);
	}

	while(myFgets(string,sizeof(string),fp)) {
		count++;

		if(string[0]==CONF_COMMENT) continue;
		numberOfArgs = buffer2array(string,&array);
		if(numberOfArgs==0) continue;
		if(2!=numberOfArgs) {
			ERROR("improperly formated config file at line %i: %s\n",count,string);
			exit(EXIT_FAILURE);
		}
		i = 0;
		while(i<CONF_NUMBER_OF_PARAMS && 0!=strcmp(conf_strings[i],array[0])) i++;
		if(i>=CONF_NUMBER_OF_PARAMS) {
			ERROR("unrecognized paramater in conf at line %i: %s\n",count,string);
			exit(EXIT_FAILURE);
		}
		
		if(conf_params[i]!=NULL) {
			if(allowCat[i]) {
				conf_params[i] = realloc(conf_params[i],
						strlen(conf_params[i])+
						strlen(CONF_CAT_CHAR)+
						strlen(array[1])+1);
				strcat(conf_params[i],CONF_CAT_CHAR);
				strcat(conf_params[i],array[1]);
			}
			else {
				free(conf_params[i]);
				conf_params[i] = strdup(array[1]);
			}
		}
		else conf_params[i] = strdup(array[1]);
		free(array[0]);
		free(array[1]);
		free(array);
	}

	fclose(fp);

	for(i=0;i<CONF_NUMBER_OF_REQUIRED;i++) {
		if(conf_params[conf_required[i]] == NULL) {
			ERROR("%s is unassigned in conf file\n",
					conf_strings[conf_required[i]]);
			exit(EXIT_FAILURE);
		}
	}

	for(i=0;i<CONF_NUMBER_OF_PATHS;i++) {
		if(conf_params[conf_absolutePaths[i]] && 
			conf_params[conf_absolutePaths[i]][0]!='/' &&
			conf_params[conf_absolutePaths[i]][0]!='~') 
		{
			ERROR("\"%s\" is not an absolute path\n",
					conf_params[conf_absolutePaths[i]]);
			exit(EXIT_FAILURE);
		}
		/* Parse ~ in path */
		else if(conf_params[conf_absolutePaths[i]] &&
			conf_params[conf_absolutePaths[i]][0]=='~') 
		{
			struct passwd * pwd = NULL;
			char * path;
			int pos = 1;
			if(conf_params[conf_absolutePaths[i]][1]=='/' ||
				conf_params[conf_absolutePaths[i]][1]=='\0') 
			{
				if(conf_params[CONF_USER] && 
						strlen(conf_params[CONF_USER]))
				{
					pwd = getpwnam(
						conf_params[CONF_USER]);
					if(!pwd) {
						ERROR("no such user: %s\n",
							conf_params[CONF_USER]);
						exit(EXIT_FAILURE);
					}
				}
				else {
					uid_t uid = geteuid();
					if((pwd = getpwuid(uid)) == NULL) {
						ERROR("problems getting passwd "
							"entry "
							"for current user\n");
						exit(EXIT_FAILURE);
					}
				}
			}
			else {
				int foundSlash = 0;
				char * ch = &(
					conf_params[conf_absolutePaths[i]][1]);
				for(;*ch!='\0' && *ch!='/';ch++);
				if(*ch=='/') foundSlash = 1;
				* ch = '\0';
				pos+= ch-
					&(conf_params[
					conf_absolutePaths[i]][1]);
				if((pwd = getpwnam(&(conf_params[
					conf_absolutePaths[i]][1]))) == NULL) 
				{
					ERROR("user \"%s\" not found\n",
						&(conf_params[
						conf_absolutePaths[i]][1]));
					exit(EXIT_FAILURE);
				}
				if(foundSlash) *ch = '/';
			}
			path = malloc(strlen(pwd->pw_dir)+strlen(
				&(conf_params[conf_absolutePaths[i]][pos]))+1);
			strcpy(path,pwd->pw_dir);
			strcat(path,&(conf_params[conf_absolutePaths[i]][pos]));
			free(conf_params[conf_absolutePaths[i]]);
			conf_params[conf_absolutePaths[i]] = path;
		}
	}

	return conf_params;
}

char ** getConf() {
	return conf_params;
}
