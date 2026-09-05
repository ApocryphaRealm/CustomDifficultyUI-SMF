# CustomDifficultyUI-SMF - changelog

Rule 61: this mod's own history, kept beside the code it describes.

> **The entries below this line were RECONSTRUCTED from `version-ledger.json` on
> 2026-08-27, not written at the time of the change.** They carry only what the ledger
> recorded - the status and the evidence - so they are thinner than a real entry and may
> be missing changes the ledger never captured. Treat them as a starting point rather
> than a record. Everything from the next version onward is written as it happens.

Each version carries its **version-ledger status**: **working** (observed in game),
**untested** (built, not confirmed), **failed** (built but broken; the number was
reclaimed), **scratch** (a hypothesis-test build that never held a real number).

<!-- VERSIONING-RULES -->
> **Versioning rules (CLAUDE.md rules 6 and 48 - identical for mods and documents):**
> * `X.Y.Z`. A change increments the THIRD number. At `.9` the MINOR rolls: `1.0.9 -> 1.1.0`;
>   `1.0.10` never exists.
> * The next number is **LAST WORKING + 1**. A failed, scratch or untested test build does NOT
>   consume its number - the next attempt at the same step REUSES it.
> * Numbers are assigned by the tooling, never by hand: mods via `version-ledger.ps1 -Action next`
>   then `set-version.ps1`; governed documents via `docs-pipeline.ps1 -Action bump`; the rules via
>   `rules-version.ps1 -Action bump`. If a number was typed by hand, it is wrong until the tool
>   agrees.

## 1.0.3 - 2026-09-05 - untested

### Added
- Added a Skyrim 1.7.99 / 1.7.104 build; the mod now installs as a FOMOD that picks the build for your game version (SE 1.5.97 / AE 1.6.1170, or Skyrim 1.7.x).

## 1.0.2 - 2026-09-02 - working

### Added
- Per-difficulty stamina, health and magicka regeneration control. Vanilla ties regeneration
  to one global setting regardless of difficulty; this mod stores a separate value for each of
  the six difficulties and switches which one is live whenever the player's difficulty changes,
  with no visit to the settings page required. Covers seven per-difficulty settings (combat
  health/magicka/stamina regen rate, and the four damaged-regen delays) plus five settings that
  stay global across all difficulties (the three regen delay ceilings, out-of-breath stamina
  delay, and essential-down combat health regen). A new "Regeneration" page holds the toggle,
  per-difficulty sliders grouped by category, and copy-to-all / copy-from-difficulty buttons.
  Off by default; enabling it is opt-in.
- Confirmed end to end with a live-engine read, independent of the mod's own bookkeeping:
  setting different values on two difficulties and switching between them shows the resolved
  GameSetting itself following the change, in both directions.

## 1.0.1 - 2026-09-01 - working

### Fixed
- The settings menu registers with Apocrypha Menu Framework (AMF) by its real module name,
  with stock SKSE Menu Framework as the fallback - on an AMF stack the page previously never
  registered and the menu was silently absent. Same fix as Dragon's Eye Minimap 1.5.8.
- The .pdb debug symbols now ship inside the main download so Crash Logger can resolve this
  mod's stack frames. There is no separate Debug Symbols download.
- "Reload from INI" reported success but applied the pristine shipped values instead of what was just saved. Settings are now read back with plain file I/O (matching how they are written), rather than through the game's Win32 profile API, which PrivateProfileRedirector intercepts and answers from a startup cache. Adds a self-contained INI parser (`ReadIniFile`/`ReadFromFile`) so startup and reload share one read path.

### Changed
- Difficulty section headings now use the names Skyrim's own menu shows (Novice / Apprentice / Adept / Expert / Master / Legendary) instead of the engine-internal Very Easy..Very High labels; added two explanatory lines above the sliders. ImGui `##` id suffixes left untouched.
- CMakeLists: state debug-symbol flags on the target (`/Zi`, `/DEBUG:FULL /OPT:REF /OPT:ICF`) since the preset chain leaves RelWithDebInfo with no CodeView record (rule 43), and strip the build machine's absolute paths from the DLL (`/d1trimfile`, `/PDBALTPATH:%_PDB%`).

### Committed
- 2026-08-28: prior uncommitted working-tree work verified to build clean and pushed to origin.

### Known
- This mod is the PRECEDENT for the PrivateProfileRedirector fix used across the project. Its settings reload returned defaults because the read went through the cached Win32 profile API while the write used plain file I/O; reading the INI with plain file I/O resolved it. Source inspection on 2026-08-27 confirms it is the only SMF mod here with zero INISettingCollection::ReadFromFile calls, which is what made the pattern recognisable in the other six.

### Carried from the earlier unreleased 1.0.1 build
### Changed
- local package only - no tag, no commit

## 1.0.0 - 2026-08-27 - working

### Changed
- git tag v1.0.0 pushed

