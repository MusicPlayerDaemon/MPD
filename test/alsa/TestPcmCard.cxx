// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/alsa/PcmCard.hxx"

#include <gtest/gtest.h>

#include <alsa/asoundlib.h>

#include <string.h>

static constexpr char config_text[] = R"(
pcm.hwnum { type hw card 3 device 0 }
pcm.hwname { type hw card Intel }
pcm.hwnocard { type hw }
pcm.args {
	@args [ CARD DEV ]
	@args.CARD { type string }
	@args.DEV { type integer default 0 }
	type hw
	card $CARD
	device $DEV
}
pcm.plugref { type plug slave.pcm "hwname" }
pcm.pluginline { type plug slave.pcm { type hw card 5 } }
pcm.plugargs { type plug slave.pcm "args:Foo,1" }
pcm.asym { type asym playback.pcm "plugref" capture.pcm "hwnum" }
pcm.empty { type empty slave.pcm "asym" }
pcm.softvol {
	type softvol
	slave.pcm "hwnum"
	control { name "Soft" card 7 }
}
pcm.loop { type plug slave.pcm "loop" }
pcm.nullpcm { type null }
)";

class AlsaPcmCard : public testing::Test {
protected:
	snd_config_t *top = nullptr;

	void SetUp() override {
		ASSERT_GE(snd_config_top(&top), 0);

		snd_input_t *input;
		ASSERT_GE(snd_input_buffer_open(&input, config_text,
						strlen(config_text)), 0);
		const int result = snd_config_load(top, input);
		snd_input_close(input);
		ASSERT_GE(result, 0);
	}

	void TearDown() override {
		if (top != nullptr)
			snd_config_delete(top);
	}

	std::string Find(const char *pcm_name) {
		return Alsa::FindPcmCard(top, pcm_name);
	}
};

TEST_F(AlsaPcmCard, Hw)
{
	EXPECT_EQ(Find("hwnum"), "hw:3");
	EXPECT_EQ(Find("hwname"), "hw:Intel");
	EXPECT_EQ(Find("hwnocard"), "");
}

TEST_F(AlsaPcmCard, Arguments)
{
	EXPECT_EQ(Find("args:Bar"), "hw:Bar");
	EXPECT_EQ(Find("args:CARD=Bar,DEV=1"), "hw:Bar");
	EXPECT_EQ(Find("args:2"), "hw:2");
}

TEST_F(AlsaPcmCard, Slave)
{
	EXPECT_EQ(Find("plugref"), "hw:Intel");
	EXPECT_EQ(Find("pluginline"), "hw:5");
	EXPECT_EQ(Find("plugargs"), "hw:Foo");
	EXPECT_EQ(Find("asym"), "hw:Intel");
	EXPECT_EQ(Find("empty"), "hw:Intel");

	/* the "card" inside "control" must be ignored */
	EXPECT_EQ(Find("softvol"), "hw:3");
}

TEST_F(AlsaPcmCard, Unknown)
{
	EXPECT_EQ(Find("loop"), "");
	EXPECT_EQ(Find("nullpcm"), "");
	EXPECT_EQ(Find("nonexistent"), "");
}
