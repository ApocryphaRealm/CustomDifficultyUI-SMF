# -*- coding: utf-8 -*-
"""lang-patch.py - one-shot, re-runnable language-support patch for Custom Difficulty UI.

Applies the consumer-side mechanism from the translation rollout plan, section 2:

  * include/utils/Strings.h is vendored separately (copied from the template, unchanged);
  * include/SKSEMenuFramework.h gets "!ApocryphaMenuFramework" as the FIRST module lookup;
  * source/MessageListeners.cpp calls strings::Configure("CustomDifficultyUI") at kDataLoaded
    (this mod's message handler lives there, not in main.cpp);
  * source/UI.cpp calls strings::Tick() as the first line of BOTH page render callbacks
    (SettingsPanel::Render, SettingsPanel::RenderRegeneration) and routes every drawn literal
    through strings::TR("CDUI_...", "English");
  * source/DevBenchTool.cpp gains an op=strings returning strings::StatusJson().

Every edit is a must-match anchor replace: an anchor that is not found EXACTLY ONCE raises, so a
stale run against changed source fails loudly instead of leaving the code half patched. Nothing is
written until every anchor for that file has matched. Each patch step is skipped when its
done-marker is already present, so the script is re-runnable.

Run: `python tools/lang-patch.py`.

Encoding note: source files are read and written as UTF-8 with newline='' in BOTH directions, so a
raw CR inside a string literal survives and the existing line endings are preserved exactly (logic
library, 2026-09-02 - "Reading source with Python's universal newlines corrupts string literals").
This repo's own sources are LF; the vendored SKSEMenuFramework.h is CRLF, which is what fit()
below is for.
"""
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def fit(s, crlf):
    """Anchors are written with LF; a file kept in CRLF gets the CRLF form of the same text."""
    return s.replace("\n", "\r\n") if crlf else s


def apply_one(text, anchor, replacement, label, crlf=False):
    anchor, replacement = fit(anchor, crlf), fit(replacement, crlf)
    n = text.count(anchor)
    if n != 1:
        raise RuntimeError("[{}] anchor found {} time(s), expected exactly 1:\n{!r}".format(label, n, anchor))
    return text.replace(anchor, replacement, 1)


def apply_all(text, pairs, label):
    crlf = "\r\n" in text
    for i, (anchor, replacement) in enumerate(pairs):
        text = apply_one(text, anchor, replacement, "{}[{}]".format(label, i), crlf)
    return text


# ------------------------------------------------------------------------------------------------
# 1) include/SKSEMenuFramework.h - "!ApocryphaMenuFramework" first, ahead of the alias name.
# ------------------------------------------------------------------------------------------------
def patch_skse_menu_framework_h():
    path = os.path.join(REPO, "include", "SKSEMenuFramework.h")
    text = read(path)
    if 'GetModuleHandleW(L"!ApocryphaMenuFramework")' in text:
        print("  SKSEMenuFramework.h: already patched")
        return
    anchor = (
        '        menuFramework = GetModuleHandleW(L"ApocryphaMenuFramework");\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"SKSEMenuFramework");\n'
        '        }\n'
    )
    replacement = (
        '        menuFramework = GetModuleHandleW(L"!ApocryphaMenuFramework");\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"ApocryphaMenuFramework");\n'
        '        }\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"SKSEMenuFramework");\n'
        '        }\n'
    )
    text = apply_one(text, anchor, replacement, "SKSEMenuFramework.h:GetMenuFrameworkModule", "\r\n" in text)
    write(path, text)
    print("  SKSEMenuFramework.h: patched")


# ------------------------------------------------------------------------------------------------
# 2) source/MessageListeners.cpp - strings::Configure(...) at kDataLoaded.
# ------------------------------------------------------------------------------------------------
def patch_message_listeners_cpp():
    path = os.path.join(REPO, "source", "MessageListeners.cpp")
    text = read(path)
    if 'strings::Configure("CustomDifficultyUI")' in text:
        print("  MessageListeners.cpp: already patched")
        return
    pairs = [
        ('#include "utils/Logger.h"',
         '#include "utils/Logger.h"\n#include "utils/Strings.h"'),
        ('\tcase SKSE::MessagingInterface::kDataLoaded:\n'
         '\t\t// The twelve vanilla difficulty GameSettings',
         '\tcase SKSE::MessagingInterface::kDataLoaded:\n'
         '\t\t// Language first (translation rollout plan, section 2.1): the settings pages read\n'
         '\t\t// their text from Data/Interface/Translations/CustomDifficultyUI_<language>.txt for\n'
         '\t\t// whatever language the Apocrypha Menu Framework reports, before anything is drawn.\n'
         '\t\tstrings::Configure("CustomDifficultyUI");\n'
         '\n'
         '\t\t// The twelve vanilla difficulty GameSettings'),
    ]
    text = apply_all(text, pairs, "MessageListeners.cpp")
    write(path, text)
    print("  MessageListeners.cpp: patched")


