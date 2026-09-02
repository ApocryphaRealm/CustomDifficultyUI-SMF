#pragma once

// Backs the WRITE-capable "customdifficulty.control" DevBench tool - separate from
// Diagnostics.h's read-only "customdifficulty.status" (CLAUDE.md rule 31), so a tool that can
// change things stays visibly distinct from one that only reports them.
namespace DevBenchTool
{
	// Looks up the DevBench interface and registers "customdifficulty.control". Same rule-17
	// retry contract as Diagnostics::Init() - call again at kPostPostLoad and kDataLoaded, a
	// no-op after the first success.
	void Init(bool a_lastAttempt = false);
}
