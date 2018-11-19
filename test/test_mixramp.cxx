/*
 * Unit tests for mixramp_interpolate()
 */

#include "player/CrossFade.cxx"

#include <gtest/gtest.h>

#include <string.h>

TEST(MixRamp, Interpolate)
{
	const char *input = "1.0 0.00;3.0 0.10;6.0 2.50;";

	char *foo = strdup(input);
	EXPECT_NEAR(double(0),
		    mixramp_interpolate(foo, 0).count(),
		    0.05);
	free(foo);

	foo = strdup(input);
	EXPECT_NEAR(float(0),
		    mixramp_interpolate(foo, 1).count(),
		    0.005);
	free(foo);

	foo = strdup(input);
	EXPECT_NEAR(float(0.1),
		    mixramp_interpolate(foo, 3).count(),
		    0.005);
	free(foo);

	foo = strdup(input);
	EXPECT_NEAR(float(2.5),
		    mixramp_interpolate(foo, 6).count(),
		    0.01);
	free(foo);

	foo = strdup(input);
	EXPECT_LT(mixramp_interpolate(foo, 6.1), FloatDuration::zero());
	free(foo);

	foo = strdup(input);
	EXPECT_NEAR(float(0.05),
		    mixramp_interpolate(foo, 2).count(),
		    0.05);
	free(foo);

	foo = strdup(input);
	EXPECT_NEAR(float(1.3),
		    mixramp_interpolate(foo, 4.5).count(),
		    0.05);
	free(foo);

	foo = strdup(input);
	EXPECT_NEAR(float(0.9),
		    mixramp_interpolate(foo, 4).count(),
		    0.05);
	free(foo);

	foo = strdup(input);
	EXPECT_NEAR(float(1.7),
		    mixramp_interpolate(foo, 5).count(),
		    0.05);
	free(foo);
}
