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

### Changed
- local package built from an uncommitted working tree - exists nowhere in git

### Known
- This mod is the PRECEDENT for the PrivateProfileRedirector fix used across the project. Its settings reload returned defaults because the read went through the cached Win32 profile API while the write used plain file I/O; reading the INI with plain file I/O resolved it. Source inspection on 2026-08-27 confirms it is the only SMF mod here with zero INISettingCollection::ReadFromFile calls, which is what made the pattern recognisable in the other six.

## 1.0.1 - 2026-08-27 - untested

### Changed
- local package only - no tag, no commit

## 1.0.0 - 2026-08-27 - working

### Changed
- git tag v1.0.0 pushed

