// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * "pwsink" mixer plugin for the PipeWire output.
 *
 * Rationale
 * ---------
 * MPD's "hardware" mixer plugin only supports outputs of type "alsa",
 * "oss", and "pulse" -- it is not wired up for type "pipewire" outputs, so
 * for a "pipewire" output, "hardware" normally just falls back to the
 * output's own registered mixer plugin (PipeWireMixerPlugin.cxx), which
 * sets SPA_PROP_channelVolumes on MPD's own pw_stream via
 * pipewire_output_set_volume() -- a per-client software gain stage applied
 * before the signal ever reaches the sink. It does not move any hardware
 * mixer control, and is not shared with other PipeWire clients
 * (shairport-sync, spotifyd, Bluetooth, etc).
 *
 * This plugin is substituted in for that default whenever an "ao" block
 * with `type "pipewire"` *explicitly* sets `mixer_type "hardware"` (see
 * the pipewire-specific special case in audio_output_load_mixer() in
 * output/Init.cxx). It opens its own native PipeWire client connection
 * (independent of MPD's own output stream) and drives a sink node's
 * volume directly via the PipeWire protocol:
 *
 *   - By default it follows the system's current default sink.
 *   - If `target "<node.name>"` is set in the same block (the same config
 *     key the "pipewire" output plugin itself already uses to pin
 *     playback to a specific node via PW_KEY_TARGET_OBJECT), this plugin
 *     follows that node's volume instead of the system default -- so
 *     audio routing and volume control stay pointed at the same node
 *     without needing two separate settings.
 *
 * WirePlumber then applies that volume the same way it would for any
 * other client -- via the ALSA hardware mixer control when the sink's
 * Route reports one (route.hw-volume = true), or via PipeWire's own
 * software volume otherwise.
 *
 * MPD's own pw_stream is pinned to unity gain (1.0) via
 * pipewire_output_set_volume(), so the sink-level volume is the only gain
 * stage in effect.
 *
 * If `mixer_type` is left unset entirely (or set to anything other than
 * "hardware", e.g. "software"/"null"/"none"), this plugin is never used --
 * an "ao" block with `type "pipewire"` and no `mixer_type` behaves exactly
 * as before, i.e. plain MPD stream volume via PipeWireMixerPlugin.cxx.
 *
 * How this plugin talks to PipeWire
 * ----------------------------------
 * This deliberately does *not* reuse PipeWireOutput's own pw_core: that
 * connection belongs to the realtime-ish audio thread and is scoped to
 * MPD's own stream. Instead PwSinkMixer opens a second, independent
 * client connection of its own (its own pw_thread_loop/pw_context/
 * pw_core).
 *
 *   1. Connect and fetch the registry.
 *   2. Find the "default" pw_metadata object (there is exactly one) and
 *      bind it; its "default.audio.sink" property is a JSON blob
 *      ({"name":"<node.name>"}) naming the currently-effective default
 *      sink. This is the same object/property WirePlumber itself updates
 *      and that `@DEFAULT_AUDIO_SINK@` resolves against.
 *   3. Match that name against Audio/Sink nodes seen in the registry,
 *      bind the matching node, and request its current SPA_PROP_Props
 *      (channelVolumes) via pw_node_enum_params().
 *   4. SetVolume() writes a new SPA_PROP_channelVolumes via
 *      pw_node_set_param(); GetVolume() reads back the last value we
 *      received via the node's "param" event.
 *
 * All of this is driven by repeated pw_core_sync()+pw_thread_loop_wait()
 * round-trips issued from Open() (and MaybeReconnect()).
 *
 * An explicit "target" config option (a literal node.name -- the same key
 * the "pipewire" output plugin uses for its own audio routing) skips the
 * metadata/default resolution entirely and pins this plugin to that node
 * too, for setups that don't want to follow the system default sink.
 *
 * WirePlumber/wpctl display and set volume on a cubic scale rather than
 * linear PCM gain (this is what gives `wpctl set-volume 0.5` roughly
 * "half loudness" rather than "half amplitude" perceptually); this is
 * replicated here so a given MPD volume percentage matches what
 * `wpctl get-volume` would show for the same node. Set "mixer_volume_curve"
 * to "linear" to bypass that and write the percentage straight into
 * channelVolumes.
 *
 * Limitations
 * -----------
 *  - Only one Audio/Sink node with a matching name is tracked; if several
 *    devices somehow expose the same node.name (shouldn't normally
 *    happen) the first one seen wins.
 *  - If the default sink changes while MPD is running (device unplugged,
 *    user switches output in pavucontrol/wpctl), this plugin does not
 *    currently re-resolve automatically; Close()+Open() (or a MPD
 *    restart / "outputs" toggle) picks up the new default.
 *  - If the PipeWire core connection reports an error (e.g. the daemon
 *    is restarted while MPD is running), any in-progress round trip is
 *    unblocked and the operation fails immediately rather than hanging
 *    until WaitUntilReady()'s retry budget is exhausted; see
 *    OnCoreError()/core_error below.
 */

#include "PwSinkMixerPlugin.hxx"
#include "mixer/Mixer.hxx"
#include "config/Block.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "output/plugins/PipeWireOutputPlugin.hxx"
#include "Log.hxx"
#include "util/Domain.hxx"

#include <pipewire/pipewire.h>
#include <pipewire/thread-loop.h>
#include <pipewire/properties.h>
#include <pipewire/extensions/metadata.h>
#include <pipewire/device.h>
#include <spa/param/props.h>
#include <spa/param/route.h>
#include <spa/pod/builder.h>
#include <spa/pod/parser.h>
#include <spa/pod/iter.h>
#include <spa/utils/json.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace {

constexpr uint32_t kMaxChannels = 32;

enum class VolumeCurve {
	CUBIC,
	LINEAR,
};

inline float
PercentToVolume(unsigned percent, VolumeCurve curve) noexcept
{
	float linear = std::clamp(percent, 0u, 100u) / 100.0f;
	return curve == VolumeCurve::CUBIC ? linear * linear * linear : linear;
}

inline unsigned
VolumeToPercent(float volume, VolumeCurve curve) noexcept
{
	if (volume < 0.0f)
		volume = 0.0f;
	float linear = curve == VolumeCurve::CUBIC ? std::cbrt(volume) : volume;
	return static_cast<unsigned>(std::lround(linear * 100.0f));
}

/**
 * Extract the "name" string field from a `{"name":"...", ...}` JSON blob
 * as published in the "default.audio.sink" / "default.configured.audio.sink"
 * metadata properties. Returns an empty string if not found or malformed.
 */
std::string
ParseDefaultNodeName(const char *json) noexcept
{
	if (json == nullptr)
		return {};

	struct spa_json outer;
	spa_json_init(&outer, json, std::strlen(json));

	struct spa_json obj;
	if (spa_json_enter_object(&outer, &obj) <= 0)
		return {};

	char key[256];
	while (spa_json_get_string(&obj, key, sizeof(key)) > 0) {
		if (std::strcmp(key, "name") == 0) {
			char value[512];
			if (spa_json_get_string(&obj, value, sizeof(value)) > 0)
				return value;
			return {};
		}

		/* skip whatever value belongs to this key -- we don't care
		   about anything but "name" */
		if (spa_json_next(&obj, nullptr) <= 0)
			break;
	}

	return {};
}

constexpr Domain pw_sink_mixer_domain("pw_sink_mixer");

} // namespace

