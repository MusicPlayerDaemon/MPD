/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu
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

#include "tag.h"
#include "path.h"
#include "myfprintf.h"
#include "sig_handlers.h"
#include "mp3_decode.h"
#include "audiofile_decode.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_OGG
#include <vorbis/vorbisfile.h>
#endif
#ifdef HAVE_FLAC
#include <FLAC/file_decoder.h>
#include <FLAC/metadata.h>
#endif
#ifdef HAVE_ID3TAG
#ifdef USE_MPD_ID3TAG
#include "libid3tag/id3tag.h"
#else
#include <id3tag.h>
#endif
#endif

void printMpdTag(FILE * fp, MpdTag * tag) {
	if(tag->artist) myfprintf(fp,"Artist: %s\n",tag->artist);
	if(tag->album) myfprintf(fp,"Album: %s\n",tag->album);
	if(tag->track) myfprintf(fp,"Track: %s\n",tag->track);
	if(tag->title) myfprintf(fp,"Title: %s\n",tag->title);
	if(tag->time>=0) myfprintf(fp,"Time: %i\n",tag->time);
}

#ifdef HAVE_ID3TAG
char * getID3Info(struct id3_tag * tag, char * id) {
	struct id3_frame const * frame;
	id3_ucs4_t const * ucs4;
	id3_utf8_t * utf8;
	union id3_field const * field;
	unsigned int nstrings;

	frame = id3_tag_findframe(tag, id, 0);
	if(!frame) return NULL;

	field = &frame->fields[1];
	nstrings = id3_field_getnstrings(field);
	if(nstrings<1) return NULL;

	ucs4 = id3_field_getstrings(field,0);
	assert(ucs4);

	utf8 = id3_ucs4_utf8duplicate(ucs4);
	if(!utf8) return NULL;

	return utf8;
}
#endif

MpdTag * id3Dup(char * utf8filename) {
	MpdTag * ret = NULL;
#ifdef HAVE_ID3TAG
	struct id3_file * file;
	struct id3_tag * tag;
	char * str;

	blockSignals();
	file = id3_file_open(rmp2amp(utf8ToFsCharset(utf8filename)),
			ID3_FILE_MODE_READONLY);
	if(!file) {
		unblockSignals();
		return NULL;
	}

	tag = id3_file_tag(file);
	if(!tag) {
		id3_file_close(file);
		unblockSignals();
		return NULL;
	}

	str = getID3Info(tag,ID3_FRAME_ARTIST);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->artist = str;
	}

	str = getID3Info(tag,ID3_FRAME_TITLE);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->title = str;
	}

	str = getID3Info(tag,ID3_FRAME_ALBUM);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->album = str;
	}

	str = getID3Info(tag,ID3_FRAME_TRACK);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->track = str;
	}

	id3_file_close(file);

	unblockSignals();
#endif
	return ret;	
}

#ifdef HAVE_AUDIOFILE
MpdTag * audiofileTagDup(char * utf8file) {
	MpdTag * ret = NULL;
	int time = getAudiofileTotalTime(rmp2amp(utf8ToFsCharset(utf8file)));
	
	if (time>=0) {
		if(!ret) ret = newMpdTag();
		ret->time = time;
	}

	return ret;
}
#endif

#ifdef HAVE_MAD
MpdTag * mp3TagDup(char * utf8file) {
	MpdTag * ret = NULL;
	int time;

	ret = id3Dup(utf8file);

	time = getMp3TotalTime(rmp2amp(utf8ToFsCharset(utf8file)));

	if(time>=0) {
		if(!ret) ret = newMpdTag();
		ret->time = time;
	}

	return ret;
}
#endif

#ifdef HAVE_OGG
MpdTag * oggTagDup(char * utf8file) {
	MpdTag * ret = NULL;
	FILE * fp;
	OggVorbis_File vf;
	char ** comments;
	char * temp;
	char * s1;
	char * s2;

	while(!(fp = fopen(rmp2amp(utf8ToFsCharset(utf8file)),"r")) 
			&& errno==EINTR);
	if(!fp) return NULL;
	blockSignals();
	if(ov_open(fp,&vf,NULL,0)<0) {
		unblockSignals();
		while(fclose(fp) && errno==EINTR);
		return NULL;
	}

	ret = newMpdTag();
	ret->time = (int)(ov_time_total(&vf,-1)+0.5);

	comments = ov_comment(&vf,-1)->user_comments;

	while(*comments) {
		temp = strdup(*comments);
		++comments;
		if(!(s1 = strtok(temp,"="))) continue;
		s2 = strtok(NULL,"");
		if(!s1 || !s2);
		else if(0==strcasecmp(s1,"artist")) {
			if(!ret->artist) {
				ret->artist = strdup(s2);
			}
		}
		else if(0==strcasecmp(s1,"title")) {
			if(!ret->title) {
				ret->title = strdup(s2);
			}
		}
		else if(0==strcasecmp(s1,"album")) {
			if(!ret->album) {
				ret->album = strdup(s2);
			}
		}
		else if(0==strcasecmp(s1,"tracknumber")) {
			if(!ret->track) {
				ret->track = strdup(s2);
			}
		}
		free(temp);
	}

	ov_clear(&vf);

	unblockSignals();
	return ret;	
}
#endif

