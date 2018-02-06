/*
 * SACD Ripper - http://code.google.com/p/sacd-ripper/
 *
 * Copyright (c) 2010-2014 by respective authors.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _SCARLETBOOK_H_INCLUDED
#define _SCARLETBOOK_H_INCLUDED

#include <stdint.h>
#include <string>

using namespace std;

#define SACD_LSN_SIZE                  2048
#define SACD_SAMPLING_FREQUENCY        2822400

#define START_OF_FILE_SYSTEM_AREA      0
#define START_OF_MASTER_TOC            510
#define MASTER_TOC_LEN                 10
#define MAX_AREA_TOC_SIZE_LSN          96
#define MAX_LANGUAGE_COUNT             8
#define MAX_CHANNEL_COUNT              6
#define SAMPLES_PER_FRAME              588
#define FRAME_SIZE_64                  (SAMPLES_PER_FRAME * 64 / 8)
#define SUPPORTED_VERSION_MAJOR        1
#define SUPPORTED_VERSION_MINOR        20

#define MAX_GENRE_COUNT                29
#define MAX_CATEGORY_COUNT             3

enum frame_format_t {
	FRAME_FORMAT_DST         = 0,
	FRAME_FORMAT_DSD_3_IN_14 = 2,
	FRAME_FORMAT_DSD_3_IN_16 = 3
};

enum character_set_t {
	CHAR_SET_UNKNOWN       = 0,
	CHAR_SET_ISO646        = 1,    // ISO 646 (IRV), no escape sequences allowed
	CHAR_SET_ISO8859_1     = 2,    // ISO 8859-1, no escape sequences allowed
	CHAR_SET_RIS506        = 3,    // MusicShiftJIS, per RIS-506 (RIAJ), Music Shift-JIS Kanji
	CHAR_SET_KSC5601       = 4,    // Korean KSC 5601-1987
	CHAR_SET_GB2312        = 5,    // Chinese GB 2312-80
	CHAR_SET_BIG5          = 6,    // Big5
	CHAR_SET_ISO8859_1_ESC = 7     // ISO 8859-1, single byte set escape sequences allowed
};

// string representation for character sets
extern const char* character_set[];

extern const char* album_genre[];

enum genre_t {
	GENRE_NOT_USED               = 0,       // 12
	GENRE_NOT_DEFINED            = 1,       // 12
	GENRE_ADULT_CONTEMPORARY     = 2,       // 12
	GENRE_ALTERNATIVE_ROCK       = 3,       // 40
	GENRE_CHILDRENS_MUSIC        = 4,       // 12
	GENRE_CLASSICAL              = 5,       // 32
	GENRE_CONTEMPORARY_CHRISTIAN = 6,       // 140
	GENRE_COUNTRY                = 7,       // 2
	GENRE_DANCE                  = 8,       // 3
	GENRE_EASY_LISTENING         = 9,       // 98
	GENRE_EROTIC                 = 10,      // 12
	GENRE_FOLK                   = 11,      // 80
	GENRE_GOSPEL                 = 12,      // 38
	GENRE_HIP_HOP                = 13,      // 7
	GENRE_JAZZ                   = 14,      // 8
	GENRE_LATIN                  = 15,      // 86
	GENRE_MUSICAL                = 16,      // 77
	GENRE_NEW_AGE                = 17,      // 10
	GENRE_OPERA                  = 18,      // 103
	GENRE_OPERETTA               = 19,      // 104
	GENRE_POP_MUSIC              = 20,      // 13
	GENRE_RAP                    = 21,      // 15
	GENRE_REGGAE                 = 22,      // 16
	GENRE_ROCK_MUSIC             = 23,      // 17
	GENRE_RHYTHM_AND_BLUES       = 24,      // 14
	GENRE_SOUND_EFFECTS          = 25,      // 37
	GENRE_SOUND_TRACK            = 26,      // 24
	GENRE_SPOKEN_WORD            = 27,      // 101
	GENRE_WORLD_MUSIC            = 28,      // 12
	GENRE_BLUES                  = 29       // 0
};

enum category_t {
	CATEGORY_NOT_USED = 0,
	CATEGORY_GENERAL  = 1,
	CATEGORY_JAPANESE = 2
};

extern const char* album_category[];

enum track_type_t {
	TRACK_TYPE_TITLE                  = 0x01,
	TRACK_TYPE_PERFORMER              = 0x02,
	TRACK_TYPE_SONGWRITER             = 0x03,
	TRACK_TYPE_COMPOSER               = 0x04,
	TRACK_TYPE_ARRANGER               = 0x05,
	TRACK_TYPE_MESSAGE                = 0x06,
	TRACK_TYPE_EXTRA_MESSAGE          = 0x07,

	TRACK_TYPE_TITLE_PHONETIC         = 0x81,
	TRACK_TYPE_PERFORMER_PHONETIC     = 0x82,
	TRACK_TYPE_SONGWRITER_PHONETIC    = 0x83,
	TRACK_TYPE_COMPOSER_PHONETIC      = 0x84,
	TRACK_TYPE_ARRANGER_PHONETIC      = 0x85,
	TRACK_TYPE_MESSAGE_PHONETIC       = 0x86,
	TRACK_TYPE_EXTRA_MESSAGE_PHONETIC = 0x87
};

#pragma pack(1)

/**
 * Common
 *
 * The following structures are used in both the Master and area TOCs.
 */

