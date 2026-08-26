#include "Diagnostics.h"
#include "Difficulty.h"
#include "Settings.h"
#include "UI.h"
#include "utils/Logger.h"

void SKSEMessageListener(SKSE::MessagingInterface::Message* a_msg)
{
	if (!a_msg)
	{
		return;
	}

	switch (a_msg->type)
	{
	case SKSE::MessagingInterface::kPostLoad:
		// DevBenchAPI's own contract: the interface can only be requested once SKSE has sent
		// kPostLoad, since that's the earliest point every plugin (DevBench included) has had
		// its own SKSEPluginLoad run.
		logger::debug("kPostLoad received; registering live diagnostics with DevBench if present");
		diagnostics::Init();
		break;

	case SKSE::MessagingInterface::kPostPostLoad:
		// By kPostPostLoad every plugin has finished its own post-load work, so SKSE Menu
		// Framework's module is guaranteed to be in the process if it is installed at all.
		logger::debug("kPostPostLoad received; registering settings page with SKSE Menu Framework");
		UI::Register();

		// Rule-17 retry: devbench's own server can still be finishing startup a moment after
		// kPostLoad, which is early enough to lose the race even though kPostLoad is
		// DevBenchAPI's own documented earliest-safe point (see AutoDraw-SMF's own
		// MessageListeners.cpp, where this was confirmed from a real launch's timestamps).
		// Cheap no-op if the kPostLoad attempt already succeeded.
		diagnostics::Init();
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		// The twelve vanilla difficulty GameSettings are guaranteed available by kDataLoaded -
		// resolve them, then apply once immediately, matching upstream's own
		// CustomDifficultyUILoadScript.OnInit() -> Maintenance() -> ControlScript.reset().
		logger::debug("kDataLoaded received; initializing Difficulty");
		if (Difficulty::Init())
		{
			Difficulty::ApplyLive();
		}

		// Last DevBench retry point - if it still isn't found here, conclude it isn't
		// installed and say so, rather than staying silent about it forever.
		diagnostics::Init(/* a_lastAttempt = */ true);
		break;

	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		// Matches upstream's own CustomDifficultyUILoadScript.OnPlayerLoadGame() - re-applies
		// on every later save load (and a fresh New Game) too, not just the game's first boot,
		// since GameSettings are process-global and a different save doesn't carry this
		// plugin's own values with it the way an actor value would.
		logger::debug("kPostLoadGame/kNewGame received; re-applying difficulty settings");
		Difficulty::ApplyLive();
		break;

	default:
		break;
	}
}
