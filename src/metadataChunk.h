#ifndef METADATA_CHUNK_H
#define METADATA_CHUNK_H

#define METADATA_BUFFER_LENGTH 1024

#include "tag.h"

typedef struct _MetadataChunk {
	int name;
	int title;
	int artist;
	int album;
	char buffer[METADATA_BUFFER_LENGTH];
} MetadataChunk;

void initMetadataChunk(MetadataChunk *);

MpdTag * metadataChunkToMpdTagDup(MetadataChunk * chunk);

void copyMpdTagToMetadataChunk(MpdTag * tag, MetadataChunk * chunk);

#endif
