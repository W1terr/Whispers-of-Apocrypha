#include "Madness.h"

#include "Papyrus.h"

#include <mutex>
#include <numbers>

namespace Madness
{
	namespace
	{
		constexpr auto kPlugin = "WhispersOfApocrypha.esp"sv;
		constexpr auto kDragonborn = "Dragonborn.esm"sv;

		// Dragonborn.esm: the effects of DLC2abApoWaterDamageActual (25 health/s wading, 70 swimming in the ink)
		constexpr RE::FormID kInkShallowEffect = 0x2A6CD;   // DLC2ApoWaterDamageEffectShallow
		constexpr RE::FormID kInkSwimmingEffect = 0x2A6CC;  // DLC2ApoWaterDamageEffectSwimming
		// Dragonborn.esm: the trigger of the Black Book levels whose script (DLC2DBBookLevelLightTrigSCRIPT) drains
		// 1% health every 0.25 s while the player's light level is below 30: the darkness outside the moving lights
		constexpr RE::FormID kBookLightTrigger = 0x1EA7B;  // DLC2DBBookLevelLightTrig
		constexpr float      kDarkLightLevel = 30.0f;
		// Dragonborn.esm: Hermaeus Mora's faces full of eyes from the walls of Apocrypha (they emerge when their
		// fToggleBlend graph variable goes to 1, as DLC2MQ05HermaeusMoraFXScript does), and the eye in the dark water
		constexpr std::array<RE::FormID, 3> kFaces{ 0x37ADA, 0x37AE0, 0x37AE1 };  // HermaeusMorasFace01-03
		constexpr RE::FormID kInkEye = 0x2454D;                                   // ApoDarkWaterEye01
		// Skyrim.esm: Hermaeus Mora himself as he appears in Discerning the Transmundane (DA04 makes him vanish
		// with the PlayAnim01 graph event)
		constexpr RE::FormID kHermaeusMora = 0x5AF26;  // FXHermaeusMoraDA04
		// Skyrim.esm: a green light without shadows (radius 256), so the faces can be seen in the dark
		constexpr RE::FormID kGreenLight = 0xF7CC2;  // DefaultGreen01NS

		// WhispersOfApocrypha.esp (tools/build_esp.py)
		constexpr RE::FormID kWhisperFirst = 0x800;
		constexpr RE::FormID kBed = 0x880;
		constexpr RE::FormID kSoulCairn = 0x881;
		constexpr RE::FormID kWordWall = 0x882;
		constexpr RE::FormID kHeartbeat = 0x883;
		constexpr RE::FormID kGaze = 0x884;
		constexpr RE::FormID kBookHum = 0x885;
		constexpr RE::FormID kRumble = 0x886;
		constexpr RE::FormID kMadnessImod = 0x890;  // DA16DreamState turned green

		constexpr std::uint32_t kSerializationID = 'WOAP';
		constexpr std::uint32_t kRecordRefs = 'REFS';
		constexpr std::uint32_t kSerializationVersion = 1;

		// seconds in the dark (exposure) where each effect begins and where it is at full strength
		constexpr float kWhispersStart = 1.5f;
		constexpr float kWhispersFull = 8.0f;
		constexpr float kVisionStart = 3.0f;
		constexpr float kVisionFull = 12.0f;
		constexpr float kEyesStart = 8.0f;
		constexpr float kEyesFull = 18.0f;
		constexpr float kSwimmingMult = 1.5f;  // exposure grows faster while swimming in the ink
		constexpr float kFadeOutTime = 2.4f;   // everything fades over this once out of the dark, then the mind is clear
		constexpr int   kMaxFaces = 10;
		constexpr int   kMaxInkEyes = 4;
		constexpr float kGazeInterval = 35.0f;  // seconds between his visits at full madness (randomized)

		constexpr float kPi = std::numbers::pi_v<float>;
		constexpr float kDegree = kPi / 180.0f;
		constexpr float kGazeLength = 9.0f;
		constexpr float kGazeVanish = 5.5f;  // seconds into the gaze when he leaves

		struct Forms
		{
			RE::EffectSetting*                       inkShallow{ nullptr };
			RE::EffectSetting*                       inkSwimming{ nullptr };
			RE::TESBoundObject*                      bookLightTrigger{ nullptr };
			std::vector<RE::TESBoundObject*>         faces;
			RE::TESBoundObject*                      inkEye{ nullptr };
			RE::TESBoundObject*                      hermaeusMora{ nullptr };
			RE::TESBoundObject*                      greenLight{ nullptr };
			RE::TESImageSpaceModifier*               imod{ nullptr };
			std::vector<RE::BGSSoundDescriptorForm*> whispers;
			RE::BGSSoundDescriptorForm*              bed{ nullptr };
			RE::BGSSoundDescriptorForm*              soulCairn{ nullptr };
			RE::BGSSoundDescriptorForm*              wordWall{ nullptr };
			RE::BGSSoundDescriptorForm*              heartbeat{ nullptr };
			RE::BGSSoundDescriptorForm*              gaze{ nullptr };
			RE::BGSSoundDescriptorForm*              bookHum{ nullptr };
			RE::BGSSoundDescriptorForm*              rumble{ nullptr };
		} forms;

