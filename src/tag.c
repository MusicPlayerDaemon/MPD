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
#include "mp3_decode.h"
#include "audiofile_decode.h"
#include "mp4_decode.h"
#include "aac_decode.h"
#include "utils.h"

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
#ifdef HAVE_FAAD
#include "mp4ff/mp4ff.h"
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

	file = id3_file_open(rmp2amp(utf8ToFsCharset(utf8filename)),
			ID3_FILE_MODE_READONLY);
	if(!file) {
		return NULL;
	}

	tag = id3_file_tag(file);
	if(!tag) {
		id3_file_close(file);
		return NULL;
	}

	str = getID3Info(tag,ID3_FRAME_ARTIST);
	if(str) {
		if(!ret) ret = newMpdTag();
		stripReturnChar(str);
		ret->artist = str;
	}

	str = getID3Info(tag,ID3_FRAME_TITLE);
	if(str) {
		if(!ret) ret = newMpdTag();
		stripReturnChar(str);
		ret->title = str;
	}

	str = getID3Info(tag,ID3_FRAME_ALBUM);
	if(str) {
		if(!ret) ret = newMpdTag();
		stripReturnChar(str);
		ret->album = str;
	}

	str = getID3Info(tag,ID3_FRAME_TRACK);
	if(str) {
		if(!ret) ret = newMpdTag();
		stripReturnChar(str);
		ret->track = str;
	}

	id3_file_close(file);

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

#ifdef HAVE_FAAD
MpdTag * aacTagDup(char * utf8file) {
	MpdTag * ret = NULL;
	int time;

	time = getAacTotalTime(rmp2amp(utf8ToFsCharset(utf8file)));

	if(time>=0) {
		if((ret = id3Dup(utf8file))==NULL) ret = newMpdTag();
		ret->time = time;
	}

	return ret;
}

MpdTag * mp4DataDup(char * utf8file, int * mp4MetadataFound) {
	MpdTag * ret = NULL;
	FILE * fh;
	mp4ff_t * mp4fh;
	mp4ff_callback_t * cb; 
	int32_t track;
	int32_t time;
	int32_t scale;

	*mp4MetadataFound = 0;

	fh = fopen(rmp2amp(utf8ToFsCharset(utf8file)),"r");
	if(!fh) {
		return NULL;
	}

	cb = malloc(sizeof(mp4ff_callback_t));
	cb->read = mp4_readCallback;
	cb->seek = mp4_seekCallback;
	cb->user_data = fh;

	mp4fh = mp4ff_open_read(cb);
	if(!mp4fh) {
		free(cb);
		fclose(fh);
		return NULL;
	}

	track = mp4_getAACTrack(mp4fh);
	if(track < 0) {
		mp4ff_close(mp4fh);
		fclose(fh);
		free(cb);
		return NULL;
	}

	ret = newMpdTag();
	time = mp4ff_get_track_duration_use_offsets(mp4fh,track);
	scale = mp4ff_time_scale(mp4fh,track);
	if(scale < 0) {
		mp4ff_close(mp4fh);
		fclose(fh);
		free(cb);
		freeMpdTag(ret);
		return NULL;
	}
	ret->time = ((float)time)/scale+0.5;

	if(!mp4ff_meta_get_artist(mp4fh,&ret->artist)) {
		*mp4MetadataFound = 1;
	}

	if(!mp4ff_meta_get_album(mp4fh,&ret->album)) {
		*mp4MetadataFound = 1;
	}

	if(!mp4ff_meta_get_title(mp4fh,&ret->title)) {
		*mp4MetadataFound = 1;
	}

	if(!mp4ff_meta_get_track(mp4fh,&ret->track)) {
		*mp4MetadataFound = 1;
	}

	mp4ff_close(mp4fh);
	fclose(fh);
	free(cb);

	return ret;
}

MpdTag * mp4TagDup(char * utf8file) {
	MpdTag * ret = NULL;
	int mp4MetadataFound = 0;

	ret = mp4DataDup(utf8file,&mp4MetadataFound);
	if(!mp4MetadataFound) {
		MpdTag * temp = id3Dup(utf8file);
		if(temp) {
			temp->time = ret->time;
			freeMpdTag(ret);
			ret = temp;
		}
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
	if(ov_open(fp,&vf,NULL,0)<0) {
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
				stripReturnChar(s2);
				ret->artist = strdup(s2);
			}
		}
		else if(0==strcasecmp(s1,"title")) {
			if(!ret->title) {
				stripReturnChar(s2);
				ret->title = strdup(s2);
			}
		}
		else if(0==strcasecmp(s1,"album")) {
			if(!ret->album) {
				stripReturnChar(s2);
				ret->album = strdup(s2);
			}
		}
		else if(0==strcasecmp(s1,"tracknumber")) {
			if(!ret->track) {
				stripReturnChar(s2);
				ret->track = strdup(s2);
			}
		}
		free(temp);
	}

	ov_clear(&vf);

	return ret;	
}
#endif

#ifdef HAVE_FLAC
MpdTag * flacMetadataDup(char * utf8file, int * vorbisCommentFound) {
	MpdTag * ret = NULL;
	FLAC__Metadata_SimpleIterator * it;
	FLAC__StreamMetadata * block = NULL;
	int offset;
	int len, pos;

	*vorbisCommentFound = 0;

	it = FLAC__metadata_simple_iterator_new();
	if(!FLAC__metadata_simple_iterator_init(it,rmp2amp(utf8ToFsCharset(utf8file)),1,0)) {
		FLAC__metadata_simple_iterator_delete(it);
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
					stripReturnChar(dup);
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
					stripReturnChar(dup);
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
					stripReturnChar(dup);
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
					stripReturnChar(dup);
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
	return ret;
}

MpdTag * flacTagDup(char * utf8file) {
	MpdTag * ret = NULL;
	int foundVorbisComment = 0;

	ret = flacMetadataDup(utf8file,&foundVorbisComment);
	if(!ret) return NULL;
	if(!foundVorbisComment) {
		MpdTag * temp = id3Dup(utf8file);
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
