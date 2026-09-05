#include "Regeneration.h"

#include "Diagnostics.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <array>
#include <algorithm>

namespace Regeneration
{
	namespace
	{
		constexpr std::size_t kPerDifficultyCount = static_cast<std::size_t>(PerDifficultySetting::kCount);
		constexpr std::size_t kGlobalCount = static_cast<std::size_t>(GlobalSetting::kCount);
		constexpr std::size_t kDifficultyCount = settings::regeneration::kDifficultyCount;

		// Display names in RE::PlayerCharacter::GetGameStatsData().difficulty order - the same
		// six names UI.cpp's existing difficulty sliders already use.
		constexpr std::array<const char*, kDifficultyCount> kDifficultyNames = {
			"Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary"
		};

		// One entry per per-difficulty setting: the ONE real vanilla GameSetting name, and a
		// pointer to this mod's own six-slot storage for it in Settings.h. RE::Setting* and the
		// captured vanilla value are filled in by Init(), not here - there is no compiled-in
		// vanilla number for these the way Difficulty.cpp has for its twelve (see Settings.h).
		struct PerDifficultyInfo
		{
			const char* gameSettingName;
			std::array<float, kDifficultyCount>* configured;
			RE::Setting* resolved = nullptr;
			float vanillaDefault = 0.0F;
		};

		std::array<PerDifficultyInfo, kPerDifficultyCount> g_perDifficulty = { {
			{ "fCombatHealthRegenRateMult", &settings::regeneration::combatHealthRegenRateMult },
			{ "fCombatMagickaRegenRateMult", &settings::regeneration::combatMagickaRegenRateMult },
			{ "fCombatStaminaRegenRateMult", &settings::regeneration::combatStaminaRegenRateMult },
			{ "fDamagedHealthRegenDelay", &settings::regeneration::damagedHealthRegenDelay },
			{ "fDamagedMagickaRegenDelay", &settings::regeneration::damagedMagickaRegenDelay },
			{ "fDamagedStaminaRegenDelay", &settings::regeneration::damagedStaminaRegenDelay },
			{ "fDamagedAVRegenDelay", &settings::regeneration::damagedAVRegenDelay },
		} };

		struct GlobalInfo
		{
			const char* gameSettingName;
			float* configured;
			RE::Setting* resolved = nullptr;
			float vanillaDefault = 0.0F;
		};

		std::array<GlobalInfo, kGlobalCount> g_global = { {
			{ "fHealthRegenDelayMax", &settings::regeneration::healthRegenDelayMax },
			{ "fMagickaRegenDelayMax", &settings::regeneration::magickaRegenDelayMax },
			{ "fStaminaRegenDelayMax", &settings::regeneration::staminaRegenDelayMax },
			{ "fOutOfBreathStaminaRegenDelay", &settings::regeneration::outOfBreathStaminaRegenDelay },
			{ "fEssentialDownCombatHealthRegenMult", &settings::regeneration::essentialDownCombatHealthRegenMult },
		} };

		int g_lastAppliedDifficulty = -1;  // -1 = ApplyLive() has never run yet

		// Reads the player's current difficulty, clamped to a sane 0..5. Defaults to Adept (2,
		// the vanilla starting difficulty) if the player does not exist yet or the field somehow
		// holds something out of range - never lets an unexpected value pick an out-of-bounds
		// storage slot.
		int CurrentDifficulty()
		{
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

			if (!player)
			{
				return 2;
			}

			const std::int32_t raw = player->GetGameStatsData().difficulty;

			if (raw < 0 || raw >= static_cast<std::int32_t>(kDifficultyCount))
			{
				logger::warn("CurrentDifficulty: player difficulty {} is out of the expected 0-5 "
							 "range; treating this application as Adept",
							 raw);

				return 2;
			}

			return raw;
		}

