/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "test_pcm_all.hxx"
#include "Compiler.h"

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include <stdlib.h>

CPPUNIT_TEST_SUITE_REGISTRATION(PcmDitherTest);
CPPUNIT_TEST_SUITE_REGISTRATION(PcmPackTest);
CPPUNIT_TEST_SUITE_REGISTRATION(PcmChannelsTest);
CPPUNIT_TEST_SUITE_REGISTRATION(PcmVolumeTest);
CPPUNIT_TEST_SUITE_REGISTRATION(PcmFormatTest);
CPPUNIT_TEST_SUITE_REGISTRATION(PcmMixTest);
CPPUNIT_TEST_SUITE_REGISTRATION(PcmExportTest);

int
main(gcc_unused int argc, gcc_unused char **argv)
{
	CppUnit::TextUi::TestRunner runner;
	auto &registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());
	return runner.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
