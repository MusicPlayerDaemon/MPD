#include "Song.hxx"
#include "java/Class.hxx"
#include "tag/Names.hxx"

jobject song_to_song_info(JNIEnv *env, std::unique_ptr<DetachedSong> &song) {
    jobject tag_map = song_to_tag_hashmap(env, song);

    Java::Class cls(env, "org/musicpd/models/SongInfo");
    jmethodID init = env->GetMethodID(cls, "<init>", "()V");
    jobject song_info = env->NewObject(cls, init);

    jstring uri = env->NewStringUTF(song->GetURI());
    jfieldID id_uri = env->GetFieldID(cls, "uri", "Ljava/lang/String;");
    env->SetObjectField(song_info, id_uri, uri);
    env->DeleteLocalRef(uri);

    const auto duration = song->GetDuration();
    jfieldID id_duration = env->GetFieldID(cls, "durationMilliseconds", "I");
    env->SetIntField(song_info, id_duration, duration.ToMS());

    jfieldID id_tags = env->GetFieldID(cls, "tags", "Ljava/util/HashMap;");
    env->SetObjectField(song_info, id_tags, tag_map);
    env->DeleteLocalRef(tag_map);

    return song_info;
}

jobject song_to_tag_hashmap(JNIEnv *env, std::unique_ptr<DetachedSong> &song) {
    Java::Class cls(env, "java/util/HashMap");
    jmethodID init = env->GetMethodID(cls, "<init>", "()V");
    jobject hash_map = env->NewObject(cls, init);
    jmethodID put = env->GetMethodID(cls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

    const Tag &tag = song->GetTag();
    for (const auto &i : tag) {
        jstring key = env->NewStringUTF(tag_item_names[i.type]);
        jstring value = env->NewStringUTF(i.value);

        env->CallObjectMethod(hash_map, put, key, value);

        env->DeleteLocalRef(key);
        env->DeleteLocalRef(value);
    }

    return hash_map;
}