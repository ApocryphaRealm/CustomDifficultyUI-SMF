# Custom Difficulty UI

Version 1.0.4

**Version 1.0.4.** A fresh, native C++ implementation of **Custom Difficulty UI** (Nexus
skyrimspecialedition/mods/14362) with a real in-game settings page, registering with
**Apocrypha Menu Framework (AMF)** by its real module name and falling back to stock SKSE Menu
Framework where AMF is not installed. This isn't a port of a compiled DLL - the original mod has
no SKSE plugin at all, only a SkyUI MCM (Papyrus + ESP) - so there is no existing C++ project to
fork; this is a fresh CommonLibSSE-NG project that mirrors the original's own real settings
surface 1:1, then goes beyond it with per-difficulty regeneration control the original never had.

**Licence: MIT.** Original code, written from scratch for this project - see `LICENSE`.

## What it does

Independent damage multipliers for each of Skyrim's six difficulty levels (Very Easy / Easy /
Normal / High / Very High / Legendary), split into two directions per level:

- **Damage to you** - how hard enemies hit you at that difficulty.
- **Damage by you** - how hard you hit enemies at that difficulty.

A master **Enabled** toggle turns the whole thing off at once - and, unlike simply "stop
touching them," actually resets all twelve multipliers back to Skyrim's own real vanilla values
immediately, matching the original mod's own `SetDefaultSettings()` behaviour exactly, not just
freezing whatever was last applied.

Every default matches vanilla exactly, so installing this changes nothing about difficulty
until you turn Enabled on and change a value:

| Difficulty | To you (vanilla) | By you (vanilla) |
| --- | --- | --- |
| Very Easy | 0.50 | 2.00 |
| Easy | 0.75 | 1.50 |
| Normal | 1.00 | 1.00 |
| High | 1.50 | 0.75 |
| Very High | 2.00 | 0.50 |
| Legendary | 3.00 | 0.25 |

## Where this came from

Custom Difficulty UI's own archive ships its actual Papyrus source (not just a compiled
`.pex`) - `CustomDifficultyUIControlScript.psc` was read directly, not decompiled or guessed
at (CLAUDE.md rule 30). Its entire mechanic is twelve calls to the SKSE-native Papyrus function
`Game.SetGameSettingFloat()`, one per difficulty level times direction, naming the exact same
twelve vanilla GameSettings this build writes to
(`fDiffMultHPToPCVE`/`fDiffMultHPByPCVE`/... through `...PCL`). `SetDefaultSettings()` is where
the vanilla-default table above comes from - upstream's own real reset values, not guessed.

This build reimplements the same mechanic in native C++ against
`RE::GameSettingCollection`/`RE::Setting` rather than Papyrus, with a real settings page (through
AMF, the SMF-compatible menu framework this project builds and maintains) in place of the
original's SkyUI MCM. Game mechanics (and the vanilla GameSetting names/values themselves) aren't
copyrightable, so this carries no licensing entanglement with the original mod - only its own
Papyrus/ESP implementation would be, and none of that was copied.

## Regeneration - per-difficulty, added in 1.0.2

Vanilla has no per-difficulty regeneration at all - `fCombatHealthRegenRateMult` and its eleven
siblings (see `4. plans\custom-difficulty-ui-regeneration\plan.md` for the full design) are each
ONE real GameSetting, global to every difficulty. This build adds the per-difficulty behaviour
itself: a value is stored for each of the six difficulties, and whichever one matches the CURRENT
difficulty is written into the game's one real GameSetting - so switching difficulty in the
game's own Settings menu changes the applied regeneration values live, with no visit to this
mod's page. Detected event-driven, via a `RE::MenuOpenCloseEvent` sink that re-checks on every
menu close and re-applies only if difficulty actually moved - no polling.

Twelve settings: three combat regen rate multipliers and four damaged-regen delays, stored per
difficulty; three delay ceilings plus two situational settings (out-of-breath stamina delay,
downed-essential health regen), stored once, global to every difficulty. Every setting name is
resolved and verified against the running game at load - a name that does not exist on this
runtime is skipped and its control does not appear, rather than writing blind. Vanilla defaults
are captured LIVE from the running game the first time this mod ever loads, since - unlike the
twelve damage settings above - there is no Papyrus source to read a compiled default from.

Proven end to end against the live engine, not just this mod's own bookkeeping: see
`.MD\scripts\devbench-loops\cdui-regeneration-gate.ps1`, which sets two difficulties to different
values, switches between them via `customdifficulty.control`, and reads the resolved
`RE::Setting*` back directly - independent proof the switch reaches the engine, in both
directions.

## The API (small, and already vanilla-documented)

- `RE::GameSettingCollection::GetSingleton()->GetSetting(name)` resolves each of the twelve
  vanilla settings once, at `kMessage_DataLoaded` - checked for both a null result and an
  unexpected `Setting::Type` (CLAUDE.md rule 30 - ask the object, don't assume) before ever
  writing to it.
- Writing a value is a direct assignment to the setting's own public `data.f` field - `Setting`
  has no `SetFloat()` method exposed, only `GetFloat()`, so this is the same pattern other
  runtime-GameSetting-tweaking SKSE plugins already use.

## Settings persistence

`Restore defaults` only resets the in-memory values to what this DLL compiles in (vanilla's own
values, see the table above) - it never touches the INI. `Save` and `Reload from INI` are the
only two actions that touch `CustomDifficultyUI.ini` on disk, written with plain file I/O (never
`WritePrivateProfileString` - Mod Organizer 2's usvfs does not reliably redirect that API; see
CLAUDE.md rule 16) so a save actually reaches disk under MO2.

Because `GameSettingCollection` is a process-global engine structure, not per-save data, this
mod also re-applies its twelve values at every `kMessage_PostLoadGame`/`kNewGame`, not just once
at boot - matching the original's own `OnPlayerLoadGame()` re-apply.

## Building

CMake + vcpkg, matching every other mod in this project:

```
configure.bat
build.bat
```

Requires `VCPKG_ROOT` pointing at a vcpkg checkout; Visual Studio (with the C++ toolset) plus
the CMake and Ninja it ships with are located automatically by `find-msvc.bat`. See
`CLAUDE.md` rules 4-5 for why these scripts never hardcode a machine-specific path.

## Status

Built - see this project's own `PROGRESS.md` for the current state (packaging/install/test
status).

## What changed

Version 1.0.4
Drawn the explanatory lines on the settings page in normal text instead of the near-invisible disabled grey.
Confirmed in game that the settings survive a save load: the values live in the INI and are re-applied on every save load.
Version 1.0.3
Added a Skyrim 1.7.99 / 1.7.104 build; the mod installs as a FOMOD that picks the build for your game version.
Version 1.0.2
Added a Regeneration page with per-difficulty regeneration control that follows the game's own difficulty setting live.
