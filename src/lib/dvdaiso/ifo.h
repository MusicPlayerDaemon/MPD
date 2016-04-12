#ifndef IFO_H
#define IFO_H

#include <stdint.h>

#pragma pack(1)

/**
 * Common
 *
 * The following structures are used in the AMGI, VMGI, ATSI, VTSI.
 */

/**
 * DVD Time Information.
 */
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t frame_u; /* The two high bits are the frame rate. */
} dvd_time_t;

/**
 * Type to store per-command data.
 */
typedef struct {
  uint8_t bytes[8];
} vm_cmd_t;
#define COMMAND_DATA_SIZE 8U

/**
 * Video Attributes.
 */
typedef struct {
  uint8_t permitted_df         : 2;
  uint8_t display_aspect_ratio : 2;
  uint8_t video_format         : 2;
  uint8_t mpeg_version         : 2;
  
  uint8_t film_mode            : 1;
  uint8_t letterboxed          : 1;
  uint8_t picture_size         : 2;
  uint8_t bit_rate             : 1;
  uint8_t unknown1             : 1;
  uint8_t line21_cc_2          : 1;
  uint8_t line21_cc_1          : 1;
} video_attr_t;

/**
 * Audio Attributes.
 */
typedef struct {
  uint8_t application_mode       : 2;
  uint8_t lang_type              : 2;
  uint8_t multichannel_extension : 1;
  uint8_t audio_format           : 3;
  
  uint8_t channels               : 3;
  uint8_t unknown1               : 1;
  uint8_t sample_frequency       : 2;
  uint8_t quantization           : 2;

  uint16_t lang_code;
  uint8_t  lang_extension;
  uint8_t  code_extension;
  uint8_t  unknown3;
  union {
    struct {
      uint8_t mode               : 1;
      uint8_t mc_intro           : 1;
      uint8_t version            : 2;
      uint8_t channel_assignment : 3;
      uint8_t unknown4           : 1;
    } karaoke;
    struct {
      uint8_t unknown6           : 3;
      uint8_t dolby_encoded      : 1;
      uint8_t unknown5           : 4;
    } surround;
  } app_info;
} audio_attr_t;

/**
 * MultiChannel Extension
 */
typedef struct {
  uint8_t ach0_gme   : 1;
  uint8_t zero1      : 7;

  uint8_t ach1_gme   : 1;
  uint8_t zero2      : 7;

  uint8_t ach2_gm2e  : 1;
  uint8_t ach2_gm1e  : 1;
  uint8_t ach2_gv2e  : 1;
  uint8_t ach2_gv1e  : 1;
  uint8_t zero3      : 4;

  uint8_t ach3_se2e  : 1;
  uint8_t ach3_gmAe  : 1;
  uint8_t ach3_gv2e  : 1;
  uint8_t ach3_gv1e  : 1;
  uint8_t zero4      : 4;

  uint8_t ach4_seBe  : 1;
  uint8_t ach4_gmBe  : 1;
  uint8_t ach4_gv2e  : 1;
  uint8_t ach4_gv1e  : 1;
  uint8_t zero5      : 4;
  uint8_t zero6[19];
} multichannel_ext_t;

/**
 * Subpicture Attributes.
 */
typedef struct {
  /*
   * type: 0 not specified
   *       1 language
   *       2 other
   * coding mode: 0 run length
   *              1 extended
   *              2 other
   * language: indicates language if type == 1
   * lang extension: if type == 1 contains the lang extension
   */
  uint8_t  type      : 2;
  uint8_t  zero1     : 3;
  uint8_t  code_mode : 3;
  uint8_t  zero2;
  uint16_t lang_code;
  uint8_t  lang_extension;
  uint8_t  code_extension;
} subp_attr_t;

/**
 * PGC Command Table.
 */ 
typedef struct {
  uint16_t  nr_of_pre;
  uint16_t  nr_of_post;
  uint16_t  nr_of_cell;
  uint16_t  last_byte;
  vm_cmd_t* pre_cmds;
  vm_cmd_t* post_cmds;
  vm_cmd_t* cell_cmds;
} pgc_command_tbl_t;
#define PGC_COMMAND_TBL_SIZE 8U

/**
 * PGC Program Map
 */
typedef uint8_t pgc_program_map_t; 

