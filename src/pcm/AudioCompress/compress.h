/*! compress.h
 *  interface to audio compression
 *
 *  (c)2007 busybee (http://beesbuzz.biz/)
 *  Licensed under the terms of the LGPL. See the file COPYING for details.
 */

#ifndef COMPRESS_H
#define COMPRESS_H

#include <stdint.h>

//! Configuration values for the compressor object
struct CompressorConfig {
	int target;
	int maxgain;
	int smooth;
};

struct Compressor;

#ifdef __cplusplus
extern "C" {
#endif

//! Create a new compressor (use history value of 0 for default)
struct Compressor *Compressor_new(unsigned int history);

//! Delete a compressor
void Compressor_delete(struct Compressor *);

//! Set the history length
void Compressor_setHistory(struct Compressor *, unsigned int history);

//! Get the configuration for a compressor
struct CompressorConfig *Compressor_getConfig(struct Compressor *);

//! Process 16-bit signed data
void Compressor_Process_int16(struct Compressor *, int16_t *data, unsigned int count);

#ifdef __cplusplus
}
#endif

//! TODO: Compressor_Process_int32, Compressor_Process_float, others as needed

//! TODO: functions for getting at the peak/gain/clip history buffers (for monitoring)

#endif