# ------------------------------------------------------------------------------------------------
# 3) source/DevBenchTool.cpp - op=strings on customdifficulty.control, and its descriptor line.
# ------------------------------------------------------------------------------------------------
def patch_devbench_tool_cpp():
    path = os.path.join(REPO, "source", "DevBenchTool.cpp")
    text = read(path)
    if '"op":"strings"' in text:
        print("  DevBenchTool.cpp: already patched")
        return
    pairs = [
        ('#include "utils/Logger.h"',
         '#include "utils/Logger.h"\n#include "utils/Strings.h"'),
        ('\t\t\tif (args.find("\\"reload\\"") != std::string_view::npos)',
         '\t\t\t// op=strings: which language the settings pages are drawing in, where that came\n'
         '\t\t\t// from and how many texts were read - the proof a translation file actually\n'
         '\t\t\t// loaded, readable without a capture.\n'
         '\t\t\tif (args.find("\\"strings\\"") != std::string_view::npos)\n'
         '\t\t\t{\n'
         '\t\t\t\ta_write(a_sink, std::format(R"({{"ok":true,"op":"strings","strings":{}}})", strings::StatusJson()).c_str());\n'
         '\t\t\t\treturn;\n'
         '\t\t\t}\n'
         '\n'
         '\t\t\tif (args.find("\\"reload\\"") != std::string_view::npos)'),
        ('"level table, the overhaul detection). No args: reports the actual vs last-applied difficulty.\\","',
         '"level table, the overhaul detection). op=strings reports the language the settings pages are "\n'
         '\t\t\t"drawn in, where it came from and how many translated texts were loaded. No args: reports "\n'
         '\t\t\t"the actual vs last-applied difficulty.\\","'),
    ]
    text = apply_all(text, pairs, "DevBenchTool.cpp")
    write(path, text)
    print("  DevBenchTool.cpp: patched")


# ------------------------------------------------------------------------------------------------
# 4) source/UI.cpp - Tick() in both page callbacks, TR() on every drawn literal.
#
# Conventions (plan 2.2): Text/TextWrapped/TextDisabled with a plain literal become
# X("%s", TR(...)); a literal that IS a printf format keeps its specifiers inside the TR text; an
# ImGui id suffix (##VE, ##shared, ##Regen, ##CopySource) stays OUTSIDE the translated text; combo
# option lists are rebuilt per frame from TR'd entries (ComboTR); the difficulty and log-level
# label tables become parallel key/label arrays translated at the use site; status strings assigned
# into statusMessage are built from TR at the moment they are set. Never routed: the framework's
# section and page names, INI keys, the raw GameSetting names (fCombatHealthRegenRateMult and the
# rest), the INI path, log lines, bare numeric formats ("%.2f", "%.1f") and the product names
# drawn on their own ("Custom Difficulty UI", "Blade and Blunt", "Requiem", "Vanilla" as a preset
# button).
# ------------------------------------------------------------------------------------------------
LOGLEVEL_OLD = (
    '\t\tconstexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };\n'
    '\t\tconstexpr int kLogLevelCount = 7;\n'
)

LOGLEVEL_NEW = (
    '\t\t// The log-level list is a parallel key/label pair: the key array names the translation\n'
    '\t\t// key, the label array holds the compiled English. ComboTR() below pairs them by index.\n'
    '\t\tconstexpr int kLogLevelCount = 7;\n'
    '\t\tconstexpr const char* const kLogLevelKeys[] = { "CDUI_Log_Trace", "CDUI_Log_Debug", "CDUI_Log_Info", "CDUI_Log_Warning", "CDUI_Log_Error", "CDUI_Log_Critical", "CDUI_Log_Off" };\n'
    '\t\tconstexpr const char* const kLogLevelLabels[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };\n'
)

DIFFICULTY_OLD = (
    '\t\tconstexpr const char* const kDifficultyNames[] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };\n'
    '\t\tconstexpr int kDifficultyCount = 6;\n'
)

DIFFICULTY_NEW = (
    '\t\t// Skyrim\'s own six difficulty names, as the game shows them. Key array + label array, the\n'
    '\t\t// same shape as the log levels above; Regeneration::DifficultyDisplayName() stays English\n'
    '\t\t// because that one feeds the log and DevBench\'s JSON, not the page.\n'
    '\t\tconstexpr const char* const kDifficultyKeys[] = { "CDUI_Diff_Novice", "CDUI_Diff_Apprentice", "CDUI_Diff_Adept", "CDUI_Diff_Expert", "CDUI_Diff_Master", "CDUI_Diff_Legendary" };\n'
    '\t\tconstexpr const char* const kDifficultyLabels[] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };\n'
    '\t\tconstexpr int kDifficultyCount = 6;\n'
    '\n'
    '\t\t// A difficulty name as the player reads it, in the active language.\n'
    '\t\tconst char* DifficultyText(int a_index)\n'
    '\t\t{\n'
    '\t\t\treturn (a_index >= 0 && a_index < kDifficultyCount) ? strings::TR(kDifficultyKeys[a_index], kDifficultyLabels[a_index])\n'
    '\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t : "";\n'
    '\t\t}\n'
    '\n'
    '\t\t// A Combo whose option list is rebuilt from TR\'d entries every frame (plan 2.2): store\n'
    '\t\t// owns the translated bytes for the duration of the call, so the pointers stay valid.\n'
    '\t\tbool ComboTR(const char* a_label, int* a_current,\n'
    '\t\t\t\t\t const char* const* a_keys, const char* const* a_labels, int a_count)\n'
    '\t\t{\n'
    '\t\t\tstd::vector<std::string> store;\n'
    '\t\t\tstore.reserve(static_cast<std::size_t>(a_count));\n'
    '\t\t\tfor (int i = 0; i < a_count; ++i) { store.emplace_back(strings::TR(a_keys[i], a_labels[i])); }\n'
    '\t\t\tstd::vector<const char*> items;\n'
    '\t\t\titems.reserve(store.size());\n'
    '\t\t\tfor (const auto& s : store) { items.push_back(s.c_str()); }\n'
    '\t\t\treturn ImGuiMCP::Combo(a_label, a_current, items.data(), a_count);\n'
    '\t\t}\n'
)

PERDIFF_OLD = (
    '\t\t\tstd::array<float, settings::regeneration::kDifficultyCount>& a_values,\n'
    '\t\t\tfloat a_min, float a_max, const char* a_format, float a_step, bool a_resolved)\n'
    '\t\t{\n'
    '\t\t\tif (!a_resolved)\n'
    '\t\t\t{\n'
    '\t\t\t\tImGuiMCP::TextWrapped("%s (%s) - not available on this build; the GameSetting "\n'
    '\t\t\t\t\t\t\t\t\t\t"could not be found", a_label, a_rawName);\n'
)

