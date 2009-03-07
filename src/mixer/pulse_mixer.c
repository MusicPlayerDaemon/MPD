#include "mixer_api.h"
#include "conf.h"

#include <glib.h>
#include <pulse/volume.h>
#include <pulse/pulseaudio.h>

#include <string.h>

struct pulse_mixer {
	struct mixer base;
	char *server;
	char *sink;
	char *output_name;
	uint32_t index;
	bool	online;
	struct pa_context *context;
	struct pa_threaded_mainloop *mainloop;
	struct pa_cvolume *volume;

};

static void
sink_input_cb(G_GNUC_UNUSED pa_context *context, const pa_sink_input_info *i,
	      int eol, void *userdata)
{

	struct pulse_mixer *pm = userdata;
	if (eol) {
		g_debug("eol error sink_input_cb");
		return;
	}

	if (!i) {
		g_debug("Sink input callback failure");
		return;
	}
	g_debug("sink input cb %s, index %d ",i->name,i->index);
	if(strcmp(i->name,pm->output_name)==0) {
		pm->index=i->index;
		pm->online=true;
		*pm->volume=i->volume;
	} else
		g_debug("bad name");
}

static void
sink_input_vol(G_GNUC_UNUSED pa_context *context, const pa_sink_input_info *i,
	       int eol, void *userdata)
{

	struct pulse_mixer *pm = userdata;
	if (eol) {
		g_debug("eol error sink_input_vol");
		return;
	}

	if (!i) {
		g_debug("Sink input callback failure");
		return;
	}
	g_debug("sink input vol %s, index %d ", i->name, i->index);
	*pm->volume=i->volume;
}

static void
subscribe_cb(G_GNUC_UNUSED pa_context *c, pa_subscription_event_type_t t,
	     uint32_t idx, void *userdata)
{

	struct pulse_mixer *pm = userdata;
	g_debug("pulse_mixer: subscribe call back");
	switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
	case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
		if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
		    PA_SUBSCRIPTION_EVENT_REMOVE)
			pm->online =false;
		else {
			pa_operation *o;

			if (!(o = pa_context_get_sink_input_info(pm->context, idx, sink_input_cb, pm))) {
				g_debug("pulse_mixer: pa_context_get_sink_input_info() failed");
				return;
			}

			pa_operation_unref(o);
		}
		break;
	}
}

static void
context_state_cb(pa_context *context, void *userdata)
{
	struct pulse_mixer *pm = userdata;
	switch (pa_context_get_state(context)) {
	case PA_CONTEXT_READY: {
		pa_operation *o;

		pa_context_set_subscribe_callback(context, subscribe_cb, pm);

		if (!(o = pa_context_subscribe(context, (pa_subscription_mask_t)
					       (PA_SUBSCRIPTION_MASK_SINK_INPUT), NULL, NULL))) {
			g_debug("pulse_mixer: pa_context_subscribe() failed");
			return;
		}
		pa_operation_unref(o);


		if (!(o = pa_context_get_sink_input_info_list(context, sink_input_cb, pm))) {
			g_debug("pulse_mixer: pa_context_get_sink_input_info_list() failed");
			return;
		}
		pa_operation_unref(o);


		pa_threaded_mainloop_signal(pm->mainloop, 0);
		break;
	}

	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		pa_threaded_mainloop_signal(pm->mainloop, 0);
		break;
	}
}


