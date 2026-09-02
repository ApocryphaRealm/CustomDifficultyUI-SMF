#include "Diagnostics.h"

#include "DevBench/DevBenchAPI.h"
#include "Regeneration.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <mutex>

namespace diagnostics
{
	namespace
	{
		using clock = std::chrono::steady_clock;

		std::mutex mtx;

		struct State
		{
			std::uint64_t applyCount = 0;
			std::optional<clock::time_point> lastApply;
			bool lastEnabled = false;
		};

		struct RegenState
		{
			std::uint64_t applyCount = 0;
			std::optional<clock::time_point> lastApply;
			bool lastEnabled = false;
			int lastDifficulty = -1;
		};

		State state;
		RegenState regenState;

		std::string SecondsAgoField(const char* a_name, const std::optional<clock::time_point>& a_when)
		{
			if (!a_when)
			{
				return std::format("\"{}SecondsAgo\": null", a_name);
			}

			const double seconds = std::chrono::duration<double>(clock::now() - *a_when).count();

			return std::format("\"{}SecondsAgo\": {:.1f}", a_name, seconds);
		}

		// One per-difficulty setting as a JSON array of six numbers, in Novice..Legendary order -
		// the plain shape a gate script can index straight into (values[0]..values[5]).
		std::string PerDifficultyField(const char* a_name, const std::array<float, settings::regeneration::kDifficultyCount>& a_values, bool a_resolved)
		{
			return std::format(
				"\"{}\":{{\"resolved\":{},\"values\":[{:.3f},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f}]}}",
				a_name, a_resolved ? "true" : "false",
				a_values[0], a_values[1], a_values[2], a_values[3], a_values[4], a_values[5]);
		}

		std::string GlobalField(const char* a_name, float a_value, bool a_resolved)
		{
			return std::format("\"{}\":{{\"resolved\":{},\"value\":{:.3f}}}", a_name, a_resolved ? "true" : "false", a_value);
		}

		// The full regeneration block: which setting resolved, the six-difficulty (or single)
		// configured value(s), the currently-ACTIVE difficulty per RE::PlayerCharacter, the
		// difficulty ApplyLive() last actually used, and when it last ran - everything a gate
		// needs to prove the point of the feature: that these change when difficulty does.
		std::string RegenerationJson()
		{
			using namespace settings::regeneration;
			using PDS = Regeneration::PerDifficultySetting;
			using GS = Regeneration::GlobalSetting;

			const int lastApplied = Regeneration::LastAppliedDifficulty();

			return std::format(
				"{{"
				"\"enabled\":{},"
				"\"lastAppliedDifficulty\":{},"
				"\"lastAppliedDifficultyName\":\"{}\","
				"\"perDifficulty\":{{{},{},{},{},{},{},{}}},"
				"\"global\":{{{},{},{},{},{}}},"
				"\"lastApply\":{{\"count\":{},\"wasEnabled\":{},{}}}"
				"}}",
				enabled ? "true" : "false",
				lastApplied,
				Regeneration::DifficultyDisplayName(lastApplied),
				PerDifficultyField("combatHealthRegenRateMult", combatHealthRegenRateMult, Regeneration::HasResolved(PDS::kCombatHealthRegenRateMult)),
				PerDifficultyField("combatMagickaRegenRateMult", combatMagickaRegenRateMult, Regeneration::HasResolved(PDS::kCombatMagickaRegenRateMult)),
				PerDifficultyField("combatStaminaRegenRateMult", combatStaminaRegenRateMult, Regeneration::HasResolved(PDS::kCombatStaminaRegenRateMult)),
				PerDifficultyField("damagedHealthRegenDelay", damagedHealthRegenDelay, Regeneration::HasResolved(PDS::kDamagedHealthRegenDelay)),
				PerDifficultyField("damagedMagickaRegenDelay", damagedMagickaRegenDelay, Regeneration::HasResolved(PDS::kDamagedMagickaRegenDelay)),
				PerDifficultyField("damagedStaminaRegenDelay", damagedStaminaRegenDelay, Regeneration::HasResolved(PDS::kDamagedStaminaRegenDelay)),
				PerDifficultyField("damagedAVRegenDelay", damagedAVRegenDelay, Regeneration::HasResolved(PDS::kDamagedAVRegenDelay)),
				GlobalField("healthRegenDelayMax", healthRegenDelayMax, Regeneration::HasResolved(GS::kHealthRegenDelayMax)),
				GlobalField("magickaRegenDelayMax", magickaRegenDelayMax, Regeneration::HasResolved(GS::kMagickaRegenDelayMax)),
				GlobalField("staminaRegenDelayMax", staminaRegenDelayMax, Regeneration::HasResolved(GS::kStaminaRegenDelayMax)),
				GlobalField("outOfBreathStaminaRegenDelay", outOfBreathStaminaRegenDelay, Regeneration::HasResolved(GS::kOutOfBreathStaminaRegenDelay)),
				GlobalField("essentialDownCombatHealthRegenMult", essentialDownCombatHealthRegenMult, Regeneration::HasResolved(GS::kEssentialDownCombatHealthRegenMult)),
				regenState.applyCount,
				regenState.lastEnabled ? "true" : "false",
				SecondsAgoField("last", regenState.lastApply));
		}