		// Resolves one GameSetting by name, verifying it against the running game rather than
		// assuming the name is correct just because it was read out of another mod's script (the
		// plan's own standard). Logs and leaves a_resolved null on any failure.
		bool ResolveOne(RE::GameSettingCollection* a_collection, const char* a_name, RE::Setting*& a_resolved)
		{
			a_resolved = a_collection->GetSetting(a_name);

			if (!a_resolved)
			{
				logger::warn("Regeneration::Init: could not resolve GameSetting \"{}\" - dropping "
							 "its control from the settings page; this build's runtime does not "
							 "have a setting by this name",
							 a_name);

				return false;
			}

			if (a_resolved->GetType() != RE::Setting::Type::kFloat)
			{
				logger::warn("Regeneration::Init: GameSetting \"{}\" is not a float setting (type "
							 "{}) - refusing to touch it",
							 a_name, static_cast<int>(a_resolved->GetType()));
				a_resolved = nullptr;

				return false;
			}

			return true;
		}

		// A MenuOpenCloseEvent sink covering every menu, deliberately - the settings menu where
		// difficulty is actually changed has no dedicated event of its own, and checking on
		// every close is one int read and a comparison, cheap enough that narrowing to a specific
		// menu name would not be worth the fragility of hard-coding one.
		class MenuCloseSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static MenuCloseSink* GetSingleton()
			{
				static MenuCloseSink singleton;

				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && !a_event->opening)
				{
					CheckAndApplyIfChanged();
				}

				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	bool Init()
	{
		RE::GameSettingCollection* collection = RE::GameSettingCollection::GetSingleton();

		if (!collection)
		{
			logger::error("Regeneration::Init: RE::GameSettingCollection::GetSingleton() returned null");

			return false;
		}

		bool allResolved = true;

		for (PerDifficultyInfo& info : g_perDifficulty)
		{
			if (!ResolveOne(collection, info.gameSettingName, info.resolved))
			{
				allResolved = false;
				continue;
			}

			info.vanillaDefault = info.resolved->GetFloat();
			logger::debug("Regeneration::Init: resolved \"{}\", real vanilla value {:.3f}",
				info.gameSettingName, info.vanillaDefault);

			for (float& slot : *info.configured)
			{
				if (slot == settings::regeneration::kUnset)
				{
					slot = info.vanillaDefault;
				}
			}
		}

		for (GlobalInfo& info : g_global)
		{
			if (!ResolveOne(collection, info.gameSettingName, info.resolved))
			{
				allResolved = false;
				continue;
			}

			info.vanillaDefault = info.resolved->GetFloat();
			logger::debug("Regeneration::Init: resolved \"{}\", real vanilla value {:.3f}",
				info.gameSettingName, info.vanillaDefault);

			if (*info.configured == settings::regeneration::kUnset)
			{
				*info.configured = info.vanillaDefault;
			}
		}

		if (!allResolved)
		{
			logger::warn("Regeneration::Init: one or more GameSettings never resolved - see the "
						 "earlier warnings; those entries are skipped everywhere else");
		}

		return allResolved;
	}

	bool HasResolved(PerDifficultySetting a_setting)
	{
		return g_perDifficulty[static_cast<std::size_t>(a_setting)].resolved != nullptr;
	}

	bool HasResolved(GlobalSetting a_setting)
	{
		return g_global[static_cast<std::size_t>(a_setting)].resolved != nullptr;
	}

	float VanillaDefault(PerDifficultySetting a_setting)
	{
		return g_perDifficulty[static_cast<std::size_t>(a_setting)].vanillaDefault;
	}

	float VanillaDefault(GlobalSetting a_setting)
	{
		return g_global[static_cast<std::size_t>(a_setting)].vanillaDefault;
	}

	void ApplyLive()
	{
		const int difficulty = CurrentDifficulty();
		const bool enabled = settings::regeneration::enabled;

		for (const PerDifficultyInfo& info : g_perDifficulty)
		{
			if (!info.resolved)
			{
				continue;
			}

			const float value = enabled ? (*info.configured)[difficulty] : info.vanillaDefault;

			info.resolved->data.f = value;
			logger::debug("Regeneration::ApplyLive: \"{}\" = {:.3f}{}", info.gameSettingName, value,
				enabled ? "" : " (vanilla - mod disabled)");
		}

		for (const GlobalInfo& info : g_global)
		{
			if (!info.resolved)
			{
				continue;
			}

			const float value = enabled ? *info.configured : info.vanillaDefault;

			info.resolved->data.f = value;
			logger::debug("Regeneration::ApplyLive: \"{}\" = {:.3f}{}", info.gameSettingName, value,
				enabled ? "" : " (vanilla - mod disabled)");
		}

		g_lastAppliedDifficulty = difficulty;
		diagnostics::RecordRegenerationApplied(enabled, difficulty);
	}

