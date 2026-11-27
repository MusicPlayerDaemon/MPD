#include "protocol/ArgParser.hxx"
#include "protocol/Ack.hxx"
#include "protocol/RangeArg.hxx"

#include <gtest/gtest.h>

TEST(ArgParser, Range)
{
	RangeArg range = ParseCommandArgRange("1");
	EXPECT_EQ(1u, range.start);
	EXPECT_EQ(2u, range.end);

	range = ParseCommandArgRange("1:5");
	EXPECT_EQ(1u, range.start);
	EXPECT_EQ(5u, range.end);

	range = ParseCommandArgRange("1:");
	EXPECT_EQ(1u, range.start);
	EXPECT_GE(range.end, 999999u);

	EXPECT_THROW(range = ParseCommandArgRange("-2"),
		     ProtocolError);
}