/**
 * Cell Playback Information.
 */
typedef struct {
  uint8_t seamless_angle    : 1;
  uint8_t stc_discontinuity : 1;
  uint8_t interleaved       : 1;
  uint8_t seamless_play     : 1;
  uint8_t block_type        : 2;
  uint8_t block_mode        : 2;
  
  uint8_t unknown2          : 6;
  uint8_t restricted        : 1;
  uint8_t playback_mode     : 1;

	uint8_t still_time;
  uint8_t cell_cmd_nr;
  dvd_time_t playback_time;
  uint32_t first_sector;
  uint32_t first_ilvu_end_sector;
  uint32_t last_vobu_start_sector;
  uint32_t last_sector;
} cell_playback_t;

#define BLOCK_TYPE_NONE         0x0
#define BLOCK_TYPE_ANGLE_BLOCK  0x1

#define BLOCK_MODE_NOT_IN_BLOCK 0x0
#define BLOCK_MODE_FIRST_CELL   0x1
#define BLOCK_MODE_IN_BLOCK     0x2
#define BLOCK_MODE_LAST_CELL    0x3

/**
 * Cell Position Information.
 */
typedef struct {
  uint16_t vob_id_nr;
  uint8_t  zero_1;
  uint8_t  cell_nr;
} cell_position_t;

/**
 * User Operations.
 */
typedef struct {
  uint8_t video_pres_mode_change         : 1; /* 24 */
  uint8_t zero                           : 7; /* 25-31 */
  
  uint8_t resume                         : 1; /* 16 */
  uint8_t button_select_or_activate      : 1;
  uint8_t still_off                      : 1;
  uint8_t pause_on                       : 1;
  uint8_t audio_stream_change            : 1;
  uint8_t subpic_stream_change           : 1;
  uint8_t angle_change                   : 1;
  uint8_t karaoke_audio_pres_mode_change : 1; /* 23 */
  
  uint8_t forward_scan                   : 1; /* 8 */
  uint8_t backward_scan                  : 1;
  uint8_t title_menu_call                : 1;
  uint8_t root_menu_call                 : 1;
  uint8_t subpic_menu_call               : 1;
  uint8_t audio_menu_call                : 1;
  uint8_t angle_menu_call                : 1;
  uint8_t chapter_menu_call              : 1; /* 15 */
  
  uint8_t title_or_time_play             : 1; /* 0 */
  uint8_t chapter_search_or_play         : 1;
  uint8_t title_play                     : 1;
  uint8_t stop                           : 1;
  uint8_t go_up                          : 1;
  uint8_t time_or_chapter_search         : 1;
  uint8_t prev_or_top_pg_search          : 1;
  uint8_t next_pg_search                 : 1; /* 7 */
} user_ops_t;

/**
 * Program Chain Information.
 */
typedef struct {
  uint16_t zero_1;
  uint8_t  nr_of_programs;
  uint8_t  nr_of_cells;
  dvd_time_t playback_time;
  user_ops_t prohibited_ops;
  uint16_t audio_control[8]; /* New type? */
  uint32_t subp_control[32]; /* New type? */
  uint16_t next_pgc_nr;
  uint16_t prev_pgc_nr;
  uint16_t goup_pgc_nr;
  uint8_t  pg_playback_mode;
  uint8_t  still_time;
  uint32_t palette[16]; /* New type struct {zero_1, Y, Cr, Cb} ? */
  uint16_t command_tbl_offset;
  uint16_t program_map_offset;
  uint16_t cell_playback_offset;
  uint16_t cell_position_offset;
  pgc_command_tbl_t* command_tbl;
  pgc_program_map_t* program_map;
  cell_playback_t* cell_playback;
  cell_position_t* cell_position;
} pgc_t;
#define PGC_SIZE 236U

/**
 * Program Chain Information Search Pointer.
 */
typedef struct {
  uint8_t  entry_id;
  uint8_t  unknown1   : 4;
  uint8_t  block_type : 2;
  uint8_t  block_mode : 2;
  uint16_t ptl_id_mask;
  uint32_t pgc_start_byte;
  pgc_t*   pgc;
} pgci_srp_t;
#define PGCI_SRP_SIZE 8U

/**
 * Program Chain Information Table.
 */
