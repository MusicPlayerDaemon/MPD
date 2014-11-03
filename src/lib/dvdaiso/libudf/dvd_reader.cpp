/*
 * Copyright (C) 2001-2004 Billy Biggs <vektor@dumbterm.net>,
 *                         Håkan Hjort <d95hjort@dtek.chalmers.se>,
 *                         Björn Englund <d4bjorn@dtek.chalmers.se>
 *
 * This file is part of libdvdread.
 *
 * libdvdread is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libdvdread is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libdvdread; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "dvd_udf.h"
#include "dvd_input.h"
#include "dvd_reader.h"

#define DEFAULT_UDF_CACHE_LEVEL 1

struct dvd_reader_s {
  /* Basic information. */
  int isImageFile;

  /* Information required for an image file. */
  dvd_input_t dev;

  /* Information required for a directory path drive. */
  char *path_root;

  /* Filesystem cache */
  int udfcache_level; /* 0 - turned off, 1 - on */
  void *udfcache;
};

#define TITLES_MAX 9

struct dvd_file_s {
  /* Basic information. */
  dvd_reader_t *dvd;

  /* Hack for selecting the right css title. */
  int css_title;

  /* Information required for an image file. */
  uint32_t lb_start;
  uint32_t seek_pos;

  /* Information required for a directory path drive. */
  size_t title_sizes[ TITLES_MAX ];
  dvd_input_t title_devs[ TITLES_MAX ];

  /* Calculated at open-time, size in blocks. */
  ssize_t filesize;
};

struct dvd_stat_s {
  off_t size;          /**< Total size of file in bytes */
  int nr_parts;        /**< Number of file parts */
  off_t parts_size[9]; /**< Size of each part in bytes */
};

/**
 * Set the level of caching on udf
 * level = 0 (no caching)
 * level = 1 (caching filesystem info)
 */
int DVDUDFCacheLevel(dvd_reader_t *device, int level)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;

  if(level > 0) {
    level = 1;
  } else if(level < 0) {
    return dev->udfcache_level;
  }

  dev->udfcache_level = level;

  return level;
}

void *GetUDFCacheHandle(dvd_reader_t *device)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;

  return dev->udfcache;
}

void SetUDFCacheHandle(dvd_reader_t *device, void *cache)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;

  dev->udfcache = cache;
}

/**
 * Open a DVD image or block device file.
 */
static dvd_reader_t *DVDOpenImageFile( dvd_input_t dev )
{
  dvd_reader_t *dvd;

	dev = dvdinput_open( dev );
  if( !dev ) {
    return NULL;
  }

  dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
  if( !dvd ) {
    dvdinput_close(dev);
    return NULL;
  }
  dvd->isImageFile = 1;
  dvd->dev = dev;
  dvd->path_root = NULL;

  dvd->udfcache_level = DEFAULT_UDF_CACHE_LEVEL;
  dvd->udfcache = NULL;

  return dvd;
}

dvd_reader_t *DVDOpen( void *dev )
{
	return DVDOpenImageFile( (dvd_input_t)dev );
}

void DVDClose( dvd_reader_t *dvd )
{
  if( dvd ) {
    if( dvd->dev ) dvdinput_close( dvd->dev );
    if( dvd->path_root ) free( dvd->path_root );
    if( dvd->udfcache ) FreeUDFCache( dvd->udfcache );
    free( dvd );
  }
}

/**
 * Internal, but used from dvd_udf.c 
 *
 * @param device A read handle.
 * @param lb_number Logical block number to start read from.
 * @param block_count Number of logical blocks to read.
 * @param data Pointer to buffer where read data should be stored.
 *             This buffer must be large enough to hold lb_number*2048 bytes.
 *             The pointer must be aligned to the logical block size when
 *             reading from a raw/O_DIRECT device.
 * @param encrypted 0 if no decryption shall be performed,
 *                  1 if decryption shall be performed
 * @param return Returns number of blocks read on success, negative on error
 */
int UDFReadBlocksRaw( dvd_reader_t *device, uint32_t lb_number,
                      size_t block_count, unsigned char *data, 
                      int encrypted )
{
  int ret;

  if( !device->dev )
    return 0;

  ret = dvdinput_seek( device->dev, (int) lb_number );
  if( ret != (int) lb_number )
    return 0;

  return dvdinput_read( device->dev, (char *) data, 
                        (int) block_count, encrypted );
}
