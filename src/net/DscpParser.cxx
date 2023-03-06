// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "DscpParser.hxx"
#include "util/StringAPI.hxx"

#include <cstdlib>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netinet/ip.h>
#endif

#ifndef IPTOS_DSCP_AF11
#define IPTOS_DSCP_AF11 0x28
#define IPTOS_DSCP_AF12 0x30
#define IPTOS_DSCP_AF13 0x38
#define IPTOS_DSCP_AF21 0x48
#define IPTOS_DSCP_AF22 0x50
#define IPTOS_DSCP_AF23 0x58
#define IPTOS_DSCP_AF31 0x68
#define IPTOS_DSCP_AF32 0x70
#define IPTOS_DSCP_AF33 0x78
#define IPTOS_DSCP_AF41 0x88
#define IPTOS_DSCP_AF42 0x90
#define IPTOS_DSCP_AF43 0x98
#endif

#ifndef IPTOS_DSCP_CS0
#define IPTOS_DSCP_CS0 0x00
#define IPTOS_DSCP_CS1 0x20
#define IPTOS_DSCP_CS2 0x40
#define IPTOS_DSCP_CS3 0x60
#define IPTOS_DSCP_CS4 0x80
#define IPTOS_DSCP_CS5 0xa0
#define IPTOS_DSCP_CS6 0xc0
#define IPTOS_DSCP_CS7 0xe0
#endif

#ifndef IPTOS_DSCP_EF
#define IPTOS_DSCP_EF 0xb8
#endif

#ifndef IPTOS_DSCP_LE
#define IPTOS_DSCP_LE 0x04
#endif

static constexpr struct {
	const char *name;
	int value;
} dscp_classes[] = {
	{ "AF11", IPTOS_DSCP_AF11 },
	{ "AF12", IPTOS_DSCP_AF12 },
	{ "AF13", IPTOS_DSCP_AF13 },
	{ "AF21", IPTOS_DSCP_AF21 },
	{ "AF22", IPTOS_DSCP_AF22 },
	{ "AF23", IPTOS_DSCP_AF23 },
	{ "AF31", IPTOS_DSCP_AF31 },
	{ "AF32", IPTOS_DSCP_AF32 },
	{ "AF33", IPTOS_DSCP_AF33 },
	{ "AF41", IPTOS_DSCP_AF41 },
	{ "AF42", IPTOS_DSCP_AF42 },
	{ "AF43", IPTOS_DSCP_AF43 },
	{ "CS0", IPTOS_DSCP_CS0 },
	{ "CS1", IPTOS_DSCP_CS1 },
	{ "CS2", IPTOS_DSCP_CS2 },
	{ "CS3", IPTOS_DSCP_CS3 },
	{ "CS4", IPTOS_DSCP_CS4 },
	{ "CS5", IPTOS_DSCP_CS5 },
	{ "CS6", IPTOS_DSCP_CS6 },
	{ "CS7", IPTOS_DSCP_CS7 },
	{ "EF", IPTOS_DSCP_EF },
	{ "LE", IPTOS_DSCP_LE },
};

int
ParseDscpClass(const char *s) noexcept
{
	for (const auto &i : dscp_classes)
		if (StringIsEqualIgnoreCase(s, i.name))
			return i.value;

	char *endptr;
	unsigned long value = strtoul(s, &endptr, 0);
	if (endptr > s && *endptr == 0 && value <= 0xff)
		return value;

	return -1;
}