/**
 * Genre Information.
 */
typedef struct {
	uint8_t  category;                        // category_t
	uint16_t reserved;
	uint8_t  genre;                           // genre_t
}
genre_table_t;

/**
 * Language & character set
 */
typedef struct {
	char    language_code[2];                 // ISO639-2 Language code
	uint8_t character_set;                    // char_set_t, 1 (ISO 646)
	uint8_t reserved;
}
locale_table_t;

/**
 * Master TOC
 *
 * The following structures are needed for Master TOC information.
 */
typedef struct {
	char           id[8];                     // SACDMTOC
	struct {
		uint8_t major;
		uint8_t minor;
	} version;                                // 1.20 / 0x0114
	uint8_t        reserved01[6];
	uint16_t       album_set_size;
	uint16_t       album_sequence_number;
	uint8_t        reserved02[4];
	char           album_catalog_number[16];  // 0x00 when empty, else padded with spaces for short strings
	genre_table_t  album_genre[4];
	uint8_t        reserved03[8];
	uint32_t       area_1_toc_1_start;
	uint32_t       area_1_toc_2_start;
	uint32_t       area_2_toc_1_start;
	uint32_t       area_2_toc_2_start;
	uint8_t        disc_type_reserved   : 7;
	uint8_t        disc_type_hybrid     : 1;
	uint8_t        reserved04[3];
	uint16_t       area_1_toc_size;
	uint16_t       area_2_toc_size;
	char           disc_catalog_number[16];   // 0x00 when empty, else padded with spaces for short strings
	genre_table_t  disc_genre[4];
	uint16_t       disc_date_year;
	uint8_t        disc_date_month;
	uint8_t        disc_date_day;
	uint8_t        reserved05[4];
	uint8_t        text_area_count;
	uint8_t        reserved06[7];
	locale_table_t locales[MAX_LANGUAGE_COUNT];
}
master_toc_t;

/**
 * Master Album Information
 */
