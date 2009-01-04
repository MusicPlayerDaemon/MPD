
#ifndef MPD_MIXER_H
#define MPD_MIXER_H

#include "conf.h"

/**
 * alsa mixer
 */

struct alsa_mixer;

struct alsa_mixer *alsa_mixer_init(void);
void alsa_mixer_finish(struct alsa_mixer *am);
void alsa_mixer_configure(struct alsa_mixer *am, ConfigParam *param);
bool alsa_mixer_open(struct alsa_mixer *am);
bool alsa_mixer_control(struct alsa_mixer *am, int cmd, void *arg);
void alsa_mixer_close(struct alsa_mixer *am);

/**
 * oss mixer
 */

struct oss_mixer;

struct oss_mixer *oss_mixer_init(void);
void oss_mixer_finish(struct oss_mixer *am);
void oss_mixer_configure(struct oss_mixer *am, ConfigParam *param);
bool oss_mixer_open(struct oss_mixer *am);
bool oss_mixer_control(struct oss_mixer *am, int cmd, void *arg);
void oss_mixer_close(struct oss_mixer *am);

#endif
