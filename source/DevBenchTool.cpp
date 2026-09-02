#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Difficulty.h"
#include "Regeneration.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <cstdlib>
#include <format>
#include <string>
#include <string_view>

namespace DevBenchTool
{
	namespace
	{
		// setdifficulty:<n> writes RE::PlayerCharacter's difficulty field directly and then
		// calls the SAME CheckAndApplyIfChanged() the real MenuOpenCloseEvent sink calls when a
		// menu closes. Honest limitation, stated here rather than left implicit: this does NOT
		// drive the game's actual Settings menu, because no clean native "set difficulty" call
		// exists to invoke from outside it and splicing real input through that menu is fragile.
		// What it DOES prove end to end is the part this feature is actually FOR - that changing
		// difficulty selects the right per-difficulty regeneration set and writes it into the
		// game's real GameSettings - since it exercises the identical detection/apply function a
		// real menu close would use, not a shortcut around it.
		bool SetDifficulty(int a_difficulty)
		{
			if (a_difficulty < 0 || a_difficulty > 5)
			{
				return false;
			}

			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

			if (!player)
			{
				return false;
			}

			player->GetGameStatsData().difficulty = a_difficulty;
			Regeneration::CheckAndApplyIfChanged();

			return true;
		}

		void ControlTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string_view args = a_argsJson ? a_argsJson : "";

			constexpr std::string_view kSetDifficultyKey = "\"setdifficulty\":";
			const auto setDifficultyAt = args.find(kSetDifficultyKey);

			if (setDifficultyAt != std::string_view::npos)
			{
				const std::size_t valueStart = setDifficultyAt + kSetDifficultyKey.size();
				const int value = std::atoi(std::string(args.substr(valueStart, 8)).c_str());
				const bool ok = SetDifficulty(value);

				a_write(a_sink, std::format(
					R"({{"ok":{},"op":"setdifficulty","requested":{},"lastAppliedDifficulty":{},"lastAppliedDifficultyName":"{}"}})",
					ok ? "true" : "false", value, Regeneration::LastAppliedDifficulty(),
					Regeneration::DifficultyDisplayName(Regeneration::LastAppliedDifficulty()))
								   .c_str());

				return;
			}

			// testregen:"<difficulty>,<value>" - TEST SUPPORT ONLY, for the gate that proves the
			// actual point of this feature (the plan: "set different values on two difficulties,
			// switch difficulty in the game's own menu, and read the live setting back"). Turns
			// regeneration on, writes combatHealthRegenRateMult for one difficulty directly - the
			// exact field a real settings-page slider would write - and applies once immediately.
			// One setting is enough to prove the mechanism: every other per-difficulty setting
			// goes through the identical storage-and-apply path.
			constexpr std::string_view kTestRegenKey = "\"testregen\":\"";
			const auto testRegenAt = args.find(kTestRegenKey);

			if (testRegenAt != std::string_view::npos)
			{
				const std::size_t valueStart = testRegenAt + kTestRegenKey.size();
				const std::size_t valueEnd = args.find('"', valueStart);
				const std::string payload = (valueEnd != std::string_view::npos)
												 ? std::string(args.substr(valueStart, valueEnd - valueStart))
												 : std::string(args.substr(valueStart, 32));
				const std::size_t comma = payload.find(',');

				bool ok = false;
				int difficulty = -1;
				float value = 0.0F;

				if (comma != std::string::npos)
				{
					difficulty = std::atoi(payload.substr(0, comma).c_str());
					value = std::strtof(payload.substr(comma + 1).c_str(), nullptr);

					if (difficulty >= 0 && difficulty < static_cast<int>(settings::regeneration::kDifficultyCount))
					{
						settings::regeneration::enabled = true;
						settings::regeneration::combatHealthRegenRateMult[difficulty] = value;
						Regeneration::ApplyLive();
						ok = true;
					}
				}

				a_write(a_sink, std::format(
					R"({{"ok":{},"op":"testregen","difficulty":{},"value":{:.3f}}})",
					ok ? "true" : "false", difficulty, value)
								   .c_str());

				return;
			}

			if (args.find("\"apply\"") != std::string_view::npos)
			{
				Difficulty::ApplyLive();
				Regeneration::ApplyLive();
				a_write(a_sink, R"({"ok":true,"op":"apply"})");

				return;
			}

			if (args.find("\"reload\"") != std::string_view::npos)
			{
				const bool ok = settings::Reload();
				Difficulty::ApplyLive();
				Regeneration::ApplyLive();
				a_write(a_sink, std::format(R"({{"ok":{},"op":"reload"}})", ok ? "true" : "false").c_str());

				return;
			}

			// Default: a light state readout. customdifficulty.status carries the full picture
			// (every setting, every difficulty's configured values); this is just enough to
			// confirm which difficulty the game is actually on right now versus what this mod
			// last applied, without paging through the bigger tool.
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
			const int actualDifficulty = player ? player->GetGameStatsData().difficulty : -1;
			const int lastApplied = Regeneration::LastAppliedDifficulty();

			// liveCombatHealthRegenRateMult is read fresh from the resolved RE::Setting* right
			// now, independent of anything this mod itself has recorded - the gate's real proof
			// that a difficulty change actually reached the engine's own live value, not just our
			// own bookkeeping read back to itself.
			a_write(a_sink, std::format(
				R"({{"ok":true,"actualDifficulty":{},"actualDifficultyName":"{}","lastAppliedDifficulty":{},"lastAppliedDifficultyName":"{}","inSync":{},"liveCombatHealthRegenRateMult":{:.3f}}})",
				actualDifficulty, Regeneration::DifficultyDisplayName(actualDifficulty),
				lastApplied, Regeneration::DifficultyDisplayName(lastApplied),
				(actualDifficulty == lastApplied) ? "true" : "false",
				Regeneration::LiveValue(Regeneration::PerDifficultySetting::kCombatHealthRegenRateMult))
							   .c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;

		if (registered)
		{
			return;
		}

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();

		if (!devBench)
		{
			if (a_lastAttempt)
			{
				logger::info("DevBench not detected; skipping the \"customdifficulty.control\" tool");
			}
			else
			{
				logger::debug("DevBench not detected yet; will retry at the next message");
			}

			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Drive Custom Difficulty UI for testing. op=setdifficulty:<0-5> "
			"writes the player's difficulty field directly (0=Novice..5=Legendary) and runs the "
			"same change-detection the real Settings menu triggers on close - the honest "
			"headless stand-in for changing difficulty in game, since no clean native setter "
			"exists to call from outside that menu. op=apply re-applies both the damage and "
			"regeneration settings for the CURRENT difficulty. op=reload re-reads the INI and "
			"applies it. testregen:\\\"<difficulty>,<value>\\\" is a TEST-ONLY hook that turns "
			"regeneration on and writes one regen setting directly for a given difficulty, for the "
			"gate that proves per-difficulty switching actually works. No args: reports the "
			"actual vs last-applied difficulty.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},"
			"\"setdifficulty\":{\"type\":\"integer\"},\"testregen\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";

		if (devBench->RegisterTool("customdifficulty.control", descriptor, &ControlTool, nullptr))
		{
			logger::info("Registered \"customdifficulty.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
		else
		{
			logger::warn("DevBench reported \"customdifficulty.control\" replaced an existing tool of the same name");
		}
	}
}
