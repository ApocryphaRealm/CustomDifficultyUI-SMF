#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Difficulty.h"
#include "Regeneration.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Strings.h"
#include "utils/Toggle.h"

#include <algorithm>
#include <format>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		// The slider the arrow keys currently drive. Set by clicking one.
		std::string selectedSlider;

		// The log-level list is a parallel key/label pair: the key array names the translation
		// key, the label array holds the compiled English. ComboTR() below pairs them by index.
		constexpr int kLogLevelCount = 7;
		constexpr const char* const kLogLevelKeys[] = { "CDUI_Log_Trace", "CDUI_Log_Debug", "CDUI_Log_Info", "CDUI_Log_Warning", "CDUI_Log_Error", "CDUI_Log_Critical", "CDUI_Log_Off" };
		constexpr const char* const kLogLevelLabels[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };

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
				// The level table (1.0.5).
				"igInputInt",
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
				ImGuiMCP::TextDisabled("%s", strings::TR("CDUI_NudgeMark", "<-->"));
			}

			return changed;
		}

		void HelpMarker(const char* a_description)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("%s", strings::TR("CDUI_HelpMark", "(?)"));

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
				ImGuiMCP::TextWrapped(strings::TR("CDUI_NotAvailable", "%s (%s) - not available on this build; the GameSetting could not be found"), a_label, a_rawName);

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
				ImGuiMCP::TextWrapped(strings::TR("CDUI_NotAvailable", "%s (%s) - not available on this build; the GameSetting could not be found"), a_label, a_rawName);

				return false;
			}

			const std::string labelWithName = std::string(a_label) + " (" + a_rawName + ")";

			return NudgeableSlider(labelWithName.c_str(), a_value, a_min, a_max, a_format, a_step);
		}

		// Skyrim's own six difficulty names, as the game shows them. Key array + label array, the
		// same shape as the log levels above; Regeneration::DifficultyDisplayName() stays English
		// because that one feeds the log and DevBench's JSON, not the page.
		constexpr const char* const kDifficultyKeys[] = { "CDUI_Diff_Novice", "CDUI_Diff_Apprentice", "CDUI_Diff_Adept", "CDUI_Diff_Expert", "CDUI_Diff_Master", "CDUI_Diff_Legendary" };
		constexpr const char* const kDifficultyLabels[] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };
		constexpr int kDifficultyCount = 6;

		// A difficulty name as the player reads it, in the active language.
		const char* DifficultyText(int a_index)
		{
			return (a_index >= 0 && a_index < kDifficultyCount) ? strings::TR(kDifficultyKeys[a_index], kDifficultyLabels[a_index])
																 : "";
		}

		// A Combo whose option list is rebuilt from TR'd entries every frame (plan 2.2): store
		// owns the translated bytes for the duration of the call, so the pointers stay valid.
		bool ComboTR(const char* a_label, int* a_current,
					 const char* const* a_keys, const char* const* a_labels, int a_count)
		{
			std::vector<std::string> store;
			store.reserve(static_cast<std::size_t>(a_count));
			for (int i = 0; i < a_count; ++i) { store.emplace_back(strings::TR(a_keys[i], a_labels[i])); }
			std::vector<const char*> items;
			items.reserve(store.size());
			for (const auto& s : store) { items.push_back(s.c_str()); }
			return ImGuiMCP::Combo(a_label, a_current, items.data(), a_count);
		}

		// One difficulty's pair. The loaded value (what this game holds at data load - vanilla, or the
		// overhaul's number) sits under each pair so an overhaul can be tuned without losing its numbers.
		// a_header and the two labels arrive ALREADY translated (the keys are at the call site);
		// a_suffix is the ImGui id disambiguator and is never part of the translated text.
		void RenderDifficultyLevel(int a_difficulty, const char* a_header, const char* a_suffix,
			const char* a_toPCLabel, float* a_toPC, const char* a_byPCLabel, float* a_byPC)
		{
			ImGuiMCP::SeparatorText(a_header);

			const std::string toLabel = std::string(a_toPCLabel) + a_suffix;
			const std::string byLabel = std::string(a_byPCLabel) + a_suffix;

			if (NudgeableSlider(toLabel.c_str(), a_toPC, 0.0F, 999.0F, "%.2f", 0.01F))
			{
				ApplyLive();
			}
			HelpMarker(strings::TR("CDUI_HelpToYou", "Damage multiplier applied to hits enemies land on you at this difficulty. Ctrl+click to type a value."));

			if (NudgeableSlider(byLabel.c_str(), a_byPC, 0.0F, 999.0F, "%.2f", 0.01F))
			{
				ApplyLive();
			}
			HelpMarker(strings::TR("CDUI_HelpByYou", "Damage multiplier applied to hits you land on enemies at this difficulty. Ctrl+click to type a value."));

			const auto to = static_cast<Difficulty::Setting>(a_difficulty);
			const auto by = static_cast<Difficulty::Setting>(6 + a_difficulty);
			ImGuiMCP::TextDisabled(strings::TR("CDUI_LoadedWith", "    loaded with: x%.2f to you, x%.2f by you"), Difficulty::LoadedValue(to), Difficulty::LoadedValue(by));
		}

		void RenderDifficultySection()
		{
			using namespace settings::difficulty;

			ImGuiMCP::SeparatorText("Custom Difficulty UI");

			// The built-in patch (plan section 21 of Character Progression Control, shared with this
			// mod): the overhaul's numbers are the loaded values, and this mod writes last while enabled.
			const std::string overhaul = Difficulty::OverhaulLoaded();
			if (!overhaul.empty())
			{
				ImGuiMCP::TextWrapped(strings::TR("CDUI_OverhaulLoaded", "%s is loaded. Its damage multipliers are the loaded values shown under each pair; nothing here touches them until Enabled is on - then this mod writes last and supersedes them."),
									  overhaul.c_str());
				bool bbFound = false;
				const bool bbScaling = Difficulty::BladeAndBluntLevelScaling(bbFound);
				if (Difficulty::BladeAndBluntPresent())
				{
					if (bbScaling) { ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_BBScaling", "BladeAndBlunt.ini has bLevelBasedDifficulty = true: its DLL steps the multipliers at levels 10 to 50 as well. Set it to false while this mod is enabled - two writers on one value is never stable. Difficulty by level below does the same job.")); }
					else if (!bbFound) { ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_BBNotFound", "BladeAndBlunt.ini was not found, so its bLevelBasedDifficulty could not be read. If it is true, set it to false while this mod is enabled.")); }
				}
				ImGuiMCP::Spacing();
			}

			if (ImGuiMCP::Toggle(strings::TR("CDUI_Enabled", "Enabled"), &enabled))
			{
				ApplyLive();
			}
			HelpMarker(strings::TR("CDUI_HelpEnabled", "Off writes nothing: the multipliers your game loaded with (vanilla, or an overhaul's) stay exactly as they are, and switching off hands them back. On: the pairs below are written, and the game reads the pair for the difficulty you play on."));
			ImGuiMCP::Spacing();

			if (ImGuiMCP::Toggle(strings::TR("CDUI_SharedPair", "One pair for every difficulty"), &sharedPair))
			{
				ApplyLive();
			}
			HelpMarker(strings::TR("CDUI_HelpSharedPair", "On: the single pair below is written for all six difficulties, so the game's difficulty setting makes no difference to damage. Off: each difficulty has its own pair."));

			if (sharedPair)
			{
				if (NudgeableSlider((std::string(strings::TR("CDUI_DamageToYou", "Damage to you")) + "##shared").c_str(), &sharedToPC, 0.0F, 999.0F, "%.2f", 0.01F)) { ApplyLive(); }
				HelpMarker(strings::TR("CDUI_HelpSharedToYou", "Damage multiplier applied to hits enemies land on you, at every difficulty. Ctrl+click to type a value."));
				if (NudgeableSlider((std::string(strings::TR("CDUI_DamageByYou", "Damage by you")) + "##shared").c_str(), &sharedByPC, 0.0F, 999.0F, "%.2f", 0.01F)) { ApplyLive(); }
				HelpMarker(strings::TR("CDUI_HelpSharedByYou", "Damage multiplier applied to hits you land on enemies, at every difficulty. Ctrl+click to type a value."));
			}
			else
			{
				ImGuiMCP::Spacing();
				ImGuiMCP::Text("%s", strings::TR("CDUI_FillFrom", "Fill the table from:"));
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(strings::TR("CDUI_BtnLoaded", "Loaded values"))) { OnMainThread([]() { Difficulty::UseLoadedValues(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusLoaded", "The table holds the values this game loaded with. Press Save to keep them."); }
				HelpMarker(strings::TR("CDUI_HelpLoaded", "Whatever your game holds at load - vanilla, or the overhaul you run. The starting point for tuning an overhaul without losing its numbers."));
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(strings::TR("CDUI_BtnVanilla", "Vanilla"))) { OnMainThread([]() { Difficulty::UseVanillaValues(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusVanilla", "The table holds Skyrim's vanilla values. Press Save to keep them."); }
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button("Blade and Blunt")) { OnMainThread([]() { Difficulty::UseBladeAndBlunt(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusBB", "The table holds Blade and Blunt's values. Press Save to keep them."); }
				HelpMarker(strings::TR("CDUI_HelpBB", "Its published pairs: to you as vanilla, by you 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5."));
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button("Requiem")) { OnMainThread([]() { Difficulty::UseRequiem(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusRequiem", "The table holds Requiem's values. Press Save to keep them."); }
				HelpMarker(strings::TR("CDUI_HelpRequiem", "Every multiplier 1.0 - in Requiem the difficulty setting does no damage scaling by design."));
				ImGuiMCP::Spacing();
				ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_SectionsNote1", "Each section below is one of Skyrim's own difficulty levels."));
				ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_SectionsNote2", "Whichever difficulty you select in game uses that section's sliders."));
				ImGuiMCP::Spacing();

				// Headed with the names the GAME shows in its own difficulty menu, not the internal
				// suffixes of the settings behind them. The ##VE/##E/... suffixes are ImGui ID
				// disambiguators, NOT visible text - changing them would give every slider a new
				// identity and silently reset any in-progress interaction state keyed on it.
				const char* const toYou = strings::TR("CDUI_DamageToYou", "Damage to you");
				const char* const byYou = strings::TR("CDUI_DamageByYou", "Damage by you");
				RenderDifficultyLevel(0, DifficultyText(0), "##VE", toYou, &toPCVE, byYou, &byPCVE);
				RenderDifficultyLevel(1, DifficultyText(1), "##E", toYou, &toPCE, byYou, &byPCE);
				RenderDifficultyLevel(2, DifficultyText(2), "##N", toYou, &toPCN, byYou, &byPCN);
				RenderDifficultyLevel(3, DifficultyText(3), "##H", toYou, &toPCH, byYou, &byPCH);
				RenderDifficultyLevel(4, DifficultyText(4), "##VH", toYou, &toPCVH, byYou, &byPCVH);
				RenderDifficultyLevel(5, DifficultyText(5), "##L", toYou, &toPCL, byYou, &byPCL);
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::SeparatorText(strings::TR("CDUI_ByLevelHeader", "Difficulty by level"));
			if (ImGuiMCP::Toggle(strings::TR("CDUI_ByLevel", "Set the game's difficulty from your level"), &byLevel))
			{
				if (byLevel) { OnMainThread([]() { Difficulty::ApplyLevelRule("switched on"); }); }
			}
			HelpMarker(strings::TR("CDUI_HelpByLevel", "On a save load and on every level-up, the highest difficulty whose level you have reached becomes the game's difficulty - the same change the Settings menu makes, so the regeneration set follows it. 0 = that difficulty is never chosen by this rule. Off: the game's difficulty is yours to set."));
			if (byLevel)
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				const int level = player ? static_cast<int>(player->GetLevel()) : -1;
				const int target = level >= 0 ? Difficulty::LevelRuleTarget(level) : -1;
				if (level >= 0) { ImGuiMCP::Text(strings::TR("CDUI_LevelArrow", "Level %d -> %s"), level, target >= 0 ? DifficultyText(target) : strings::TR("CDUI_NoRow", "no row applies")); }
				for (int d = 0; d < kDifficultyCount; ++d)
				{
					ImGuiMCP::PushID(std::format("levelfor{}", d).c_str());
					int from = static_cast<int>(levelFor[static_cast<std::size_t>(d)]);
					char levelLabel[256] = {};
					std::snprintf(levelLabel, sizeof(levelLabel), strings::TR("CDUI_FromLevel", "%s from level"), DifficultyText(d));
					if (ImGuiMCP::InputInt(levelLabel, &from))
					{
						levelFor[static_cast<std::size_t>(d)] = static_cast<std::uint32_t>(std::clamp(from, 0, 1000));
					}
					ImGuiMCP::PopID();
				}
				HelpMarker(strings::TR("CDUI_HelpLevelTable", "Defaults are Blade and Blunt's milestones: one difficulty tier per ten levels."));
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::SeparatorText(strings::TR("CDUI_NowHeader", "What the game is using right now"));
			const int now = Difficulty::CurrentDifficulty();
			if (now >= 0)
			{
				const auto to = static_cast<Difficulty::Setting>(now);
				const auto by = static_cast<Difficulty::Setting>(6 + now);
				ImGuiMCP::Text(strings::TR("CDUI_DamageAt", "Damage at %s: x%.2f to you, x%.2f by you (loaded with x%.2f / x%.2f)"), DifficultyText(now),
							   Difficulty::LiveValue(to), Difficulty::LiveValue(by), Difficulty::LoadedValue(to), Difficulty::LoadedValue(by));
			}
			else
			{
				ImGuiMCP::TextDisabled("%s", strings::TR("CDUI_NoCharacter", "No character loaded."));
			}
			if (!enabled) { ImGuiMCP::TextDisabled("%s", strings::TR("CDUI_NotEnabled", "Not enabled - nothing is written; the values above are whatever the game loaded with.")); }
		}


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

			ImGuiMCP::SeparatorText(strings::TR("CDUI_RegenHeader", "Regeneration"));

			if (ImGuiMCP::Toggle((std::string(strings::TR("CDUI_Enabled", "Enabled")) + "##Regen").c_str(), &enabled))
			{
				RegenApplyLive();
			}
			HelpMarker(strings::TR("CDUI_HelpRegenEnabled", "Off resets every setting below to the real vanilla value this mod captured the first time it loaded - not just \"stop touching them.\""));

			ImGuiMCP::Spacing();
			ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_RegenIntro", "Vanilla has no per-difficulty regeneration - this mod adds it. Each difficulty below keeps its own combat rates and delays; switching difficulty in the game's own menu switches which set applies, live, with no need to open this page."));
			ImGuiMCP::Spacing();

			// ---- difficulty selector + the live "what's actually in force" readout ----
			int editingIndex = g_editingDifficulty;
			if (ComboTR(strings::TR("CDUI_Editing", "Editing"), &editingIndex, kDifficultyKeys, kDifficultyLabels, kDifficultyCount))
			{
				g_editingDifficulty = editingIndex;
			}
			HelpMarker(strings::TR("CDUI_HelpEditing", "Which difficulty's own values the sliders below are showing and editing."));

			const int active = Regeneration::LastAppliedDifficulty();
			if (active >= 0 && active < kDifficultyCount)
			{
				ImGuiMCP::TextWrapped(strings::TR("CDUI_CurrentDifficulty", "Current difficulty: %s. %s's values are active."),
									  DifficultyText(active), DifficultyText(active));
			}
			else
			{
				ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_CurrentNotApplied", "Current difficulty: not applied yet."));
			}

			ImGuiMCP::Spacing();

			// ---- copy helpers - nobody wants to type six sets by hand ----
			if (ImGuiMCP::Button(strings::TR("CDUI_CopyToAll", "Copy this set to every difficulty")))
			{
				const int from = g_editingDifficulty;
				OnMainThread([from]() {
					Regeneration::CopyToAllDifficulties(from);
					Regeneration::ApplyLive();
				});
				statusMessage = strings::TR("CDUI_StatusCopiedAll", "Copied to every difficulty. Press Save to keep it.");
			}
			HelpMarker(strings::TR("CDUI_HelpCopyToAll", "Overwrites every OTHER difficulty's combat rates and delays with the set you are currently editing."));

			ImGuiMCP::SameLine();
			ImGuiMCP::PushItemWidth(150.0F);
			static int copySource = 0;
			ComboTR("##CopySource", &copySource, kDifficultyKeys, kDifficultyLabels, kDifficultyCount);
			ImGuiMCP::PopItemWidth();
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(strings::TR("CDUI_CopyFrom", "Copy from")))
			{
				const int from = copySource;
				const int to = g_editingDifficulty;
				OnMainThread([from, to]() {
					Regeneration::CopyDifficulty(from, to);
					Regeneration::ApplyLive();
				});
				statusMessage = strings::TR("CDUI_StatusCopied", "Copied. Press Save to keep it.");
			}
			HelpMarker(strings::TR("CDUI_HelpCopyFrom", "Starts the difficulty you are editing from the picked difficulty's current values."));

			ImGuiMCP::Spacing();

			// ---- In combat: the three settings this feature exists for ----
			ImGuiMCP::SeparatorText(strings::TR("CDUI_InCombat", "In combat"));
			if (RenderPerDifficultySlider(strings::TR("CDUI_HealthRegenRate", "Health regen rate"), "fCombatHealthRegenRateMult",
					combatHealthRegenRateMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(PDS::kCombatHealthRegenRateMult)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider(strings::TR("CDUI_MagickaRegenRate", "Magicka regen rate"), "fCombatMagickaRegenRateMult",
					combatMagickaRegenRateMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(PDS::kCombatMagickaRegenRateMult)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider(strings::TR("CDUI_StaminaRegenRate", "Stamina regen rate"), "fCombatStaminaRegenRateMult",
					combatStaminaRegenRateMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(PDS::kCombatStaminaRegenRateMult)))
			{
				RegenApplyLive();
			}

			ImGuiMCP::Spacing();

			// ---- After damage: the pause before regen resumes, plus its ceiling ----
			ImGuiMCP::SeparatorText(strings::TR("CDUI_AfterDamage", "After damage"));
			if (RenderPerDifficultySlider(strings::TR("CDUI_HealthRegenDelay", "Health regen delay (s)"), "fDamagedHealthRegenDelay",
					damagedHealthRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedHealthRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider(strings::TR("CDUI_MagickaRegenDelay", "Magicka regen delay (s)"), "fDamagedMagickaRegenDelay",
					damagedMagickaRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedMagickaRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider(strings::TR("CDUI_StaminaRegenDelay", "Stamina regen delay (s)"), "fDamagedStaminaRegenDelay",
					damagedStaminaRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedStaminaRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderPerDifficultySlider(strings::TR("CDUI_AVRegenDelay", "Generic damaged-attribute delay (s)"), "fDamagedAVRegenDelay",
					damagedAVRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(PDS::kDamagedAVRegenDelay)))
			{
				RegenApplyLive();
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_CeilingsNote", "Delay ceilings - one value, every difficulty (the plan: these only matter once a delay above is raised past them):"));
			if (RenderGlobalSlider(strings::TR("CDUI_HealthCeiling", "Health delay ceiling (s)"), "fHealthRegenDelayMax",
					&healthRegenDelayMax, 0.0F, 300.0F, "%.1f", 1.0F,
					Regeneration::HasResolved(GS::kHealthRegenDelayMax)))
			{
				RegenApplyLive();
			}
			if (RenderGlobalSlider(strings::TR("CDUI_MagickaCeiling", "Magicka delay ceiling (s)"), "fMagickaRegenDelayMax",
					&magickaRegenDelayMax, 0.0F, 300.0F, "%.1f", 1.0F,
					Regeneration::HasResolved(GS::kMagickaRegenDelayMax)))
			{
				RegenApplyLive();
			}
			if (RenderGlobalSlider(strings::TR("CDUI_StaminaCeiling", "Stamina delay ceiling (s)"), "fStaminaRegenDelayMax",
					&staminaRegenDelayMax, 0.0F, 300.0F, "%.1f", 1.0F,
					Regeneration::HasResolved(GS::kStaminaRegenDelayMax)))
			{
				RegenApplyLive();
			}

			ImGuiMCP::Spacing();

			// ---- Situational - the edge cases, not the reason this feature exists ----
			ImGuiMCP::SeparatorText(strings::TR("CDUI_Situational", "Situational"));
			if (RenderGlobalSlider(strings::TR("CDUI_OutOfBreath", "Out of breath stamina delay (s)"), "fOutOfBreathStaminaRegenDelay",
					&outOfBreathStaminaRegenDelay, 0.0F, 60.0F, "%.2f", 0.5F,
					Regeneration::HasResolved(GS::kOutOfBreathStaminaRegenDelay)))
			{
				RegenApplyLive();
			}
			if (RenderGlobalSlider(strings::TR("CDUI_EssentialDown", "Downed essential NPC regen rate"), "fEssentialDownCombatHealthRegenMult",
					&essentialDownCombatHealthRegenMult, 0.0F, 20.0F, "%.2f", 0.05F,
					Regeneration::HasResolved(GS::kEssentialDownCombatHealthRegenMult)))
			{
				RegenApplyLive();
			}
		}

		void RenderDebugSection()
		{
			using namespace settings;

			ImGuiMCP::SeparatorText(strings::TR("CDUI_DebugHeader", "Debug"));

			int level = static_cast<int>(debug::logLevel);
			if (ComboTR(strings::TR("CDUI_LogLevel", "Log level"), &level, kLogLevelKeys, kLogLevelLabels, kLogLevelCount))
			{
				debug::logLevel = static_cast<logger::level>(level);

				OnMainThread([]() { logger::set_level(settings::debug::logLevel, settings::debug::logLevel); });
			}
			HelpMarker(strings::TR("CDUI_HelpLogLevel", "Applies to the log immediately. Ships at Trace by default - see CLAUDE.md rule 31."));
		}

		void RenderButtons()
		{
			if (ImGuiMCP::Button(strings::TR("CDUI_SaveBtn", "Save")))
			{
				OnMainThread([]() {
					statusMessage = settings::Save() ? strings::TR("CDUI_StatusSaved", "Settings saved.")
													  : strings::TR("CDUI_StatusSaveFail", "Could not save the INI. See the log for why.");
				});
			}
			HelpMarker(strings::TR("CDUI_HelpSave", "Writes every setting above back to the INI. Comments and unrelated keys are left alone."));

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button(strings::TR("CDUI_ReloadBtn", "Reload from INI")))
			{
				OnMainThread([]() {
					statusMessage = settings::Reload()
										 ? strings::TR("CDUI_StatusReloaded", "Settings reloaded from the INI.")
										 : strings::TR("CDUI_StatusReloadFail", "Could not read the INI. See the log for why.");
					Difficulty::ApplyLive();
					Regeneration::ApplyLive();
				});
			}
			HelpMarker(strings::TR("CDUI_HelpReload", "Throws away any change made here since the last save, re-reads the INI from disk, and applies it immediately."));

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button(strings::TR("CDUI_RestoreBtn", "Restore defaults")))
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

				statusMessage = strings::TR("CDUI_StatusRestored", "Defaults restored and applied. Press Save to keep them.");
			}
			HelpMarker(strings::TR("CDUI_HelpRestore", "Puts every setting back to the value it has on a fresh install, and applies it immediately. Nothing is written to the INI until you press Save."));

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
		strings::Tick();

		ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_SettingsIntro", "Changes below apply immediately to the game's own difficulty damage multipliers - the same ones the vanilla difficulty slider sets, per level. Press Save separately to keep them for next time."));
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
		strings::Tick();

		ImGuiMCP::PushItemWidth(260.0F);

		RenderRegenerationSection();
		ImGuiMCP::Spacing();

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
