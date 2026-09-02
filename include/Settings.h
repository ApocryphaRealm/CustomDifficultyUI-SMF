#pragma once

#include <array>
#include <cstddef>

namespace SKSE::log
{
	using level = spdlog::level::level_enum;
}
namespace logger = SKSE::log;

namespace settings
{
	// Reads the INI into the variables below. The values the variables hold when this is
	// called are remembered as the built-in defaults, so RestoreDefaults() can put them back.
	void Init(const std::string& a_iniFileName);

	// Writes every setting below back to the INI that Init() read, leaving the comments and
	// any unrelated keys in that file alone. Returns false if the file could not be written.
	bool Save();

	// Puts every setting back to its built-in default. This only touches the variables;
	// follow it with Save() to persist, and with UI::ApplyLiveSettings() to show it in game.
	void RestoreDefaults();

	// Re-reads the INI that Init() read, discarding any unsaved change made since. Returns
	// false if the file could not be read, leaving the current values alone.
	bool Reload();

	// Full path of the INI Init() read, or an empty string before Init() has run.
	const std::string& GetIniPath();

	namespace debug
	{
		// Ships at trace by default (project standard) so a submitted log carries the detail
		// needed to diagnose a compatibility, timing or stability report without asking the
		// reporter to change anything first - see CLAUDE.md rule 31.
		inline logger::level logLevel = logger::level::trace;
	}

	// Every default below is vanilla's own real value (from
	// CustomDifficultyUIControlScript.psc's own SetDefaultSettings(), read directly from the
	// original archive's included Papyrus source - not guessed), so a fresh install changes
	// nothing about difficulty behaviour until "Enabled" is turned on and a value actually
	// changed. "ToPC" = damage dealt TO the player (by enemies); "ByPC" = damage dealt BY the
	// player. VE/E/N/H/VH/L = Very Easy/Easy/Normal/High/Very High/Legendary.
	namespace difficulty
	{
		// Master toggle, off by default - matches upstream's own apparent default (its
		// CustomDifficultyUIEnabled GlobalVariable ships at 0; the mod is inert until the
		// player explicitly opens the MCM and turns it on). Off resets every GameSetting below
		// to its real vanilla value (see Difficulty::ApplyLive), not just "stop touching them."
		inline bool enabled = false;

		inline float toPCVE = 0.50F;
		inline float toPCE = 0.75F;
		inline float toPCN = 1.00F;
		inline float toPCH = 1.50F;
		inline float toPCVH = 2.00F;
		inline float toPCL = 3.00F;

		inline float byPCVE = 2.00F;
		inline float byPCE = 1.50F;
		inline float byPCN = 1.00F;
		inline float byPCH = 0.75F;
		inline float byPCVH = 0.50F;
		inline float byPCL = 0.25F;
	}

	// Regeneration control, added 2026-09-02 per plan
	// "4. plans\custom-difficulty-ui-regeneration\plan.md". Unlike difficulty above, vanilla has
	// no per-difficulty regeneration at all - each setting below is ONE real GameSetting, not
	// six - so there is no Papyrus source to read real vanilla numbers from. Instead every value
	// here starts at kUnset (a negative sentinel no real rate/delay/multiplier can legitimately
	// hold) and Regeneration::Init() overwrites it with whatever the RUNNING GAME reports the
	// first time this mod ever loads, before anything is ever applied. A value already read from
	// the INI (a real number, not the sentinel) is left alone. See Regeneration.h/.cpp for the
	// per-difficulty switching and the vanilla-restore path.
	namespace regeneration
	{
		// Negative because every setting below is a rate, delay or multiplier and vanilla never
		// uses a negative one - so this can never be confused with a real saved value.
		inline constexpr float kUnset = -1.0F;

		// Index 0..5 = Novice/Apprentice/Adept/Expert/Master/Legendary, matching
		// RE::PlayerCharacter::GetGameStatsData().difficulty. See Regeneration.h.
		inline constexpr std::size_t kDifficultyCount = 6;

		// Off by default - matches the difficulty half's own convention. Off restores every
		// GameSetting below to the real value captured at Init(), not just "stop touching them."
		inline bool enabled = false;

		// PER-DIFFICULTY (the point of this feature): one stored value per difficulty for each
		// of these seven settings; whichever slot matches the CURRENT difficulty is the one
		// written into the game's one real GameSetting of that name.
		inline std::array<float, kDifficultyCount> combatHealthRegenRateMult{ kUnset, kUnset, kUnset, kUnset, kUnset, kUnset };
		inline std::array<float, kDifficultyCount> combatMagickaRegenRateMult{ kUnset, kUnset, kUnset, kUnset, kUnset, kUnset };
		inline std::array<float, kDifficultyCount> combatStaminaRegenRateMult{ kUnset, kUnset, kUnset, kUnset, kUnset, kUnset };
		inline std::array<float, kDifficultyCount> damagedHealthRegenDelay{ kUnset, kUnset, kUnset, kUnset, kUnset, kUnset };
		inline std::array<float, kDifficultyCount> damagedMagickaRegenDelay{ kUnset, kUnset, kUnset, kUnset, kUnset, kUnset };
		inline std::array<float, kDifficultyCount> damagedStaminaRegenDelay{ kUnset, kUnset, kUnset, kUnset, kUnset, kUnset };
		inline std::array<float, kDifficultyCount> damagedAVRegenDelay{ kUnset, kUnset, kUnset, kUnset, kUnset, kUnset };

		// NOT per-difficulty (design decision in the plan): the three ceilings only matter once
		// a delay above has been raised past them, and the two situational values are edge cases
		// rather than the reason this feature exists. One value each, for every difficulty.
		inline float healthRegenDelayMax = kUnset;
		inline float magickaRegenDelayMax = kUnset;
		inline float staminaRegenDelayMax = kUnset;
		inline float outOfBreathStaminaRegenDelay = kUnset;
		inline float essentialDownCombatHealthRegenMult = kUnset;
	}
}
