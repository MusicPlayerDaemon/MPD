#include "metadataChunk.h"

#include <string.h>

void initMetadataChunk(MetadataChunk * chunk) {
	memset(chunk, 0, sizeof(MetadataChunk));
	
	chunk->name = -1;
	chunk->artist = -1;
	chunk->album = -1;
	chunk->title = -1;
}

MpdTag * metadataChunkToMpdTagDup(MetadataChunk * chunk) {
	MpdTag * ret = newMpdTag();

	if(chunk->name >= 0) ret->name = strdup(chunk->buffer+chunk->name);
	if(chunk->artist >= 0) ret->artist = strdup(chunk->buffer+chunk->artist);
	if(chunk->album >= 0) ret->album = strdup(chunk->buffer+chunk->album);
	if(chunk->title >= 0) ret->title = strdup(chunk->buffer+chunk->title);

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
