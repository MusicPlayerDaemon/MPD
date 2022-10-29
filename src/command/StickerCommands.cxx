// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StickerCommands.hxx"
#include "Request.hxx"
#include "SongPrint.hxx"
#include "db/Interface.hxx"
#include "sticker/Sticker.hxx"
#include "sticker/TagSticker.hxx"
#include "sticker/Database.hxx"
#include "sticker/SongSticker.hxx"
#include "sticker/AllowedTags.hxx"
#include "sticker/Print.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "util/StringAPI.hxx"
#include "util/ScopeExit.hxx"
#include "tag/ParseName.hxx"
#include "tag/Names.hxx"
#include "sticker/TagSticker.hxx"
#include "song/LightSong.hxx"
#include "PlaylistFile.hxx"
#include "db/PlaylistInfo.hxx"
#include "db/PlaylistVector.hxx"
#include "db/DatabaseLock.hxx"
#include "song/Filter.hxx"

namespace {

class DomainHandler {
public:
	virtual ~DomainHandler() = default;

	virtual CommandResult Get(const char *uri, const char *name) {
		const auto value = sticker_database.LoadValue(sticker_type,
							      ValidateUri(uri).c_str(),
							      name);
		if (value.empty()) {
			response.FmtError(ACK_ERROR_NO_EXIST, "no such sticker: \"{}\"", name);
			return CommandResult::ERROR;
		}

		sticker_print_value(response, name, value.c_str());

		return CommandResult::OK;
	}

	virtual CommandResult Set(const char *uri, const char *name, const char *value) {
		sticker_database.StoreValue(sticker_type,
					    ValidateUri(uri).c_str(),
					    name,
					    value);

		return CommandResult::OK;
	}

	virtual CommandResult Delete(const char *uri, const char *name) {
		std::string validated_uri = ValidateUri(uri);
		uri = validated_uri.c_str();
		bool ret = name == nullptr
			   ? sticker_database.Delete(sticker_type, uri)
			   : sticker_database.DeleteValue(sticker_type, uri, name);
		if (!ret) {
			response.FmtError(ACK_ERROR_NO_EXIST, "no such sticker: \"{}\"", name);
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	}

	virtual CommandResult List(const char *uri) {
		const auto sticker = sticker_database.Load(sticker_type,
							   ValidateUri(uri).c_str());
		sticker_print(response, sticker);

		return CommandResult::OK;
	}

	virtual CommandResult Find(const char *uri, const char *name, StickerOperator op, const char *value) {
		auto data = CallbackContext{
			.name = name,
			.sticker_type = sticker_type,
			.response = response,
			.is_song = StringIsEqual("song", sticker_type)
		};

		auto callback = [](const char *found_uri, const char *found_value, void *user_data) {
			auto context = reinterpret_cast<CallbackContext *>(user_data);
			context->response.Fmt("{}: {}\n",
 					      context->is_song ? "file" : context->sticker_type, found_uri);
			sticker_print_value(context->response, context->name, found_value);
		};

		sticker_database.Find(sticker_type,
				      uri,
				      name,
				      op, value,
				      callback, &data);

		return CommandResult::OK;
	}

protected:
	DomainHandler(Response &_response,
		      const Database &_database,
		      StickerDatabase &_sticker_database,
		      const char *_sticker_type) :
		sticker_type(_sticker_type),
		response(_response),
		database(_database),
		sticker_database(_sticker_database) {
	}

	/**
	 * Validate the command uri or throw std::runtime_error if not valid.
	 *
	 * @param uri the uri from the sticker command
	 *
	 * @return the uri to use in the sticker database query
	 */
	virtual std::string ValidateUri(const char *uri) {
		return {uri};
	}

	const char *const sticker_type;
	Response &response;
	const Database &database;
	StickerDatabase &sticker_database;

private:
	struct CallbackContext {
		const char *const name;
		const char *const sticker_type;
		Response &response;
		const bool is_song;
	};
};

/**
 * 'song' stickers handler
 */
class SongHandler final : public DomainHandler {
public:
	SongHandler(Response &_response,
		    const Database &_database,
		    StickerDatabase &_sticker_database) :
		DomainHandler(_response, _database, _sticker_database, "song") {
	}

	~SongHandler() override {
		if (song != nullptr)
			database.ReturnSong(song);
	}

	CommandResult Find(const char *uri, const char *name, StickerOperator op, const char *value) override {
		struct sticker_song_find_data data = {
			response,
			name,
		};

		sticker_song_find(sticker_database, database, uri, data.name,
				  op, value,
				  sticker_song_find_print_cb, &data);

		return CommandResult::OK;
	}

protected:
	std::string ValidateUri(const char *uri) override {
		// will throw if song uri not found
		song = database.GetSong(uri);
		assert(song != nullptr);
		return song->GetURI();
	}

private:
	struct sticker_song_find_data {
		Response &r;
		const char *name;
	};

	static void
	sticker_song_find_print_cb(const LightSong &song, const char *value,
				   void *user_data)
	{
		auto *data = (struct sticker_song_find_data *)user_data;

		song_print_uri(data->r, song);
		sticker_print_value(data->r, data->name, value);
	}

