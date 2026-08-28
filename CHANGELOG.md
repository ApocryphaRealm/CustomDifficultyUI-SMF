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

## 1.0.2 - 2026-08-27 - untested

### Fixed
- "Reload from INI" reported success but applied the pristine shipped values instead of what was just saved. Settings are now read back with plain file I/O (matching how they are written), rather than through the game's Win32 profile API, which PrivateProfileRedirector intercepts and answers from a startup cache. Adds a self-contained INI parser (`ReadIniFile`/`ReadFromFile`) so startup and reload share one read path.

### Changed
- Difficulty section headings now use the names Skyrim's own menu shows (Novice / Apprentice / Adept / Expert / Master / Legendary) instead of the engine-internal Very Easy..Very High labels; added two explanatory lines above the sliders. ImGui `##` id suffixes left untouched.
- CMakeLists: state debug-symbol flags on the target (`/Zi`, `/DEBUG:FULL /OPT:REF /OPT:ICF`) since the preset chain leaves RelWithDebInfo with no CodeView record (rule 43), and strip the build machine's absolute paths from the DLL (`/d1trimfile`, `/PDBALTPATH:%_PDB%`).

### Committed
- 2026-08-28: prior uncommitted working-tree work verified to build clean and pushed to origin.

### Known
- This mod is the PRECEDENT for the PrivateProfileRedirector fix used across the project. Its settings reload returned defaults because the read went through the cached Win32 profile API while the write used plain file I/O; reading the INI with plain file I/O resolved it. Source inspection on 2026-08-27 confirms it is the only SMF mod here with zero INISettingCollection::ReadFromFile calls, which is what made the pattern recognisable in the other six.

## 1.0.1 - 2026-08-27 - untested

### Changed
- local package only - no tag, no commit

## 1.0.0 - 2026-08-27 - working

### Changed
- git tag v1.0.0 pushed