/**
 * RAII helper for pw_thread_loop_lock()/unlock(), so an exception thrown
 * partway through setup (or from a Fmt/RuntimeError below) can never
 * leave the loop locked for the next call.
 */
class PwThreadLoopLock {
	struct pw_thread_loop *const loop;

public:
	explicit PwThreadLoopLock(struct pw_thread_loop *_loop) noexcept
		:loop(_loop)
	{
		pw_thread_loop_lock(loop);
	}

	~PwThreadLoopLock() noexcept {
		pw_thread_loop_unlock(loop);
	}

	PwThreadLoopLock(const PwThreadLoopLock &) = delete;
	PwThreadLoopLock &operator=(const PwThreadLoopLock &) = delete;
};

class PwSinkMixer final : public Mixer {
	/**
	 * The associated PipeWire output. Only touched to pin its own
	 * pw_stream to unity gain, so the sink-level volume driven by this
	 * plugin is the only gain stage in effect.
	 */
	PipeWireOutput &output;

	VolumeCurve curve = VolumeCurve::CUBIC;

	/**
	 * Explicit "target" node.name override (same config key the
	 * "pipewire" output plugin itself uses for audio routing); empty
	 * means "follow whatever the system default sink is".
	 */
	std::string configured_target;

	/* --- This plugin's own, independent PipeWire connection --- */

	struct pw_thread_loop *loop = nullptr;
	struct pw_context *context = nullptr;
	struct pw_core *core = nullptr;
	struct spa_hook core_listener {};
	int last_sync_seq = -1;

