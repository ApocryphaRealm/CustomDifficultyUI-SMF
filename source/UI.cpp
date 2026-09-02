#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Difficulty.h"
#include "Regeneration.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Toggle.h"

#include <algorithm>
#include <array>
#include <string>

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

		// Same idea, for the regeneration page - Regeneration::ApplyLive() writes every resolved
		// GameSetting for whatever the CURRENT difficulty is, which is not necessarily the one
		// being edited (see g_editingDifficulty below): editing Legendary while playing on Adept
		// changes what Legendary will look like next time you play it, without touching Adept's
		// live values now. That is deliberate - the point of per-difficulty storage.
		void RegenApplyLive()
		{
			OnMainThread([]() { Regeneration::ApplyLive(); });
		}

		// Which difficulty the Regeneration page's sliders are currently bound to - purely a
		// render-thread UI concern, not a setting itself, so it lives here rather than in
		// Settings.h. Starts at Adept (2) until the page has actually been drawn once, at which
		// point it snaps to whatever difficulty is really active (see RenderRegenerationSection).
		int g_editingDifficulty = 2;
		bool g_editingDifficultyInitialized = false;

		// One row bound to the currently-edited difficulty's slot in a per-difficulty setting.
		// The raw GameSetting name is appended to the label per the plan (this mod's users
		// compare notes against wiki pages and INI guides). Draws nothing but a disabled note if
		// this setting never resolved on this runtime (the plan: "drop a control rather than
		// write to a name that does not exist").
		bool RenderPerDifficultySlider(const char* a_label, const char* a_rawName,
			std::array<float, settings::regeneration::kDifficultyCount>& a_values,
			float a_min, float a_max, const char* a_format, float a_step, bool a_resolved)
		{
			if (!a_resolved)
			{
				ImGuiMCP::TextDisabled("%s (%s) - not available on this build; the GameSetting "
										"could not be found", a_label, a_rawName);

				return false;
			}

			const std::string labelWithName = std::string(a_label) + " (" + a_rawName + ")";

			return NudgeableSlider(labelWithName.c_str(), &a_values[g_editingDifficulty], a_min, a_max, a_format, a_step);
		}

		// One row for a GLOBAL regeneration setting - not per-difficulty, so it is not bound to
		// g_editingDifficulty at all (see the plan's "decision to make once, not per player").
		bool RenderGlobalSlider(const char* a_label, const char* a_rawName, float* a_value,
			float a_min, float a_max, const char* a_format, float a_step, bool a_resolved)
		{
			if (!a_resolved)
			{
				ImGuiMCP::TextDisabled("%s (%s) - not available on this build; the GameSetting "
										"could not be found", a_label, a_rawName);

				return false;
			}

			const std::string labelWithName = std::string(a_label) + " (" + a_rawName + ")";

			return NudgeableSlider(labelWithName.c_str(), a_value, a_min, a_max, a_format, a_step);
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

		constexpr const char* const kDifficultyNames[] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };
		constexpr int kDifficultyCount = 6;

		void RenderRegenerationSection()
		{
			using namespace settings::regeneration;
			using PDS = Regeneration::PerDifficultySetting;
			using GS = Regeneration::GlobalSetting;

			// Snap the editor to whatever difficulty is really active the first time this page is
			// ever drawn, rather than always opening on Adept - a player who is actually playing on
			// Master should not have to hunt for the right section before anything looks familiar.
			if (!g_editingDifficultyInitialized)
			{
				const int active = Regeneration::LastAppliedDifficulty();
				if (active >= 0 && active < kDifficultyCount)
				{
					g_editingDifficulty = active;
				}
				g_editingDifficultyInitialized = true;
			}

			ImGuiMCP::SeparatorText("Regeneration");

			if (ImGuiMCP::Toggle("Enabled##Regen", &enabled))
			{
				RegenApplyLive();
			}
			HelpMarker("Off resets every setting below to the real vanilla value this mod captured "
					   "the first time it loaded - not just \"stop touching them.\"");

			ImGuiMCP::Spacing();
			ImGuiMCP::TextWrapped("Vanilla has no per-difficulty regeneration - this mod adds it. "
								  "Each difficulty below keeps its own combat rates and delays; "
								  "switching difficulty in the game's own menu switches which set "
								  "applies, live, with no need to open this page.");
			ImGuiMCP::Spacing();

			// ---- difficulty selector + the live "what's actually in force" readout ----
			int editingIndex = g_editingDifficulty;
			if (ImGuiMCP::Combo("Editing", &editingIndex, kDifficultyNames, kDifficultyCount))
			{
				g_editingDifficulty = editingIndex;
			}
			HelpMarker("Which difficulty's own values the sliders below are showing and editing.");

			const int active = Regeneration::LastAppliedDifficulty();
			if (active >= 0 && active < kDifficultyCount)
			{
				ImGuiMCP::TextWrapped("Current difficulty: %s. %s's values are active.",
									  kDifficultyNames[active], kDifficultyNames[active]);
			}
			else
			{
				ImGuiMCP::TextDisabled("Current difficulty: not applied yet.");
			}

			ImGuiMCP::Spacing();

			// ---- copy helpers - nobody wants to type six sets by hand ----
			if (ImGuiMCP::Button("Copy this set to every difficulty"))
			{
				const int from = g_editingDifficulty;
				OnMainThread([from]() {
					Regeneration::CopyToAllDifficulties(from);
					Regeneration::ApplyLive();
				});
				statusMessage = "Copied to every difficulty. Press Save to keep it.";
			}
			HelpMarker("Overwrites every OTHER difficulty's combat rates and delays with the set "
					   "you are currently editing.");

			ImGuiMCP::SameLine();
			ImGuiMCP::PushItemWidth(150.0F);
			static int copySource = 0;
			ImGuiMCP::Combo("##CopySource", &copySource, kDifficultyNames, kDifficultyCount);
			ImGuiMCP::PopItemWidth();
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Copy from"))
			{
				const int from = copySource;
				const int to = g_editingDifficulty;
				OnMainThread([from, to]() {
					Regeneration::CopyDifficulty(from, to);
					Regeneration::ApplyLive();
				});
				statusMessage = "Copied. Press Save to keep it.";
			}
			HelpMarker("Starts the difficulty you are editing from the picked difficulty's current "
					   "values.");

			ImGuiMCP::Spacing();

			// ---- In combat: the three settings this feature exists for ----
			ImGuiMCP::SeparatorText("In combat");
			if (RenderPerDifficultySlider("Health regen rate", "fCombatHealthRegenRateMult",
					combatHealthRegenRateMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(PDS::kCombatHealthRegenRateMult)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider("Magicka regen rate", "fCombatMagickaRegenRateMult",
					combatMagickaRegenRateMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(PDS::kCombatMagickaRegenRateMult)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider("Stamina regen rate", "fCombatStaminaRegenRateMult",
					combatStaminaRegenRateMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(PDS::kCombatStaminaRegenRateMult)))
			{
				RegenApplyLive();
			}

			ImGuiMCP::Spacing();

			// ---- After damage: the pause before regen resumes, plus its ceiling ----
			ImGuiMCP::SeparatorText("After damage");
			if (RenderPerDifficultySlider("Health regen delay (s)", "fDamagedHealthRegenDelay",
					damagedHealthRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedHealthRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider("Magicka regen delay (s)", "fDamagedMagickaRegenDelay",
					damagedMagickaRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedMagickaRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider("Stamina regen delay (s)", "fDamagedStaminaRegenDelay",
					damagedStaminaRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedStaminaRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider("Generic damaged-attribute delay (s)", "fDamagedAVRegenDelay",
					damagedAVRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedAVRegenDelay)))
			{
				RegenApplyLive();
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::TextDisabled("Delay ceilings - one value, every difficulty (the plan: these "
									"only matter once a delay above is raised past them):");
			if (RenderGlobalSlider("Health delay ceiling (s)", "fHealthRegenDelayMax",
					&healthRegenDelayMax, 0.0F, 300.0F, "%.1f", 1.0F,
					Regeneration::HasResolved(GS::kHealthRegenDelayMax)))
			{
				RegenApplyLive();
			}
			if (RenderGlobalSlider("Magicka delay ceiling (s)", "fMagickaRegenDelayMax",
					&magickaRegenDelayMax, 0.0F, 300.0F, "%.1f", 1.0F,
					Regeneration::HasResolved(GS::kMagickaRegenDelayMax)))
			{
				RegenApplyLive();
			}
			if (RenderGlobalSlider("Stamina delay ceiling (s)", "fStaminaRegenDelayMax",
					&staminaRegenDelayMax, 0.0F, 300.0F, "%.1f", 1.0F,
					Regeneration::HasResolved(GS::kStaminaRegenDelayMax)))
			{
				RegenApplyLive();
			}

			ImGuiMCP::Spacing();

			// ---- Situational - the edge cases, not the reason this feature exists ----
			ImGuiMCP::SeparatorText("Situational");
			if (RenderGlobalSlider("Out of breath stamina delay (s)", "fOutOfBreathStaminaRegenDelay",
					&outOfBreathStaminaRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(GS::kOutOfBreathStaminaRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderGlobalSlider("Downed essential NPC regen rate", "fEssentialDownCombatHealthRegenMult",
					&essentialDownCombatHealthRegenMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(GS::kEssentialDownCombatHealthRegenMult)))
			{
				RegenApplyLive();
			}
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
					Regeneration::ApplyLive();
				});
			}
			HelpMarker("Throws away any change made here since the last save, re-reads the INI from disk, and applies it immediately.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Restore defaults"))
			{
				OnMainThread([]() {
					settings::RestoreDefaults();
					// settings::RestoreDefaults() only resets the regen ENABLED toggle - there is
					// no compiled-in default for the regen float settings the way there is for
					// difficulty's twelve, so their own restore path is this call, sourced from
					// the real vanilla value Regeneration::Init() captured live (see Regeneration.h).
					Regeneration::RestoreDefaults();
					Difficulty::ApplyLive();
					Regeneration::ApplyLive();
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
		SKSEMenuFramework::AddSectionItem("Regeneration", SettingsPanel::RenderRegeneration);

		logger::info("Registered the settings pages with SKSE Menu Framework");
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

	void __stdcall SettingsPanel::RenderRegeneration()
	{
		ImGuiMCP::PushItemWidth(260.0F);

		RenderRegenerationSection();
		ImGuiMCP::Spacing();

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
