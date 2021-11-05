/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "CommandLine.hxx"
#include "GitVersion.hxx"
#include "ls.hxx"
#include "LogInit.hxx"
#include "Log.hxx"
#include "config/File.hxx"
#include "decoder/DecoderList.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "output/Registry.hxx"
#include "output/OutputPlugin.hxx"
#include "input/Registry.hxx"
#include "input/InputPlugin.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/PlaylistPlugin.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/StandardDirectory.hxx"
#include "event/Features.h"
#include "io/uring/Features.h"
#include "util/Domain.hxx"
#include "util/OptionDef.hxx"
#include "util/OptionParser.hxx"
#include "Version.h"

#ifdef _WIN32
#include "system/Error.hxx"
#endif

#ifdef ENABLE_DATABASE
#include "db/Registry.hxx"
#include "db/DatabasePlugin.hxx"
#include "storage/Registry.hxx"
#include "storage/StoragePlugin.hxx"
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
#include "neighbor/Registry.hxx"
#include "neighbor/NeighborPlugin.hxx"
#endif

#include "encoder/Features.h"
#ifdef ENABLE_ENCODER
#include "encoder/EncoderList.hxx"
#include "encoder/EncoderPlugin.hxx"
#endif

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#endif

namespace {
#ifdef _WIN32
constexpr auto CONFIG_FILE_LOCATION = Path::FromFS(PATH_LITERAL("mpd\\mpd.conf"));
constexpr auto APP_CONFIG_FILE_LOCATION = Path::FromFS(PATH_LITERAL("conf\\mpd.conf"));
#else
constexpr auto USER_CONFIG_FILE_LOCATION1 = Path::FromFS(PATH_LITERAL(".mpdconf"));
constexpr auto USER_CONFIG_FILE_LOCATION2 = Path::FromFS(PATH_LITERAL(".mpd/mpd.conf"));
constexpr auto USER_CONFIG_FILE_LOCATION_XDG = Path::FromFS(PATH_LITERAL("mpd/mpd.conf"));
#endif
} // namespace

enum Option {
	OPTION_KILL,
	OPTION_NO_CONFIG,
	OPTION_NO_DAEMON,
#ifdef __linux__
	OPTION_SYSTEMD,
#endif
	OPTION_STDOUT,
	OPTION_STDERR,
	OPTION_VERBOSE,
	OPTION_VERSION,
	OPTION_HELP,
	OPTION_HELP2,
};

static constexpr OptionDef option_defs[] = {
	{"kill", "kill the currently running mpd session"},
	{"no-config", "don't read from config"},
	{"no-daemon", "don't detach from console"},
#ifdef __linux__
	{"systemd", "systemd service mode"},
#endif
	{"stdout", nullptr}, // hidden, compatibility with old versions
	{"stderr", "print messages to stderr"},
	{"verbose", 'v', "verbose logging"},
	{"version", 'V', "print version number"},
	{"help", 'h', "show help options"},
	{nullptr, '?', nullptr}, // hidden, standard alias for --help
};

static constexpr Domain cmdline_domain("cmdline");

[[noreturn]]
static void version()
{
	printf("Music Player Daemon " VERSION " (%s)"
	       "\n"
	       "Copyright 2003-2007 Warren Dukes <warren.dukes@gmail.com>\n"
	       "Copyright 2008-2021 Max Kellermann <max.kellermann@gmail.com>\n"
	       "This is free software; see the source for copying conditions.  There is NO\n"
	       "warranty; not even MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
	       GIT_VERSION);

#ifdef ENABLE_DATABASE
	printf("\n"
	       "Database plugins:\n");

	for (auto i = database_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);

	printf("\n\n"
	       "Storage plugins:\n");

	for (auto i = storage_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);

	printf("\n");
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
	printf("\n"
	       "Neighbor plugins:\n");
	for (auto i = neighbor_plugins; *i != nullptr; ++i)
		printf(" %s", (*i)->name);

#endif

	printf("\n"
	       "\n"
	       "Decoders plugins:\n");

	decoder_plugins_for_each([](const DecoderPlugin &plugin){
		printf(" [%s]", plugin.name);

		const char *const*suffixes = plugin.suffixes;
		if (suffixes != nullptr)
			for (; *suffixes != nullptr; ++suffixes)
				printf(" %s", *suffixes);

		if (plugin.protocols != nullptr)
			for (const auto &i : plugin.protocols())
				printf(" %s", i.c_str());

		printf("\n");
	});

	printf("\n"
	       "Filters:\n"
#ifdef ENABLE_LIBSAMPLERATE
	       " libsamplerate"
#endif
#ifdef ENABLE_SOXR
	       " soxr"
#endif
	       "\n\n"
	       "Tag plugins:\n"
#ifdef ENABLE_ID3TAG
	       " id3tag"
#endif
	       "\n\n"
	       "Output plugins:\n");
	audio_output_plugins_for_each(plugin)
		printf(" %s", plugin->name);
	printf("\n"

#ifdef ENABLE_ENCODER
	       "\n"
	       "Encoder plugins:\n");
	encoder_plugins_for_each(plugin)
		printf(" %s", plugin->name);
	printf("\n"
#endif

#ifdef ENABLE_ARCHIVE
	       "\n"
	       "Archive plugins:\n");
	archive_plugins_for_each(plugin) {
		printf(" [%s]", plugin->name);

		const char *const*suffixes = plugin->suffixes;
		if (suffixes != nullptr)
			for (; *suffixes != nullptr; ++suffixes)
				printf(" %s", *suffixes);

		printf("\n");
	}

	printf(""
#endif

	       "\n"
	       "Input plugins:\n"
	       " file"
#ifdef HAVE_URING
	       " io_uring"
#endif
#ifdef ENABLE_ARCHIVE
	       " archive"
#endif
	       );
	input_plugins_for_each(plugin)
		printf(" %s", plugin->name);

	printf("\n\n"
	       "Playlist plugins:\n");
	playlist_plugins_for_each(plugin)
		printf(" %s", plugin->name);

	printf("\n\n"
	       "Protocols:\n");
	print_supported_uri_schemes_to_fp(stdout);

	printf("\n"
	       "Other features:\n"
#ifdef HAVE_AVAHI
	       " avahi"
#endif
#ifdef ENABLE_DBUS
	       " dbus"
#endif
#ifdef ENABLE_UDISKS
	       " udisks"
#endif
#ifdef USE_EPOLL
	       " epoll"
#endif
#ifdef HAVE_ICONV
	       " iconv"
#endif
#ifdef HAVE_ICU
	       " icu"
#endif
#ifdef ENABLE_INOTIFY
	       " inotify"
#endif
#ifdef HAVE_IPV6
	       " ipv6"
#endif
#ifdef ENABLE_SYSTEMD_DAEMON
	       " systemd"
#endif
#ifdef HAVE_TCP
	       " tcp"
#endif
#ifdef HAVE_UN
	       " un"
#endif
	       "\n");

	std::exit(EXIT_SUCCESS);
}