	/**
	 * Set by OnCoreError() and cleared at the start of each Connect().
	 * Checked by Roundtrip()/WaitUntilReady() so a core error unblocks
	 * any thread waiting in pw_thread_loop_wait() immediately, instead
	 * of spinning silently until WaitUntilReady()'s retry budget runs
	 * out. Holds the PipeWire error code (negative errno), or 0 if no
	 * error has been reported.
	 */
	int core_error = 0;

	struct pw_registry *registry = nullptr;
	struct spa_hook registry_listener {};

	struct pw_proxy *metadata_proxy = nullptr;
	struct spa_hook metadata_listener {};

	struct pw_proxy *sink_proxy = nullptr;
	struct spa_hook sink_node_listener {};
	uint32_t sink_global_id = SPA_ID_INVALID;

	/**
	 * The Device that owns the resolved sink (its "device.id" property).
	 * For hardware-routed sinks (route.hw-volume), THIS -- not the
	 * Node's own Props -- is what actually drives the audible/ALSA
	 * mixer-control volume; see the Route fields below.
	 */
	struct pw_proxy *device_proxy = nullptr;
	struct spa_hook device_listener {};
	uint32_t device_global_id = SPA_ID_INVALID;

	/**
	 * The currently active output Route on that Device, as last
	 * reported by the server. "index"/"device"/"direction" must be
	 * echoed back unchanged in our own SPA_PARAM_Route write -- only
	 * the embedded Props (channelVolumes) actually changes.
	 *
	 * NOTE: for a device with more than one selectable output route
	 * (e.g. built-in speaker + headphone jack), simply taking the
	 * first Output-direction route seen is not fully correct -- it
	 * should be matched against whichever route is actually selected.
	 * That refinement isn't implemented; simple single-route DACs (the
	 * common case for USB/HAT audio interfaces) are unaffected.
	 */
	int32_t route_index = -1;
	int32_t route_device = -1;
	uint32_t route_direction = 0;
	bool have_route = false;

	/** false if the resolved sink has no device.id at all (e.g. a
	    virtual/software-only sink) -- in that case there is no Route to
	    wait for, and Node-level Props is the only volume path */
	bool has_hw_route = true;

	/** {node.name, device.id} of every Audio/Sink seen so far, keyed by
	    global id */
	struct SinkInfo {
		std::string name;
		uint32_t device_id = SPA_ID_INVALID;
	};
	std::map<uint32_t, SinkInfo> known_sinks;

	/** node.name we're trying to bind, resolved from metadata or config */
	std::string target_name;

	float current_volumes[kMaxChannels] {};
	uint32_t n_current_volumes = 0;
	bool have_current_volume = false;

	/** volumes as read back from the Device's active Route, i.e. the
	    ones that actually reflect the real hardware/ALSA state */
	float route_volumes[kMaxChannels] {};
	uint32_t n_route_volumes = 0;

	/**
	 * Cached value to return from GetVolume() right after a successful
	 * SetVolume(), to avoid returning a value affected by intermediate
	 * rounding on the way through the cubic curve and back.
	 */
	int cached_volume = -1;

public:
	PwSinkMixer(PipeWireOutput &_output,
		    MixerListener &_listener) noexcept
		:Mixer(pw_sink_mixer_plugin, _listener),
		 output(_output) {}

	void Configure(const ConfigBlock &block) {
		configured_target = block.GetBlockValue("target", "");
		if (configured_target == "default")
			configured_target.clear();

		const char *curve_name = block.GetBlockValue("mixer_volume_curve",
							       "cubic");
		if (std::strcmp(curve_name, "linear") == 0)
			curve = VolumeCurve::LINEAR;
		else if (std::strcmp(curve_name, "cubic") == 0)
			curve = VolumeCurve::CUBIC;
		else
			throw FmtRuntimeError("invalid mixer_volume_curve: {}",
					      curve_name);
	}

	/* virtual methods from class Mixer */
	void Open() override;
	void Close() noexcept override;

	int GetVolume() override;
	void SetVolume(unsigned volume) override;

private:
	void Connect();
	void Disconnect() noexcept;

	/**
	 * Block (from the calling thread, NOT from inside a PipeWire
	 * callback) until all requests issued so far have been processed
	 * by the server, or until the core reports an error.
	 *
	 * @return true on success, false if the core reported an error
	 * (see core_error)
	 */
	bool Roundtrip() noexcept;

