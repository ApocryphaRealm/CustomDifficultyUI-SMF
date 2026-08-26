#pragma once

// This mod's real mechanic - see CLAUDE.md rule 30 (ask the object, don't infer). Custom
// Difficulty UI (Nexus 14362) ships its actual Papyrus source in the archive, not just a
// compiled .pex, so this wasn't reverse-engineered - CustomDifficultyUIControlScript.psc's
// SetPlayerSettings()/SetDefaultSettings() were read directly: the entire mechanic is twelve
// calls to the SKSE-native Papyrus function Game.SetGameSettingFloat(), one per difficulty
// level (Very Easy/Easy/Normal/High/Very High/Legendary) times two directions (damage the
// player deals "By", damage the player takes "To"). The native equivalent is
// RE::GameSettingCollection - see below.
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

	// Resolves all twelve RE::Setting* pointers via RE::GameSettingCollection::GetSingleton().
	// Returns false (logging exactly which name failed) if any are missing - these are all
	// vanilla base-game settings, so a miss here would mean something is badly wrong with the
	// game installation, not a normal condition. Call once, at kMessage_DataLoaded.
	bool Init();

	// Writes every one of the twelve GameSettings to whatever settings::difficulty currently
	// says - or, if settings::difficulty::enabled is false, to vanilla's own real default
	// values (matching upstream's own SetDefaultSettings(), not just "leave them as they
	// were" - so turning the mod off is a real reset, not a freeze). Safe to call any time
	// after Init() has succeeded; called at load and every time a setting changes.
	void ApplyLive();
}
