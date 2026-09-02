#pragma once

namespace UI
{
	// Adds this mod's page to the SKSE Menu Framework's Mod Control Panel. Safe to call when
	// the framework is missing or too old to drive: it logs why and does nothing else.
	void Register();

	namespace SettingsPanel
	{
		void __stdcall Render();

		// The per-difficulty regeneration page, added 2026-09-02 - see
		// "4. plans\custom-difficulty-ui-regeneration\plan.md". A separate page rather than a
		// section on Render() above, since twelve settings across six difficulties is seventy-two
		// numbers - too much for one page alongside the existing difficulty sliders.
		void __stdcall RenderRegeneration();
	}
}