	/**
	 * Repeatedly Roundtrip() until either the sink node is bound and
	 * its initial volume has arrived, or we give up.
	 *
	 * @return true if ready, false on timeout or core error
	 */
	bool WaitUntilReady() noexcept;

	bool IsReady() const noexcept {
		if (sink_proxy == nullptr || !have_current_volume)
			return false;
		if (!has_hw_route)
			return true;
		return device_proxy != nullptr && have_route;
	}

	void MaybeBindSinkNode() noexcept;
	void ApplyCurrentVolume();
	void ApplyRouteVolume();

public:
	/*
	 * PipeWire event handlers (run on the loop's own thread). Public
	 * because the pw_*_events structs that reference them are built at
	 * file scope below, outside the class.
	 */
	static void OnCoreDone(void *data, uint32_t id, int seq) noexcept;
	static void OnCoreError(void *data, uint32_t id, int seq, int res,
				 const char *message) noexcept;
	static void OnGlobal(void *data, uint32_t id, uint32_t permissions,
			     const char *type, uint32_t version,
			     const struct spa_dict *props) noexcept;
	static void OnGlobalRemove(void *data, uint32_t id) noexcept;
	static int OnMetadataProperty(void *data, uint32_t subject,
				       const char *key, const char *type,
				       const char *value) noexcept;
	static void OnSinkNodeParam(void *data, int seq, uint32_t id,
				     uint32_t index, uint32_t next,
				     const struct spa_pod *param) noexcept;
	static void OnDeviceParam(void *data, int seq, uint32_t id,
				   uint32_t index, uint32_t next,
				   const struct spa_pod *param) noexcept;
};

static constexpr struct pw_core_events core_events = {
	.version = PW_VERSION_CORE_EVENTS,
	.done = PwSinkMixer::OnCoreDone,
	.error = PwSinkMixer::OnCoreError,
};

static constexpr struct pw_registry_events registry_events = {
	.version = PW_VERSION_REGISTRY_EVENTS,
	.global = PwSinkMixer::OnGlobal,
	.global_remove = PwSinkMixer::OnGlobalRemove,
};

static constexpr struct pw_metadata_events metadata_events = {
	.version = PW_VERSION_METADATA_EVENTS,
	.property = PwSinkMixer::OnMetadataProperty,
};

static constexpr struct pw_node_events sink_node_events = {
	.version = PW_VERSION_NODE_EVENTS,
	.param = PwSinkMixer::OnSinkNodeParam,
};

static constexpr struct pw_device_events device_events = {
	.version = PW_VERSION_DEVICE_EVENTS,
	.param = PwSinkMixer::OnDeviceParam,
};

void
PwSinkMixer::OnCoreDone(void *data, uint32_t id, int seq) noexcept
{
	auto &self = *static_cast<PwSinkMixer *>(data);
	if (id == PW_ID_CORE) {
		self.last_sync_seq = seq;
		pw_thread_loop_signal(self.loop, false);
	}
}

void
PwSinkMixer::OnCoreError(void *data, uint32_t id, int seq, int res,
			  const char *message) noexcept
{
	auto &self = *static_cast<PwSinkMixer *>(data);

	FmtError(pw_sink_mixer_domain,
		 "PipeWire core error: id={} seq={} res={} ({}): {}",
		 id, seq, res, strerror(-res), message != nullptr ? message : "");

	/*
	 * Record the fault and wake up anyone blocked in
	 * pw_thread_loop_wait() inside Roundtrip(): without this, a core
	 * error arriving mid-round-trip would leave the caller waiting
	 * until WaitUntilReady()'s fixed retry budget silently expired,
	 * rather than failing promptly.
	 */
	self.core_error = (res != 0) ? res : -EIO;
	pw_thread_loop_signal(self.loop, false);
}

