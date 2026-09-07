#include "Difficulty.h"

#include "Diagnostics.h"
#include "Regeneration.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>

namespace Difficulty
{
	namespace
	{
		constexpr std::size_t kCount = static_cast<std::size_t>(Setting::kCount);
		constexpr int kDifficulties = 6;
		constexpr const char* kDifficultyNames[kDifficulties] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };

		// The real vanilla GameSetting name for each entry, and the real vanilla default value -
		// both taken directly from CustomDifficultyUIControlScript.psc, not guessed. The vanilla
		// number is the slider default and the help text only (1.0.5); what "off" leaves alone and
		// hands back is the LOADED value captured at Init.
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

		// Blade and Blunt's published pairs (its ESP's GMST records, read 2026-09-06 under its open
		// permissions): damage to the player as vanilla, damage by the player gentler.
		constexpr std::array<float, kCount> kBladeAndBlunt = { 0.50F, 0.75F, 1.00F, 1.50F, 2.00F, 3.00F, 1.50F, 1.25F, 1.00F, 1.00F, 0.75F, 0.50F };

		std::array<RE::Setting*, kCount> g_settings{};
		std::array<float, kCount> g_loaded{};
		bool g_wrote = false;            // this process has written the twelve, so disabled hands the loaded values back once
		bool g_bladeAndBlunt = false;
		bool g_requiem = false;
		bool g_bbIniFound = false;
		bool g_bbLevelScaling = false;
		int g_levelRuleLast = -1;
		std::atomic<bool> g_levelSinkInstalled{ false };

		bool IsToPlayer(Setting a_setting) { return static_cast<std::size_t>(a_setting) < kDifficulties; }
		int DifficultyOf(Setting a_setting) { return static_cast<int>(static_cast<std::size_t>(a_setting) % kDifficulties); }
		Setting ToSetting(int a_difficulty) { return static_cast<Setting>(a_difficulty); }
		Setting BySetting(int a_difficulty) { return static_cast<Setting>(kDifficulties + a_difficulty); }

		// A plugin is present if the game's own data handler loaded it under that filename.
		bool PluginLoaded(const char* a_file)
		{
			auto* handler = RE::TESDataHandler::GetSingleton();
			if (!handler) { return false; }
			return handler->LookupModByName(a_file) != nullptr;
		}

