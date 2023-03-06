// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef SLES_ENGINE_HPP
#define SLES_ENGINE_HPP

#include <SLES/OpenSLES.h>

namespace SLES {
	/**
	 * OO wrapper for an OpenSL/ES SLEngineItf variable.
	 */
	class Engine {
		SLEngineItf engine;

	public:
		Engine() = default;
		explicit Engine(SLEngineItf _engine):engine(_engine) {}

		SLresult CreateAudioPlayer(SLObjectItf *pPlayer,
					   SLDataSource *pAudioSrc, SLDataSink *pAudioSnk,
					   SLuint32 numInterfaces,
					   const SLInterfaceID *pInterfaceIds,
					   const SLboolean *pInterfaceRequired) {
			return (*engine)->CreateAudioPlayer(engine, pPlayer,
							    pAudioSrc, pAudioSnk,
							    numInterfaces, pInterfaceIds,
							    pInterfaceRequired);
		}

		SLresult CreateOutputMix(SLObjectItf *pMix,
					 SLuint32 numInterfaces,
					 const SLInterfaceID *pInterfaceIds,
					 const SLboolean *pInterfaceRequired) {
			return (*engine)->CreateOutputMix(engine, pMix,
							  numInterfaces, pInterfaceIds,
							  pInterfaceRequired);
		}
	};
}

#endif
