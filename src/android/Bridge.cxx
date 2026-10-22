// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "org_musicpd_Bridge.h"
#include "Context.hxx"
#include "Environment.hxx"
#include "java/Global.hxx"
#include "java/File.hxx"
#include "event/Call.hxx"
#include "util/Compiler.h" // for gcc_visibility_default
#include "util/ScopeExit.hxx"
#include "Main.hxx"
#include "Instance.hxx"
#include "Log.hxx"
#include "Partition.hxx"

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_run(JNIEnv *env, jclass, jobject _context, jobject _logListener)
{
	Java::Init(env);
	Java::Object::Initialise(env);
	Java::File::Initialise(env);

	Environment::Initialise(env);
	AtScopeExit(env) { Environment::Deinitialise(env); };

	Context::Initialise(env);

	context = new Context(env, _context);
	AtScopeExit() { delete context; };

	if (_logListener != nullptr)
		logListener = new LogListener(env, _logListener);
	AtScopeExit() { delete logListener; };

	try {
		AndroidMain(env);
	} catch (...) {
		LogError(std::current_exception());
	}
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_shutdown(JNIEnv *, jclass)
{
	if (global_instance != nullptr)
		global_instance->event_loop.InjectBreak();
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_pause(JNIEnv *, jclass)
{
	if (global_instance != nullptr)
		for (auto &partition : global_instance->partitions)
			partition.pc.LockSetPause(true);
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_playPause(JNIEnv *, jclass)
{
	if (global_instance != nullptr)
		for (auto &partition : global_instance->partitions)
			partition.pc.LockPause();

}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_playNext(JNIEnv *, jclass)
{
	if (global_instance != nullptr)
		BlockingCall(global_instance->event_loop, [&](){
			for (auto &partition : global_instance->partitions)
				if (partition.playlist.playing) {
					partition.PlayNext();
				}
		});
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_playPrevious(JNIEnv *, jclass)
{
	if (global_instance != nullptr)
		BlockingCall(global_instance->event_loop, [&](){
			for (auto &partition : global_instance->partitions)
				if (partition.playlist.playing) {
					partition.PlayPrevious();
				}
		});
}