void
PwSinkMixer::OnGlobal(void *data, uint32_t id,
		       [[maybe_unused]] uint32_t permissions,
		       const char *type, [[maybe_unused]] uint32_t version,
		       const struct spa_dict *props) noexcept
{
	auto &self = *static_cast<PwSinkMixer *>(data);

	if (props == nullptr)
		return;

	if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
		const char *name = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
		if (name != nullptr && std::strcmp(name, "default") == 0 &&
		    self.metadata_proxy == nullptr) {
			self.metadata_proxy = static_cast<struct pw_proxy *>(
				pw_registry_bind(self.registry, id, type,
						  PW_VERSION_METADATA, 0));
			pw_metadata_add_listener(
				reinterpret_cast<struct pw_metadata *>(self.metadata_proxy),
				&self.metadata_listener, &metadata_events, &self);
		}
	} else if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
		const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
		const char *node_name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
		if (media_class != nullptr && node_name != nullptr &&
		    std::strcmp(media_class, "Audio/Sink") == 0) {
			const char *device_id_str = spa_dict_lookup(props, PW_KEY_DEVICE_ID);
			uint32_t device_id = SPA_ID_INVALID;
			if (device_id_str != nullptr)
				device_id = static_cast<uint32_t>(std::strtoul(device_id_str, nullptr, 10));

			FmtDebug(pw_sink_mixer_domain,
				 "saw sink node id={} name={} device.id={}",
				 id, node_name, device_id);
			self.known_sinks.emplace(id, SinkInfo{node_name, device_id});
			self.MaybeBindSinkNode();
		}
	}
}

void
PwSinkMixer::OnGlobalRemove(void *data, uint32_t id) noexcept
{
	auto &self = *static_cast<PwSinkMixer *>(data);

	self.known_sinks.erase(id);

	if (id == self.sink_global_id) {
		/* the sink we were bound to disappeared (unplugged, etc.);
		   drop it, GetVolume()/SetVolume() will throw until Open()
		   is called again */
		if (self.sink_proxy != nullptr) {
			pw_proxy_destroy(self.sink_proxy);
			self.sink_proxy = nullptr;
		}
		self.sink_global_id = SPA_ID_INVALID;
		self.have_current_volume = false;
	}
}

int
PwSinkMixer::OnMetadataProperty(void *data, uint32_t subject,
				 const char *key, [[maybe_unused]] const char *type,
				 const char *value) noexcept
{
	auto &self = *static_cast<PwSinkMixer *>(data);

	if (subject != PW_ID_CORE || key == nullptr)
		return 0;

	/* prefer the effective default; fall back to the user-configured
	   one if that's all we have */
	if (std::strcmp(key, "default.audio.sink") == 0 ||
	    (self.target_name.empty() &&
	     std::strcmp(key, "default.configured.audio.sink") == 0)) {
		std::string name = ParseDefaultNodeName(value);
		if (!name.empty()) {
			self.target_name = name;
			FmtDebug(pw_sink_mixer_domain,
				 "resolved default sink target_name={}", name);
			self.MaybeBindSinkNode();
		}
	}

	return 0;
}

void
PwSinkMixer::OnSinkNodeParam(void *data, [[maybe_unused]] int seq,
			      uint32_t id,
			      [[maybe_unused]] uint32_t index,
			      [[maybe_unused]] uint32_t next,
			      const struct spa_pod *param) noexcept
{
	auto &self = *static_cast<PwSinkMixer *>(data);

	if (param == nullptr || !spa_pod_is_object(param))
		return;

	const auto *obj = reinterpret_cast<const struct spa_pod_object *>(param);
	const struct spa_pod_prop *prop;

	SPA_POD_OBJECT_FOREACH(obj, prop) {
		if (prop->key != SPA_PROP_channelVolumes)
			continue;

		uint32_t n_volumes = 0;
		const void *raw = spa_pod_get_array(&prop->value, &n_volumes);
		if (raw == nullptr || n_volumes == 0)
			return;

		n_volumes = std::min(n_volumes, kMaxChannels);
		std::copy_n(static_cast<const float *>(raw), n_volumes,
			    self.current_volumes);
		self.n_current_volumes = n_volumes;
		self.have_current_volume = true;
		FmtDebug(pw_sink_mixer_domain,
			 "read back volume for sink_global_id={} n_volumes={} volumes[0]={}",
			 id, n_volumes, self.current_volumes[0]);
		return;
	}
}