static void PrintOption(const OptionDef &opt)
{
	if (opt.HasShortOption())
		printf("  -%c, --%-12s%s\n",
		       opt.GetShortOption(),
		       opt.GetLongOption(),
		       opt.GetDescription());
	else
		printf("  --%-16s%s\n",
		       opt.GetLongOption(),
		       opt.GetDescription());
}

[[noreturn]]
static void help()
{
	printf("Usage:\n"
	       "  mpd [OPTION...] [path/to/mpd.conf]\n"
	       "\n"
	       "Music Player Daemon - a daemon for playing music.\n"
	       "\n"
	       "Options:\n");

	for (const auto &i : option_defs)
		if(i.HasDescription()) // hide hidden options from help print
			PrintOption(i);

	std::exit(EXIT_SUCCESS);
}

class ConfigLoader
{
	ConfigData &config;

public:
	explicit ConfigLoader(ConfigData &_config) noexcept
		:config(_config) {}

	bool TryFile(Path path);
	bool TryFile(const AllocatedPath &base_path, Path path);
};

bool ConfigLoader::TryFile(Path path)
{
	if (FileExists(path)) {
		ReadConfigFile(config, path);
		return true;
	}
	return false;
}

bool ConfigLoader::TryFile(const AllocatedPath &base_path, Path path)
{
	if (base_path.IsNull())
		return false;
	auto full_path = base_path / path;
	return TryFile(full_path);
}

void
ParseCommandLine(int argc, char **argv, CommandLineOptions &options,
		 ConfigData &config)
{
	bool use_config_file = true;

	// First pass: handle command line options
	OptionParser parser(option_defs, argc, argv);
	while (auto o = parser.Next()) {
		switch (Option(o.index)) {
		case OPTION_KILL:
			options.kill = true;
			break;

		case OPTION_NO_CONFIG:
			use_config_file = false;
			break;

		case OPTION_NO_DAEMON:
			options.daemon = false;
			break;

#ifdef __linux__
		case OPTION_SYSTEMD:
			options.daemon = false;
			options.systemd = true;
			break;
#endif

		case OPTION_STDOUT:
		case OPTION_STDERR:
			options.log_stderr = true;
			break;

		case OPTION_VERBOSE:
			options.verbose = true;
			break;

		case OPTION_VERSION:
			version();

		case OPTION_HELP:
		case OPTION_HELP2:
			help();
		}
	}

	/* initialize the logging library, so the configuration file
	   parser can use it already */
	log_early_init(options.verbose);

	if (!use_config_file) {
		LogDebug(cmdline_domain,
			 "Ignoring config, using daemon defaults");
		return;
	}

	// Second pass: find non-option parameters (i.e. config file)
	const char *config_file = nullptr;
	for (const char *i : parser.GetRemaining()) {
		if (config_file == nullptr) {
			config_file = i;
			continue;
		}

		throw std::runtime_error("too many arguments");
	}

	if (config_file != nullptr) {
		/* use specified configuration file */
		ReadConfigFile(config, FromNarrowPath(config_file));
		return;
	}

	/* use default configuration file path */

	ConfigLoader loader(config);

	bool found =
#ifdef _WIN32
		loader.TryFile(GetUserConfigDir(), CONFIG_FILE_LOCATION) ||
		loader.TryFile(GetSystemConfigDir(), CONFIG_FILE_LOCATION) ||
		loader.TryFile(GetAppBaseDir(), APP_CONFIG_FILE_LOCATION);
#else
		loader.TryFile(GetUserConfigDir(),
			       USER_CONFIG_FILE_LOCATION_XDG) ||
		loader.TryFile(GetHomeDir(), USER_CONFIG_FILE_LOCATION1) ||
		loader.TryFile(GetHomeDir(), USER_CONFIG_FILE_LOCATION2) ||
		loader.TryFile(Path::FromFS(SYSTEM_CONFIG_FILE_LOCATION));
#endif
	if (!found)
		throw std::runtime_error("No configuration file found");
}