typedef struct {
  uint16_t nr_of_pgci_srp;
  uint16_t zero_1;
  uint32_t last_byte;
  pgci_srp_t* pgci_srp;
} pgcit_t;
#define PGCIT_SIZE 8U

/**
 * Menu PGCI Language Unit.
 */
typedef struct {
  uint16_t lang_code;
  uint8_t  lang_extension;
  uint8_t  exists;
  uint32_t lang_start_byte;
  pgcit_t* pgcit;
} pgci_lu_t;
#define PGCI_LU_SIZE 8U

/**
 * Menu PGCI Unit Table.
 */
typedef struct {
  uint16_t nr_of_lus;
  uint16_t zero_1;
  uint32_t last_byte;
  pgci_lu_t* lu;
} pgci_ut_t;
#define PGCI_UT_SIZE 8U

/**
 * Cell Address Information.
 */
typedef struct {
  uint16_t vob_id;
  uint8_t  cell_id;
  uint8_t  zero_1;
  uint32_t start_sector;
  uint32_t last_sector;
} cell_adr_t;

/**
 * Cell Address Table.
 */
typedef struct {
  uint16_t nr_of_vobs; /* VOBs */
  uint16_t zero_1;
  uint32_t last_byte;
  cell_adr_t* cell_adr_table;  /* No explicit size given. */
} c_adt_t;
#define C_ADT_SIZE 8U

/**
 * VOBU Address Map.
 */
typedef struct {
  uint32_t  last_byte;
  uint32_t* vobu_start_sectors;
} vobu_admap_t;
#define VOBU_ADMAP_SIZE 4U

/**
 * VMGI
 *
 * The following structures relate to the Video Manager.
 */

/**
 * Video Manager Information Management Table.
 */
typedef struct {
  char     vmg_identifier[12];
  uint32_t vmg_last_sector;
  uint8_t  zero_1[12];
  uint32_t vmgi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vmg_category;
  uint16_t vmg_nr_of_volumes;
  uint16_t vmg_this_volume_nr;
  uint8_t  disc_side;
  uint8_t  zero_3[19];
  uint16_t vmg_nr_of_title_sets;  /* Number of VTSs. */
  char     provider_identifier[32];
  uint64_t vmg_pos_code;
  uint8_t  zero_4[24];
  uint32_t vmgi_last_byte;
  uint32_t first_play_pgc;
  uint8_t  zero_5[56];
  uint32_t vmgm_vobs;             /* sector */
  uint32_t tt_srpt;               /* sector */
  uint32_t vmgm_pgci_ut;          /* sector */
  uint32_t ptl_mait;              /* sector */
  uint32_t vts_atrt;              /* sector */
  uint32_t txtdt_mgi;             /* sector */
  uint32_t vmgm_c_adt;            /* sector */
  uint32_t vmgm_vobu_admap;       /* sector */
  uint8_t  zero_6[32];
  
  video_attr_t vmgm_video_attr;
  uint8_t  zero_7;
  uint8_t  nr_of_vmgm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vmgm_audio_attr;
  audio_attr_t zero_8[7];
  uint8_t  zero_9[17];
  uint8_t  nr_of_vmgm_subp_streams; /* should be 0 or 1 */
  subp_attr_t vmgm_subp_attr;
  subp_attr_t zero_10[27];  /* XXX: how much 'padding' here? */
} vmgi_mat_t;

typedef struct {
  uint8_t title_or_time_play        : 1;
  uint8_t chapter_search_or_play    : 1;
  uint8_t jlc_exists_in_tt_dom      : 1;
  uint8_t jlc_exists_in_button_cmd  : 1;
  uint8_t jlc_exists_in_prepost_cmd : 1;
  uint8_t jlc_exists_in_cell_cmd    : 1;
  uint8_t multi_or_random_pgc_title : 1;
  uint8_t zero_1                    : 1;
} playback_type_t;

/**
 * Title Information.
 */
typedef struct {
  playback_type_t pb_ty;
  uint8_t  nr_of_angles;
  uint16_t nr_of_ptts;
  uint16_t parental_id;
  uint8_t  title_set_nr;
  uint8_t  vts_ttn;
  uint32_t title_set_sector;
} title_info_t;

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  title_info_t* title;
} tt_srpt_t;
#define TT_SRPT_SIZE 8U