		// a looping sound whose volume follows a level; started and stopped as the level crosses zero
		class Loop
		{
		public:
			void Set(RE::BGSSoundDescriptorForm* a_form, float a_volume)
			{
				if (!a_form) {
					return;
				}
				if (a_volume > 0.004f) {
					if (!playing || !handle.IsValid()) {
						const auto audio = RE::BSAudioManager::GetSingleton();
						handle = RE::BSSoundHandle();
						if (audio && audio->GetSoundHandle(handle, a_form)) {
							handle.SetVolume(a_volume);
							handle.FadeInPlay(600);
							playing = true;
							volume = a_volume;
						}
					} else if (std::abs(a_volume - volume) > 0.01f) {
						handle.SetVolume(a_volume);
						volume = a_volume;
					}
				} else {
					Stop(900);
				}
			}

			void Stop(std::uint16_t a_fadeMS)
			{
				if (playing && handle.IsValid()) {
					handle.FadeOutAndRelease(a_fadeMS);
				}
				playing = false;
				volume = 0.0f;
			}

		private:
			RE::BSSoundHandle handle;
			bool              playing{ false };
			float             volume{ 0.0f };
		};

		// one of our placed references: a face, an eye in the ink, or Hermaeus Mora
		struct Apparition
		{
			enum class Kind
			{
				kFace,
				kInkEye,
				kHermaeusMora
			};

			RE::ObjectRefHandle handle;
			RE::ObjectRefHandle light;  // the green light on it
			Kind                kind{ Kind::kFace };
			float               age{ 0.0f };
			float               life{ 10.0f };
			float               size{ 100.0f };   // rough radius, for "looked at"
			float               watched{ 0.0f };  // seconds the player has looked straight at it
			float               leaving{ -1.0f }; // >= 0: seconds since it started to leave
			bool                shown{ false };   // the emerge animation was started
			bool                seen{ false };    // the line of sight to it was checked
		};

		struct State
		{
			float exposure{ 0.0f };
			float presence{ 0.0f };  // 1 in the dark, falls to 0 over kFadeOutTime once out of it
			int   ink{ 0 };          // 0 out, 1 wading in the ink, 2 swimming in the ink, 3 in a Black Book's darkness
			float inkIn{ 0.0f };     // the dark is checked every 0.1 s
			bool  quiet{ true };     // nothing is playing or showing: the update has nothing to do out of the dark

			// the player's cell, and whether it has the Black Book darkness trigger
			RE::TESObjectCELL* scannedCell{ nullptr };
			bool               darknessCell{ false };
			float              lightLevel{ 100.0f };

			// smoothed levels 0..1
			float whisper{ 0.0f };
			float vision{ 0.0f };
			float eyes{ 0.0f };

			float              whisperIn{ 2.0f };
			std::array<int, 6> recent{ -1, -1, -1, -1, -1, -1 };
			std::size_t        recentNext{ 0 };

			std::vector<Apparition> apparitions;
			float                   faceIn{ 1.0f };
			int                     hiddenInARow{ 0 };  // faces placed out of sight one after another
			bool                    checkSight{ true };  // off if the check never passes (it would hide everything)
			float                   inkEyeIn{ 1.0f };

			float gazeIn{ -1.0f };  // < 0: not scheduled
			float gazeT{ -1.0f };   // >= 0: Hermaeus Mora is here, seconds into it

			RE::NiPointer<RE::ImageSpaceModifierInstanceForm> imod;

			int lastStage{ 0 };

			Loop bed, soulCairn, wordWall, heartbeat;
		};

		std::mutex   lock;
		State        state;
		bool         ready{ false };
		std::mt19937 rng{ std::random_device{}() };

		// references a save kept (saved while they were out), removed once the game has loaded
		std::vector<RE::FormID> leftovers;

		float Random(float a_min, float a_max)
		{
			return std::uniform_real_distribution<float>(a_min, a_max)(rng);
		}

