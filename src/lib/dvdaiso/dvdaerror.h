/*
* MPD DVD-Audio Decoder plugin
* Copyright (c) 2014 Maxim V.Anisiutkin <maxim.anisiutkin@gmail.com>
*
* DVD-Audio Decoder is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* DVD-Audio Decoder is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with FFmpeg; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef DVDAERROR_H
#define DVDAERROR_H

typedef int DVDAERROR;

#define DVDAERR_OK                        (  0)
#define DVDAERR_CANNOT_OPEN_AUDIO_TS_IFO  ( -1)
#define DVDAERR_CANNOT_OPEN_ATS_XX_0_IFO  ( -2)
#define DVDAERR_CANNOT_OPEN_ATS_XX_X_AOB  ( -3)
#define DVDAERR_CANNOT_CLOSE_AUDIO_TS_IFO ( -4)
#define DVDAERR_CANNOT_CLOSE_ATS_XX_0_IFO ( -5)
#define DVDAERR_CANNOT_CLOSE_ATS_XX_X_AOB ( -6)
#define DVDAERR_CANNOT_READ_AUDIO_TS_IFO  ( -7)
#define DVDAERR_CANNOT_READ_ATS_XX_0_IFO  ( -8)
#define DVDAERR_CANNOT_READ_ATS_XX_X_AOB  ( -9)
#define DVDAERR_CANNOT_SEEK_AUDIO_TS_IFO  (-10)
#define DVDAERR_CANNOT_SEEK_ATS_XX_0_IFO  (-11)
#define DVDAERR_CANNOT_SEEK_ATS_XX_X_AOB  (-12)
#define DVDAERR_CANNOT_CREATE_DUMP_FILE   (-13)
#define DVDAERR_CANNOT_DELETE_DUMP_FILE   (-14)
#define DVDAERR_CANNOT_CLOSE_DUMP_FILE    (-15)
#define DVDAERR_CANNOT_WRITE_DUMP_FILE    (-16)
#define DVDAERR_AOB_BLOCK_SCRAMBLED       (-17)
#define DVDAERR_AOB_BLOCK_NOT_FOUND       (-18)
#define DVDAERR_BAD_TITLESET_ID           (-19)
#define DVDAERR_CANNOT_CREATE_DUMPER      (-20)
#define DVDAERR_DUMPER_IS_NOT_ASSIGNED    (-21)
#define DVDAERR_PCM_BLOCK_OVERRUN         (-22)
#define DVDAERR_CPXM_NO_DLL               (-23)
#define DVDAERR_CPXM_AUTH_FAILED          (-24)
#define DVDAERR_MLP_NO_DLL                (-25)
#define DVDAERR_MLP_INIT_FAILED           (-26)
#define DVDAERR_MLP_DECODE_FAILED         (-27)
#define DVDAERR_MLP_BUFFER_OVERRUN        (-28)
#define DVDAERR_UNKNOWN_DVDA_ZONE         (-29)
#define DVDAERR_EXTRACTION_CANCELLED      (-30)
#define DVDAERR_CANNOT_CREATE_ENCODER     (-31)
#define DVDAERR_CANNOT_START_ENCODER      (-32)
#define DVDAERR_NON_EXTRACTABLE_OBJECT    (-33)
#define DVDAERR_CANNOT_DOWNMIX_STREAM     (-34)

#endif
