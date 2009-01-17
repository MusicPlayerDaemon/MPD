
#ifndef MPD_MIXER_H
#define MPD_MIXER_H

#include "conf.h"

/*
 * list of currently implemented mixers
 */

extern struct mixer_plugin alsa_mixer;
extern struct mixer_plugin oss_mixer;

struct mixer_data;

struct mixer_plugin {

        /**
         * Allocate and initialize mixer data
	 */
        struct mixer_data *(*init)(void);

        /**
	 * Finish and free mixer data
         */
        void (*finish)(struct mixer_data *data);

        /**
	 * Setup and configure mixer
         */
	void (*configure)(struct mixer_data *data, struct config_param *param);

        /**
    	 * Open mixer device
	 */
	bool (*open)(struct mixer_data *data);

        /**
	 * Control mixer device.
         */
	bool (*control)(struct mixer_data *data, int cmd, void *arg);

        /**
    	 * Close mixer device
	 */
	void (*close)(struct mixer_data *data);
};

struct mixer {
	struct mixer_plugin *plugin;
	struct mixer_data *data;
};

void mixer_init(struct mixer *mixer, struct mixer_plugin *plugin);
void mixer_finish(struct mixer *mixer);
void mixer_configure(struct mixer *mixer, struct config_param *param);
bool mixer_open(struct mixer *mixer);
bool mixer_control(struct mixer *mixer, int cmd, void *arg);
void mixer_close(struct mixer *mixer);

#endif