		// Blade and Blunt's DLL steps the twelve at levels 10..50 while bLevelBasedDifficulty is true
		// in its INI (the file it ships says true). The DLL is GPL and is not read; the INI is a text
		// file next to ours, and the page asks for false while the mod is present.
		void ReadBladeAndBluntIni()
		{
			g_bbIniFound = false;
			g_bbLevelScaling = false;
			std::error_code ec;
			const auto path = std::filesystem::current_path(ec) / "Data" / "SKSE" / "Plugins" / "BladeAndBlunt.ini";
			std::ifstream in(path);
			if (!in) { return; }
			std::string line;
			while (std::getline(in, line))
			{
				std::string lowered;
				for (const char c : line) { if (!std::isspace(static_cast<unsigned char>(c))) { lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c))); } }
				if (lowered.rfind("blevelbaseddifficulty=", 0) != 0) { continue; }
				g_bbIniFound = true;
				const auto value = lowered.substr(std::string("blevelbaseddifficulty=").size());
				g_bbLevelScaling = value.rfind("true", 0) == 0 || value.rfind("1", 0) == 0;
				return;
			}
		}

		class LevelSink : public RE::BSTEventSink<RE::LevelIncrease::Event>
		{
		public:
			static LevelSink* GetSingleton()
			{
				static LevelSink singleton;
				return &singleton;
			}
			RE::BSEventNotifyControl ProcessEvent(const RE::LevelIncrease::Event* a_event, RE::BSTEventSource<RE::LevelIncrease::Event>*) override
			{
				if (a_event)
				{
					logger::debug("Difficulty: level-up event (level {})", a_event->newLevel);
					ApplyLevelRule("level up");
					// Queued, so it lands after every other plugin's handler of this same event.
					RequestApplyLive();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void InstallLevelSink()
		{
			if (g_levelSinkInstalled.exchange(true)) { return; }
			if (auto* source = RE::LevelIncrease::GetEventSource())
			{
				source->AddEventSink(LevelSink::GetSingleton());
				logger::info("Difficulty: level-up event sink registered (the level rule and a re-apply run on every level-up)");
			}
			else
			{
				logger::warn("Difficulty: the SKSE LevelIncrease event source is unavailable - the level rule runs on load only");
			}
		}
	}

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

	const char* Name(Setting a_setting) { return kSettingInfo[static_cast<std::size_t>(a_setting)].name; }

	float LoadedValue(Setting a_setting)
	{
		const auto i = static_cast<std::size_t>(a_setting);
		return g_settings[i] ? g_loaded[i] : kSettingInfo[i].vanillaDefault;
	}

	float LiveValue(Setting a_setting)
	{
		const auto i = static_cast<std::size_t>(a_setting);
		return g_settings[i] ? g_settings[i]->GetFloat() : 0.0F;
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
				// The LOADED value: what this game holds after every plugin's records. This is what
				// "disabled" leaves alone and hands back - never the compiled vanilla number.
				g_loaded[i] = g_settings[i]->GetFloat();
				logger::debug("Init: resolved \"{}\", loaded value {:.2f} (vanilla {:.2f})", kSettingInfo[i].name,
					g_loaded[i], kSettingInfo[i].vanillaDefault);
			}
		}

		g_bladeAndBlunt = PluginLoaded("BladeAndBlunt.esp");
		g_requiem = PluginLoaded("Requiem.esp");
		ReadBladeAndBluntIni();
		const auto overhaul = OverhaulLoaded();
		if (!overhaul.empty())
		{
			logger::info("Init: {} is loaded - its values are the loaded values, and this mod writes last while enabled", overhaul);
		}
		if (g_bladeAndBlunt)
		{
			if (!g_bbIniFound) { logger::info("Init: BladeAndBlunt.ini was not found beside this mod's INI, so its bLevelBasedDifficulty could not be read"); }
			else if (g_bbLevelScaling) { logger::warn("Init: BladeAndBlunt.ini has bLevelBasedDifficulty = true - its DLL will step the damage multipliers at levels 10 to 50 as well; set it to false while this mod is enabled"); }
			else { logger::info("Init: BladeAndBlunt.ini has bLevelBasedDifficulty = false - its DLL leaves the damage multipliers alone"); }
		}
		InstallLevelSink();

		return allResolved;
	}

	void ApplyLive()
	{
		bool anyMissing = false;
		const bool enabled = settings::difficulty::enabled;

		if (!enabled && !g_wrote)
		{
			// Off writes nothing (logic library 43): the loaded values stay exactly as they are.
			logger::debug("ApplyLive: disabled and nothing written this session - the loaded values stay");
			diagnostics::RecordApplied(enabled);
			return;
		}

		for (std::size_t i = 0; i < kCount; ++i)
		{
			if (!g_settings[i])
			{
				anyMissing = true;
				continue;
			}

			const Setting setting = static_cast<Setting>(i);
			float value = g_loaded[i];
			if (enabled)
			{
				value = settings::difficulty::sharedPair
							? (IsToPlayer(setting) ? settings::difficulty::sharedToPC : settings::difficulty::sharedByPC)
							: ConfiguredValue(setting);
			}

			g_settings[i]->data.f = value;
			logger::debug("ApplyLive: \"{}\" = {:.2f}{}", kSettingInfo[i].name, value,
				enabled ? (settings::difficulty::sharedPair ? " (the shared pair)" : "") : " (the loaded value - mod disabled)");
		}

		if (!enabled) { logger::info("ApplyLive: disabled - the loaded damage multipliers are back"); }
		g_wrote = enabled;

		if (anyMissing)
		{
			logger::warn("ApplyLive: one or more GameSettings never resolved at Init() - see the earlier "
						 "error log; those entries were skipped");
		}

		diagnostics::RecordApplied(enabled);
	}

	void RequestApplyLive()
	{
		if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([]() { ApplyLive(); }); }
		else { ApplyLive(); }
	}

	void OnGameLoaded()
	{
		ApplyLevelRule("save loaded");
		if (auto* tasks = SKSE::GetTaskInterface())
		{
			tasks->AddTask([]() { ApplyLive(); Regeneration::ApplyLive(); });
		}
		else
		{
			ApplyLive();
			Regeneration::ApplyLive();
		}
	}

	int CurrentDifficulty()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return -1; }
		const std::int32_t raw = player->GetGameStatsData().difficulty;
		return (raw < 0 || raw >= kDifficulties) ? -1 : static_cast<int>(raw);
	}

	bool SetGameDifficulty(int a_difficulty, const char* a_reason)
	{
		if (a_difficulty < 0 || a_difficulty >= kDifficulties) { return false; }
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return false; }
		player->GetGameStatsData().difficulty = a_difficulty;
		// What the game's own Settings menu also does: keep the preference in step so the value
		// survives, rather than only the live copy.
		if (auto* prefs = RE::INIPrefSettingCollection::GetSingleton())
		{
			if (auto* setting = prefs->GetSetting("iDifficulty:Gameplay")) { setting->data.i = a_difficulty; }
		}
		logger::info("Difficulty: the game's difficulty is now {} ({})", kDifficultyNames[a_difficulty], a_reason);
		Regeneration::CheckAndApplyIfChanged();
		return true;
	}

	int LevelRuleTarget(int a_level)
	{
		int best = -1;
		for (int d = 0; d < kDifficulties; ++d)
		{
			const int from = static_cast<int>(settings::difficulty::levelFor[static_cast<std::size_t>(d)]);
			if (from <= 0 || a_level < from) { continue; }
			// The highest threshold reached wins; on a tie the higher difficulty does.
			if (best < 0 || from >= static_cast<int>(settings::difficulty::levelFor[static_cast<std::size_t>(best)])) { best = d; }
		}
		return best;
	}

	int ApplyLevelRule(const char* a_reason)
	{
		if (!settings::difficulty::byLevel) { return -1; }
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return -1; }
		const int level = static_cast<int>(player->GetLevel());
		const int target = LevelRuleTarget(level);
		if (target < 0)
		{
			logger::debug("Difficulty by level ({}): level {} reaches no row of the table; the game's difficulty stays", a_reason, level);
			return -1;
		}
		const int now = CurrentDifficulty();
		if (now == target)
		{
			logger::debug("Difficulty by level ({}): level {} says {}, which the game already is", a_reason, level, kDifficultyNames[target]);
			g_levelRuleLast = target;
			return target;
		}
		logger::info("Difficulty by level ({}): level {} -> {} (the game was on {})", a_reason, level, kDifficultyNames[target], now >= 0 ? kDifficultyNames[now] : "none");
		SetGameDifficulty(target, a_reason);
		g_levelRuleLast = target;
		return target;
	}

	void UseLoadedValues()
	{
		for (std::size_t i = 0; i < kCount; ++i) { ConfiguredValue(static_cast<Setting>(i)) = LoadedValue(static_cast<Setting>(i)); }
		logger::info("Difficulty: the table now holds the values this game loaded with");
	}
	void UseVanillaValues()
	{
		for (std::size_t i = 0; i < kCount; ++i) { ConfiguredValue(static_cast<Setting>(i)) = kSettingInfo[i].vanillaDefault; }
		logger::info("Difficulty: the table now holds Skyrim's vanilla values");
	}
	void UseBladeAndBlunt()
	{
		for (std::size_t i = 0; i < kCount; ++i) { ConfiguredValue(static_cast<Setting>(i)) = kBladeAndBlunt[i]; }
		logger::info("Difficulty: the table now holds Blade and Blunt's values");
	}
	void UseRequiem()
	{
		for (std::size_t i = 0; i < kCount; ++i) { ConfiguredValue(static_cast<Setting>(i)) = 1.0F; }
		logger::info("Difficulty: the table now holds Requiem's values (every multiplier 1.0)");
	}

	std::string OverhaulLoaded()
	{
		std::string out;
		if (g_bladeAndBlunt) { out = "Blade and Blunt"; }
		if (g_requiem) { out += out.empty() ? "Requiem" : " and Requiem"; }
		return out;
	}
	bool BladeAndBluntPresent() { return g_bladeAndBlunt; }
	bool RequiemPresent() { return g_requiem; }
	bool BladeAndBluntLevelScaling(bool& a_found)
	{
		a_found = g_bbIniFound;
		return g_bbLevelScaling;
	}

	std::string StatusJson()
	{
		std::string rows, levels;
		for (int d = 0; d < kDifficulties; ++d)
		{
			rows += std::format(R"({}{{"difficulty":"{}","toPC":{:.3f},"byPC":{:.3f},"loadedToPC":{:.3f},"loadedByPC":{:.3f},"liveToPC":{:.3f},"liveByPC":{:.3f}}})",
								d ? "," : "", kDifficultyNames[d], ConfiguredValue(ToSetting(d)), ConfiguredValue(BySetting(d)),
								LoadedValue(ToSetting(d)), LoadedValue(BySetting(d)), LiveValue(ToSetting(d)), LiveValue(BySetting(d)));
			levels += std::format("{}{}", d ? "," : "", settings::difficulty::levelFor[static_cast<std::size_t>(d)]);
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		const int level = player ? static_cast<int>(player->GetLevel()) : -1;
		const int now = CurrentDifficulty();
		return std::format(R"({{"enabled":{},"sharedPair":{},"sharedToPC":{:.3f},"sharedByPC":{:.3f},"byLevel":{},"levelFor":[{}],"playerLevel":{},"levelTarget":{},"levelRuleLast":{},"wrote":{},"difficulty":{},"difficultyName":"{}","overhaul":"{}","bladeAndBlunt":{},"requiem":{},"bbIniFound":{},"bbLevelScaling":{},"rows":[{}]}})",
						   settings::difficulty::enabled ? "true" : "false", settings::difficulty::sharedPair ? "true" : "false",
						   settings::difficulty::sharedToPC, settings::difficulty::sharedByPC, settings::difficulty::byLevel ? "true" : "false", levels,
						   level, level >= 0 ? LevelRuleTarget(level) : -1, g_levelRuleLast, g_wrote ? "true" : "false",
						   now, now >= 0 ? kDifficultyNames[now] : "none", OverhaulLoaded(), g_bladeAndBlunt ? "true" : "false", g_requiem ? "true" : "false",
						   g_bbIniFound ? "true" : "false", g_bbLevelScaling ? "true" : "false", rows);
	}
}