	void CheckAndApplyIfChanged()
	{
		const int difficulty = CurrentDifficulty();

		// Unconditional trace: without this there is no record of what the engine actually
		// reported at each menu close, which left a difficulty-reverting report undiagnosable.
		logger::trace("Regeneration::CheckAndApplyIfChanged: menu closed; engine difficulty is "
					  "{} ({}), last applied {}",
			difficulty, kDifficultyNames[difficulty], g_lastAppliedDifficulty);

		if (difficulty != g_lastAppliedDifficulty)
		{
			logger::info("Regeneration::CheckAndApplyIfChanged: difficulty changed {} -> {} ({}); re-applying",
				g_lastAppliedDifficulty, difficulty, kDifficultyNames[difficulty]);
			ApplyLive();
		}
	}

	void RegisterMenuEventSink()
	{
		static bool registered = false;

		if (registered)
		{
			return;
		}

		RE::UI* ui = RE::UI::GetSingleton();

		if (!ui)
		{
			logger::error("Regeneration::RegisterMenuEventSink: RE::UI::GetSingleton() returned null; "
						 "per-difficulty regeneration will only re-check on load, not on a live "
						 "difficulty change");

			return;
		}

		ui->AddEventSink(MenuCloseSink::GetSingleton());
		registered = true;
		logger::debug("Regeneration: MenuOpenCloseEvent sink registered");
	}

	void RestoreDefaults()
	{
		for (PerDifficultyInfo& info : g_perDifficulty)
		{
			info.configured->fill(info.vanillaDefault);
		}

		for (GlobalInfo& info : g_global)
		{
			*info.configured = info.vanillaDefault;
		}
	}

	void CopyToAllDifficulties(int a_from)
	{
		if (a_from < 0 || a_from >= static_cast<int>(kDifficultyCount))
		{
			logger::warn("Regeneration::CopyToAllDifficulties: {} is out of range; nothing copied", a_from);

			return;
		}

		for (PerDifficultyInfo& info : g_perDifficulty)
		{
			info.configured->fill((*info.configured)[a_from]);
		}

		logger::info("Regeneration::CopyToAllDifficulties: every difficulty now matches {}",
			kDifficultyNames[a_from]);
	}

	void CopyDifficulty(int a_from, int a_to)
	{
		if (a_from < 0 || a_from >= static_cast<int>(kDifficultyCount) ||
			a_to < 0 || a_to >= static_cast<int>(kDifficultyCount))
		{
			logger::warn("Regeneration::CopyDifficulty: {} -> {} has an out-of-range index; nothing copied",
				a_from, a_to);

			return;
		}

		if (a_from == a_to)
		{
			return;
		}

		for (PerDifficultyInfo& info : g_perDifficulty)
		{
			(*info.configured)[a_to] = (*info.configured)[a_from];
		}

		logger::info("Regeneration::CopyDifficulty: {} copied into {}", kDifficultyNames[a_from], kDifficultyNames[a_to]);
	}

	float LiveValue(PerDifficultySetting a_setting)
	{
		const RE::Setting* resolved = g_perDifficulty[static_cast<std::size_t>(a_setting)].resolved;

		return resolved ? resolved->GetFloat() : 0.0F;
	}

	int LastAppliedDifficulty() { return g_lastAppliedDifficulty; }

	const char* DifficultyDisplayName(int a_difficulty)
	{
		if (a_difficulty < 0 || a_difficulty >= static_cast<int>(kDifficultyCount))
		{
			return "?";
		}

		return kDifficultyNames[a_difficulty];
	}
}