typedef struct {
	char     id[8];                           // SACDText
	uint8_t  reserved[8];
	uint16_t album_title_position;
	uint16_t album_artist_position;
	uint16_t album_publisher_position;
	uint16_t album_copyright_position;
	uint16_t album_title_phonetic_position;
	uint16_t album_artist_phonetic_position;
	uint16_t album_publisher_phonetic_position;
	uint16_t album_copyright_phonetic_position;
	uint16_t disc_title_position;
	uint16_t disc_artist_position;
	uint16_t disc_publisher_position;
	uint16_t disc_copyright_position;
	uint16_t disc_title_phonetic_position;
	uint16_t disc_artist_phonetic_position;
	uint16_t disc_publisher_phonetic_position;
	uint16_t disc_copyright_phonetic_position;
	uint8_t  data[2000];
}
master_sacd_text_t;

typedef struct {
	string album_title;
	string album_artist;
	string album_publisher;
	string album_copyright;
	string album_title_phonetic;
	string album_artist_phonetic;
	string album_publisher_phonetic;
	string album_copyright_phonetic;
	string disc_title;
	string disc_artist;
	string disc_publisher;
	string disc_copyright;
	string disc_title_phonetic;
	string disc_artist_phonetic;
	string disc_publisher_phonetic;
	string disc_copyright_phonetic;
} 
master_text_t;

/**
 * Unknown Structure
 */
typedef struct {
	char    id[8];                             // SACD_Man, manufacturer information
	uint8_t information[2040];
}
master_man_t;

/**
 * Area TOC
 *
 * The following structures are needed for Area TOC information.
 *
 */
typedef struct {
	char           id[8];                     // TWOCHTOC or MULCHTOC
	struct {
		uint8_t major;
		uint8_t minor;
	} version;                                // 1.20 / 0x0114
	uint16_t       size;                      // ex. 40 (total size of TOC)
	uint8_t        reserved01[4];
	uint32_t       max_byte_rate;
	uint8_t        sample_frequency;          // 0x04 = (64 * 44.1 kHz) (physically there can be no other values, or..? :)
	uint8_t        frame_format : 4;
	uint8_t        reserved02   : 4;
	uint8_t        reserved03[10];
	uint8_t        channel_count;
	uint8_t        loudspeaker_config : 5;
	uint8_t        extra_settings : 3;
	uint8_t        max_available_channels;
	uint8_t        area_mute_flags;
	uint8_t        reserved04[12];
	uint8_t        track_attribute : 4;
	uint8_t        reserved05 : 4;
	uint8_t        reserved06[15];
	struct {
		uint8_t minutes;
		uint8_t seconds;
		uint8_t frames;
	} total_playtime;
	uint8_t        reserved07;
	uint8_t        track_offset;
	uint8_t        track_count;
	uint8_t        reserved08[2];
	uint32_t       track_start;
	uint32_t       track_end;
	uint8_t        text_area_count;
	uint8_t        reserved09[7];
	locale_table_t languages[10];
	uint16_t       track_text_offset;
	uint16_t       index_list_offset;
	uint16_t       access_list_offset;
	uint8_t        reserved10[10];
	uint16_t       area_description_offset;
	uint16_t       copyright_offset;
	uint16_t       area_description_phonetic_offset;
	uint16_t       copyright_phonetic_offset;
	uint8_t        data[1896];
}
area_toc_t;

typedef struct {
	string track_type_title;
	string track_type_performer;
	string track_type_songwriter;
	string track_type_composer;
	string track_type_arranger;
	string track_type_message;
	string track_type_extra_message;
	string track_type_title_phonetic;
	string track_type_performer_phonetic;
	string track_type_songwriter_phonetic;
	string track_type_composer_phonetic;
	string track_type_arranger_phonetic;
	string track_type_message_phonetic;
	string track_type_extra_message_phonetic;
} 
area_track_text_t;

typedef struct {
	char     id[8];                           // SACDTTxt, Track Text
	uint16_t track_text_position[1];
}
area_text_t;

typedef struct {
	char country_code[2];
	char owner_code[3];
	char recording_year[2];
	char designation_code[5];
}
isrc_t;

typedef struct {
	char          id[8];                      // SACD_IGL, ISRC and Genre List
	isrc_t        isrc[255];
	uint32_t      reserved;
	genre_table_t track_genre[255];
}
area_isrc_genre_t;