static struct mixer *
pulse_mixer_init(const struct config_param *param)
{
	struct pulse_mixer *pm = g_new(struct pulse_mixer,1);
	mixer_init(&pm->base, &pulse_mixer);
	pm->server = NULL;
	pm->sink = NULL;
	pm->context=NULL;
	pm->mainloop=NULL;
	pm->volume=NULL;
	pm->output_name=NULL;
	pm->index=0;
	pm->online=false;

	pm->volume = g_new(struct pa_cvolume,1);

	pm->server = param != NULL
		? config_dup_block_string(param, "server", NULL) : NULL;
	pm->sink = param != NULL
		? config_dup_block_string(param, "sink", NULL) : NULL;
	pm->output_name = param != NULL
		? config_dup_block_string(param, "name", NULL) : NULL;


	g_debug("pulse_mixer: init");

	if(!(pm->mainloop = pa_threaded_mainloop_new())) {
		g_debug("pulse_mixer: failed mainloop");
		g_free(pm);
		return NULL;
	}

	if(!(pm->context = pa_context_new(pa_threaded_mainloop_get_api(pm->mainloop),
					  "Mixer mpd"))) {
		g_debug("pulse_mixer: failed context");
		g_free(pm);
		return NULL;
	}

	pa_context_set_state_callback(pm->context, context_state_cb, pm);

	if (pa_context_connect(pm->context, pm->server,
			       (pa_context_flags_t)0, NULL) < 0) {
		g_debug("pulse_mixer: context server fail");
		g_free(pm);
		return NULL;
	}

	pa_threaded_mainloop_lock(pm->mainloop);
	if (pa_threaded_mainloop_start(pm->mainloop) < 0) {
		g_debug("pulse_mixer: error start mainloop");
		g_free(pm);
		return NULL;
	}

	pa_threaded_mainloop_wait(pm->mainloop);

	if (pa_context_get_state(pm->context) != PA_CONTEXT_READY) {
		g_debug("pulse_mixer: error context not ready");
		g_free(pm);
		return NULL;
	}

	pa_threaded_mainloop_unlock(pm->mainloop);
	return &pm->base ;

}

static void
pulse_mixer_finish(struct mixer *data)
{
	struct pulse_mixer *pm = (struct pulse_mixer *) data;
	pm->context  = NULL;
	pm->mainloop = NULL;
	pm->volume   = NULL;
	pm->online   = false;
	g_free(pm);
}

static bool
pulse_mixer_open(G_GNUC_UNUSED struct mixer *data)
{
	g_debug("pulse mixer open");
	return true;
}

static void
pulse_mixer_close(G_GNUC_UNUSED struct mixer *data)
{
	return;
}

static int
pulse_mixer_get_volume(struct mixer *mixer)
{
	struct pulse_mixer *pm=(struct pulse_mixer *) mixer;
	int ret;
	pa_operation *o;

	g_debug("pulse_mixer: get_volume %s",
		pm->online == TRUE ? "online" : "offline");
	if(pm->online) {
		if (!(o = pa_context_get_sink_input_info(pm->context, pm->index,
							 sink_input_vol, pm))) {
			g_debug("pa_context_get_sink_input_info() failed");
			return false;
		}
		pa_operation_unref(o);

		ret = (int)((100*(pa_cvolume_avg(pm->volume)+1))/PA_VOLUME_NORM);
		g_debug("pulse_mixer: volume %d", ret);
		return ret;
	}

	return false;
}

static bool
pulse_mixer_set_volume(struct mixer *mixer, unsigned volume)
{
	struct pulse_mixer *pm=(struct pulse_mixer *) mixer;
	pa_operation *o;
	if (pm->online) {
		pa_cvolume_set(pm->volume, (pm->volume)->channels,
				(pa_volume_t)(volume)*PA_VOLUME_NORM/100+0.5);

		if (!(o = pa_context_set_sink_input_volume(pm->context, pm->index,
							   pm->volume, NULL, NULL))) {
			g_debug("pulse_mixer: pa_context_set_sink_input_volume() failed");
			return false;
		}

		pa_operation_unref(o);
	}

	return true;
}

const struct mixer_plugin pulse_mixer = {
	.init = pulse_mixer_init,
	.finish = pulse_mixer_finish,
	.open = pulse_mixer_open,
	.close = pulse_mixer_close,
	.get_volume = pulse_mixer_get_volume,
	.set_volume = pulse_mixer_set_volume,
};