		void StatusTool(void*, const char*, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			std::string json;

			{
				std::scoped_lock lock(mtx);
				using namespace settings::difficulty;

				json = std::format(
					"{{"
					"\"settings\":{{"
					"\"enabled\":{},"
					"\"toPC\":{{\"veryEasy\":{:.2f},\"easy\":{:.2f},\"normal\":{:.2f},\"high\":{:.2f},"
					"\"veryHigh\":{:.2f},\"legendary\":{:.2f}}},"
					"\"byPC\":{{\"veryEasy\":{:.2f},\"easy\":{:.2f},\"normal\":{:.2f},\"high\":{:.2f},"
					"\"veryHigh\":{:.2f},\"legendary\":{:.2f}}}"
					"}},"
					"\"lastApply\":{{"
					"\"count\":{},"
					"\"wasEnabled\":{},"
					"{}"
					"}},"
					"\"regeneration\":{}"
					"}}",
					enabled ? "true" : "false",
					toPCVE, toPCE, toPCN, toPCH, toPCVH, toPCL,
					byPCVE, byPCE, byPCN, byPCH, byPCVH, byPCL,
					state.applyCount,
					state.lastEnabled ? "true" : "false",
					SecondsAgoField("last", state.lastApply),
					RegenerationJson());
			}

			a_write(a_sink, json.c_str());
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
				logger::info("DevBench not detected; skipping the \"customdifficulty.status\" live-diagnostics "
							 "tool (logging alone still covers this session - see CLAUDE.md rule 31)");
			}
			else
			{
				logger::debug("DevBench not detected yet; will retry at the next message");
			}

			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Live Custom Difficulty UI state: current per-difficulty multipliers, "
			"and when they were last (re-)applied to the game's own GameSettings.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{}},"
			"\"readOnly\":true"
			"}";

		if (devBench->RegisterTool("customdifficulty.status", descriptor, &StatusTool, nullptr))
		{
			logger::info("Registered \"customdifficulty.status\" with DevBench (build {})", devBench->GetBuildNumber());
		}
		else
		{
			logger::warn("DevBench reported \"customdifficulty.status\" replaced an existing tool of the same name");
		}

		registered = true;
	}

	void RecordApplied(bool a_enabled)
	{
		std::scoped_lock lock(mtx);

		++state.applyCount;
		state.lastApply = clock::now();
		state.lastEnabled = a_enabled;
	}

	void RecordRegenerationApplied(bool a_enabled, int a_difficulty)
	{
		std::scoped_lock lock(mtx);

		++regenState.applyCount;
		regenState.lastApply = clock::now();
		regenState.lastEnabled = a_enabled;
		regenState.lastDifficulty = a_difficulty;
	}
}
