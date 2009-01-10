
#include <stdio.h>
#include <assert.h>

#include "mixer_api.h"

void mixer_init(struct mixer *mixer, struct mixer_plugin *plugin)
{
	assert(plugin != NULL);
	assert(mixer != NULL);
	mixer->plugin = plugin;
	mixer->data = mixer->plugin->init();
}

void mixer_finish(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	mixer->plugin->finish(mixer->data);
	mixer->data = NULL;
	mixer->plugin = NULL;
}

void mixer_configure(struct mixer *mixer, ConfigParam *param)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	mixer->plugin->configure(mixer->data, param);
}

bool mixer_open(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	return mixer->plugin->open(mixer->data);
}

bool mixer_control(struct mixer *mixer, int cmd, void *arg)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	return mixer->plugin->control(mixer->data, cmd, arg);
}

void mixer_close(struct mixer *mixer)
{
	assert(mixer != NULL && mixer->plugin != NULL);
	mixer->plugin->close(mixer->data);
}
