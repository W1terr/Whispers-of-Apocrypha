#include "StressAndFear.h"

#include "Papyrus.h"

#include <Windows.h>

// Stress and Fear keeps its stress in a global and raises it when its stress potions are drunk; this does the same, so
// the mod shows its usual messages. Its MCM switches (stress, fear) are respected.
namespace StressAndFear
{
	namespace
	{
		namespace API = WhispersOfApocrypha::API;

		constexpr auto kStressPlugin = "Stress and Fear.esp"sv;
		// the separate patch plugin this support replaces: while it is installed, it does the work (no double stress)
		constexpr auto kOldPatch = L"WhispersOfApocryphaStressAndFear.dll";

		// Stress and Fear.esp (ESL)
		constexpr RE::FormID kStressTotal = 0x801;    // Stress_Total, 0 - 100
		constexpr RE::FormID kStressEnabled = 0x8A5;  // Stress_Enabled
		constexpr RE::FormID kFearEnabled = 0x8A6;    // Stress_FearEnabled
		constexpr RE::FormID kIncrease10 = 0x828;     // Stress_Potion_Increase10
		constexpr RE::FormID kIncrease25 = 0x829;     // Stress_Potion_Increase25
		constexpr RE::FormID kBreathing = 0x81F;      // Stress_Quest_BreathingTopic
		constexpr RE::FormID kTrauma = 0x899;         // Stress_Quest_Trauma
		constexpr RE::FormID kFearImod = 0x81C;       // Stress_PhobiaImageEffect

		// stress gained per second in the dark, by madness stage (clear mind, whispers, green nightmare, eyes)
		constexpr std::array kStressPerSecond{ 0.25f, 0.5f, 1.0f, 1.5f };

		struct Forms
		{
			RE::TESGlobal*             total{ nullptr };
			RE::TESGlobal*             stressEnabled{ nullptr };
			RE::TESGlobal*             fearEnabled{ nullptr };
			RE::AlchemyItem*           increase10{ nullptr };
			RE::AlchemyItem*           increase25{ nullptr };
			RE::TESTopic*              breathing{ nullptr };
			RE::TESTopic*              trauma{ nullptr };
			RE::TESImageSpaceModifier* fearImod{ nullptr };
		} forms;

		bool active{ false };

		template <class T>
		T* Lookup(RE::FormID a_id)
		{
			const auto form = RE::TESDataHandler::GetSingleton()->LookupForm<T>(a_id, kStressPlugin);
			if (!form) {
				logs::error("Stress and Fear: {:03X} not found in {}", a_id, kStressPlugin);
			}
			return form;
		}

		bool On(const RE::TESGlobal* a_switch)
		{
			return !a_switch || a_switch->value > 0.0f;
		}

		// the way Stress and Fear raises stress itself: the player drinks one of its stress potions (added if missing,
		// silently), its script adds the stress and tells the player how stressed they are
		void Drink(RE::AlchemyItem* a_potion)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (player && a_potion) {
				Papyrus::CallMethod(static_cast<RE::Actor*>(player), "EquipItem", static_cast<RE::TESForm*>(a_potion), false, true);
			}
		}

		void Say(RE::TESTopic* a_topic)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (player && a_topic) {
				Papyrus::CallMethod(static_cast<RE::Actor*>(player), "Say", a_topic, static_cast<RE::Actor*>(nullptr), false);
			}
		}

		void OnDarkness(const API::DarknessData& a_data)
		{
			if (!On(forms.stressEnabled)) {
				return;
			}
			const auto stage = static_cast<std::size_t>(std::clamp(a_data.stage, 0, 3));
			forms.total->value = std::min(forms.total->value + kStressPerSecond[stage] * a_data.seconds, 100.0f);
		}

		// the eyes open: the fear reaction Stress and Fear gives a phobia
		void OnStage(const API::StageData& a_data)
		{
			if (a_data.stage != API::kEyes || a_data.previous >= API::kEyes || !On(forms.fearEnabled)) {
				return;
			}
			Say(forms.breathing);
			if (forms.fearImod) {
				RE::ImageSpaceModifierInstanceForm::Trigger(forms.fearImod, 1.0f, nullptr);
			}
			if (On(forms.stressEnabled)) {
				Drink(forms.increase10);
			}
			logs::info("Stress and Fear: the eyes open, fear");
		}

		// Hermaeus Mora himself: a trauma
		void OnGaze()
		{
			if (!On(forms.fearEnabled)) {
				return;
			}
			Say(forms.trauma);
			if (On(forms.stressEnabled)) {
				Drink(forms.increase25);
			}
			logs::info("Stress and Fear: Hermaeus Mora appears, trauma");
		}
	}

	void Init()
	{
		if (!RE::TESDataHandler::GetSingleton()->LookupModByName(kStressPlugin)) {
			return;  // not installed
		}
		if (GetModuleHandleW(kOldPatch)) {
			logs::warn("Stress and Fear: the separate patch ({}) is installed and does the work; it is no longer needed",
				"WhispersOfApocryphaStressAndFear.dll");
			return;
		}
		forms.total = Lookup<RE::TESGlobal>(kStressTotal);
		forms.stressEnabled = Lookup<RE::TESGlobal>(kStressEnabled);
		forms.fearEnabled = Lookup<RE::TESGlobal>(kFearEnabled);
		forms.increase10 = Lookup<RE::AlchemyItem>(kIncrease10);
		forms.increase25 = Lookup<RE::AlchemyItem>(kIncrease25);
		forms.breathing = Lookup<RE::TESTopic>(kBreathing);
		forms.trauma = Lookup<RE::TESTopic>(kTrauma);
		forms.fearImod = Lookup<RE::TESImageSpaceModifier>(kFearImod);
		active = forms.total != nullptr;
		logs::info("Stress and Fear {}", active ? "found: the dark of Apocrypha brings stress and fear" : "forms missing: no stress or fear");
	}

	void Handle(API::Message a_message, const void* a_data, std::uint32_t a_size)
	{
		if (!active) {
			return;
		}
		switch (a_message) {
		case API::kDarkness:
			if (a_size >= sizeof(API::DarknessData)) {
				OnDarkness(*static_cast<const API::DarknessData*>(a_data));
			}
			break;
		case API::kStage:
			if (a_size >= sizeof(API::StageData)) {
				OnStage(*static_cast<const API::StageData*>(a_data));
			}
			break;
		case API::kGaze:
			OnGaze();
			break;
		default:
			break;
		}
	}
}