void
PwSinkMixer::OnDeviceParam(void *data, [[maybe_unused]] int seq,
			    [[maybe_unused]] uint32_t id,
			    [[maybe_unused]] uint32_t index,
			    [[maybe_unused]] uint32_t next,
			    const struct spa_pod *param) noexcept
{
	auto &self = *static_cast<PwSinkMixer *>(data);

	if (param == nullptr || !spa_pod_is_object(param))
		return;

	const auto *obj = reinterpret_cast<const struct spa_pod_object *>(param);
	if (obj->body.id != SPA_PARAM_Route)
		return;

	const struct spa_pod_prop *prop;
	int32_t found_index = -1, found_device = -1;
	uint32_t found_direction = 0;
	bool have_index = false, have_direction = false;
	float volumes[kMaxChannels];
	uint32_t n_volumes = 0;
	bool have_volumes = false;

	SPA_POD_OBJECT_FOREACH(obj, prop) {
		switch (prop->key) {
		case SPA_PARAM_ROUTE_index:
			if (spa_pod_get_int(&prop->value, &found_index) >= 0)
				have_index = true;
			break;
		case SPA_PARAM_ROUTE_device:
			spa_pod_get_int(&prop->value, &found_device);
			break;
		case SPA_PARAM_ROUTE_direction:
			if (spa_pod_get_id(&prop->value, &found_direction) >= 0)
				have_direction = true;
			break;
		case SPA_PARAM_ROUTE_props: {
			if (!spa_pod_is_object(&prop->value))
				break;
			const auto *props_obj = reinterpret_cast<
				const struct spa_pod_object *>(&prop->value);
			const struct spa_pod_prop *pp;
			SPA_POD_OBJECT_FOREACH(props_obj, pp) {
				if (pp->key != SPA_PROP_channelVolumes)
					continue;
				uint32_t n = 0;
				const void *raw = spa_pod_get_array(&pp->value, &n);
				if (raw != nullptr && n > 0) {
					n = std::min(n, kMaxChannels);
					std::copy_n(static_cast<const float *>(raw),
						    n, volumes);
					n_volumes = n;
					have_volumes = true;
				}
			}
			break;
		}
		default:
			break;
		}
	}

	/* only take the first Output-direction route we see -- see the
	   caveat on route_index et al. above for devices with more than
	   one selectable route */
	if (self.have_route || !have_index || !have_direction ||
	    found_direction != SPA_DIRECTION_OUTPUT)
		return;

	self.route_index = found_index;
	self.route_device = found_device;
	self.route_direction = found_direction;
	self.have_route = true;

	if (have_volumes) {
		std::copy_n(volumes, n_volumes, self.route_volumes);
		self.n_route_volumes = n_volumes;
	}

	FmtDebug(pw_sink_mixer_domain,
		 "resolved route index={} device={} direction={} volumes[0]={}",
		 self.route_index, self.route_device, self.route_direction,
		 have_volumes ? self.route_volumes[0] : -1.0f);
}

void
PwSinkMixer::MaybeBindSinkNode() noexcept
{
	if (sink_proxy != nullptr)
		return;

	const std::string &wanted = !configured_target.empty()
		? configured_target : target_name;
	if (wanted.empty())
		return;

	for (const auto &[id, info] : known_sinks) {
		if (info.name != wanted)
			continue;

		FmtDebug(pw_sink_mixer_domain,
			 "binding sink id={} name={} device.id={}",
			 id, info.name, info.device_id);

		sink_global_id = id;
		sink_proxy = static_cast<struct pw_proxy *>(
			pw_registry_bind(registry, id, PW_TYPE_INTERFACE_Node,
					  PW_VERSION_NODE, 0));
		pw_node_add_listener(reinterpret_cast<struct pw_node *>(sink_proxy),
				      &sink_node_listener, &sink_node_events, this);
		pw_node_enum_params(reinterpret_cast<struct pw_node *>(sink_proxy),
				     0, SPA_PARAM_Props, 0, UINT32_MAX, nullptr);

		/* the Node's own Props (above) reflect real-but-inert software
		   state for a hardware-routed sink; the object that actually
		   drives the audible/ALSA mixer-control volume is the parent
		   Device's active Route, so bind that too */
		if (info.device_id != SPA_ID_INVALID) {
			device_global_id = info.device_id;
			device_proxy = static_cast<struct pw_proxy *>(
				pw_registry_bind(registry, info.device_id,
						  PW_TYPE_INTERFACE_Device,
						  PW_VERSION_DEVICE, 0));
			pw_device_add_listener(
				reinterpret_cast<struct pw_device *>(device_proxy),
				&device_listener, &device_events, this);
			pw_device_enum_params(
				reinterpret_cast<struct pw_device *>(device_proxy),
				0, SPA_PARAM_Route, 0, UINT32_MAX, nullptr);
		} else {
			has_hw_route = false;
			FmtDebug(pw_sink_mixer_domain,
				 "sink node id={} has no device.id -- no hardware "
				 "Route available, only software Node volume "
				 "will be used", id);
		}
		return;
	}
}

