#include "metadataChunk.h"

#include <string.h>

void initMetadataChunk(MetadataChunk * chunk) {
	memset(chunk, 0, sizeof(MetadataChunk));
	
	chunk->name = -1;
	chunk->artist = -1;
	chunk->album = -1;
	chunk->title = -1;
}

#define dupElementToTag(string, element) { \
	if(element >= 0 && element < METADATA_BUFFER_LENGTH) { \
		string = strdup(chunk->buffer+element); \
	} \
}

MpdTag * metadataChunkToMpdTagDup(MetadataChunk * chunk) {
	MpdTag * ret = newMpdTag();

	chunk->buffer[METADATA_BUFFER_LENGTH] = '\0';

	dupElementToTag(ret->name, chunk->name);
	dupElementToTag(ret->title, chunk->title);
	dupElementToTag(ret->artist, chunk->artist);
	dupElementToTag(ret->album, chunk->album);

	return ret;
}

#define copyStringToChunk(string, element) { \
	if(string && (slen = strlen(string)) && \
                        pos < METADATA_BUFFER_LENGTH-1) \
        { \
		strncpy(chunk->buffer+pos, string, \
                                METADATA_BUFFER_LENGTH-1-pos); \
		element = pos; \
		pos += slen+1; \
	} \
}

void copyMpdTagToMetadataChunk(MpdTag * tag, MetadataChunk * chunk) {
	int pos = 0;
	int slen;

	initMetadataChunk(chunk);

	copyStringToChunk(tag->name, chunk->name);
	copyStringToChunk(tag->title, chunk->title);
	copyStringToChunk(tag->artist, chunk->artist);
	copyStringToChunk(tag->album, chunk->album);
}