	const LightSong* song = nullptr;
};

/**
 * Base for Tag and Filter handlers
 */
class SelectionHandler : public DomainHandler {
protected:
	SelectionHandler(Response &_response,
		   const Database &_database,
		   StickerDatabase &_sticker_database,
		   const char* _sticker_type) :
		DomainHandler(_response, _database, _sticker_database, _sticker_type) {
	}
};

/**
 * Tag type stickers handler
 */
class TagHandler : public SelectionHandler {
public:
	TagHandler(Response &_response,
		   const Database &_database,
		   StickerDatabase &_sticker_database,
		   TagType _tag_type) :
		SelectionHandler(_response, _database, _sticker_database, tag_item_names[_tag_type]),
		tag_type(_tag_type) {

		assert(tag_type != TAG_NUM_OF_ITEM_TYPES);
	}

protected:
	std::string ValidateUri(const char *uri) override {
		if (tag_type == TAG_NUM_OF_ITEM_TYPES)
			throw std::invalid_argument(fmt::format("no such tag: \"{}\"", sticker_type));

		if (!sticker_allowed_tags.Test(tag_type))
			throw std::invalid_argument(fmt::format("unsupported tag: \"{}\"", sticker_type));

		if (!TagExists(database, tag_type, uri))
			throw std::invalid_argument(fmt::format("no such {}: \"{}\"", sticker_type, uri));

		return {uri};
	}

private:
	const TagType tag_type;
};

/**
 * Filter stickers handler
 *
 * The URI is parsed as a SongFilter
 */
class FilterHandler : public SelectionHandler {
public:
	FilterHandler(Response &_response,
		   const Database &_database,
		   StickerDatabase &_sticker_database) :
		SelectionHandler(_response, _database, _sticker_database, "filter") {
	}

protected:
	std::string ValidateUri(const char *uri) override {

		auto filter = MakeSongFilter(uri);

		auto normalized = filter.ToExpression();

		if (!FilterMatches(database, filter))
			throw std::invalid_argument(fmt::format("no matches found: \"{}\"", normalized));

		return normalized;
	}
};

/**
 * playlist stickers handler
 */
class PlaylistHandler : public DomainHandler {
public:
	PlaylistHandler(Response &_response,
			const Database &_database,
			StickerDatabase &_sticker_database) :
		DomainHandler(_response, _database, _sticker_database, "playlist") {
	}

private:
	std::string ValidateUri(const char *uri) override {
		PlaylistVector playlists = ListPlaylistFiles();

		const ScopeDatabaseLock protect;
		if (!playlists.exists(uri))
			throw std::invalid_argument(fmt::format("no such playlist: \"{}\"", uri));

		return {uri};
	}
};

} // namespace

CommandResult
handle_sticker(Client &client, Request args, Response &r)
{
	// must be enforced by the caller
	assert(args.size() >= 3);

	auto &instance = client.GetInstance();
	if (!instance.HasStickerDatabase()) {
		r.Error(ACK_ERROR_UNKNOWN, "sticker database is disabled");
		return CommandResult::ERROR;
	}

	auto &db = client.GetPartition().GetDatabaseOrThrow();
	auto &sticker_database = *instance.sticker_database;

	auto cmd = args.front();
	auto sticker_type = args[1];
	auto uri = args[2];
	auto sticker_name = args.GetOptional(3);

	std::unique_ptr<DomainHandler> handler;

	if (StringIsEqual(sticker_type, "song"))
		handler = std::make_unique<SongHandler>(r, db, sticker_database);

	else if (StringIsEqual(sticker_type, "playlist"))
		handler = std::make_unique<PlaylistHandler>(r, db, sticker_database);

	else if (StringIsEqual(sticker_type, "filter"))
		handler = std::make_unique<FilterHandler>(r, db, sticker_database);

		// allow tags in the command to be case insensitive
	// the handler will normalize the tag name with tag_item_names()
	else if (auto tag_type = tag_name_parse_i(sticker_type); tag_type != TAG_NUM_OF_ITEM_TYPES)
		handler = std::make_unique<TagHandler>(r, db, sticker_database, tag_type);

	else {
		r.FmtError(ACK_ERROR_ARG, "unknown sticker domain \"{}\"", sticker_type);
		return CommandResult::ERROR;
	}

	/* get */
	if (args.size() == 4 && StringIsEqual(cmd, "get"))
		return handler->Get(uri, sticker_name);

	/* list */
	if (args.size() == 3 && StringIsEqual(cmd, "list"))
		return handler->List(uri);

	/* set */
	if (args.size() == 5 && StringIsEqual(cmd, "set"))
		return handler->Set(uri, sticker_name, args[4]);

	/* delete */
	if ((args.size() == 3 || args.size() == 4) && StringIsEqual(cmd, "delete"))
		return handler->Delete(uri, sticker_name);

	/* find */
	if ((args.size() == 4 || args.size() == 6) && StringIsEqual(cmd, "find")) {
		bool has_op = args.size() > 4;
		auto value = has_op ? args[5] : nullptr;
		StickerOperator op = StickerOperator::EXISTS;
		if (has_op) {
			/* match the value */
			auto op_s = args[4];
			if (StringIsEqual(op_s, "="))
				op = StickerOperator::EQUALS;
			else if (StringIsEqual(op_s, "<"))
				op = StickerOperator::LESS_THAN;
			else if (StringIsEqual(op_s, ">"))
				op = StickerOperator::GREATER_THAN;
			else {
				r.FmtError(ACK_ERROR_ARG, "bad operator \"{}\"", op_s);
				return CommandResult::ERROR;
			}
		}
		return handler->Find(uri, sticker_name, op, value);
	}

	r.Error(ACK_ERROR_ARG, "bad request");
	return CommandResult::ERROR;
}
