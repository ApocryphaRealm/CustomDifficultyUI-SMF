#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Difficulty.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Toggle.h"

#include <algorithm>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		// The slider the arrow keys currently drive. Set by clicking one.
		std::string selectedSlider;

		constexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount = 7;

		// The framework renders from the renderer's present hook, which is not the thread the
		// game's own systems expect to be talked to from - anything beyond touching this
		// plugin's own settings variables has to be handed to the main thread first. This
		// mirrors upstream's own OnPlayerLoadGame()-triggered reset, just from a settings-page
		// change instead.
		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface())
			{
				taskInterface->AddTask(std::move(a_task));
			}
		}

		// See AutoDraw-SMF/source/UI.cpp's identical check (itself ported from
		// CompassNavigationOverhaul/source/UI.cpp - CLAUDE.md rule 24) for the reasoning: older
		// SMF builds do not export every cimgui function a page needs, and calling through a
		// null function pointer crashes on the first draw rather than failing to register - so
		// every export this page's widgets resolve at runtime is probed here first, by its
		// *resolved* name (varargs widgets resolve to a "...V"-suffixed export).
		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextDisabledV",
				"igTextWrappedV",
				"igSetTooltipV",
				"igSeparatorText",
				"igCombo_Str_arr",
				"igSliderFloat",
				"igIsItemHovered",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igPushItemWidth",
				"igPopItemWidth",
				// Needed by NudgeableSlider's arrow-key nudge.
				"igIsKeyPressed_Bool",
				"igIsItemClicked",
				"igIsItemActive",
				// Needed by utils/Toggle.h's hand-drawn switch (CLAUDE.md rule 32 - boolean
				// settings render as a switch, not a checkbox).
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igGetFrameHeight",
				"igInvisibleButton",
				"igPushID_Str",
				"igPopID",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
			};

			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("SKSE Menu Framework does not export \"{}\"", name);

					return false;
				}
			}

			return true;
		}

		// A slider that the arrow keys can also nudge, once it has been clicked. Ported
		// verbatim from Dragon's Eye Minimap's UI.cpp via AutoDraw-SMF/PerkReallocation-SMF -
		// CLAUDE.md rule 24.
		bool NudgeableSlider(const char* a_label, float* a_value, float a_min, float a_max,
							 const char* a_format, float a_step)
		{
			bool changed = ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);

			if (ImGuiMCP::IsItemClicked() || ImGuiMCP::IsItemActive())
			{
				selectedSlider = a_label;
			}

			if (selectedSlider == a_label)
			{
				float nudge = 0.0F;

				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_DownArrow))
				{
					nudge -= a_step;
				}
				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_UpArrow))
				{
					nudge += a_step;
				}

				if (nudge != 0.0F)
				{
					*a_value = std::clamp(*a_value + nudge, a_min, a_max);
					changed = true;
				}

				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("<-->");
			}

			return changed;
		}

		void HelpMarker(const char* a_description)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("(?)");

			if (ImGuiMCP::IsItemHovered())
			{
				ImGuiMCP::SetTooltip("%s", a_description);
			}
		}

		// Every slider below applies live: pressed once for the whole page after any change,
		// rather than per-widget - Difficulty::ApplyLive() writes all twelve GameSettings
		// unconditionally anyway, so there is nothing to gain from a narrower per-field apply.
		void ApplyLive()
		{
			OnMainThread([]() { Difficulty::ApplyLive(); });
		}

		void RenderDifficultyLevel(const char* a_header, const char* a_toPCLabel, float* a_toPC,
			const char* a_byPCLabel, float* a_byPC)
		{
			ImGuiMCP::SeparatorText(a_header);

			if (NudgeableSlider(a_toPCLabel, a_toPC, 0.0F, 10.0F, "%.2f", 0.05F))
			{
				ApplyLive();
			}
			HelpMarker("Damage multiplier applied to hits enemies land on you at this difficulty.");

			if (NudgeableSlider(a_byPCLabel, a_byPC, 0.0F, 10.0F, "%.2f", 0.05F))
			{
				ApplyLive();
			}
			HelpMarker("Damage multiplier applied to hits you land on enemies at this difficulty.");
		}

		void RenderDifficultySection()
		{
			using namespace settings::difficulty;

			ImGuiMCP::SeparatorText("Custom Difficulty UI");

			if (ImGuiMCP::Toggle("Enabled", &enabled))
			{
				ApplyLive();
			}
			HelpMarker("Off resets every multiplier below to Skyrim's own real vanilla defaults - "
					   "not just \"stop touching them.\"");
			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("Each section below is one of Skyrim's own difficulty levels.");
			ImGuiMCP::TextDisabled("Whichever difficulty you select in game uses that section's sliders.");
			ImGuiMCP::Spacing();

			// Headed with the names the GAME shows in its own difficulty menu, not the internal
			// suffixes of the settings behind them. Each section writes the vanilla
			// fDiffMultHPToPC*/fDiffMultHPByPC* game setting for that difficulty, so the section
			// a player is editing is exactly the one that takes effect when they select that
			// difficulty in game - picking Legendary uses the Legendary sliders, with nothing
			// extra needed to connect them.
			//
			// The old headings said "Very Easy / Easy / Normal / High / Very High" - the engine's
			// internal names, and not even accurately ("Hard"/"Very Hard" internally). Nothing in
			// Skyrim's own UI ever calls a difficulty "Very High", so a player had to guess which
			// slider was the one they were actually playing on.
			//
			// The ##VE/##E/... suffixes are ImGui ID disambiguators, NOT visible text - they are
			// deliberately left alone. Changing them would give every slider a new identity and
			// silently reset any in-progress interaction state keyed on it.
			RenderDifficultyLevel("Novice", "Damage to you##VE", &toPCVE, "Damage by you##VE", &byPCVE);
			RenderDifficultyLevel("Apprentice", "Damage to you##E", &toPCE, "Damage by you##E", &byPCE);
			RenderDifficultyLevel("Adept", "Damage to you##N", &toPCN, "Damage by you##N", &byPCN);
			RenderDifficultyLevel("Expert", "Damage to you##H", &toPCH, "Damage by you##H", &byPCH);
			RenderDifficultyLevel("Master", "Damage to you##VH", &toPCVH, "Damage by you##VH", &byPCVH);
			RenderDifficultyLevel("Legendary", "Damage to you##L", &toPCL, "Damage by you##L", &byPCL);
		}

		void RenderDebugSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText("Debug");

			int level = static_cast<int>(debug::logLevel);
			if (ImGuiMCP::Combo("Log level", &level, kLogLevelNames, kLogLevelCount))
			{
				debug::logLevel = static_cast<logger::level>(level);

				OnMainThread([]() { logger::set_level(settings::debug::logLevel, settings::debug::logLevel); });
			}
			HelpMarker("Applies to the log immediately. Ships at Trace by default - see CLAUDE.md rule 31.");
		}

		void RenderButtons()
		{
			if (ImGuiMCP::Button("Save"))
			{
				OnMainThread([]() {
					statusMessage = settings::Save() ? "Settings saved." : "Could not save the INI. See the log for why.";
				});
			}
			HelpMarker("Writes every setting above back to the INI. Comments and unrelated keys are left alone.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Reload from INI"))
			{
				OnMainThread([]() {
					statusMessage = settings::Reload()
										 ? "Settings reloaded from the INI."
										 : "Could not read the INI. See the log for why.";
					Difficulty::ApplyLive();
				});
			}
			HelpMarker("Throws away any change made here since the last save, re-reads the INI from disk, and applies it immediately.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Restore defaults"))
			{
				OnMainThread([]() {
					settings::RestoreDefaults();
					Difficulty::ApplyLive();
				});

				statusMessage = "Defaults restored and applied. Press Save to keep them.";
			}
			HelpMarker("Puts every setting back to the value it has on a fresh install, and applies it immediately. Nothing is written to the INI until you press Save.");

			if (!statusMessage.empty())
			{
				ImGuiMCP::TextWrapped("%s", statusMessage.c_str());
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("SKSE Menu Framework is not installed; settings will be read from the INI only");

			return;
		}

		if (!HasRequiredExports())
		{
			logger::warn("The installed SKSE Menu Framework is older than this plugin's settings "
						 "menu needs. Update it to a newer version to configure Custom Difficulty UI in game.");

			return;
		}

		SKSEMenuFramework::SetSection("Custom Difficulty UI");
		SKSEMenuFramework::AddSectionItem("Settings", SettingsPanel::Render);

		logger::info("Registered the settings page with SKSE Menu Framework");
	}

	void __stdcall SettingsPanel::Render()
	{
		ImGuiMCP::TextWrapped("Changes below apply immediately to the game's own difficulty "
							  "damage multipliers - the same ones the vanilla difficulty slider "
							  "sets, per level. Press Save separately to keep them for next time.");
		ImGuiMCP::Spacing();

		ImGuiMCP::PushItemWidth(260.0F);

		RenderDifficultySection();
		ImGuiMCP::Spacing();

		RenderDebugSection();
		ImGuiMCP::Spacing();

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
