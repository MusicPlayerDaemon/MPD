
#include "../output_api.h"
#include "../mixer_api.h"

#include <glib.h>
#include <alsa/asoundlib.h>

#define VOLUME_MIXER_ALSA_DEFAULT		"default"
#define VOLUME_MIXER_ALSA_CONTROL_DEFAULT	"PCM"

struct alsa_mixer {
	char *device;
	char *control;
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	long volume_min;
	long volume_max;
	int volume_set;
};

static struct mixer_data *
alsa_mixer_init(void)
{
	struct alsa_mixer *am = g_malloc(sizeof(struct alsa_mixer));
	am->device = NULL;
	am->control = NULL;
	am->handle = NULL;
	am->elem = NULL;
	am->volume_min = 0;
	am->volume_max = 0;
	am->volume_set = -1;
	return (struct mixer_data *)am;
}

static void
alsa_mixer_finish(struct mixer_data *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;

	g_free(am->device);
	g_free(am->control);
	g_free(am);
}

static void
alsa_mixer_configure(struct mixer_data *data, const struct config_param *param)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;
	const char *value;

	value = config_get_block_string(param, "mixer_device", NULL);
	if (value != NULL) {
		g_free(am->device);
		am->device = g_strdup(value);
	}

	value = config_get_block_string(param, "mixer_control", NULL);
	if (value != NULL) {
		g_free(am->control);
		am->control = g_strdup(value);
	}
}

static void
alsa_mixer_close(struct mixer_data *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;
	if (am->handle) snd_mixer_close(am->handle);
	am->handle = NULL;
}

static bool
alsa_mixer_open(struct mixer_data *data)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;
	int err;
	snd_mixer_elem_t *elem;
	const char *control_name = VOLUME_MIXER_ALSA_CONTROL_DEFAULT;
	const char *device = VOLUME_MIXER_ALSA_DEFAULT;

	if (am->device) {
		device = am->device;
	}
	err = snd_mixer_open(&am->handle, 0);
	snd_config_update_free_global();
	if (err < 0) {
		g_warning("problems opening alsa mixer: %s\n", snd_strerror(err));
		return false;
	}

	if ((err = snd_mixer_attach(am->handle, device)) < 0) {
		g_warning("problems attaching alsa mixer: %s\n",
			snd_strerror(err));
		alsa_mixer_close(data);
		return false;
	}

	if ((err = snd_mixer_selem_register(am->handle, NULL,
		    NULL)) < 0) {
		g_warning("problems snd_mixer_selem_register'ing: %s\n",
			snd_strerror(err));
		alsa_mixer_close(data);
		return false;
	}

	if ((err = snd_mixer_load(am->handle)) < 0) {
		g_warning("problems snd_mixer_selem_register'ing: %s\n",
			snd_strerror(err));
		alsa_mixer_close(data);
		return false;
	}

	elem = snd_mixer_first_elem(am->handle);

	if (am->control) {
		control_name = am->control;
	}

	while (elem) {
		if (snd_mixer_elem_get_type(elem) == SND_MIXER_ELEM_SIMPLE) {
			if (strcasecmp(control_name,
				       snd_mixer_selem_get_name(elem)) == 0) {
				break;
			}
		}
		elem = snd_mixer_elem_next(elem);
	}

	if (elem) {
		am->elem = elem;
		snd_mixer_selem_get_playback_volume_range(am->elem,
							  &am->volume_min,
							  &am->volume_max);
		return true;
	}

	g_warning("can't find alsa mixer control \"%s\"\n", control_name);

	alsa_mixer_close(data);
	return false;
}

static bool
alsa_mixer_control(struct mixer_data *data, int cmd, void *arg)
{
	struct alsa_mixer *am = (struct alsa_mixer *)data;
	switch (cmd) {
	case AC_MIXER_CONFIGURE:
		alsa_mixer_configure(data, (const struct config_param *)arg);
		if (am->handle)
			alsa_mixer_close(data);
		return true;
	case AC_MIXER_GETVOL:
	{
		int err;
		int ret, *volume = arg;
		long level;

		if (!am->handle && !alsa_mixer_open(data)) {
			return false;
		}
		if ((err = snd_mixer_handle_events(am->handle)) < 0) {
			g_warning("problems getting alsa volume: %s (snd_mixer_%s)\n",
				snd_strerror(err), "handle_events");
			alsa_mixer_close(data);
			return false;
		}
		if ((err = snd_mixer_selem_get_playback_volume(am->elem,
			       SND_MIXER_SCHN_FRONT_LEFT, &level)) < 0) {
			g_warning("problems getting alsa volume: %s (snd_mixer_%s)\n",
				snd_strerror(err), "selem_get_playback_volume");
			alsa_mixer_close(data);
			return false;
		}
		ret = ((am->volume_set / 100.0) * (am->volume_max - am->volume_min)
			+ am->volume_min) + 0.5;
		if (am->volume_set > 0 && ret == level) {
			ret = am->volume_set;
		} else {
			ret = (int)(100 * (((float)(level - am->volume_min)) /
				(am->volume_max - am->volume_min)) + 0.5);
		}
		*volume = ret;
		return true;
	}
	case AC_MIXER_SETVOL:
	{
		float vol;
		long level;
		int *volume = arg;
		int err;

		if (!am->handle && !alsa_mixer_open(data)) {
			return false;
		}
		vol = *volume;

		am->volume_set = vol + 0.5;
		am->volume_set = am->volume_set > 100 ? 100 :
			    (am->volume_set < 0 ? 0 : am->volume_set);

		level = (long)(((vol / 100.0) * (am->volume_max - am->volume_min) +
			 am->volume_min) + 0.5);
		level = level > am->volume_max ? am->volume_max : level;
		level = level < am->volume_min ? am->volume_min : level;

		if ((err = snd_mixer_selem_set_playback_volume_all(am->elem,
								level)) < 0) {
			g_warning("problems setting alsa volume: %s\n",
				snd_strerror(err));
			alsa_mixer_close(data);
			return false;
		}
		return true;
	}
	default:
		g_warning("Unsuported alsa control\n");
		break;
	}
	return false;
}

struct mixer_plugin alsa_mixer = {
	.init = alsa_mixer_init,
	.finish = alsa_mixer_finish,
	.configure = alsa_mixer_configure,
	.open = alsa_mixer_open,
	.control = alsa_mixer_control,
	.close = alsa_mixer_close
};
