#pragma once

// Whispers of Apocrypha's messages to other SKSE plugins, through SKSE messaging (sender "WhispersOfApocrypha").
// Register for them in kPostLoad or later:
//     SKSE::GetMessagingInterface()->RegisterListener(WhispersOfApocrypha::API::kSender, OnMessage);
// They are sent on the main thread while the game runs.
namespace WhispersOfApocrypha::API
{
	inline constexpr auto kSender = "WhispersOfApocrypha";

	enum Message : std::uint32_t
	{
		kDarkness = 'WOAd',  // once a second while the player is in the dark: DarknessData
		kStage = 'WOAs',     // the madness stage changed: StageData
		kGaze = 'WOAg',      // Hermaeus Mora appears in front of the player: no data
	};

	enum Dark : std::int32_t
	{
		kInkWading = 1,     // not sent since 1.3: the ink water no longer counts
		kInkSwimming = 2,   // not sent since 1.3
		kBookDarkness = 3,  // a Black Book level, out of the light
	};

	enum Stage : std::int32_t
	{
		kClearMind = 0,
		kWhispers = 1,
		kGreenNightmare = 2,
		kEyes = 3,
	};

	struct DarknessData
	{
		std::int32_t dark;      // Dark
		std::int32_t stage;     // Stage
		float        exposure;  // seconds spent in the dark
		float        seconds;   // time since the last kDarkness message
	};

	struct StageData
	{
		std::int32_t stage;     // Stage
		std::int32_t previous;  // Stage
	};
}