bool
PwSinkMixer::Roundtrip() noexcept
{
	int seq = pw_core_sync(core, PW_ID_CORE, 0);
	while (last_sync_seq != seq) {
		if (core_error != 0)
			return false;
		pw_thread_loop_wait(loop);
	}

	return core_error == 0;
}

bool
PwSinkMixer::WaitUntilReady() noexcept
{
	for (int i = 0; i < 20 && !IsReady(); ++i)
		if (!Roundtrip())
			return false;

	return IsReady();
}

void
PwSinkMixer::Connect()
{
	static bool pw_initialized = false;
	if (!pw_initialized) {
		pw_init(nullptr, nullptr);
		pw_initialized = true;
	}

	core_error = 0;

	loop = pw_thread_loop_new("mpd-pwsink-mixer", nullptr);
	if (loop == nullptr)
		throw std::runtime_error("pw_thread_loop_new() failed");

	{
		const PwThreadLoopLock lock(loop);

		context = pw_context_new(
			pw_thread_loop_get_loop(loop),
			pw_properties_new(
				PW_KEY_MEDIA_CATEGORY, "Manager",
				PW_KEY_APP_NAME, "mpd-pwsink-mixer",
				nullptr),
			0);
		if (context == nullptr)
			throw std::runtime_error("pw_context_new() failed");

		if (pw_thread_loop_start(loop) < 0)
			throw std::runtime_error("pw_thread_loop_start() failed");

		core = pw_context_connect(context, nullptr, 0);
		if (core == nullptr)
			throw std::runtime_error("pw_context_connect() failed "
						  "(is PipeWire running?)");

		pw_core_add_listener(core, &core_listener, &core_events, this);

		registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
		if (registry == nullptr)
			throw std::runtime_error("pw_core_get_registry() failed");

		pw_registry_add_listener(registry, &registry_listener,
					  &registry_events, this);

		if (!configured_target.empty())
			/* skip metadata resolution entirely; MaybeBindSinkNode()
			   will match directly against configured_target as
			   registry globals arrive */
			MaybeBindSinkNode();

		if (!WaitUntilReady()) {
			if (core_error != 0)
				throw FmtRuntimeError(
					"PipeWire core error while resolving {} "
					"sink volume: {}",
					configured_target.empty()
						? "default" : configured_target.c_str(),
					strerror(-core_error));

			throw FmtRuntimeError(
				"timed out resolving PipeWire {} sink volume",
				configured_target.empty()
					? "default" : configured_target.c_str());
		}
	}
}

void
PwSinkMixer::Disconnect() noexcept
{
	if (loop == nullptr)
		return;

	{
		const PwThreadLoopLock lock(loop);

		if (sink_proxy != nullptr) {
			pw_proxy_destroy(sink_proxy);
			sink_proxy = nullptr;
		}

		if (device_proxy != nullptr) {
			pw_proxy_destroy(device_proxy);
			device_proxy = nullptr;
		}

		if (metadata_proxy != nullptr) {
			pw_proxy_destroy(metadata_proxy);
			metadata_proxy = nullptr;
		}

		if (registry != nullptr) {
			pw_proxy_destroy(reinterpret_cast<struct pw_proxy *>(registry));
			registry = nullptr;
		}

		if (core != nullptr) {
			pw_core_disconnect(core);
			core = nullptr;
		}
	}

	pw_thread_loop_stop(loop);

	if (context != nullptr) {
		pw_context_destroy(context);
		context = nullptr;
	}

	pw_thread_loop_destroy(loop);
	loop = nullptr;

	known_sinks.clear();
	sink_global_id = SPA_ID_INVALID;
	device_global_id = SPA_ID_INVALID;
	have_current_volume = false;
	have_route = false;
	has_hw_route = true;
	n_route_volumes = 0;
	target_name.clear();
	core_error = 0;
}

void
PwSinkMixer::Open()
{
	cached_volume = -1;

	Connect();

	/* pin the stream to unity so the sink-level volume is the only
	   gain stage in effect */
	pipewire_output_set_volume(output, 1.0f);
}

void
PwSinkMixer::Close() noexcept
{
	cached_volume = -1;

	Disconnect();
}