/**
 * Parental Management Information Unit Table.
 * Level 1 (US: G), ..., 7 (US: NC-17), 8
 */
typedef uint16_t pf_level_t[8];

/**
 * Parental Management Information Unit Table.
 */
typedef struct {
  uint16_t country_code;
  uint16_t zero_1;
  uint16_t pf_ptl_mai_start_byte;
  uint16_t zero_2;
  pf_level_t* pf_ptl_mai; /* table of (nr_of_vtss + 1), video_ts is first */
} ptl_mait_country_t;
#define PTL_MAIT_COUNTRY_SIZE 8U

/**
 * Parental Management Information Table.
 */
typedef struct {
  uint16_t nr_of_countries;
  uint16_t nr_of_vtss;
  uint32_t last_byte;
  ptl_mait_country_t* countries;
} ptl_mait_t;
#define PTL_MAIT_SIZE 8U

/**
 * Video Title Set Attributes.
 */
typedef struct {
  uint32_t last_byte;
  uint32_t vts_cat;
  
  video_attr_t vtsm_vobs_attr;
  uint8_t  zero_1;
  uint8_t  nr_of_vtsm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_2[7];  
  uint8_t  zero_3[16];
  uint8_t  zero_4;
  uint8_t  nr_of_vtsm_subp_streams; /* should be 0 or 1 */
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_5[27];
  
  uint8_t  zero_6[2];
  
  video_attr_t vtstt_vobs_video_attr;
  uint8_t  zero_7;
  uint8_t  nr_of_vtstt_audio_streams;
  audio_attr_t vtstt_audio_attr[8];
  uint8_t  zero_8[16];
  uint8_t  zero_9;
  uint8_t  nr_of_vtstt_subp_streams;
  subp_attr_t vtstt_subp_attr[32];
} vts_attributes_t;
#define VTS_ATTRIBUTES_SIZE 542U
#define VTS_ATTRIBUTES_MIN_SIZE 356U

/**
 * Video Title Set Attribute Table.
 */
typedef struct {
  uint16_t nr_of_vtss;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_attributes_t* vts;
  uint32_t* vts_atrt_offsets; /* offsets table for each vts_attributes */
} vts_atrt_t;
#define VTS_ATRT_SIZE 8U

/**
 * Text Data. (Incomplete)
 */
typedef struct {
  uint32_t last_byte;    /* offsets are relative here */
  uint16_t offsets[100]; /* == nr_of_srpts + 1 (first is disc title) */
#if 0  
  uint16_t unknown; /* 0x48 ?? 0x48 words (16bit) info following */
  uint16_t zero_1;
  
  uint8_t type_of_info; /* ?? 01 == disc, 02 == Title, 04 == Title part */
  uint8_t unknown1;
  uint8_t unknown2;
  uint8_t unknown3;
  uint8_t unknown4; /* ?? allways 0x30 language?, text format? */
  uint8_t unknown5;
  uint16_t offset; /* from first */
  
  char text[12]; /* ended by 0x09 */
#endif
} txtdt_t;

/**
 * Text Data Language Unit. (Incomplete)
 */ 
typedef struct {
  uint16_t lang_code;
  uint16_t unknown;      /* 0x0001, title 1? disc 1? side 1? */
  uint32_t txtdt_start_byte;  /* prt, rel start of vmg_txtdt_mgi  */
  txtdt_t* txtdt;
} txtdt_lu_t;
#define TXTDT_LU_SIZE 8U

/**
 * Text Data Manager Information. (Incomplete)
 */
typedef struct {
  char disc_name[14];            /* how many bytes?? */
  uint16_t nr_of_language_units; /* 32bit??          */
  uint32_t last_byte;
  txtdt_lu_t* lu;
} txtdt_mgi_t;
#define TXTDT_MGI_SIZE 20U

/**
 * VTS
 *
 * Structures relating to the Video Title Set (VTS).
 */

/**
 * Video Title Set Information Management Table.
 */