		float Ramp(float a_value, float a_start, float a_full)
		{
			const float t = std::clamp((a_value - a_start) / (a_full - a_start), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float Approach(float a_current, float a_target, float a_rate, float a_delta)
		{
			return a_current + (a_target - a_current) * (1.0f - std::exp(-a_rate * a_delta));
		}

		float Ease(float a_t)
		{
			a_t = std::clamp(a_t, 0.0f, 1.0f);
			return a_t * a_t * (3.0f - 2.0f * a_t);
		}

		template <class T>
		T* Lookup(RE::FormID a_id, std::string_view a_file)
		{
			return RE::TESDataHandler::GetSingleton()->LookupForm<T>(a_id, a_file);
		}

		// LookupForm<TESBoundObject> fails: the base class has no form type of its own
		RE::TESBoundObject* LookupBound(RE::FormID a_id, std::string_view a_file)
		{
			const auto form = RE::TESDataHandler::GetSingleton()->LookupForm(a_id, a_file);
			if (!form || !form->IsBoundObject()) {
				logs::error("Base object {:05X} not found in {}", a_id, a_file);
				return nullptr;
			}
			return static_cast<RE::TESBoundObject*>(form);
		}

		// camera position and forward (the camera's world rotation holds forward, up, right as columns)
		struct View
		{
			RE::NiPoint3 position, forward;
		};

		std::optional<View> CameraView()
		{
			const auto camera = RE::Main::WorldRootCamera();
			if (!camera) {
				return std::nullopt;
			}
			const auto& m = camera->world.rotate;
			return View{ camera->world.translate, { m.entry[0][0], m.entry[1][0], m.entry[2][0] } };
		}

		// standing in damaging water (Apocrypha's ink: DLC2ApocryphaWater / DLC2ApocryphaWaterSmall)
		bool InInkWater(RE::PlayerCharacter* a_player)
		{
			const auto data = a_player->loadedData;
			const auto water = data ? data->currentWaterType : nullptr;
			return water && water->flags.any(RE::TESWaterForm::Flag::kCauseDamage) && data->relevantWaterHeight > a_player->GetPositionZ();
		}

		// Actor::GetLightLevel, what the GetLightLevel condition and Papyrus function return (Anniversary Edition id;
		// on older runtimes the Black Book darkness is not detected, see Init)
		float LightLevel(RE::Actor* a_actor)
		{
			using func_t = float(RE::Actor*);
			static REL::Relocation<func_t> func{ REL::ID(37775) };
			return func(a_actor);
		}

		// in a Black Book level, out of the light: exactly where its trigger script hurts the player (it checks the light
		// level every 0.1 s too)
		bool InBookDarkness(RE::PlayerCharacter* a_player)
		{
			const auto cell = a_player->GetParentCell();
			if (cell != state.scannedCell) {
				state.scannedCell = cell;
				state.darknessCell = false;
				if (cell && forms.bookLightTrigger) {
					cell->ForEachReference([&](RE::TESObjectREFR* a_ref) {
						if (a_ref && a_ref->GetBaseObject() == forms.bookLightTrigger && !a_ref->IsDisabled()) {
							state.darknessCell = true;
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
				}
			}
			if (!state.darknessCell) {
				return false;
			}
			state.lightLevel = LightLevel(a_player);
			return state.lightLevel < kDarkLightLevel;
		}

		// 0 out of the dark, 1 wading in the ink, 2 swimming in it, 3 in a Black Book's darkness
		int InkState(RE::PlayerCharacter* a_player)
		{
			if (InBookDarkness(a_player)) {
				return 3;
			}
			if (InInkWater(a_player)) {
				return a_player->AsActorState()->IsSwimming() ? 2 : 1;
			}
			const auto target = a_player->AsMagicTarget();
			const auto list = target ? target->GetActiveEffectList() : nullptr;
			if (!list) {
				return 0;
			}
			int result = 0;
			for (const auto effect : *list) {
				if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
					continue;
				}
				const auto base = effect->GetBaseObject();
				if (base && base == forms.inkSwimming) {
					return 2;
				}
				if (base && base == forms.inkShallow) {
					result = 1;
				}
			}
			return result;
		}

		bool IsOurs(RE::TESBoundObject* a_base)
		{
			return a_base && (a_base == forms.inkEye || a_base == forms.hermaeusMora || a_base == forms.greenLight || std::ranges::find(forms.faces, a_base) != forms.faces.end());
		}

		// ------------------------------------------------------------------------------------------ sounds

		void PlayAt(RE::BGSSoundDescriptorForm* a_form, float a_volume, const RE::NiPoint3* a_position)
		{
			const auto audio = RE::BSAudioManager::GetSingleton();
			if (!audio || !a_form || a_volume <= 0.0f) {
				return;
			}
			RE::BSSoundHandle handle;
			if (!audio->GetSoundHandle(handle, a_form)) {
				return;
			}
			if (a_position) {
				handle.SetPosition(*a_position);
			}
			handle.SetVolume(a_volume);
			handle.Play();
		}

		void Whisper(const View& a_view)
		{
			if (forms.whispers.empty()) {
				return;
			}
			// a voice nobody heard lately
			int index = 0;
			for (int attempt = 0; attempt < 12; ++attempt) {
				index = std::uniform_int_distribution<int>(0, static_cast<int>(forms.whispers.size()) - 1)(rng);
				if (std::ranges::find(state.recent, index) == state.recent.end()) {
					break;
				}
			}
			state.recent[state.recentNext] = index;
			state.recentNext = (state.recentNext + 1) % state.recent.size();

			// somewhere close around your head, often behind you; sometimes right inside it
			RE::NiPoint3 position = a_view.position;
			if (Random(0.0f, 1.0f) > 0.2f) {
				const float heading = Random(0.0f, 2.0f * kPi);
				const float distance = Random(50.0f, 220.0f);
				position += RE::NiPoint3{ std::sin(heading) * distance, std::cos(heading) * distance, Random(-30.0f, 50.0f) };
			}
			PlayAt(forms.whispers[static_cast<std::size_t>(index)], 0.4f + 0.6f * state.whisper, &position);
		}

		void UpdateSounds(float a_delta, const View& a_view)
		{
			const float w = state.whisper;
			state.bed.Set(forms.bed, 0.85f * w);
			state.soulCairn.Set(forms.soulCairn, 0.6f * Ramp(w, 0.25f, 1.0f));
			state.wordWall.Set(forms.wordWall, 0.45f * state.vision);
			state.heartbeat.Set(forms.heartbeat, 0.75f * state.eyes);

			if (w > 0.03f) {
				state.whisperIn -= a_delta;
				if (state.whisperIn <= 0.0f) {
					Whisper(a_view);
					state.whisperIn = std::lerp(9.0f, 1.6f, w) * Random(0.7f, 1.3f);
				}
			} else {
				state.whisperIn = Random(0.5f, 1.5f);
			}
		}

		void StopSounds(std::uint16_t a_fadeMS)
		{
			state.bed.Stop(a_fadeMS);
			state.soulCairn.Stop(a_fadeMS);
			state.wordWall.Stop(a_fadeMS);
			state.heartbeat.Stop(a_fadeMS);
		}

		// ------------------------------------------------------------------------------------------ the green nightmare

		void SetVision(float a_strength)
		{
			if (!forms.imod) {
				return;
			}
			if (a_strength > 0.005f) {
				// the image space manager holds the other reference; alone with ours it was dropped (a load)
				if (!state.imod || state.imod->GetRefCount() <= 1) {
					state.imod.reset(RE::ImageSpaceModifierInstanceForm::Trigger(forms.imod, a_strength, nullptr));
				}
				if (state.imod) {
					state.imod->strength = a_strength;
				}
			} else if (state.imod) {
				RE::ImageSpaceModifierInstanceForm::Stop(forms.imod);
				state.imod.reset();
			}
		}

		// ------------------------------------------------------------------------------------------ apparitions

		RE::NiPointer<RE::TESObjectREFR> Place(RE::PlayerCharacter* a_player, RE::TESBoundObject* a_base, const RE::NiPoint3& a_position, float a_scale)
		{
			auto ref = a_player->PlaceObjectAtMe(a_base, false);
			if (!ref) {
				return nullptr;
			}
			ref->Disable();  // faded in by Papyrus, below
			ref->SetPosition(a_position);
			const RE::NiPoint3 toPlayer = a_player->GetPosition() - a_position;
			ref->SetAngle({ 0.0f, 0.0f, std::atan2(toPlayer.x, toPlayer.y) });
			ref->SetScale(a_scale);
			Papyrus::CallMethod(ref.get(), "EnableNoWait", true);
			return ref;
		}

		void Remove(RE::TESObjectREFR* a_ref)
		{
			if (a_ref) {
				a_ref->Disable();
				a_ref->SetDelete(true);
			}
		}

		// a green light between the apparition and the player: the darkness would hide it otherwise. It stays far from
		// the player (its radius is 256), so it does not light the player out of the Black Book's darkness.
		RE::ObjectRefHandle PlaceLight(RE::PlayerCharacter* a_player, const RE::NiPoint3& a_at, float a_towardPlayer)
		{
			if (!forms.greenLight) {
				return {};
			}
			RE::NiPoint3 to = a_player->GetPosition() - a_at;
			to.z = 0.0f;
			to.Unitize();
			const auto light = a_player->PlaceObjectAtMe(forms.greenLight, false);
			if (!light) {
				return {};
			}
			light->SetPosition(a_at + to * a_towardPlayer);
			return light->GetHandle();
		}

		void RemoveLight(Apparition& a_apparition)
		{
			if (const auto light = a_apparition.light.get()) {
				Remove(light.get());
			}
			a_apparition.light = {};
		}

		// a direction in front of you, mostly inside your view, sometimes at its edge
		RE::NiPoint3 AroundDirection(const View& a_view)
		{
			const float yaw0 = std::atan2(a_view.forward.x, a_view.forward.y);
			const float side = Random(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
			const float offset = Random(0.0f, 1.0f) < 0.25f ? Random(40.0f, 75.0f) : Random(6.0f, 35.0f);
			const float yaw = yaw0 + side * offset * kDegree;
			return { std::sin(yaw), std::cos(yaw), 0.0f };
		}

		bool Crowded(const RE::NiPoint3& a_position, float a_room)
		{
			for (const auto& a : state.apparitions) {
				const auto ref = a.handle.get();
				if (ref && ref->GetPosition().GetDistance(a_position) < a_room) {
					return true;
				}
			}
			return false;
		}

		int Count(Apparition::Kind a_kind)
		{
			return static_cast<int>(std::ranges::count_if(state.apparitions, [&](const Apparition& a) { return a.kind == a_kind && a.leaving < 0.0f; }));
		}

		void SpawnFace(RE::PlayerCharacter* a_player, const View& a_view)
		{
			if (forms.faces.empty()) {
				return;
			}
			for (int attempt = 0; attempt < 8; ++attempt) {
				const auto   dir = AroundDirection(a_view);
				RE::NiPoint3 position = a_player->GetPosition() + dir * Random(420.0f, 700.0f);
				position.z += Random(40.0f, 240.0f);
				if (Crowded(position, 260.0f)) {
					continue;
				}
				const auto base = forms.faces[std::uniform_int_distribution<std::size_t>(0, forms.faces.size() - 1)(rng)];
				const float scale = Random(0.55f, 0.9f) * (0.9f + 0.3f * state.eyes);
				const auto  ref = Place(a_player, base, position, scale);
				if (!ref) {
					return;
				}
				Apparition a;
				a.handle = ref->GetHandle();
				a.light = PlaceLight(a_player, position, 130.0f);
				a.kind = Apparition::Kind::kFace;
				a.life = Random(10.0f, 20.0f);
				a.size = 250.0f * scale;
				state.apparitions.push_back(a);
				PlayAt(forms.bookHum, 0.45f * (0.5f + 0.5f * state.eyes), nullptr);
				return;
			}
		}

		void SpawnInkEye(RE::PlayerCharacter* a_player, const View& a_view)
		{
			const float water = a_player->GetWaterHeight();
			if (!forms.inkEye || water < a_player->GetPositionZ() - 50.0f || water > a_player->GetPositionZ() + 400.0f) {
				return;  // no surface near the player
			}
			for (int attempt = 0; attempt < 8; ++attempt) {
				const float  heading = std::atan2(a_view.forward.x, a_view.forward.y) + Random(-100.0f, 100.0f) * kDegree;
				RE::NiPoint3 position = a_player->GetPosition() + RE::NiPoint3{ std::sin(heading), std::cos(heading), 0.0f } * Random(160.0f, 480.0f);
				position.z = water;
				if (Crowded(position, 120.0f)) {
					continue;
				}
				const auto ref = Place(a_player, forms.inkEye, position, Random(0.9f, 1.4f));
				if (!ref) {
					return;
				}
				Apparition a;
				a.handle = ref->GetHandle();
				a.kind = Apparition::Kind::kInkEye;
				a.life = Random(5.0f, 11.0f);
				a.size = 40.0f;
				a.shown = true;
				state.apparitions.push_back(a);
				return;
			}
		}

		void SummonHermaeusMora(RE::PlayerCharacter* a_player, const View& a_view)
		{
			if (!forms.hermaeusMora) {
				return;
			}
			RE::NiPoint3 ahead{ a_view.forward.x, a_view.forward.y, 0.0f };
			if (ahead.Unitize() < 1e-3f) {
				ahead = { 0.0f, 1.0f, 0.0f };
			}
			RE::NiPoint3 position = a_player->GetPosition() + ahead * 480.0f;
			position.z += 40.0f;
			const auto ref = Place(a_player, forms.hermaeusMora, position, 1.4f);
			if (!ref) {
				return;
			}
			Apparition a;
			a.handle = ref->GetHandle();
			a.light = PlaceLight(a_player, position + RE::NiPoint3{ 0.0f, 0.0f, 120.0f }, 180.0f);
			a.kind = Apparition::Kind::kHermaeusMora;
			a.life = kGazeVanish;
			a.size = 300.0f;
			a.shown = true;
			state.apparitions.push_back(a);
		}

		void UpdateApparitions(float a_delta, RE::PlayerCharacter* a_player, const View& a_view)
		{
			const float level = state.eyes;

			// faces in the dark
			const int faces = level > 0.02f ? 1 + static_cast<int>(level * static_cast<float>(kMaxFaces - 1) + 0.5f) : 0;
			if (Count(Apparition::Kind::kFace) < faces && state.gazeT < 0.0f) {
				state.faceIn -= a_delta;
				if (state.faceIn <= 0.0f) {
					SpawnFace(a_player, a_view);
					state.faceIn = std::lerp(3.0f, 0.6f, level) * Random(0.7f, 1.3f);
				}
			}
			// eyes in the ink, only while you stand in it
			const int inkEyes = (state.ink == 1 || state.ink == 2) && level > 0.02f ? 1 + static_cast<int>(level * static_cast<float>(kMaxInkEyes - 1) + 0.5f) : 0;
			if (Count(Apparition::Kind::kInkEye) < inkEyes) {
				state.inkEyeIn -= a_delta;
				if (state.inkEyeIn <= 0.0f) {
					SpawnInkEye(a_player, a_view);
					state.inkEyeIn = std::lerp(4.0f, 1.0f, level) * Random(0.7f, 1.3f);
				}
			}

			for (auto& a : state.apparitions) {
				a.age += a_delta;
				const auto ref = a.handle.get();
				if (!ref) {
					RemoveLight(a);
					a.leaving = 100.0f;  // gone
					continue;
				}
				if (!a.shown && ref->Is3DLoaded()) {
					// its graph may not be ready on the first frames: retry until it takes
					a.shown = ref->SetGraphVariableFloat("fToggleBlend", 1.0f);
					if (!a.shown && a.age > 4.0f) {
						logs::warn("Face {:08X} has no fToggleBlend graph variable", ref->GetFormID());
						a.shown = true;
					}
				}
				// a face behind a wall or a pillar is wasted: drop it and try another place soon
				if (a.kind == Apparition::Kind::kFace && a.shown && !a.seen && a.leaving < 0.0f && state.checkSight) {
					a.seen = true;
					bool       unused = false;
					const bool visible = a_player->HasLineOfSight(ref.get(), unused);
					if (!visible) {
						RemoveLight(a);
						Remove(ref.get());
						a.leaving = 100.0f;
						state.faceIn = 0.1f;
						if (++state.hiddenInARow >= 8) {
							state.checkSight = false;
							logs::warn("No face was ever in sight: the line of sight check is off");
						}
						continue;
					}
					state.hiddenInARow = 0;
				}
				if (a.leaving < 0.0f) {
					// looking straight at one makes it leave: you are not meant to see them
					RE::NiPoint3 to = ref->GetPosition() - a_view.position;
					if (a.kind == Apparition::Kind::kFace) {
						to.z += a.size * 0.5f;
					}
					const float distance = to.Unitize();
					const float angle = std::acos(std::clamp(to.Dot(a_view.forward), -1.0f, 1.0f));
					if (a.kind != Apparition::Kind::kHermaeusMora && angle < std::max(8.0f * kDegree, std::atan2(a.size, std::max(distance, 1.0f)))) {
						a.watched += a_delta;
					}
					// out of the dark everything leaves at once; walking up to a face makes it leave too
					const bool outOfDark = state.ink == 0 && state.presence < 0.8f;  // not on a flicker at the edge of a light
					const bool unwanted = outOfDark || (a.kind != Apparition::Kind::kHermaeusMora && level <= 0.02f) ||
					                      (a.kind == Apparition::Kind::kInkEye && state.ink != 1 && state.ink != 2);
					const bool approached = a.kind == Apparition::Kind::kFace && distance < 350.0f;
					if (a.watched > 2.5f || approached || a.age > a.life || unwanted || (state.gazeT >= 0.0f && a.kind == Apparition::Kind::kFace)) {
						a.leaving = 0.0f;
						switch (a.kind) {
						case Apparition::Kind::kFace:
							ref->SetGraphVariableFloat("fToggleBlend", 0.0f);  // sinks back into the wall of the dark
							break;
						case Apparition::Kind::kInkEye:
							Papyrus::CallMethod(ref.get(), "DisableNoWait", true);
							break;
						case Apparition::Kind::kHermaeusMora:
							ref->NotifyAnimationGraph("PlayAnim01");  // his vanishing
							break;
						}
					}
					continue;
				}
				a.leaving += a_delta;
				if (a.kind == Apparition::Kind::kFace && a.leaving > 0.8f && a.leaving - a_delta <= 0.8f) {
					Papyrus::CallMethod(ref.get(), "DisableNoWait", true);
				}
				const float dark = a.kind == Apparition::Kind::kHermaeusMora ? 1.6f : 1.2f;  // its light goes out
				if (a.leaving > dark) {
					RemoveLight(a);
				}
				const float gone = a.kind == Apparition::Kind::kInkEye ? 2.0f : 2.4f;
				if (a.leaving > gone) {
					RemoveLight(a);
					Remove(ref.get());
					a.leaving = 100.0f;
				}
			}
			std::erase_if(state.apparitions, [](const Apparition& a) { return a.leaving >= 100.0f; });
		}

		// ------------------------------------------------------------------------------------------ the gaze

		void UpdateGaze(float a_delta, RE::PlayerCharacter* a_player, const View& a_view)
		{
			if (state.gazeT >= 0.0f) {
				state.gazeT += a_delta;
				if (state.gazeT >= kGazeLength || (state.ink == 0 && state.presence <= 0.0f)) {
					state.gazeT = -1.0f;
					state.gazeIn = kGazeInterval * Random(0.8f, 1.3f);
				}
				return;
			}
			if (state.eyes < 0.97f || state.ink == 0) {
				state.gazeIn = -1.0f;
				return;
			}
			if (state.gazeIn < 0.0f) {
				state.gazeIn = Random(3.0f, 6.0f);  // soon after the madness is complete
			}
			state.gazeIn -= a_delta;
			if (state.gazeIn > 0.0f) {
				return;
			}
			state.gazeT = 0.0f;
			SummonHermaeusMora(a_player, a_view);
			PlayAt(forms.gaze, 1.0f, nullptr);
			PlayAt(forms.rumble, 0.8f, nullptr);
			RE::ShakeCamera(0.4f, a_player->GetPosition(), 2.0f);
			logs::info("Hermaeus Mora appears (exposure {:.1f} s)", state.exposure);
		}

		// ------------------------------------------------------------------------------------------ update

		void Update(float a_delta)
		{
			if (!ready || a_delta <= 0.0f) {
				return;
			}
			a_delta = std::min(a_delta, 0.25f);
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->Is3DLoaded()) {
				return;
			}
			std::scoped_lock guard(lock);
			state.inkIn -= a_delta;
			if (state.inkIn <= 0.0f) {
				state.inkIn = 0.1f;
				const int ink = InkState(player);
				if (ink != state.ink) {
					constexpr std::array names{ "Out of the dark", "Wading in the ink", "Swimming in the ink", "In the darkness of the Black Book" };
					logs::info("{} (light level {:.0f}, exposure {:.1f} s)", names[static_cast<std::size_t>(ink)], state.lightLevel, state.exposure);
					state.ink = ink;
				}
			}
			const int ink = state.ink;
			if (ink == 0 && state.quiet) {
				return;  // out of the dark and nothing left to fade
			}
			const auto view = CameraView();
			if (!view) {
				return;
			}

			if (ink > 0) {
				state.exposure = std::min(state.exposure + a_delta * (ink == 2 ? kSwimmingMult : 1.0f), kEyesFull + 6.0f);
				state.presence = 1.0f;
			} else if (state.presence > 0.0f) {
				// out of the dark: everything fades out over kFadeOutTime, then the mind is clear again
				state.presence = std::max(state.presence - a_delta / kFadeOutTime, 0.0f);
				if (state.presence <= 0.0f) {
					state.exposure = 0.0f;
				}
			}

			// levels rise smoothly with exposure, and fall with the fade straight away
			const float e = state.exposure;
			const auto  level = [&](float a_current, float a_target, float a_rate) {
				a_target *= state.presence;
				return a_target < a_current && ink == 0 ? a_target : Approach(a_current, a_target, a_rate, a_delta);
			};
			state.whisper = level(state.whisper, Ramp(e, kWhispersStart, kWhispersFull), 1.5f);
			state.vision = level(state.vision, Ramp(e, kVisionStart, kVisionFull), 1.0f);
			state.eyes = level(state.eyes, Ramp(e, kEyesStart, kEyesFull), 1.0f);

			const int stage = e >= kEyesStart ? 3 : (e >= kVisionStart ? 2 : (e >= kWhispersStart ? 1 : 0));
			if (stage != state.lastStage) {
				constexpr std::array names{ "clear mind", "whispers", "green nightmare", "eyes" };
				logs::info("Madness stage {} ({}), exposure {:.1f} s", stage, names[static_cast<std::size_t>(stage)], e);
				state.lastStage = stage;
			}

			UpdateSounds(a_delta, *view);
			UpdateApparitions(a_delta, player, *view);
			UpdateGaze(a_delta, player, *view);

			// stronger while he is here
			float gaze = 0.0f;
			if (state.gazeT >= 0.0f) {
				gaze = Ease(state.gazeT / 1.5f) * (1.0f - Ease((state.gazeT - kGazeVanish) / 2.0f)) * state.presence;
			}
			SetVision(std::clamp(state.vision + gaze * 0.5f, 0.0f, 2.0f));

			state.quiet = state.presence <= 0.0f && state.apparitions.empty() && !state.imod && state.gazeT < 0.0f;
		}

		struct PlayerUpdate
		{
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				func(a_this, a_delta);
				Update(a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// ------------------------------------------------------------------------------------------ save / load

		// our references are saved with the game when it is saved while they are out: remember them to remove them
		void OnSave(SKSE::SerializationInterface* a_intfc)
		{
			std::scoped_lock         guard(lock);
			std::vector<RE::FormID> ids;
			for (const auto& a : state.apparitions) {
				if (const auto ref = a.handle.get()) {
					ids.push_back(ref->GetFormID());
				}
				if (const auto light = a.light.get()) {
					ids.push_back(light->GetFormID());
				}
			}
			const auto count = static_cast<std::uint32_t>(ids.size());
			if (a_intfc->OpenRecord(kRecordRefs, kSerializationVersion)) {
				a_intfc->WriteRecordData(count);
				for (const auto id : ids) {
					a_intfc->WriteRecordData(id);
				}
			}
		}

		void OnLoad(SKSE::SerializationInterface* a_intfc)
		{
			std::uint32_t type = 0, version = 0, length = 0;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				if (type != kRecordRefs) {
					continue;
				}
				std::uint32_t count = 0;
				a_intfc->ReadRecordData(count);
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::FormID id = 0;
					RE::FormID resolved = 0;
					if (a_intfc->ReadRecordData(id) && a_intfc->ResolveFormID(id, resolved)) {
						leftovers.push_back(resolved);
					}
				}
			}
		}
	}

	void InstallHooks()
	{
		REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdate::func = player.write_vfunc(0xAD, PlayerUpdate::thunk);

		const auto serialization = SKSE::GetSerializationInterface();
		serialization->SetUniqueID(kSerializationID);
		serialization->SetSaveCallback(OnSave);
		serialization->SetLoadCallback(OnLoad);
	}

	void Init()
	{
		forms.inkShallow = Lookup<RE::EffectSetting>(kInkShallowEffect, kDragonborn);
		forms.inkSwimming = Lookup<RE::EffectSetting>(kInkSwimmingEffect, kDragonborn);
		if (!forms.inkShallow || !forms.inkSwimming) {
			logs::error("Dragonborn's ink damage effects not found: is Dragonborn.esm loaded?");
		}
		if (REL::Module::IsAE()) {
			forms.bookLightTrigger = LookupBound(kBookLightTrigger, kDragonborn);
		} else {
			logs::warn("Not the Anniversary Edition: the darkness of the Black Books is not detected, only the ink");
		}
		for (const auto id : kFaces) {
			if (const auto face = LookupBound(id, kDragonborn)) {
				forms.faces.push_back(face);
			}
		}
		forms.inkEye = LookupBound(kInkEye, kDragonborn);
		forms.hermaeusMora = LookupBound(kHermaeusMora, "Skyrim.esm"sv);
		forms.greenLight = LookupBound(kGreenLight, "Skyrim.esm"sv);
		forms.imod = Lookup<RE::TESImageSpaceModifier>(kMadnessImod, kPlugin);

		for (RE::FormID id = kWhisperFirst; id < kWhisperFirst + 0x40; ++id) {
			const auto form = Lookup<RE::BGSSoundDescriptorForm>(id, kPlugin);
			if (!form) {
				break;
			}
			forms.whispers.push_back(form);
		}
		forms.bed = Lookup<RE::BGSSoundDescriptorForm>(kBed, kPlugin);
		forms.soulCairn = Lookup<RE::BGSSoundDescriptorForm>(kSoulCairn, kPlugin);
		forms.wordWall = Lookup<RE::BGSSoundDescriptorForm>(kWordWall, kPlugin);
		forms.heartbeat = Lookup<RE::BGSSoundDescriptorForm>(kHeartbeat, kPlugin);
		forms.gaze = Lookup<RE::BGSSoundDescriptorForm>(kGaze, kPlugin);
		forms.bookHum = Lookup<RE::BGSSoundDescriptorForm>(kBookHum, kPlugin);
		forms.rumble = Lookup<RE::BGSSoundDescriptorForm>(kRumble, kPlugin);
		if (forms.whispers.empty() || !forms.imod) {
			logs::error("{} not loaded (or incomplete): no sounds and no green nightmare", kPlugin);
		}
		logs::info("Forms: book darkness trigger {}, ink effects {}/{}, faces {}, ink eye {}, Hermaeus Mora {}, imod {}, {} whispers, ambient sounds {}",
			forms.bookLightTrigger != nullptr, forms.inkShallow != nullptr, forms.inkSwimming != nullptr, forms.faces.size(), forms.inkEye != nullptr, forms.hermaeusMora != nullptr,
			forms.imod != nullptr, forms.whispers.size(),
			forms.bed && forms.soulCairn && forms.wordWall && forms.heartbeat && forms.gaze && forms.bookHum && forms.rumble);
		ready = forms.bookLightTrigger || forms.inkShallow || forms.inkSwimming;
	}

	void Reset()
	{
		std::scoped_lock guard(lock);
		StopSounds(100);
		if (state.imod && forms.imod) {
			RE::ImageSpaceModifierInstanceForm::Stop(forms.imod);
		}
		state.imod.reset();
		state.apparitions.clear();  // they belong to the world being left; a save that kept them is cleaned on load
		state.exposure = 0.0f;
		state.presence = 0.0f;
		state.ink = 0;
		state.inkIn = 0.0f;
		state.quiet = true;
		state.scannedCell = nullptr;
		state.darknessCell = false;
		state.whisper = state.vision = state.eyes = 0.0f;
		state.gazeIn = state.gazeT = -1.0f;
		state.lastStage = 0;
		leftovers.clear();
	}

	void OnGameLoaded()
	{
		std::scoped_lock guard(lock);
		int removed = 0;
		for (const auto id : leftovers) {
			const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
			if (ref && IsOurs(ref->GetBaseObject())) {
				Remove(ref);
				++removed;
			}
		}
		if (removed > 0) {
			logs::info("Removed {} apparitions the save had kept", removed);
		}
		leftovers.clear();
	}
}