void
PwSinkMixer::ApplyCurrentVolume()
{
	uint32_t n = n_current_volumes > 0 ? n_current_volumes : 2;

	uint8_t buffer[512];
	struct spa_pod_builder b {};
	spa_pod_builder_init(&b, buffer, sizeof(buffer));

	struct spa_pod_frame obj_frame, array_frame;
	spa_pod_builder_push_object(&b, &obj_frame,
				     SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
	spa_pod_builder_prop(&b, SPA_PROP_channelVolumes, 0);
	spa_pod_builder_push_array(&b, &array_frame);
	for (uint32_t i = 0; i < n; ++i)
		spa_pod_builder_float(&b, current_volumes[i]);
	spa_pod_builder_pop(&b, &array_frame);
	const struct spa_pod *param = static_cast<const struct spa_pod *>(
		spa_pod_builder_pop(&b, &obj_frame));

	const PwThreadLoopLock lock(loop);
	pw_node_set_param(reinterpret_cast<struct pw_node *>(sink_proxy),
			   SPA_PARAM_Props, 0, param);
	Roundtrip();
}

void
PwSinkMixer::ApplyRouteVolume()
{
	if (device_proxy == nullptr || !have_route)
		return;

	uint32_t n = n_current_volumes > 0 ? n_current_volumes : 2;

	uint8_t buffer[1024];
	struct spa_pod_builder b {};
	spa_pod_builder_init(&b, buffer, sizeof(buffer));

	struct spa_pod_frame route_frame, props_frame, array_frame;
	spa_pod_builder_push_object(&b, &route_frame,
				     SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route);

	spa_pod_builder_prop(&b, SPA_PARAM_ROUTE_index, 0);
	spa_pod_builder_int(&b, route_index);

	spa_pod_builder_prop(&b, SPA_PARAM_ROUTE_device, 0);
	spa_pod_builder_int(&b, route_device);

	spa_pod_builder_prop(&b, SPA_PARAM_ROUTE_props, 0);
	spa_pod_builder_push_object(&b, &props_frame,
				     SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
	spa_pod_builder_prop(&b, SPA_PROP_channelVolumes, 0);
	spa_pod_builder_push_array(&b, &array_frame);
	for (uint32_t i = 0; i < n; ++i)
		spa_pod_builder_float(&b, current_volumes[i]);
	spa_pod_builder_pop(&b, &array_frame);
	spa_pod_builder_pop(&b, &props_frame);

	spa_pod_builder_prop(&b, SPA_PARAM_ROUTE_save, 0);
	spa_pod_builder_bool(&b, true);

	const struct spa_pod *param = static_cast<const struct spa_pod *>(
		spa_pod_builder_pop(&b, &route_frame));

	const PwThreadLoopLock lock(loop);
	pw_device_set_param(reinterpret_cast<struct pw_device *>(device_proxy),
			     SPA_PARAM_Route, 0, param);
	Roundtrip();
}

int
PwSinkMixer::GetVolume()
{
	if (sink_proxy == nullptr)
		throw std::runtime_error("PipeWire sink not connected");

	if (cached_volume >= 0)
		return cached_volume;

	if (have_route && n_route_volumes > 0)
		return static_cast<int>(VolumeToPercent(route_volumes[0], curve));

	if (!have_current_volume)
		throw std::runtime_error("PipeWire sink volume not known yet");

	return static_cast<int>(VolumeToPercent(current_volumes[0], curve));
}

void
PwSinkMixer::SetVolume(unsigned volume)
{
	if (sink_proxy == nullptr)
		throw std::runtime_error("PipeWire sink not connected");

	float v = PercentToVolume(volume, curve);
	uint32_t n = n_current_volumes > 0 ? n_current_volumes : 2;
	std::fill_n(current_volumes, n, v);
	n_current_volumes = n;

	FmtDebug(pw_sink_mixer_domain,
		 "SetVolume({}) -> sink_global_id={} n={} v={}",
		 volume, sink_global_id, n, v);

	ApplyCurrentVolume();
	ApplyRouteVolume();
	cached_volume = static_cast<int>(volume);

	/*
	 * Re-pin the stream to unity on every SetVolume() call too, not
	 * just in Open(): a quality/format change can cause the output
	 * plugin to recreate its pw_stream, which would otherwise leave a
	 * fresh stream instance at PipeWire's own default volume until the
	 * next explicit SetVolume() call.
	 */
	pipewire_output_set_volume(output, 1.0f);
}

static Mixer *
pw_sink_mixer_init([[maybe_unused]] EventLoop &event_loop, AudioOutput &ao,
		    MixerListener &listener, const ConfigBlock &block)
{
	auto &po = (PipeWireOutput &)ao;
	auto *mixer = new PwSinkMixer(po, listener);
	mixer->Configure(block);
	return mixer;
}

const MixerPlugin pw_sink_mixer_plugin = {
	pw_sink_mixer_init,
	true,
};
