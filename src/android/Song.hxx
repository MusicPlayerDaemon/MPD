#include "song/DetachedSong.hxx"

#include "java/Object.hxx"

jobject song_to_song_info(JNIEnv *env, std::unique_ptr<DetachedSong> &song);

jobject song_to_tag_hashmap(JNIEnv *env, std::unique_ptr<DetachedSong> &song);