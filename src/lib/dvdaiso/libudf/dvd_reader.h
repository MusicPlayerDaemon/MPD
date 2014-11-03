/*
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>,
 *                          Björn Englund <d4bjorn@dtek.chalmers.se>
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

#ifndef LIBDVDREAD_DVD_READER_H
#define LIBDVDREAD_DVD_READER_H

#include <stdint.h>

//typedef int ssize_t;

/**
 * The DVD access interface.
 *
 * This file contains the functions that form the interface to to
 * reading files located on a DVD.
 */

/**
 * The current version.
 */
#define DVDREAD_VERSION 904

/**
 * The length of one Logical Block of a DVD.
 */
#define DVD_VIDEO_LB_LEN 2048

/**
 * Maximum length of filenames allowed in UDF.
 */
#define MAX_UDF_FILE_NAME_LEN 2048

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque type that is used as a handle for one instance of an opened DVD.
 */
typedef struct dvd_reader_s dvd_reader_t;

/**
 * Opens a block device of a DVD-ROM file, or an image file, or a directory
 * name for a mounted DVD or HD copy of a DVD.
 *
 * If the given file is a block device, or is the mountpoint for a block
 * device, then that device is used for CSS authentication using libdvdcss.
 * If no device is available, then no CSS authentication is performed,
 * and we hope that the image is decrypted.
 *
 * If the path given is a directory, then the files in that directory may be
 * in any one of these formats:
 *
 *   path/VIDEO_TS/VTS_01_1.VOB
 *   path/video_ts/vts_01_1.vob
 *   path/VTS_01_1.VOB
 *   path/vts_01_1.vob
 *
 * @param path Specifies the the device, file or directory to be used.
 * @return If successful a a read handle is returned. Otherwise 0 is returned.
 *
 * dvd = DVDOpen(path);
 */
dvd_reader_t *DVDOpen( void *dev );

/**
 * Closes and cleans up the DVD reader object.
 *
 * You must close all open files before calling this function.
 *
 * @param dvd A read handle that should be closed.
 *
 * DVDClose(dvd);
 */
void DVDClose( dvd_reader_t * );

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
                      int encrypted );

/**
 * Sets the level of caching that is done when reading from a device
 *
 * @param dvd A read handle to get the disc ID from
 * @param level The level of caching wanted.
 *             -1 - returns the current setting.
 *              0 - UDF Cache turned off.
 *              1 - (default level) Pointers to IFO files and some data from
 *                  PrimaryVolumeDescriptor are cached.
 *
 * @return The level of caching.
 */
int DVDUDFCacheLevel( dvd_reader_t *, int );

#ifdef __cplusplus
};
#endif
#endif /* LIBDVDREAD_DVD_READER_H */
