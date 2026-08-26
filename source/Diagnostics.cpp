#include "Diagnostics.h"

#include "DevBench/DevBenchAPI.h"
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

		State state;

		std::string SecondsAgoField(const char* a_name, const std::optional<clock::time_point>& a_when)
		{
			if (!a_when)
			{
				return std::format("\"{}SecondsAgo\": null", a_name);
			}

			const double seconds = std::chrono::duration<double>(clock::now() - *a_when).count();

			return std::format("\"{}SecondsAgo\": {:.1f}", a_name, seconds);
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
					"}}"
					"}}",
					enabled ? "true" : "false",
					toPCVE, toPCE, toPCN, toPCH, toPCVH, toPCL,
					byPCVE, byPCE, byPCN, byPCH, byPCVH, byPCL,
					state.applyCount,
					state.lastEnabled ? "true" : "false",
					SecondsAgoField("last", state.lastApply));
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
}