typedef struct {
  char vts_identifier[12];
  uint32_t vts_last_sector;
  uint8_t  zero_1[12];
  uint32_t vtsi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t vts_category;
  uint16_t zero_3;
  uint16_t zero_4;
  uint8_t  zero_5;
  uint8_t  zero_6[19];
  uint16_t zero_7;
  uint8_t  zero_8[32];
  uint64_t zero_9;
  uint8_t  zero_10[24];
  uint32_t vtsi_last_byte;
  uint32_t zero_11;
  uint8_t  zero_12[56];
  uint32_t vtsm_vobs;       /* sector */
  uint32_t vtstt_vobs;      /* sector */
  uint32_t vts_ptt_srpt;    /* sector */
  uint32_t vts_pgcit;       /* sector */
  uint32_t vtsm_pgci_ut;    /* sector */
  uint32_t vts_tmapt;       /* sector */
  uint32_t vtsm_c_adt;      /* sector */
  uint32_t vtsm_vobu_admap; /* sector */
  uint32_t vts_c_adt;       /* sector */
  uint32_t vts_vobu_admap;  /* sector */
  uint8_t  zero_13[24];
  
  video_attr_t vtsm_video_attr;
  uint8_t  zero_14;
  uint8_t  nr_of_vtsm_audio_streams; /* should be 0 or 1 */
  audio_attr_t vtsm_audio_attr;
  audio_attr_t zero_15[7];
  uint8_t  zero_16[17];
  uint8_t  nr_of_vtsm_subp_streams; /* should be 0 or 1 */
  subp_attr_t vtsm_subp_attr;
  subp_attr_t zero_17[27];
  uint8_t  zero_18[2];
  
  video_attr_t vts_video_attr;
  uint8_t  zero_19;
  uint8_t  nr_of_vts_audio_streams;
  audio_attr_t vts_audio_attr[8];
  uint8_t  zero_20[17];
  uint8_t  nr_of_vts_subp_streams;
  subp_attr_t vts_subp_attr[32];
  uint16_t zero_21;
  multichannel_ext_t vts_mu_audio_attr[8];
  /* XXX: how much 'padding' here, if any? */
} vtsi_mat_t;

/**
 * PartOfTitle Unit Information.
 */
typedef struct {
  uint16_t pgcn;
  uint16_t pgn;
} ptt_info_t;

/**
 * PartOfTitle Information.
 */
typedef struct {
  uint16_t nr_of_ptts;
  ptt_info_t* ptt;
} ttu_t;

/**
 * PartOfTitle Search Pointer Table.
 */
typedef struct {
  uint16_t nr_of_srpts;
  uint16_t zero_1;
  uint32_t last_byte;
  ttu_t*   title;
  uint32_t* ttu_offset; /* offset table for each ttu */
} vts_ptt_srpt_t;
#define VTS_PTT_SRPT_SIZE 8U

/**
 * Time Map Entry.
 */
/* Should this be bit field at all or just the uint32_t? */
typedef uint32_t map_ent_t;

/**
 * Time Map.
 */
typedef struct {
  uint8_t  tmu;   /* Time unit, in seconds */
  uint8_t  zero_1;
  uint16_t nr_of_entries;
  map_ent_t* map_ent;
} vts_tmap_t;
#define VTS_TMAP_SIZE 4U

/**
 * Time Map Table.
 */
typedef struct {
  uint16_t nr_of_tmaps;
  uint16_t zero_1;
  uint32_t last_byte;
  vts_tmap_t* tmap;
  uint32_t* tmap_offset; /* offset table for each tmap */
} vts_tmapt_t;
#define VTS_TMAPT_SIZE 8U

/**
 * SAMG
 *
 * The following structures relate to the Simple Audio Manager.
 */
typedef struct {
  uint8_t gr2_bits  : 4;
  uint8_t gr1_bits  : 4;
  uint8_t gr2_freq  : 4;
  uint8_t gr1_freq  : 4;
  uint8_t ch_gr_assgn;
} channel_fmt_t;

typedef struct {
  uint8_t       zero_1[2];
  uint8_t       group_nr;
  uint8_t	      track_nr;
  uint32_t      first_pts;
  uint32_t      len_in_pts;
  uint8_t       zero_2[4];
  uint8_t       unknown_1 : 5;
  uint8_t       zone      : 1; /* 0 - track in AOB, 1 - track in VOB */
  uint8_t       unknown_2 : 2;
  channel_fmt_t channel_fmt;
  uint8_t       zero_3[20];
  uint32_t      abs_first_sect;
  uint32_t      abs_first_sect_dup;
  uint32_t      abs_last_sect;
} samg_track_t;

