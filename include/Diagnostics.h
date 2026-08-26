#pragma once

// Backs the "customdifficulty.status" DevBench tool - see CLAUDE.md rule 31 (every mod's first
// version ships with live-queryable state, not just logs written after the fact).
namespace diagnostics
{
	// Looks up the DevBench interface (present only if the DevBench plugin is installed) and
	// registers "customdifficulty.status". Safe to call repeatedly - see AutoDraw-SMF's own
	// Diagnostics.h for why this is a rule-17 retry rather than a one-shot lookup: call it
	// again at kPostPostLoad and kDataLoaded too. Every call after the first successful one is
	// a cheap no-op; only the final call (a_lastAttempt = true) logs that DevBench was never
	// found, so the "not installed" conclusion is not reported before every retry is exhausted.
	void Init(bool a_lastAttempt = false);

	// The twelve GameSettings were actually (re-)applied - a_enabled says whether they were
	// applied at their configured values or reset to vanilla defaults.
	void RecordApplied(bool a_enabled);
}