PERDIFF_NEW = (
    '\t\t\tstd::array<float, settings::regeneration::kDifficultyCount>& a_values,\n'
    '\t\t\tfloat a_min, float a_max, const char* a_format, float a_step, bool a_resolved)\n'
    '\t\t{\n'
    '\t\t\tif (!a_resolved)\n'
    '\t\t\t{\n'
    '\t\t\t\tImGuiMCP::TextWrapped(strings::TR("CDUI_NotAvailable", "%s (%s) - not available on this build; the GameSetting could not be found"), a_label, a_rawName);\n'
)

GLOBAL_OLD = (
    '\t\tbool RenderGlobalSlider(const char* a_label, const char* a_rawName, float* a_value,\n'
    '\t\t\tfloat a_min, float a_max, const char* a_format, float a_step, bool a_resolved)\n'
    '\t\t{\n'
    '\t\t\tif (!a_resolved)\n'
    '\t\t\t{\n'
    '\t\t\t\tImGuiMCP::TextWrapped("%s (%s) - not available on this build; the GameSetting "\n'
    '\t\t\t\t\t\t\t\t\t\t"could not be found", a_label, a_rawName);\n'
)

GLOBAL_NEW = (
    '\t\tbool RenderGlobalSlider(const char* a_label, const char* a_rawName, float* a_value,\n'
    '\t\t\tfloat a_min, float a_max, const char* a_format, float a_step, bool a_resolved)\n'
    '\t\t{\n'
    '\t\t\tif (!a_resolved)\n'
    '\t\t\t{\n'
    '\t\t\t\tImGuiMCP::TextWrapped(strings::TR("CDUI_NotAvailable", "%s (%s) - not available on this build; the GameSetting could not be found"), a_label, a_rawName);\n'
)

LEVELROW_OLD = (
    '\t\tvoid RenderDifficultyLevel(int a_difficulty, const char* a_header, const char* a_toPCLabel, float* a_toPC,\n'
    '\t\t\tconst char* a_byPCLabel, float* a_byPC)\n'
    '\t\t{\n'
    '\t\t\tImGuiMCP::SeparatorText(a_header);\n'
    '\n'
    '\t\t\tif (NudgeableSlider(a_toPCLabel, a_toPC, 0.0F, 999.0F, "%.2f", 0.01F))\n'
)

LEVELROW_NEW = (
    '\t\t// a_header and the two labels arrive ALREADY translated (the keys are at the call site);\n'
    '\t\t// a_suffix is the ImGui id disambiguator and is never part of the translated text.\n'
    '\t\tvoid RenderDifficultyLevel(int a_difficulty, const char* a_header, const char* a_suffix,\n'
    '\t\t\tconst char* a_toPCLabel, float* a_toPC, const char* a_byPCLabel, float* a_byPC)\n'
    '\t\t{\n'
    '\t\t\tImGuiMCP::SeparatorText(a_header);\n'
    '\n'
    '\t\t\tconst std::string toLabel = std::string(a_toPCLabel) + a_suffix;\n'
    '\t\t\tconst std::string byLabel = std::string(a_byPCLabel) + a_suffix;\n'
    '\n'
    '\t\t\tif (NudgeableSlider(toLabel.c_str(), a_toPC, 0.0F, 999.0F, "%.2f", 0.01F))\n'
)

SIXROWS_OLD = (
    '\t\t\t\tRenderDifficultyLevel(0, "Novice", "Damage to you##VE", &toPCVE, "Damage by you##VE", &byPCVE);\n'
    '\t\t\t\tRenderDifficultyLevel(1, "Apprentice", "Damage to you##E", &toPCE, "Damage by you##E", &byPCE);\n'
    '\t\t\t\tRenderDifficultyLevel(2, "Adept", "Damage to you##N", &toPCN, "Damage by you##N", &byPCN);\n'
    '\t\t\t\tRenderDifficultyLevel(3, "Expert", "Damage to you##H", &toPCH, "Damage by you##H", &byPCH);\n'
    '\t\t\t\tRenderDifficultyLevel(4, "Master", "Damage to you##VH", &toPCVH, "Damage by you##VH", &byPCVH);\n'
    '\t\t\t\tRenderDifficultyLevel(5, "Legendary", "Damage to you##L", &toPCL, "Damage by you##L", &byPCL);\n'
)

SIXROWS_NEW = (
    '\t\t\t\tconst char* const toYou = strings::TR("CDUI_DamageToYou", "Damage to you");\n'
    '\t\t\t\tconst char* const byYou = strings::TR("CDUI_DamageByYou", "Damage by you");\n'
    '\t\t\t\tRenderDifficultyLevel(0, DifficultyText(0), "##VE", toYou, &toPCVE, byYou, &byPCVE);\n'
    '\t\t\t\tRenderDifficultyLevel(1, DifficultyText(1), "##E", toYou, &toPCE, byYou, &byPCE);\n'
    '\t\t\t\tRenderDifficultyLevel(2, DifficultyText(2), "##N", toYou, &toPCN, byYou, &byPCN);\n'
    '\t\t\t\tRenderDifficultyLevel(3, DifficultyText(3), "##H", toYou, &toPCH, byYou, &byPCH);\n'
    '\t\t\t\tRenderDifficultyLevel(4, DifficultyText(4), "##VH", toYou, &toPCVH, byYou, &byPCVH);\n'
    '\t\t\t\tRenderDifficultyLevel(5, DifficultyText(5), "##L", toYou, &toPCL, byYou, &byPCL);\n'
)

