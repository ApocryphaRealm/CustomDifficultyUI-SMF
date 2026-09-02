#include "Diagnostics.h"
#include "DevBenchTool.h"
#include "Difficulty.h"
#include "Regeneration.h"
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
		DevBenchTool::Init();
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
		DevBenchTool::Init();
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

		// Regeneration control (added 2026-09-02, per-difficulty regeneration -
		// see "4. plans\custom-difficulty-ui-regeneration\plan.md"). Init() resolves the twelve
		// real GameSettings and captures their live values as this mod's own record of vanilla,
		// since - unlike the twelve difficulty settings above - there is no Papyrus source to
		// read these from. ApplyLive() then does the initial write for whatever difficulty the
		// player is on; RegisterMenuEventSink() wires up the event-driven re-check on every
		// later menu close, so a difficulty change in the game's own Settings menu is picked up
		// with no polling and no visit to this mod's page.
		logger::debug("kDataLoaded received; initializing Regeneration");
		if (Regeneration::Init())
		{
			Regeneration::ApplyLive();
		}
		Regeneration::RegisterMenuEventSink();

		// Last DevBench retry point - if it still isn't found here, conclude it isn't
		// installed and say so, rather than staying silent about it forever.
		diagnostics::Init(/* a_lastAttempt = */ true);
		DevBenchTool::Init(/* a_lastAttempt = */ true);
		break;

	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		// Matches upstream's own CustomDifficultyUILoadScript.OnPlayerLoadGame() - re-applies
		// on every later save load (and a fresh New Game) too, not just the game's first boot,
		// since GameSettings are process-global and a different save doesn't carry this
		// plugin's own values with it the way an actor value would.
		logger::debug("kPostLoadGame/kNewGame received; re-applying difficulty and regeneration settings");
		Difficulty::ApplyLive();

		// A hard re-apply, not just a change-check: unlike the menu-close path, a fresh load is
		// exactly when the plan says to unconditionally "read the difficulty, apply that set" -
		// there is no meaningful "last applied" value carried over from a previous game session.
		Regeneration::ApplyLive();
		break;

	default:
		break;
	}
}