typedef struct {
  char     samg_identifier[12];
  uint16_t samg_nr_of_tracks;
  uint8_t  zero_1;
  uint8_t  specification_version;
	samg_track_t track[314];
  uint8_t  zero_2[40];
} samg_mat_t;

/**
 * ASVS
 *
 * The following structures relate to the Audio Still Video Set.
 */
typedef struct {
  uint16_t off_sect;
} asv_img_t;
#define ASV_IMG_SIZE 2U

typedef struct {
  uint8_t  nr_of_stills;
  uint8_t  zero_1;
  uint16_t base_still_nr;
  uint32_t base_sect;
  asv_img_t* asv_img;
} asvu_t;
#define ASVU_SIZE 8U

typedef struct {
  char     asvs_identifier[12];
  uint16_t asvs_nr_of_asvus;
  uint8_t  zero_1;
  uint8_t  specification_version;
  uint8_t  zero_2[16];
  uint32_t palette[16];
  asvu_t*  asvu;
} asvs_mat_t;
#define ASVS_MAT_SIZE 96U

/**
 * AMGI
 *
 * The following structures relate to the Audio Manager.
 */

/**
 * Audio Manager Information Management Table.
 */
typedef struct {
  char     amg_identifier[12];
  uint32_t amg_last_sector;
  uint8_t  zero_1[12];
  uint32_t amgi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t amg_category;
  uint16_t amg_nr_of_volumes;
  uint16_t amg_this_volume_nr;
  uint8_t  disc_side;
	uint8_t  zero_3[5];
	uint32_t amg_asvs;                    /* sector */
  uint8_t  zero_4[10];
  uint8_t  amg_nr_of_video_title_sets;  /* Number of VTSs. */
  uint8_t  amg_nr_of_audio_title_sets;  /* Number of ATSs. */
  char     provider_identifier[32];
  uint64_t amg_pos_code;
  uint8_t  zero_5[24];
  uint32_t amgi_last_byte;
  uint32_t first_play_pgc;
  uint8_t  zero_6[56];
  uint32_t amgm_vobs;             /* sector */
  uint32_t att_srpt;              /* sector */
  uint32_t aott_srpt;             /* sector */
  uint32_t amgm_pgci_ut;          /* sector */
  uint32_t ats_atrt;              /* sector */
  uint32_t txtdt_mgi;             /* sector */
  uint32_t amgm_c_adt;            /* sector */
  uint32_t amgm_vobu_admap;       /* sector */
  uint8_t  zero_7[32];
  
  video_attr_t amgm_video_attr;
  uint8_t  zero_8;
  uint8_t  nr_of_amgm_audio_streams; /* should be 0 or 1 */
  audio_attr_t amgm_audio_attr;
  audio_attr_t zero_9[7];
  uint8_t  zero_10[17];
  uint8_t  nr_of_amgm_subp_streams; /* should be 0 or 1 */
  subp_attr_t amgm_subp_attr;
  subp_attr_t zero_11[27];  /* XXX: how much 'padding' here? */
} amgi_mat_t;

/**
 * Audio Title Information.
 */
typedef struct {
  uint8_t title_set_nr : 4;
  uint8_t type_ext     : 3;
  uint8_t is_audio     : 1;
} audio_playback_type_t;

typedef struct {
  audio_playback_type_t pb_ty;
  uint8_t  nr_of_tracks;
  uint8_t  zero_1[2];
	uint32_t len_in_pts;
	uint8_t  title_set_nr;
	uint8_t  title_nr;
	uint32_t atsi_mat;              /* sector */
} audio_title_info_t;

/**
 * PartOfAudioTitle Search Pointer Table.
 */
typedef struct {
  uint16_t nr_of_srpts;
  uint16_t last_byte;
  audio_title_info_t* title;
} audio_tt_srpt_t;
#define AUDIO_TT_SRPT_SIZE 4U

typedef struct {
  uint16_t      audio_type;
  channel_fmt_t channel_fmt;
  uint8_t       zero_1[11];
} audio_format_t;

#define DOWNMIX_MATRICES 14
#define DOWNMIX_MATRIX_SIZE 18U
#define DOWNMIX_CHANNELS 8

