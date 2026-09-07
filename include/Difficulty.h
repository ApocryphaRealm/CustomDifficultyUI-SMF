#pragma once

// This mod's real mechanic - see CLAUDE.md rule 30 (ask the object, don't infer). Custom
// Difficulty UI (Nexus 14362) ships its actual Papyrus source in the archive, not just a
// compiled .pex, so this wasn't reverse-engineered - CustomDifficultyUIControlScript.psc's
// SetPlayerSettings()/SetDefaultSettings() were read directly: the entire mechanic is twelve
// calls to the SKSE-native Papyrus function Game.SetGameSettingFloat(), one per difficulty
// level (Very Easy/Easy/Normal/High/Very High/Legendary) times two directions (damage the
// player deals "By", damage the player takes "To"). The native equivalent is
// RE::GameSettingCollection - see below.
//
// 1.0.5 (plan: character-progression-control section 21, shared with that mod's Difficulty tab):
//   * OFF WRITES NOTHING (logic library 43). The twelve are captured at kDataLoaded - after every
//     plugin's records - as the LOADED values: vanilla on a plain game, Blade and Blunt's or
//     Requiem's numbers when they are installed. Disabled never writes; an on->off flip hands the
//     loaded values back once. The compiled vanilla numbers are the slider defaults only.
//   * ONE PAIR FOR EVERY DIFFICULTY (Yet Another Difficulty Mod's Simple mode).
//   * DIFFICULTY BY LEVEL (its Dynamic mode): six level thresholds; on a save load and on every
//     level-up the highest difficulty whose level the player has reached becomes the game's
//     difficulty, through the same field the Settings menu writes plus iDifficulty:Gameplay.
//   * THE BUILT-IN PATCH: BladeAndBlunt.esp and Requiem.esp are detected, presets fill the table
//     from their numbers, and while enabled this mod writes last - from the SKSE task queue after
//     the other plugins' handlers of the same event, and again on every level-up.
#include <array>
#include <cstddef>
#include <string>

namespace Difficulty
{
	// One of the twelve GameSettings this mod touches, in the same order Settings.h's own
	// fields are declared - kept as an array (rather than twelve named pointers) so Init()/
	// ApplyLive() can loop instead of repeating the same twelve lines twice.
	enum class Setting : std::size_t
	{
		kToPCVE = 0, kToPCE, kToPCN, kToPCH, kToPCVH, kToPCL,
		kByPCVE, kByPCE, kByPCN, kByPCH, kByPCVH, kByPCL,
		kCount
	};

	// Resolves all twelve RE::Setting* pointers via RE::GameSettingCollection::GetSingleton()
	// and captures each one's current value as the LOADED value. Detects the overhauls and reads
	// Blade and Blunt's INI; installs the level-up listener. Returns false (logging exactly which
	// name failed) if any are missing. Call once, at kMessage_DataLoaded.
	bool Init();

	// Writes every one of the twelve GameSettings to whatever settings::difficulty currently
	// says (the shared pair six times when that mode is on). Disabled: writes nothing, except
	// once after an on->off flip, when the loaded values are handed back. Safe to call any time
	// after Init() has succeeded; called at load, on a settings-page change and on level-up.
	void ApplyLive();
	void RequestApplyLive();   // from the SKSE task queue - lands after the other plugins' handlers

	// A save loaded or a new game started: the level rule runs, then both this module and the
	// regeneration module re-apply from the task queue.
	void OnGameLoaded();

	// The game's difficulty as the player record holds it (0 Novice .. 5 Legendary), -1 without a
	// player. SetGameDifficulty writes that field and iDifficulty:Gameplay the way the Settings
	// menu does, then lets the regeneration module re-check.
	int CurrentDifficulty();
	bool SetGameDifficulty(int a_difficulty, const char* a_reason);

	// The difficulty-by-level rule. Returns the difficulty it settled on, or -1.
	int ApplyLevelRule(const char* a_reason);
	int LevelRuleTarget(int a_level);   // what the table says for a level, -1 when no row applies

	// Per-setting readings for the page and the tools.
	const char* Name(Setting a_setting);
	float LoadedValue(Setting a_setting);   // captured at kDataLoaded (the vanilla default if unresolved)
	float LiveValue(Setting a_setting);     // straight from the resolved RE::Setting
	float& ConfiguredValue(Setting a_setting);

	// Presets that fill the twelve configured values.
	void UseLoadedValues();
	void UseVanillaValues();
	void UseBladeAndBlunt();   // its ESP's records, read 2026-09-06 under its open permissions
	void UseRequiem();         // all 1.0 - its repository's records

	// The overhauls the built-in patch knows. BladeAndBluntLevelScaling: its INI's
	// bLevelBasedDifficulty as read at Init (a_found false when the file or key is absent).
	std::string OverhaulLoaded();   // "" when none
	bool BladeAndBluntPresent();
	bool RequiemPresent();
	bool BladeAndBluntLevelScaling(bool& a_found);

	std::string StatusJson();
}