typedef struct {
	char     id[8];                           // SACD_ACC, Access List
	uint16_t entry_count;
	uint8_t  main_step_size;
	uint8_t  reserved01[5];
	uint8_t  main_access_list[6550][5];
	uint8_t  reserved02[2];
	uint8_t  detailed_access_list[32768];
}
area_access_list_t;

typedef struct {
	char     id[8];                           // SACDTRL1
	uint32_t track_start_lsn[255];
	uint32_t track_length_lsn[255];
}
area_tracklist_offset_t;

typedef struct {
	uint8_t minutes;
	uint8_t seconds;
	uint8_t frames;
	uint8_t reserved : 5;
	uint8_t extra_use : 3;
}
area_tracklist_time_start_t;

typedef struct {
	uint8_t minutes;
	uint8_t seconds;
	uint8_t frames;
	uint8_t reserved : 3;
	uint8_t track_flags_tmf1 : 1;
	uint8_t track_flags_tmf2 : 1;
	uint8_t track_flags_tmf3 : 1;
	uint8_t track_flags_tmf4 : 1;
	uint8_t track_flags_ilp : 1;
}
area_tracklist_time_duration_t;

typedef struct {
	char                           id[8];     // SACDTRL2
	area_tracklist_time_start_t    start[255];
	area_tracklist_time_duration_t duration[255];
} 
area_tracklist_time_t;

enum audio_packet_data_type_t {
	DATA_TYPE_AUDIO         = 2,
	DATA_TYPE_SUPPLEMENTARY = 3,
	DATA_TYPE_PADDING       = 7
};

// It's no use to make a little & big endian struct. On little 
// endian systems this needs to be filled manually anyway.
typedef struct {
	uint8_t  frame_start   : 1;
	uint8_t  reserved      : 1;
	uint8_t  data_type     : 3;
	uint16_t packet_length : 11;
} 
audio_packet_info_t;
#define AUDIO_PACKET_INFO_SIZE 2U

typedef struct {
	struct {
		uint8_t minutes;
		uint8_t seconds;
		uint8_t frames;
	} timecode;

	// Note: the following byte is only filled
	// on DST encoded audio frames
	uint8_t channel_bit_3 : 1;
	uint8_t channel_bit_2 : 1;
	uint8_t sector_count  : 5;
	uint8_t channel_bit_1 : 1;
} 
audio_frame_info_t;
#define AUDIO_FRAME_INFO_SIZE 4U

typedef struct {
	uint8_t dst_encoded       : 1;
	uint8_t reserved          : 1;
	uint8_t frame_info_count  : 3;
	uint8_t packet_info_count : 3;
}
audio_frame_header_t;
#define AUDIO_SECTOR_HEADER_SIZE 1U

typedef struct {
	audio_frame_header_t header;
	audio_packet_info_t  packet[7];
	audio_frame_info_t   frame[7];
} 
audio_sector_t;

typedef struct {
	uint8_t*                 area_data;
	area_toc_t*              area_toc;
	area_tracklist_offset_t* area_tracklist_offset;
	area_tracklist_time_t*   area_tracklist_time;
	area_text_t*             area_text;
	area_track_text_t        area_track_text[255];      // max of 255 supported tracks
	area_isrc_genre_t*       area_isrc_genre;

	string                   description;
	string                   copyright;
	string                   description_phonetic;
	string                   copyright_phonetic;
}
scarletbook_area_t;

typedef struct {
	void*               sacd;                          // sacd_reader_t

	uint8_t*            master_data;
	master_toc_t*       master_toc;
	master_man_t*       master_man;
	master_text_t       master_text;

	int                 twoch_area_idx;
	int                 mulch_area_idx;
	int                 area_count;
	scarletbook_area_t  area[2];
} 
scarletbook_handle_t;

#pragma pack()

#endif