typedef struct {
	struct {
		uint8_t L;
		uint8_t R;
	} phase;
	struct {
		uint8_t L;
		uint8_t R;
	} coef[DOWNMIX_CHANNELS];
} downmix_matrix_t;

/**
 * ATS
 *
 * Structures relating to the Audio Title Set (ATS).
 */

/**
 * Audio Title Set Information Management Table.
 */
typedef struct {
  char ats_identifier[12];
  uint32_t ats_last_sector;
  uint8_t  zero_1[12];
  uint32_t atsi_last_sector;
  uint8_t  zero_2;
  uint8_t  specification_version;
  uint32_t ats_category;
  uint16_t zero_3;
  uint16_t zero_4;
  uint8_t  zero_5;
  uint8_t  zero_6[19];
  uint16_t zero_7;
  uint8_t  zero_8[32];
  uint64_t zero_9;
  uint8_t  zero_10[24];
  uint32_t atsi_last_byte;
  uint32_t zero_11;
  uint8_t  zero_12[56];
  uint32_t atsm_vobs;       /* sector */
  uint32_t atstt_vobs;      /* sector */
  uint32_t ats_ptt_srpt;    /* sector */
  uint32_t ats_pgcit;       /* sector */
  uint32_t atsm_pgci_ut;    /* sector */
  uint32_t ats_tmapt;       /* sector */
  uint32_t atsm_c_adt;      /* sector */
  uint32_t atsm_vobu_admap; /* sector */
  uint32_t ats_c_adt;       /* sector */
  uint32_t ats_vobu_admap;  /* sector */
  uint8_t  zero_13[24];
  
  audio_format_t   ats_audio_format[8];
  downmix_matrix_t ats_downmix_matrices[DOWNMIX_MATRICES];
  
  /*
  video_attr_t atsm_video_attr;
  uint8_t  zero_14;
  uint8_t  nr_of_atsm_audio_streams; // should be 0 or 1
  audio_attr_t atsm_audio_attr;
  audio_attr_t zero_15[7];
  uint8_t  zero_16[17];
  uint8_t  nr_of_atsm_subp_streams; //
  subp_attr_t atsm_subp_attr;
  subp_attr_t zero_17[27];
  uint8_t  zero_18[2];
  
  video_attr_t ats_video_attr;
  uint8_t  zero_19;
  uint8_t  nr_of_ats_audio_streams;
  audio_attr_t ats_audio_attr[8];
  uint8_t  zero_20[17];
  uint8_t  nr_of_ats_subp_streams;
  subp_attr_t ats_subp_attr[32];
  uint16_t zero_21;
  multichannel_ext_t ats_mu_audio_attr[8];
  */
} atsi_mat_t;

typedef struct {
	uint8_t  track_type;
	uint8_t  downmix_matrix;
	uint8_t  zero_1[2];
	uint8_t  n;
	uint8_t  zero_2;
	uint32_t first_pts;
	uint32_t len_in_pts;
	uint8_t  zero_3[6];
} ats_track_timestamp_t;
#define ATS_TRACK_TIMESTAMP_SIZE 20U

typedef struct {
	uint8_t  zero_1[4];
	uint32_t first;
	uint32_t last;
} ats_track_sector_t;
#define ATS_TRACK_SECTOR_SIZE 12U

typedef struct {
  uint8_t  title_nr;
  uint8_t  zero_1[3];
  uint32_t title_table_offset;
} ats_title_idx_t;
#define ATS_TITLE_IDX_SIZE 8U

typedef struct {
  uint8_t  zero_1[2];
  uint8_t  tracks;
  uint8_t  indexes;
  uint32_t len_in_pts;
  uint8_t  zero_2[4];
  uint16_t track_sector_table_offset;
  uint8_t  zero_3[2];
  ats_track_timestamp_t* ats_track_timestamp;
  ats_track_sector_t*    ats_track_sector;
} ats_title_t;
#define ATS_TITLE_SIZE 16U

typedef struct {
  uint16_t nr_of_titles;
  uint8_t  zero_1[2];
  uint32_t last_byte;
  ats_title_idx_t* ats_title_idx;
  ats_title_t*     ats_title;
} audio_pgcit_t;
#define AUDIO_PGCIT_SIZE 8U

#pragma pack()

#endif /* IFO_H */
