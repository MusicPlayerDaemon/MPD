// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Bridge.hxx"
#include "org_musicpd_Bridge.h"
#include "Context.hxx"
#include "Environment.hxx"
#include "java/Global.hxx"
#include "java/File.hxx"
#include "thread/Mutex.hxx"
#include "util/Compiler.h" // for gcc_visibility_default
#include "util/ScopeExit.hxx"
#include "Main.hxx"
#include "Instance.hxx"
#include "Log.hxx"
#include "Partition.hxx"

/**
 * Protects #android_control.
 */
static Mutex android_control_mutex;

/**
 * The #AndroidControl for the JNI functions; it is cleared (while
 * holding #android_control_mutex) before the #Instance is destroyed.
 */
static AndroidControl *android_control;

AndroidControl::AndroidControl(Instance &_instance) noexcept
	:instance(_instance),
	 inject_event(instance.event_loop, BIND_THIS_METHOD(OnInject))
{
	const std::lock_guard lock{android_control_mutex};
	android_control = this;
}

AndroidControl::~AndroidControl() noexcept
{
	const std::lock_guard lock{android_control_mutex};
	android_control = nullptr;
}

void
AndroidControl::Shutdown() noexcept
{
	instance.event_loop.InjectBreak();
}

void
AndroidControl::OnInject() noexcept
{
	if (pause.exchange(false))
		for (auto &partition : instance.partitions)
			partition.pc.LockSetPause(true);

	if (play_pause.exchange(0) % 2 != 0)
		for (auto &partition : instance.partitions)
			partition.pc.LockPause();

	if (next.exchange(false))
		for (auto &partition : instance.partitions)
			if (partition.playlist.playing)
				partition.PlayNext();

	if (previous.exchange(false))
		for (auto &partition : instance.partitions)
			if (partition.playlist.playing)
				partition.PlayPrevious();
}

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
	const std::lock_guard lock{android_control_mutex};
	if (android_control != nullptr)
		android_control->Shutdown();
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_pause(JNIEnv *, jclass)
{
	const std::lock_guard lock{android_control_mutex};
	if (android_control != nullptr)
		android_control->Pause();
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_playPause(JNIEnv *, jclass)
{
	const std::lock_guard lock{android_control_mutex};
	if (android_control != nullptr)
		android_control->PlayPause();
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_playNext(JNIEnv *, jclass)
{
	const std::lock_guard lock{android_control_mutex};
	if (android_control != nullptr)
		android_control->PlayNext();
}

gcc_visibility_default
JNIEXPORT void JNICALL
Java_org_musicpd_Bridge_playPrevious(JNIEnv *, jclass)
{
	const std::lock_guard lock{android_control_mutex};
	if (android_control != nullptr)
		android_control->PlayPrevious();
}