UI_PAIRS = [
    # --- includes ---
    ('#include "utils/Logger.h"\n#include "utils/Toggle.h"',
     '#include "utils/Logger.h"\n#include "utils/Strings.h"\n#include "utils/Toggle.h"'),
    ('#include <algorithm>\n#include <format>\n#include <array>\n#include <string>',
     '#include <algorithm>\n#include <format>\n#include <array>\n#include <cstdio>\n#include <string>\n#include <vector>'),

    # --- the two label tables and the helpers that draw from them ---
    (LOGLEVEL_OLD, LOGLEVEL_NEW),
    (DIFFICULTY_OLD, DIFFICULTY_NEW),

    # --- NudgeableSlider / HelpMarker ---
    ('\t\t\t\tImGuiMCP::TextDisabled("<-->");',
     '\t\t\t\tImGuiMCP::TextDisabled("%s", strings::TR("CDUI_NudgeMark", "<-->"));'),
    ('\t\t\tImGuiMCP::TextDisabled("(?)");',
     '\t\t\tImGuiMCP::TextDisabled("%s", strings::TR("CDUI_HelpMark", "(?)"));'),

    # --- the two "GameSetting could not be found" rows ---
    (PERDIFF_OLD, PERDIFF_NEW),
    (GLOBAL_OLD, GLOBAL_NEW),

    # --- RenderDifficultyLevel ---
    (LEVELROW_OLD, LEVELROW_NEW),
    ('\t\t\tif (NudgeableSlider(a_byPCLabel, a_byPC, 0.0F, 999.0F, "%.2f", 0.01F))',
     '\t\t\tif (NudgeableSlider(byLabel.c_str(), a_byPC, 0.0F, 999.0F, "%.2f", 0.01F))'),
    ('\t\t\tHelpMarker("Damage multiplier applied to hits enemies land on you at this difficulty. Ctrl+click to type a value.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpToYou", "Damage multiplier applied to hits enemies land on you at this difficulty. Ctrl+click to type a value."));'),
    ('\t\t\tHelpMarker("Damage multiplier applied to hits you land on enemies at this difficulty. Ctrl+click to type a value.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpByYou", "Damage multiplier applied to hits you land on enemies at this difficulty. Ctrl+click to type a value."));'),
    ('\t\t\tImGuiMCP::TextDisabled("    loaded with: x%.2f to you, x%.2f by you", Difficulty::LoadedValue(to), Difficulty::LoadedValue(by));',
     '\t\t\tImGuiMCP::TextDisabled(strings::TR("CDUI_LoadedWith", "    loaded with: x%.2f to you, x%.2f by you"), Difficulty::LoadedValue(to), Difficulty::LoadedValue(by));'),

    # --- RenderDifficultySection ---
    ('\t\t\t\tImGuiMCP::TextWrapped("%s is loaded. Its damage multipliers are the loaded values shown under each pair; nothing "\n'
     '\t\t\t\t\t\t\t\t\t  "here touches them until Enabled is on - then this mod writes last and supersedes them.",\n'
     '\t\t\t\t\t\t\t\t\t  overhaul.c_str());',
     '\t\t\t\tImGuiMCP::TextWrapped(strings::TR("CDUI_OverhaulLoaded", "%s is loaded. Its damage multipliers are the loaded values shown under each pair; nothing here touches them until Enabled is on - then this mod writes last and supersedes them."),\n'
     '\t\t\t\t\t\t\t\t\t  overhaul.c_str());'),
    ('if (bbScaling) { ImGuiMCP::TextWrapped("BladeAndBlunt.ini has bLevelBasedDifficulty = true: its DLL steps the multipliers at levels 10 to 50 as well. Set it to false while this mod is enabled - two writers on one value is never stable. Difficulty by level below does the same job."); }',
     'if (bbScaling) { ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_BBScaling", "BladeAndBlunt.ini has bLevelBasedDifficulty = true: its DLL steps the multipliers at levels 10 to 50 as well. Set it to false while this mod is enabled - two writers on one value is never stable. Difficulty by level below does the same job.")); }'),
    ('else if (!bbFound) { ImGuiMCP::TextWrapped("BladeAndBlunt.ini was not found, so its bLevelBasedDifficulty could not be read. If it is true, set it to false while this mod is enabled."); }',
     'else if (!bbFound) { ImGuiMCP::TextWrapped("%s", strings::TR("CDUI_BBNotFound", "BladeAndBlunt.ini was not found, so its bLevelBasedDifficulty could not be read. If it is true, set it to false while this mod is enabled.")); }'),
    ('\t\t\tif (ImGuiMCP::Toggle("Enabled", &enabled))',
     '\t\t\tif (ImGuiMCP::Toggle(strings::TR("CDUI_Enabled", "Enabled"), &enabled))'),
    ('\t\t\tHelpMarker("Off writes nothing: the multipliers your game loaded with (vanilla, or an overhaul\'s) stay exactly "\n'
     '\t\t\t\t\t   "as they are, and switching off hands them back. On: the pairs below are written, and the game "\n'
     '\t\t\t\t\t   "reads the pair for the difficulty you play on.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpEnabled", "Off writes nothing: the multipliers your game loaded with (vanilla, or an overhaul\'s) stay exactly as they are, and switching off hands them back. On: the pairs below are written, and the game reads the pair for the difficulty you play on."));'),
    ('\t\t\tif (ImGuiMCP::Toggle("One pair for every difficulty", &sharedPair))',
     '\t\t\tif (ImGuiMCP::Toggle(strings::TR("CDUI_SharedPair", "One pair for every difficulty"), &sharedPair))'),
    ('\t\t\tHelpMarker("On: the single pair below is written for all six difficulties, so the game\'s difficulty setting "\n'
     '\t\t\t\t\t   "makes no difference to damage. Off: each difficulty has its own pair.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpSharedPair", "On: the single pair below is written for all six difficulties, so the game\'s difficulty setting makes no difference to damage. Off: each difficulty has its own pair."));'),
    ('\t\t\t\tif (NudgeableSlider("Damage to you##shared", &sharedToPC, 0.0F, 999.0F, "%.2f", 0.01F)) { ApplyLive(); }',
     '\t\t\t\tif (NudgeableSlider((std::string(strings::TR("CDUI_DamageToYou", "Damage to you")) + "##shared").c_str(), &sharedToPC, 0.0F, 999.0F, "%.2f", 0.01F)) { ApplyLive(); }'),
    ('\t\t\t\tHelpMarker("Damage multiplier applied to hits enemies land on you, at every difficulty. Ctrl+click to type a value.");',
     '\t\t\t\tHelpMarker(strings::TR("CDUI_HelpSharedToYou", "Damage multiplier applied to hits enemies land on you, at every difficulty. Ctrl+click to type a value."));'),
    ('\t\t\t\tif (NudgeableSlider("Damage by you##shared", &sharedByPC, 0.0F, 999.0F, "%.2f", 0.01F)) { ApplyLive(); }',
     '\t\t\t\tif (NudgeableSlider((std::string(strings::TR("CDUI_DamageByYou", "Damage by you")) + "##shared").c_str(), &sharedByPC, 0.0F, 999.0F, "%.2f", 0.01F)) { ApplyLive(); }'),
    ('\t\t\t\tHelpMarker("Damage multiplier applied to hits you land on enemies, at every difficulty. Ctrl+click to type a value.");',
     '\t\t\t\tHelpMarker(strings::TR("CDUI_HelpSharedByYou", "Damage multiplier applied to hits you land on enemies, at every difficulty. Ctrl+click to type a value."));'),
    ('\t\t\t\tImGuiMCP::Text("Fill the table from:");',
     '\t\t\t\tImGuiMCP::Text("%s", strings::TR("CDUI_FillFrom", "Fill the table from:"));'),
    ('if (ImGuiMCP::Button("Loaded values")) { OnMainThread([]() { Difficulty::UseLoadedValues(); Difficulty::ApplyLive(); }); statusMessage = "The table holds the values this game loaded with. Press Save to keep them."; }',
     'if (ImGuiMCP::Button(strings::TR("CDUI_BtnLoaded", "Loaded values"))) { OnMainThread([]() { Difficulty::UseLoadedValues(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusLoaded", "The table holds the values this game loaded with. Press Save to keep them."); }'),
    ('\t\t\t\tHelpMarker("Whatever your game holds at load - vanilla, or the overhaul you run. The starting point for tuning an overhaul without losing its numbers.");',
     '\t\t\t\tHelpMarker(strings::TR("CDUI_HelpLoaded", "Whatever your game holds at load - vanilla, or the overhaul you run. The starting point for tuning an overhaul without losing its numbers."));'),
    ('if (ImGuiMCP::Button("Vanilla")) { OnMainThread([]() { Difficulty::UseVanillaValues(); Difficulty::ApplyLive(); }); statusMessage = "The table holds Skyrim\'s vanilla values. Press Save to keep them."; }',
     'if (ImGuiMCP::Button(strings::TR("CDUI_BtnVanilla", "Vanilla"))) { OnMainThread([]() { Difficulty::UseVanillaValues(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusVanilla", "The table holds Skyrim\'s vanilla values. Press Save to keep them."); }'),
    ('{ OnMainThread([]() { Difficulty::UseBladeAndBlunt(); Difficulty::ApplyLive(); }); statusMessage = "The table holds Blade and Blunt\'s values. Press Save to keep them."; }',
     '{ OnMainThread([]() { Difficulty::UseBladeAndBlunt(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusBB", "The table holds Blade and Blunt\'s values. Press Save to keep them."); }'),
    ('\t\t\t\tHelpMarker("Its published pairs: to you as vanilla, by you 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.");',
     '\t\t\t\tHelpMarker(strings::TR("CDUI_HelpBB", "Its published pairs: to you as vanilla, by you 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5."));'),
    ('{ OnMainThread([]() { Difficulty::UseRequiem(); Difficulty::ApplyLive(); }); statusMessage = "The table holds Requiem\'s values. Press Save to keep them."; }',
     '{ OnMainThread([]() { Difficulty::UseRequiem(); Difficulty::ApplyLive(); }); statusMessage = strings::TR("CDUI_StatusRequiem", "The table holds Requiem\'s values. Press Save to keep them."); }'),
    ('\t\t\t\tHelpMarker("Every multiplier 1.0 - in Requiem the difficulty setting does no damage scaling by design.");',
     '\t\t\t\tHelpMarker(strings::TR("CDUI_HelpRequiem", "Every multiplier 1.0 - in Requiem the difficulty setting does no damage scaling by design."));'),
    ('\t\t\t\tImGuiMCP::TextWrapped("Each section below is one of Skyrim\'s own difficulty levels.");',
     '\t\t\t\tImGuiMCP::TextWrapped("%s", strings::TR("CDUI_SectionsNote1", "Each section below is one of Skyrim\'s own difficulty levels."));'),
    ('\t\t\t\tImGuiMCP::TextWrapped("Whichever difficulty you select in game uses that section\'s sliders.");',
     '\t\t\t\tImGuiMCP::TextWrapped("%s", strings::TR("CDUI_SectionsNote2", "Whichever difficulty you select in game uses that section\'s sliders."));'),
    (SIXROWS_OLD, SIXROWS_NEW),
    ('\t\t\tImGuiMCP::SeparatorText("Difficulty by level");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("CDUI_ByLevelHeader", "Difficulty by level"));'),
    ('\t\t\tif (ImGuiMCP::Toggle("Set the game\'s difficulty from your level", &byLevel))',
     '\t\t\tif (ImGuiMCP::Toggle(strings::TR("CDUI_ByLevel", "Set the game\'s difficulty from your level"), &byLevel))'),
    ('\t\t\tHelpMarker("On a save load and on every level-up, the highest difficulty whose level you have reached becomes the "\n'
     '\t\t\t\t\t   "game\'s difficulty - the same change the Settings menu makes, so the regeneration set follows it. "\n'
     '\t\t\t\t\t   "0 = that difficulty is never chosen by this rule. Off: the game\'s difficulty is yours to set.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpByLevel", "On a save load and on every level-up, the highest difficulty whose level you have reached becomes the game\'s difficulty - the same change the Settings menu makes, so the regeneration set follows it. 0 = that difficulty is never chosen by this rule. Off: the game\'s difficulty is yours to set."));'),
    ('\t\t\t\tif (level >= 0) { ImGuiMCP::Text("Level %d -> %s", level, target >= 0 ? kDifficultyNames[target] : "no row applies"); }',
     '\t\t\t\tif (level >= 0) { ImGuiMCP::Text(strings::TR("CDUI_LevelArrow", "Level %d -> %s"), level, target >= 0 ? DifficultyText(target) : strings::TR("CDUI_NoRow", "no row applies")); }'),
    ('\t\t\t\t\tif (ImGuiMCP::InputInt(std::format("{} from level", kDifficultyNames[d]).c_str(), &from))',
     '\t\t\t\t\tchar levelLabel[256] = {};\n'
     '\t\t\t\t\tstd::snprintf(levelLabel, sizeof(levelLabel), strings::TR("CDUI_FromLevel", "%s from level"), DifficultyText(d));\n'
     '\t\t\t\t\tif (ImGuiMCP::InputInt(levelLabel, &from))'),
    ('\t\t\t\tHelpMarker("Defaults are Blade and Blunt\'s milestones: one difficulty tier per ten levels.");',
     '\t\t\t\tHelpMarker(strings::TR("CDUI_HelpLevelTable", "Defaults are Blade and Blunt\'s milestones: one difficulty tier per ten levels."));'),
    ('\t\t\tImGuiMCP::SeparatorText("What the game is using right now");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("CDUI_NowHeader", "What the game is using right now"));'),
    ('\t\t\t\tImGuiMCP::Text("Damage at %s: x%.2f to you, x%.2f by you (loaded with x%.2f / x%.2f)", kDifficultyNames[now],',
     '\t\t\t\tImGuiMCP::Text(strings::TR("CDUI_DamageAt", "Damage at %s: x%.2f to you, x%.2f by you (loaded with x%.2f / x%.2f)"), DifficultyText(now),'),
    ('\t\t\t\tImGuiMCP::TextDisabled("No character loaded.");',
     '\t\t\t\tImGuiMCP::TextDisabled("%s", strings::TR("CDUI_NoCharacter", "No character loaded."));'),
    ('if (!enabled) { ImGuiMCP::TextDisabled("Not enabled - nothing is written; the values above are whatever the game loaded with."); }',
     'if (!enabled) { ImGuiMCP::TextDisabled("%s", strings::TR("CDUI_NotEnabled", "Not enabled - nothing is written; the values above are whatever the game loaded with.")); }'),

    # --- RenderRegenerationSection ---
    ('\t\t\tImGuiMCP::SeparatorText("Regeneration");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("CDUI_RegenHeader", "Regeneration"));'),
    ('\t\t\tif (ImGuiMCP::Toggle("Enabled##Regen", &enabled))',
     '\t\t\tif (ImGuiMCP::Toggle((std::string(strings::TR("CDUI_Enabled", "Enabled")) + "##Regen").c_str(), &enabled))'),
    ('\t\t\tHelpMarker("Off resets every setting below to the real vanilla value this mod captured "\n'
     '\t\t\t\t\t   "the first time it loaded - not just \\"stop touching them.\\"");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpRegenEnabled", "Off resets every setting below to the real vanilla value this mod captured the first time it loaded - not just \\"stop touching them.\\""));'),
    ('\t\t\tImGuiMCP::TextWrapped("Vanilla has no per-difficulty regeneration - this mod adds it. "\n'
     '\t\t\t\t\t\t\t\t  "Each difficulty below keeps its own combat rates and delays; "\n'
     '\t\t\t\t\t\t\t\t  "switching difficulty in the game\'s own menu switches which set "\n'
     '\t\t\t\t\t\t\t\t  "applies, live, with no need to open this page.");',
     '\t\t\tImGuiMCP::TextWrapped("%s", strings::TR("CDUI_RegenIntro", "Vanilla has no per-difficulty regeneration - this mod adds it. Each difficulty below keeps its own combat rates and delays; switching difficulty in the game\'s own menu switches which set applies, live, with no need to open this page."));'),
    ('\t\t\tif (ImGuiMCP::Combo("Editing", &editingIndex, kDifficultyNames, kDifficultyCount))',
     '\t\t\tif (ComboTR(strings::TR("CDUI_Editing", "Editing"), &editingIndex, kDifficultyKeys, kDifficultyLabels, kDifficultyCount))'),
    ('\t\t\tHelpMarker("Which difficulty\'s own values the sliders below are showing and editing.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpEditing", "Which difficulty\'s own values the sliders below are showing and editing."));'),
    ('\t\t\t\tImGuiMCP::TextWrapped("Current difficulty: %s. %s\'s values are active.",\n'
     '\t\t\t\t\t\t\t\t\t  kDifficultyNames[active], kDifficultyNames[active]);',
     '\t\t\t\tImGuiMCP::TextWrapped(strings::TR("CDUI_CurrentDifficulty", "Current difficulty: %s. %s\'s values are active."),\n'
     '\t\t\t\t\t\t\t\t\t  DifficultyText(active), DifficultyText(active));'),
    ('\t\t\t\tImGuiMCP::TextWrapped("Current difficulty: not applied yet.");',
     '\t\t\t\tImGuiMCP::TextWrapped("%s", strings::TR("CDUI_CurrentNotApplied", "Current difficulty: not applied yet."));'),
    ('\t\t\tif (ImGuiMCP::Button("Copy this set to every difficulty"))',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("CDUI_CopyToAll", "Copy this set to every difficulty")))'),
    ('\t\t\t\tstatusMessage = "Copied to every difficulty. Press Save to keep it.";',
     '\t\t\t\tstatusMessage = strings::TR("CDUI_StatusCopiedAll", "Copied to every difficulty. Press Save to keep it.");'),
    ('\t\t\tHelpMarker("Overwrites every OTHER difficulty\'s combat rates and delays with the set "\n'
     '\t\t\t\t\t   "you are currently editing.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpCopyToAll", "Overwrites every OTHER difficulty\'s combat rates and delays with the set you are currently editing."));'),
    ('\t\t\tImGuiMCP::Combo("##CopySource", &copySource, kDifficultyNames, kDifficultyCount);',
     '\t\t\tComboTR("##CopySource", &copySource, kDifficultyKeys, kDifficultyLabels, kDifficultyCount);'),
    ('\t\t\tif (ImGuiMCP::Button("Copy from"))',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("CDUI_CopyFrom", "Copy from")))'),
    ('\t\t\t\tstatusMessage = "Copied. Press Save to keep it.";',
     '\t\t\t\tstatusMessage = strings::TR("CDUI_StatusCopied", "Copied. Press Save to keep it.");'),
    ('\t\t\tHelpMarker("Starts the difficulty you are editing from the picked difficulty\'s current "\n'
     '\t\t\t\t\t   "values.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpCopyFrom", "Starts the difficulty you are editing from the picked difficulty\'s current values."));'),
    ('\t\t\tImGuiMCP::SeparatorText("In combat");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("CDUI_InCombat", "In combat"));'),
    ('if (RenderPerDifficultySlider("Health regen rate", "fCombatHealthRegenRateMult",',
     'if (RenderPerDifficultySlider(strings::TR("CDUI_HealthRegenRate", "Health regen rate"), "fCombatHealthRegenRateMult",'),
    ('if (RenderPerDifficultySlider("Magicka regen rate", "fCombatMagickaRegenRateMult",',
     'if (RenderPerDifficultySlider(strings::TR("CDUI_MagickaRegenRate", "Magicka regen rate"), "fCombatMagickaRegenRateMult",'),
    ('if (RenderPerDifficultySlider("Stamina regen rate", "fCombatStaminaRegenRateMult",',
     'if (RenderPerDifficultySlider(strings::TR("CDUI_StaminaRegenRate", "Stamina regen rate"), "fCombatStaminaRegenRateMult",'),
    ('\t\t\tImGuiMCP::SeparatorText("After damage");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("CDUI_AfterDamage", "After damage"));'),
    ('if (RenderPerDifficultySlider("Health regen delay (s)", "fDamagedHealthRegenDelay",',
     'if (RenderPerDifficultySlider(strings::TR("CDUI_HealthRegenDelay", "Health regen delay (s)"), "fDamagedHealthRegenDelay",'),
    ('if (RenderPerDifficultySlider("Magicka regen delay (s)", "fDamagedMagickaRegenDelay",',
     'if (RenderPerDifficultySlider(strings::TR("CDUI_MagickaRegenDelay", "Magicka regen delay (s)"), "fDamagedMagickaRegenDelay",'),
    ('if (RenderPerDifficultySlider("Stamina regen delay (s)", "fDamagedStaminaRegenDelay",',
     'if (RenderPerDifficultySlider(strings::TR("CDUI_StaminaRegenDelay", "Stamina regen delay (s)"), "fDamagedStaminaRegenDelay",'),
    ('if (RenderPerDifficultySlider("Generic damaged-attribute delay (s)", "fDamagedAVRegenDelay",',
     'if (RenderPerDifficultySlider(strings::TR("CDUI_AVRegenDelay", "Generic damaged-attribute delay (s)"), "fDamagedAVRegenDelay",'),
    ('\t\t\tImGuiMCP::TextWrapped("Delay ceilings - one value, every difficulty (the plan: these "\n'
     '\t\t\t\t\t\t\t\t\t"only matter once a delay above is raised past them):");',
     '\t\t\tImGuiMCP::TextWrapped("%s", strings::TR("CDUI_CeilingsNote", "Delay ceilings - one value, every difficulty (the plan: these only matter once a delay above is raised past them):"));'),
    ('if (RenderGlobalSlider("Health delay ceiling (s)", "fHealthRegenDelayMax",',
     'if (RenderGlobalSlider(strings::TR("CDUI_HealthCeiling", "Health delay ceiling (s)"), "fHealthRegenDelayMax",'),
    ('if (RenderGlobalSlider("Magicka delay ceiling (s)", "fMagickaRegenDelayMax",',
     'if (RenderGlobalSlider(strings::TR("CDUI_MagickaCeiling", "Magicka delay ceiling (s)"), "fMagickaRegenDelayMax",'),
    ('if (RenderGlobalSlider("Stamina delay ceiling (s)", "fStaminaRegenDelayMax",',
     'if (RenderGlobalSlider(strings::TR("CDUI_StaminaCeiling", "Stamina delay ceiling (s)"), "fStaminaRegenDelayMax",'),
    ('\t\t\tImGuiMCP::SeparatorText("Situational");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("CDUI_Situational", "Situational"));'),
    ('if (RenderGlobalSlider("Out of breath stamina delay (s)", "fOutOfBreathStaminaRegenDelay",',
     'if (RenderGlobalSlider(strings::TR("CDUI_OutOfBreath", "Out of breath stamina delay (s)"), "fOutOfBreathStaminaRegenDelay",'),
    ('if (RenderGlobalSlider("Downed essential NPC regen rate", "fEssentialDownCombatHealthRegenMult",',
     'if (RenderGlobalSlider(strings::TR("CDUI_EssentialDown", "Downed essential NPC regen rate"), "fEssentialDownCombatHealthRegenMult",'),

    # --- RenderDebugSection ---
    ('\t\t\tImGuiMCP::SeparatorText("Debug");',
     '\t\t\tImGuiMCP::SeparatorText(strings::TR("CDUI_DebugHeader", "Debug"));'),
    ('\t\t\tif (ImGuiMCP::Combo("Log level", &level, kLogLevelNames, kLogLevelCount))',
     '\t\t\tif (ComboTR(strings::TR("CDUI_LogLevel", "Log level"), &level, kLogLevelKeys, kLogLevelLabels, kLogLevelCount))'),
    ('\t\t\tHelpMarker("Applies to the log immediately. Ships at Trace by default - see CLAUDE.md rule 31.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpLogLevel", "Applies to the log immediately. Ships at Trace by default - see CLAUDE.md rule 31."));'),

    # --- RenderButtons ---
    ('\t\t\tif (ImGuiMCP::Button("Save"))',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("CDUI_SaveBtn", "Save")))'),
    ('\t\t\t\t\tstatusMessage = settings::Save() ? "Settings saved." : "Could not save the INI. See the log for why.";',
     '\t\t\t\t\tstatusMessage = settings::Save() ? strings::TR("CDUI_StatusSaved", "Settings saved.")\n'
     '\t\t\t\t\t\t\t\t\t\t\t\t\t  : strings::TR("CDUI_StatusSaveFail", "Could not save the INI. See the log for why.");'),
    ('\t\t\tHelpMarker("Writes every setting above back to the INI. Comments and unrelated keys are left alone.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpSave", "Writes every setting above back to the INI. Comments and unrelated keys are left alone."));'),
    ('\t\t\tif (ImGuiMCP::Button("Reload from INI"))',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("CDUI_ReloadBtn", "Reload from INI")))'),
    ('\t\t\t\t\t\t\t\t\t\t ? "Settings reloaded from the INI."\n'
     '\t\t\t\t\t\t\t\t\t\t : "Could not read the INI. See the log for why.";',
     '\t\t\t\t\t\t\t\t\t\t ? strings::TR("CDUI_StatusReloaded", "Settings reloaded from the INI.")\n'
     '\t\t\t\t\t\t\t\t\t\t : strings::TR("CDUI_StatusReloadFail", "Could not read the INI. See the log for why.");'),
    ('\t\t\tHelpMarker("Throws away any change made here since the last save, re-reads the INI from disk, and applies it immediately.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpReload", "Throws away any change made here since the last save, re-reads the INI from disk, and applies it immediately."));'),
    ('\t\t\tif (ImGuiMCP::Button("Restore defaults"))',
     '\t\t\tif (ImGuiMCP::Button(strings::TR("CDUI_RestoreBtn", "Restore defaults")))'),
    ('\t\t\t\tstatusMessage = "Defaults restored and applied. Press Save to keep them.";',
     '\t\t\t\tstatusMessage = strings::TR("CDUI_StatusRestored", "Defaults restored and applied. Press Save to keep them.");'),
    ('\t\t\tHelpMarker("Puts every setting back to the value it has on a fresh install, and applies it immediately. Nothing is written to the INI until you press Save.");',
     '\t\t\tHelpMarker(strings::TR("CDUI_HelpRestore", "Puts every setting back to the value it has on a fresh install, and applies it immediately. Nothing is written to the INI until you press Save."));'),

    # --- the two page callbacks: Tick() first, then the Settings page's intro line ---
    ('\tvoid __stdcall SettingsPanel::Render()\n'
     '\t{\n'
     '\t\tImGuiMCP::TextWrapped("Changes below apply immediately to the game\'s own difficulty "\n'
     '\t\t\t\t\t\t\t  "damage multipliers - the same ones the vanilla difficulty slider "\n'
     '\t\t\t\t\t\t\t  "sets, per level. Press Save separately to keep them for next time.");',
     '\tvoid __stdcall SettingsPanel::Render()\n'
     '\t{\n'
     '\t\tstrings::Tick();\n'
     '\n'
     '\t\tImGuiMCP::TextWrapped("%s", strings::TR("CDUI_SettingsIntro", "Changes below apply immediately to the game\'s own difficulty damage multipliers - the same ones the vanilla difficulty slider sets, per level. Press Save separately to keep them for next time."));'),
    ('\tvoid __stdcall SettingsPanel::RenderRegeneration()\n'
     '\t{\n'
     '\t\tImGuiMCP::PushItemWidth(260.0F);',
     '\tvoid __stdcall SettingsPanel::RenderRegeneration()\n'
     '\t{\n'
     '\t\tstrings::Tick();\n'
     '\n'
     '\t\tImGuiMCP::PushItemWidth(260.0F);'),
]


def patch_ui_cpp():
    path = os.path.join(REPO, "source", "UI.cpp")
    text = read(path)
    if "strings::Tick();" in text:
        print("  UI.cpp: already patched")
        return
    text = apply_all(text, UI_PAIRS, "UI.cpp")
    write(path, text)
    print("  UI.cpp: patched ({} anchors)".format(len(UI_PAIRS)))


def main():
    print("lang-patch.py - Custom Difficulty UI")
    patch_skse_menu_framework_h()
    patch_message_listeners_cpp()
    patch_devbench_tool_cpp()
    patch_ui_cpp()
    print("done")


if __name__ == "__main__":
    main()
