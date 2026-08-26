#include "Difficulty.h"

#include "Diagnostics.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <array>

namespace Difficulty
{
	namespace
	{
		constexpr std::size_t kCount = static_cast<std::size_t>(Setting::kCount);

		// The real vanilla GameSetting name for each entry, and the real vanilla default value
		// upstream's own SetDefaultSettings() restores when the mod is disabled - both taken
		// directly from CustomDifficultyUIControlScript.psc, not guessed.
		struct SettingInfo
		{
			const char* name;
			float vanillaDefault;
		};

		constexpr std::array<SettingInfo, kCount> kSettingInfo = { {
			{ "fDiffMultHPToPCVE", 0.50F },
			{ "fDiffMultHPToPCE", 0.75F },
			{ "fDiffMultHPToPCN", 1.00F },
			{ "fDiffMultHPToPCH", 1.50F },
			{ "fDiffMultHPToPCVH", 2.00F },
			{ "fDiffMultHPToPCL", 3.00F },
			{ "fDiffMultHPByPCVE", 2.00F },
			{ "fDiffMultHPByPCE", 1.50F },
			{ "fDiffMultHPByPCN", 1.00F },
			{ "fDiffMultHPByPCH", 0.75F },
			{ "fDiffMultHPByPCVH", 0.50F },
			{ "fDiffMultHPByPCL", 0.25F },
		} };

		std::array<RE::Setting*, kCount> g_settings{};

		// Settings::difficulty's own field for a given entry - a small indirection so Init()/
		// ApplyLive() can loop over all twelve rather than repeating each by name.
		float& ConfiguredValue(Setting a_setting)
		{
			using namespace settings::difficulty;

			switch (a_setting)
			{
			case Setting::kToPCVE: return toPCVE;
			case Setting::kToPCE: return toPCE;
			case Setting::kToPCN: return toPCN;
			case Setting::kToPCH: return toPCH;
			case Setting::kToPCVH: return toPCVH;
			case Setting::kToPCL: return toPCL;
			case Setting::kByPCVE: return byPCVE;
			case Setting::kByPCE: return byPCE;
			case Setting::kByPCN: return byPCN;
			case Setting::kByPCH: return byPCH;
			case Setting::kByPCVH: return byPCVH;
			default: return byPCL;
			}
		}
	}

	bool Init()
	{
		RE::GameSettingCollection* collection = RE::GameSettingCollection::GetSingleton();

		if (!collection)
		{
			logger::error("Init: RE::GameSettingCollection::GetSingleton() returned null");

			return false;
		}

		bool allResolved = true;

		for (std::size_t i = 0; i < kCount; ++i)
		{
			g_settings[i] = collection->GetSetting(kSettingInfo[i].name);

			if (!g_settings[i])
			{
				logger::error("Init: could not resolve GameSetting \"{}\" - this entry will never apply",
					kSettingInfo[i].name);
				allResolved = false;
			}
			else if (g_settings[i]->GetType() != RE::Setting::Type::kFloat)
			{
				// Ask the object, don't assume (CLAUDE.md rule 30) - a GameSetting resolved
				// under an unexpected type would silently corrupt whatever data.f actually
				// means if written blindly.
				logger::error("Init: GameSetting \"{}\" is not a float setting (type {}) - refusing to touch it",
					kSettingInfo[i].name, static_cast<int>(g_settings[i]->GetType()));
				g_settings[i] = nullptr;
				allResolved = false;
			}
			else
			{
				logger::debug("Init: resolved \"{}\", current value {:.2f}", kSettingInfo[i].name,
					g_settings[i]->GetFloat());
			}
		}

		return allResolved;
	}

	void ApplyLive()
	{
		bool anyMissing = false;

		for (std::size_t i = 0; i < kCount; ++i)
		{
			if (!g_settings[i])
			{
				anyMissing = true;
				continue;
			}

			const Setting setting = static_cast<Setting>(i);
			const float value = settings::difficulty::enabled ? ConfiguredValue(setting) : kSettingInfo[i].vanillaDefault;

			g_settings[i]->data.f = value;
			logger::debug("ApplyLive: \"{}\" = {:.2f}{}", kSettingInfo[i].name, value,
				settings::difficulty::enabled ? "" : " (vanilla default - mod disabled)");
		}

		if (anyMissing)
		{
			logger::warn("ApplyLive: one or more GameSettings never resolved at Init() - see the earlier "
						 "error log; those entries were skipped");
		}

		diagnostics::RecordApplied(settings::difficulty::enabled);
	}
}
