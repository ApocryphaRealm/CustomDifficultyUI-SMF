#pragma once

// PER-DIFFICULTY REGENERATION CONTROL - see "4. plans\custom-difficulty-ui-regeneration\plan.md"
// for the full design. Vanilla has no per-difficulty regeneration at all: every setting this
// file drives (fCombatHealthRegenRateMult and friends) is ONE real GameSetting, global to every
// difficulty. This mod adds the per-difficulty behaviour itself by storing one value per
// difficulty (settings::regeneration::*) and writing whichever slot matches the CURRENT
// difficulty into that one real GameSetting - so switching difficulty in the game's own menu
// changes which stored value applies, live, with no visit to this mod's own settings page.
//
// Setting names were read out of a published settings mod's compiled script (see the plan for
// where), so they are Bethesda's own names, not anyone's asset - but they are still resolved and
// verified against the RUNNING GAME at Init(), never assumed to exist just because they were
// read somewhere. A name that does not resolve is skipped, logged, and its slider does not
// appear on the page - see HasResolved()/RenderableCount() usage in UI.cpp.
namespace Regeneration
{
	// Which of the seven per-difficulty settings, in storage order - kept as an array (rather
	// than seven named accessors) so Init()/ApplyLive()/RestoreDefaults() can loop instead of
	// repeating each by name, matching Difficulty.h's own reasoning.
	enum class PerDifficultySetting : std::size_t
	{
		kCombatHealthRegenRateMult = 0,
		kCombatMagickaRegenRateMult,
		kCombatStaminaRegenRateMult,
		kDamagedHealthRegenDelay,
		kDamagedMagickaRegenDelay,
		kDamagedStaminaRegenDelay,
		kDamagedAVRegenDelay,
		kCount
	};

	// The five settings that are NOT per-difficulty (design decision in the plan: the delay
	// ceilings and the two situational values do not need six copies each).
	enum class GlobalSetting : std::size_t
	{
		kHealthRegenDelayMax = 0,
		kMagickaRegenDelayMax,
		kStaminaRegenDelayMax,
		kOutOfBreathStaminaRegenDelay,
		kEssentialDownCombatHealthRegenMult,
		kCount
	};

	// Resolves every RE::Setting* this file needs via RE::GameSettingCollection::GetSingleton(),
	// AND captures each one's CURRENT value as this mod's own record of "real vanilla" - there is
	// no Papyrus source to read these from the way Difficulty::Init() reads its twelve, so the
	// running game is the only honest source. Any settings::regeneration:: value still holding
	// settings::regeneration::kUnset (meaning nothing was ever saved for it) is seeded from that
	// same captured vanilla value, for every difficulty - so a fresh install is identical on
	// every difficulty until the player changes something, exactly like the difficulty half.
	// Returns false (logging exactly which name failed) if any setting could not be resolved -
	// these are being asserted to exist, per the plan, not assumed; a failure here means this
	// setting is skipped everywhere else, not that the whole feature refuses to work. Call once,
	// at kDataLoaded, after Difficulty::Init().
	bool Init();

	// True once Init() has successfully resolved this setting - UI.cpp uses this to decide
	// whether to draw a control for it at all, per the plan's "drop a control rather than write
	// to a name that does not exist."
	bool HasResolved(PerDifficultySetting a_setting);
	bool HasResolved(GlobalSetting a_setting);

	// The real vanilla value Init() captured for this setting (0.0F if it never resolved).
	// Used by RestoreDefaults() and by the settings page's own "vanilla was X" readout.
	float VanillaDefault(PerDifficultySetting a_setting);
	float VanillaDefault(GlobalSetting a_setting);

	// Reads RE::PlayerCharacter's current difficulty (clamped 0..5; defaults to Adept/2 if the
	// player does not exist yet or the value is out of range) and writes every resolved
	// GameSetting to whatever settings::regeneration currently says for THAT difficulty - or, if
	// settings::regeneration::enabled is false, to the real vanilla value Init() captured, for
	// every one of the seven per-difficulty settings and all five global ones (matching
	// Difficulty::ApplyLive()'s own "off is a real reset" contract). Safe to call any time after
	// Init() has succeeded; called at load, on a settings-page change, and by
	// CheckAndApplyIfChanged() below when the difficulty has actually moved.
	void ApplyLive();

	// The event-driven difficulty-change detector the plan calls for - no polling. Reads the
	// current difficulty, compares it against whatever ApplyLive() last actually used, and calls
	// ApplyLive() again ONLY if it moved. Registered against RE::UI's MenuOpenCloseEvent (checked
	// whenever any menu closes, which is cheap - one int read and compare - and covers the game's
	// own Settings menu without this mod needing to know that menu's name); also worth calling
	// directly after any devbench-driven change to the game's difficulty field, for testing.
	void CheckAndApplyIfChanged();

	// Registers the MenuOpenCloseEvent sink above with RE::UI. Safe to call once; call after
	// Init() has succeeded (kDataLoaded), since the sink's own handler assumes the setting
	// pointers are already resolved.
	void RegisterMenuEventSink();

	// Puts every settings::regeneration configured value back to the REAL vanilla value Init()
	// captured - for every difficulty, in the per-difficulty case - not a compiled-in guess,
	// since none exists for these settings the way it does for the twelve difficulty ones. Only
	// touches the variables; follow with settings::Save() to persist and ApplyLive() to show it
	// in game, matching settings::RestoreDefaults()'s own contract.
	void RestoreDefaults();

	// Copy helpers for the settings page (the plan: "nobody wants to type six sets by hand").
	// Both touch every one of the seven per-difficulty settings at once, never the five global
	// ones (which have no per-difficulty concept to copy). Neither applies the result to the game
	// - follow with ApplyLive() to show it live, matching every other settings-page mutation here.

	// Every per-difficulty setting's a_from slot is copied into every OTHER difficulty, so all
	// six become identical to a_from. No-op if a_from is out of range.
	void CopyToAllDifficulties(int a_from);

	// Every per-difficulty setting's a_from slot is copied into JUST a_to, leaving every other
	// difficulty untouched. No-op if either index is out of range, or if they are equal.
	void CopyDifficulty(int a_from, int a_to);

	// The difficulty index ApplyLive() actually used last time it ran (0..5), and the six display
	// names in that order - both used by the settings page's "Current difficulty: X, X's values
	// are active" readout and by the "customdifficulty.status" DevBench tool.
	// Reads the setting's REAL, LIVE value straight from the resolved RE::Setting* right now -
	// not settings::regeneration's own configured array, which only says what we last WROTE and
	// proves nothing about whether it actually reached the engine. Used by the gate that proves
	// the point of this feature: independent proof, not our own bookkeeping read back to itself.
	// 0.0F if the setting never resolved.
	float LiveValue(PerDifficultySetting a_setting);

	int LastAppliedDifficulty();
	const char* DifficultyDisplayName(int a_difficulty);
}