#ifdef HAVE_FLAC
MpdTag * flacMetadataDup(char * file, int * vorbisCommentFound) {
	MpdTag * ret = NULL;
	FLAC__Metadata_SimpleIterator * it;
	FLAC__StreamMetadata * block = NULL;
	int offset;
	int len, pos;

	*vorbisCommentFound = 0;

	blockSignals();
	it = FLAC__metadata_simple_iterator_new();
	if(!FLAC__metadata_simple_iterator_init(it,rmp2amp(file),1,0)) {
		FLAC__metadata_simple_iterator_delete(it);
		unblockSignals();
		return ret;
	}
	
	do {
		block = FLAC__metadata_simple_iterator_get_block(it);
		if(!block) break;
		if(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			char * dup;

			offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block,0,"artist");
			if(offset>=0) {
				*vorbisCommentFound = 1;
				if(!ret) ret = newMpdTag();
				pos = strlen("artist=");
				len = block->data.vorbis_comment.comments[offset].length-pos;
				if(len>0) {
					dup = malloc(len+1);
					memcpy(dup,&(block->data.vorbis_comment.comments[offset].entry[pos]),len);
					dup[len] = '\0';
					ret->artist = dup;
				}
			}
			offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block,0,"album");
			if(offset>=0) {
				*vorbisCommentFound = 1;
				if(!ret) ret = newMpdTag();
				pos = strlen("album=");
				len = block->data.vorbis_comment.comments[offset].length-pos;
				if(len>0) {
					dup = malloc(len+1);
					memcpy(dup,&(block->data.vorbis_comment.comments[offset].entry[pos]),len);
					dup[len] = '\0';
					ret->album = dup;
				}
			}
			offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block,0,"title");
			if(offset>=0) {
				*vorbisCommentFound = 1;
				if(!ret) ret = newMpdTag();
				pos = strlen("title=");
				len = block->data.vorbis_comment.comments[offset].length-pos;
				if(len>0) {
					dup = malloc(len+1);
					memcpy(dup,&(block->data.vorbis_comment.comments[offset].entry[pos]),len);
					dup[len] = '\0';
					ret->title = dup;
				}
			}
			offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block,0,"tracknumber");
			if(offset>=0) {
				*vorbisCommentFound = 1;
				if(!ret) ret = newMpdTag();
				pos = strlen("tracknumber=");
				len = block->data.vorbis_comment.comments[offset].length-pos;
				if(len>0) {
					dup = malloc(len+1);
					memcpy(dup,&(block->data.vorbis_comment.comments[offset].entry[pos]),len);
					dup[len] = '\0';
					ret->track = dup;
				}
			}
		}
		else if(block->type == FLAC__METADATA_TYPE_STREAMINFO) {
			if(!ret) ret = newMpdTag();
			ret->time = ((float)block->data.stream_info.
					total_samples) /
					block->data.stream_info.sample_rate +
					0.5;
		}
		FLAC__metadata_object_delete(block);
	} while(FLAC__metadata_simple_iterator_next(it));

	FLAC__metadata_simple_iterator_delete(it);
	unblockSignals();
	return ret;
}

MpdTag * flacTagDup(char * file) {
	MpdTag * ret = NULL;
	int foundVorbisComment = 0;

	ret = flacMetadataDup(file,&foundVorbisComment);
	if(!ret) return NULL;
	if(!foundVorbisComment) {
		MpdTag * temp = id3Dup(file);
		if(temp) {
			temp->time = ret->time;
			freeMpdTag(ret);
			ret = temp;
		}
	}

	return ret;
}
#endif

MpdTag * newMpdTag() {
	MpdTag * ret = malloc(sizeof(MpdTag));
	ret->album = NULL;
	ret->artist = NULL;
	ret->title = NULL;
	ret->track = NULL;
	ret->time = -1;
	return ret;
}

void freeMpdTag(MpdTag * tag) {
	if(tag->artist) free(tag->artist);
	if(tag->album) free(tag->album);
	if(tag->title) free(tag->title);
	if(tag->track) free(tag->track);
	free(tag);
}

MpdTag * mpdTagDup(MpdTag * tag) {
	MpdTag * ret = NULL;

	if(tag) {
		ret = newMpdTag();
		ret->artist = strdup(tag->artist);
		ret->album = strdup(tag->album);
		ret->title = strdup(tag->title);
		ret->track = strdup(tag->track);
		ret->time = tag->time;
	}

	return ret;
}
