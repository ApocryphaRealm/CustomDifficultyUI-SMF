#include "Settings.h"

#include "utils/INISettingCollection.h"
#include "utils/Logger.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <map>
#include <string>
#include <type_traits>

namespace settings
{
	using namespace utils;

	namespace
	{
		constexpr const char* kDebugSection = "Debug";
		constexpr const char* kDifficultySection = "Difficulty";
		constexpr const char* kRegenSection = "Regeneration";

		// Shared with the difficulty section's own field names above (VE/E/N/H/VH/L) so the two
		// read the same at a glance in the INI.
		constexpr std::array<const char*, regeneration::kDifficultyCount> kDifficultySuffix = {
			"VE", "E", "N", "H", "VH", "L"
		};

		// One entry per per-difficulty regeneration setting: the base INI key name (the suffix
		// above is appended per difficulty) and a pointer to its six-slot storage in Settings.h.
		// Declared here, not in Regeneration.cpp, because Regeneration.cpp's own table exists for
		// a different job (resolving and applying real GameSettings) - this one exists only to
		// keep Init()/ReadFromCollection()/Save() from repeating each of the 42 keys by hand.
		struct RegenPerDifficultyField
		{
			const char* baseName;
			std::array<float, regeneration::kDifficultyCount>* values;
		};

		std::array<RegenPerDifficultyField, 7> kRegenPerDifficultyFields = { {
			{ "fCombatHealthRegenRateMult", &regeneration::combatHealthRegenRateMult },
			{ "fCombatMagickaRegenRateMult", &regeneration::combatMagickaRegenRateMult },
			{ "fCombatStaminaRegenRateMult", &regeneration::combatStaminaRegenRateMult },
			{ "fDamagedHealthRegenDelay", &regeneration::damagedHealthRegenDelay },
			{ "fDamagedMagickaRegenDelay", &regeneration::damagedMagickaRegenDelay },
			{ "fDamagedStaminaRegenDelay", &regeneration::damagedStaminaRegenDelay },
			{ "fDamagedAVRegenDelay", &regeneration::damagedAVRegenDelay },
		} };

		struct RegenGlobalField
		{
			const char* name;
			float* value;
		};

		std::array<RegenGlobalField, 5> kRegenGlobalFields = { {
			{ "fHealthRegenDelayMax", &regeneration::healthRegenDelayMax },
			{ "fMagickaRegenDelayMax", &regeneration::magickaRegenDelayMax },
			{ "fStaminaRegenDelayMax", &regeneration::staminaRegenDelayMax },
			{ "fOutOfBreathStaminaRegenDelay", &regeneration::outOfBreathStaminaRegenDelay },
			{ "fEssentialDownCombatHealthRegenMult", &regeneration::essentialDownCombatHealthRegenMult },
		} };

		// A hand-edited negative value would otherwise be indistinguishable from
		// regeneration::kUnset (the "never saved, seed me from live vanilla" sentinel) once read
		// back from the file - clamped to 0 instead, which is a real, valid "no regen"/"no delay"
		// value that can never collide with the sentinel.
		float ClampRegen(float a_value)
		{
			return a_value < 0.0F ? 0.0F : a_value;
		}

		std::string iniPath;
		std::string iniFileName;

		// The values the plugin compiles in, captured before the INI is read so that
		// "Restore defaults" means "what you would get with no INI at all". Regeneration's own
		// float settings are NOT here - there is no compiled-in default for them the way there is
		// for difficulty's twelve (see Settings.h), so their restore path is
		// Regeneration::RestoreDefaults() instead, sourced from the real vanilla value captured
		// live at Regeneration::Init(). Only the enabled toggle has a real compile-time default
		// (false), so only it is captured here.
		struct Defaults
		{
			logger::level logLevel;

			bool enabled;
			float toPCVE, toPCE, toPCN, toPCH, toPCVH, toPCL;
			float byPCVE, byPCE, byPCN, byPCH, byPCVH, byPCL;

			bool regenEnabled;
		};

		Defaults defaults;

		void CaptureDefaults()
		{
			using namespace difficulty;

			defaults.logLevel = debug::logLevel;

			defaults.enabled = enabled;
			defaults.toPCVE = toPCVE; defaults.toPCE = toPCE; defaults.toPCN = toPCN;
			defaults.toPCH = toPCH; defaults.toPCVH = toPCVH; defaults.toPCL = toPCL;
			defaults.byPCVE = byPCVE; defaults.byPCE = byPCE; defaults.byPCN = byPCN;
			defaults.byPCH = byPCH; defaults.byPCVH = byPCVH; defaults.byPCL = byPCL;

			defaults.regenEnabled = regeneration::enabled;
		}

		// One key a Save() is about to write. Queued rather than written on the spot so the
		// whole file is rewritten once at the end instead of once per key.
		struct PendingWrite
		{
			std::string section;
			std::string key;
			std::string value;
		};

		std::vector<PendingWrite> pendingWrites;

		// Section and key lookups are case-insensitive, matching how EqualsIgnoreCase already
		// compares them elsewhere in this file and how INI files are conventionally treated.
		struct CaseInsensitiveLess
		{
			bool operator()(const std::string& a_lhs, const std::string& a_rhs) const
			{
				return std::lexicographical_compare(
					a_lhs.begin(), a_lhs.end(), a_rhs.begin(), a_rhs.end(),
					[](unsigned char a_l, unsigned char a_r) {
						return std::tolower(a_l) < std::tolower(a_r);
					});
			}
		};

		bool EqualsIgnoreCase(std::string_view a_lhs, std::string_view a_rhs)
		{
			return std::ranges::equal(a_lhs, a_rhs, [](char a_l, char a_r) {
				return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
			});
		}

		std::string_view Trim(std::string_view a_text)
		{
			constexpr std::string_view kSpace = " \t\r\n";

			const std::size_t first = a_text.find_first_not_of(kSpace);

			if (first == std::string_view::npos)
			{
				return {};
			}

			return a_text.substr(first, a_text.find_last_not_of(kSpace) - first + 1);
		}

		// Queues a key for the next FlushPendingWrites(). Cannot fail on its own - the file is
		// only touched at flush time, so that is where a write error can surface.
		bool WriteRaw(const char* a_section, const char* a_key, const std::string& a_value)
		{
			pendingWrites.emplace_back(a_section, a_key, a_value);

			return true;
		}

		// Rewrites the INI with every queued change applied in place, leaving comments and any
		// keys this plugin does not know about untouched.
		//
		// Deliberately plain file I/O rather than WritePrivateProfileString. Mod Organizer 2's
		// usvfs does not reliably redirect the Win32 profile APIs, so a call could report
		// success while never reaching disk - see CLAUDE.md rule 16. Ordinary file reads and
		// writes go through the VFS correctly.
		bool FlushPendingWrites()
		{
			if (pendingWrites.empty())
			{
				return true;
			}

			std::string text;

			{
				std::ifstream in(iniPath, std::ios::binary);

				if (in)
				{
					text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				}
				else
				{
					logger::warn("Could not read {} before saving; writing it from scratch", iniPath);
				}
			}

			const std::string newline = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";

			std::vector<std::string> lines;

			for (std::size_t start = 0; start <= text.size();)
			{
				const std::size_t end = text.find('\n', start);

				if (end == std::string::npos)
				{
					if (start < text.size())
					{
						lines.emplace_back(text.substr(start));
					}

					break;
				}

				std::string line = text.substr(start, end - start);

				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				lines.push_back(std::move(line));
				start = end + 1;
			}

			std::vector<bool> applied(pendingWrites.size(), false);

			std::string currentSection;

			for (std::string& line : lines)
			{
				const std::string_view trimmed = Trim(line);

				if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
				{
					currentSection = std::string{ trimmed.substr(1, trimmed.size() - 2) };

					continue;
				}

				if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#')
				{
					continue;
				}

				const std::size_t separator = line.find('=');

				if (separator == std::string::npos)
				{
					continue;
				}

				const std::string_view key = Trim(std::string_view{ line }.substr(0, separator));

				for (std::size_t i = 0; i < pendingWrites.size(); ++i)
				{
					if (applied[i] || !EqualsIgnoreCase(currentSection, pendingWrites[i].section) ||
						!EqualsIgnoreCase(key, pendingWrites[i].key))
					{
						continue;
					}

					line = std::format("{}={}", key, pendingWrites[i].value);
					applied[i] = true;

					break;
				}
			}

			for (std::size_t i = 0; i < pendingWrites.size(); ++i)
			{
				if (applied[i])
				{
					continue;
				}

				const PendingWrite& pending = pendingWrites[i];

				std::size_t insertAt = lines.size();
				bool sectionFound = false;

				for (std::size_t l = 0; l < lines.size(); ++l)
				{
					const std::string_view trimmed = Trim(lines[l]);

					if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
					{
						continue;
					}

					if (sectionFound)
					{
						insertAt = l;

						break;
					}

					if (EqualsIgnoreCase(trimmed.substr(1, trimmed.size() - 2), pending.section))
					{
						sectionFound = true;
						insertAt = lines.size();
					}
				}

				if (!sectionFound)
				{
					if (!lines.empty() && !Trim(lines.back()).empty())
					{
						lines.emplace_back();
					}

					lines.push_back(std::format("[{}]", pending.section));
					insertAt = lines.size();
				}

				while (insertAt > 0 && Trim(lines[insertAt - 1]).empty())
				{
					--insertAt;
				}

				lines.insert(lines.begin() + insertAt, std::format("{}={}", pending.key, pending.value));
				applied[i] = true;
			}

			std::string output;

			for (const std::string& line : lines)
			{
				output += line;
				output += newline;
			}

			std::ofstream out(iniPath, std::ios::binary | std::ios::trunc);

			if (!out)
			{
				logger::error("Could not open {} for writing; settings were not saved", iniPath);

				return false;
			}

			out.write(output.data(), static_cast<std::streamsize>(output.size()));
			out.close();

			if (!out)
			{
				logger::error("Could not write {}; settings were not saved", iniPath);

				return false;
			}

			logger::debug("FlushPendingWrites: wrote {} key(s) to {}", pendingWrites.size(), iniPath);

			return true;
		}

		bool WriteFloat(const char* a_section, const char* a_key, float a_value)
		{
			return WriteRaw(a_section, a_key, std::format("{:g}", a_value));
		}

		bool WriteUInt(const char* a_section, const char* a_key, std::uint32_t a_value)
		{
			return WriteRaw(a_section, a_key, std::format("{}", a_value));
		}

		bool WriteBool(const char* a_section, const char* a_key, bool a_value)
		{
			return WriteRaw(a_section, a_key, a_value ? "1" : "0");
		}

		// RE::INISettingCollection::GetSetting returns null for a name that is not in the
		// collection, and the templated GetSetting<T> helpers dereference that without
		// checking. Read through here instead: the value keeps whatever default it already
		// had, and the log says which setting went missing.
		template <typename T>
		T Read(INISettingCollection* a_collection, const char* a_name, T a_fallback)
		{
			if (!a_collection->GetSetting(a_name))
			{
				logger::error("Setting \"{}\" is missing from the collection; keeping the current value", a_name);

				return a_fallback;
			}

			return a_collection->GetSetting<T>(a_name);
		}

		// MakeSetting takes the setting's type from the first letter of its name - i signed,
		// u unsigned, f float, b bool, s string - and quietly hands back a setting with a null
		// name when the value passed does not match. The game's collection dereferences that
		// name, so inserting one crashes on startup with nothing useful in the log. Refuse it
		// here instead, where the message can say which setting is at fault.
		void AddChecked(INISettingCollection* a_collection, RE::Setting* a_setting, const char* a_name)
		{
			if (a_setting && a_setting->name)
			{
				a_collection->AddSettings(a_setting);

				return;
			}

			logger::critical("Setting \"{}\" was built with a value that does not match the type its "
							 "name prefix promises, so it has been skipped", a_name);
		}

		// Every multiplier is clamped to a generous but sane range rather than trusting a
		// hand-edited INI verbatim - vanilla's own values never exceed roughly this range.
		float ClampMult(float a_value)
		{
			return std::clamp(a_value, 0.0F, 20.0F);
		}


		// -----------------------------------------------------------------------------------
		// Reading the INI ourselves, with plain file I/O
		// -----------------------------------------------------------------------------------
		//
		// This exists because of a real, reproduced bug (2026-08-27): "Reload from INI" reported
		// success and applied the pristine shipped values instead of what had just been saved.
		//
		// The cause was an ASYMMETRY between how this file was written and how it was read:
		//
		//   * FlushPendingWrites() below writes with plain file I/O, deliberately - rule 16
		//     records that MO2's usvfs does not reliably redirect the Win32 profile APIs, so
		//     WritePrivateProfileString can report success without reaching disk.
		//   * Reload() used to read through the game's own INI machinery, which IS the Win32
		//     profile API - and PrivateProfileRedirector (present in most large modlists) hooks
		//     that family and answers from an in-memory cache it populated at startup.
		//
		// So our write went to disk and the read came from a cache that never learned about it.
		// The redirector also periodically writes its cache back, which makes this a data-LOSS
		// risk and not merely a stale-read one.
		//
		// The fix is symmetry: read the file the same way we write it. Do NOT "fix" this by
		// switching the write to WritePrivateProfileString instead - that would make both halves
		// agree by putting both back on the API rule 16 says is unreliable, trading a visible bug
		// for a silent one.
		using IniData = std::map<std::string, std::map<std::string, std::string, CaseInsensitiveLess>, CaseInsensitiveLess>;

		// Parses iniPath into section -> key -> raw value. Comments, blank lines and keys this
		// plugin does not know about are simply not represented; nothing is written back here.
		// A missing or unreadable file yields an empty map, which callers treat as "keep current
		// values" rather than as an error.
		IniData ReadIniFile()
		{
			IniData data;

			std::ifstream in(iniPath, std::ios::binary);

			if (!in)
			{
				logger::warn("Could not open {} for reading; keeping the values already loaded", iniPath);

				return data;
			}

			std::string line;
			std::string currentSection;

			while (std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				const std::string_view trimmed = Trim(line);

				if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#')
				{
					continue;
				}

				if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
				{
					currentSection = std::string{ trimmed.substr(1, trimmed.size() - 2) };

					continue;
				}

				const std::size_t separator = trimmed.find('=');

				if (separator == std::string_view::npos)
				{
					continue;
				}

				std::string key{ Trim(trimmed.substr(0, separator)) };
				std::string value{ Trim(trimmed.substr(separator + 1)) };

				// An inline "; comment" after a value is common in hand-edited INIs and silently
				// breaks a numeric parse, so it is stripped here rather than reaching the caller.
				const std::size_t comment = value.find_first_of(";#");

				if (comment != std::string::npos)
				{
					value = std::string{ Trim(std::string_view{ value }.substr(0, comment)) };
				}

				if (!key.empty())
				{
					data[currentSection][key] = std::move(value);
				}
			}

			return data;
		}

		// Reads one "key:Section"-style name out of the parsed file, falling back to the current
		// in-memory value when the key is absent or unparseable. The name format matches what
		// Init() registers, so the two cannot drift apart.
		template <typename T>
		T ReadFromFile(const IniData& a_data, const char* a_name, T a_fallback)
		{
			const std::string_view name{ a_name };
			const std::size_t      colon = name.find(':');

			if (colon == std::string_view::npos)
			{
				logger::error("Setting name \"{}\" has no section suffix; keeping the current value", a_name);

				return a_fallback;
			}

			const std::string key{ name.substr(0, colon) };
			const std::string section{ name.substr(colon + 1) };

			const auto sectionIt = a_data.find(section);

			if (sectionIt == a_data.end())
			{
				return a_fallback;
			}

			const auto keyIt = sectionIt->second.find(key);

			if (keyIt == sectionIt->second.end())
			{
				return a_fallback;
			}

			const std::string& raw = keyIt->second;

			try
			{
				if constexpr (std::is_same_v<T, bool>)
				{
					if (EqualsIgnoreCase(raw, "true")) { return true; }
					if (EqualsIgnoreCase(raw, "false")) { return false; }

					return std::stoi(raw) != 0;
				}
				else if constexpr (std::is_same_v<T, float>)
				{
					return std::stof(raw);
				}
				else
				{
					return static_cast<T>(std::stoul(raw));
				}
			}
			catch (const std::exception&)
			{
				logger::error("Setting \"{}\" has an unparseable value \"{}\"; keeping the current value",
					a_name, raw);

				return a_fallback;
			}
		}

		// Renamed in spirit, kept by name so every caller stays put: this now reads the INI
		// from disk rather than out of the game's setting collection. See ReadIniFile above for
		// why the collection cannot be trusted as a read source on a modlist with
		// PrivateProfileRedirector installed.
		void ReadFromCollection()
		{
			const IniData c = ReadIniFile();

			{
				using namespace debug;
				const auto raw = ReadFromFile<std::uint32_t>(c, "uLogLevel:Debug", static_cast<std::uint32_t>(logLevel));

				logLevel = raw <= static_cast<std::uint32_t>(logger::level::off)
							   ? static_cast<logger::level>(raw)
							   : logger::level::info;
			}

			{
				using namespace difficulty;

				enabled = ReadFromFile<bool>(c, "bEnabled:Difficulty", enabled);

				toPCVE = ClampMult(ReadFromFile<float>(c, "fToPCVE:Difficulty", toPCVE));
				toPCE = ClampMult(ReadFromFile<float>(c, "fToPCE:Difficulty", toPCE));
				toPCN = ClampMult(ReadFromFile<float>(c, "fToPCN:Difficulty", toPCN));
				toPCH = ClampMult(ReadFromFile<float>(c, "fToPCH:Difficulty", toPCH));
				toPCVH = ClampMult(ReadFromFile<float>(c, "fToPCVH:Difficulty", toPCVH));
				toPCL = ClampMult(ReadFromFile<float>(c, "fToPCL:Difficulty", toPCL));

				byPCVE = ClampMult(ReadFromFile<float>(c, "fByPCVE:Difficulty", byPCVE));
				byPCE = ClampMult(ReadFromFile<float>(c, "fByPCE:Difficulty", byPCE));
				byPCN = ClampMult(ReadFromFile<float>(c, "fByPCN:Difficulty", byPCN));
				byPCH = ClampMult(ReadFromFile<float>(c, "fByPCH:Difficulty", byPCH));
				byPCVH = ClampMult(ReadFromFile<float>(c, "fByPCVH:Difficulty", byPCVH));
				byPCL = ClampMult(ReadFromFile<float>(c, "fByPCL:Difficulty", byPCL));
			}

			{
				using namespace regeneration;

				enabled = ReadFromFile<bool>(c, "bEnabled:Regeneration", enabled);

				// A value still equal to kUnset after this (nothing was ever saved for it) is
				// left exactly as kUnset - Regeneration::Init() is what seeds it from the real
				// live vanilla value, since no compile-time default exists for these settings.
				for (RegenPerDifficultyField& field : kRegenPerDifficultyFields)
				{
					for (std::size_t i = 0; i < kDifficultyCount; ++i)
					{
						const std::string key = std::string(field.baseName) + kDifficultySuffix[i] + ":Regeneration";
						float& slot = (*field.values)[i];
						const float raw = ReadFromFile<float>(c, key.c_str(), slot);
						slot = (raw == kUnset) ? kUnset : ClampRegen(raw);
					}
				}

				for (RegenGlobalField& field : kRegenGlobalFields)
				{
					const std::string key = std::string(field.name) + ":Regeneration";
					const float raw = ReadFromFile<float>(c, key.c_str(), *field.value);
					*field.value = (raw == kUnset) ? kUnset : ClampRegen(raw);
				}
			}
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		CaptureDefaults();

		iniFileName = a_iniFileName;
		iniPath = std::filesystem::current_path().append("Data\\SKSE\\Plugins").append(a_iniFileName).string();

		INISettingCollection* iniSettingCollection = INISettingCollection::GetSingleton();

		const auto add = [iniSettingCollection](const char* a_name, auto a_value) {
			AddChecked(iniSettingCollection, MakeSetting(a_name, a_value), a_name);
		};

		{
			using namespace debug;
			add("uLogLevel:Debug", static_cast<std::uint32_t>(logLevel));
		}

		{
			using namespace difficulty;
			add("bEnabled:Difficulty", enabled);
			add("fToPCVE:Difficulty", toPCVE);
			add("fToPCE:Difficulty", toPCE);
			add("fToPCN:Difficulty", toPCN);
			add("fToPCH:Difficulty", toPCH);
			add("fToPCVH:Difficulty", toPCVH);
			add("fToPCL:Difficulty", toPCL);
			add("fByPCVE:Difficulty", byPCVE);
			add("fByPCE:Difficulty", byPCE);
			add("fByPCN:Difficulty", byPCN);
			add("fByPCH:Difficulty", byPCH);
			add("fByPCVH:Difficulty", byPCVH);
			add("fByPCL:Difficulty", byPCL);
		}

		{
			using namespace regeneration;
			add("bEnabled:Regeneration", enabled);

			for (RegenPerDifficultyField& field : kRegenPerDifficultyFields)
			{
				for (std::size_t i = 0; i < kDifficultyCount; ++i)
				{
					add((std::string(field.baseName) + kDifficultySuffix[i] + ":Regeneration").c_str(), (*field.values)[i]);
				}
			}

			for (RegenGlobalField& field : kRegenGlobalFields)
			{
				add((std::string(field.name) + ":Regeneration").c_str(), *field.value);
			}
		}

		// The settings stay REGISTERED with the collection above, but their values come from
		// ReadFromCollection's direct file read below rather than the collection's own
		// Win32-profile-API read. One read path for startup and reload alike, so the two can
		// never disagree with each other.
		ReadFromCollection();
	}

	bool Reload()
	{
		if (iniFileName.empty())
		{
			logger::error("Cannot reload settings before Init() has run");

			return false;
		}

		// Deliberately does NOT call INISettingCollection::ReadFromFile any more. That routes
		// through the game's Win32 profile API, which PrivateProfileRedirector intercepts and
		// answers from a cache populated at startup - so a reload returned the file as it was
		// when the game launched, not as we had just written it. ReadFromCollection reads the
		// file directly instead. See ReadIniFile's comment for the full account.
		ReadFromCollection();

		logger::info("Reloaded settings from {}", iniPath);

		return true;
	}

	bool Save()
	{
		if (iniPath.empty())
		{
			logger::error("Cannot save settings before Init() has run");

			return false;
		}

		bool ok = true;

		pendingWrites.clear();

		using namespace difficulty;

		ok &= WriteUInt(kDebugSection, "uLogLevel", static_cast<std::uint32_t>(debug::logLevel));
		ok &= WriteBool(kDifficultySection, "bEnabled", enabled);
		ok &= WriteFloat(kDifficultySection, "fToPCVE", toPCVE);
		ok &= WriteFloat(kDifficultySection, "fToPCE", toPCE);
		ok &= WriteFloat(kDifficultySection, "fToPCN", toPCN);
		ok &= WriteFloat(kDifficultySection, "fToPCH", toPCH);
		ok &= WriteFloat(kDifficultySection, "fToPCVH", toPCVH);
		ok &= WriteFloat(kDifficultySection, "fToPCL", toPCL);
		ok &= WriteFloat(kDifficultySection, "fByPCVE", byPCVE);
		ok &= WriteFloat(kDifficultySection, "fByPCE", byPCE);
		ok &= WriteFloat(kDifficultySection, "fByPCN", byPCN);
		ok &= WriteFloat(kDifficultySection, "fByPCH", byPCH);
		ok &= WriteFloat(kDifficultySection, "fByPCVH", byPCVH);
		ok &= WriteFloat(kDifficultySection, "fByPCL", byPCL);

		// Qualified explicitly rather than another "using namespace regeneration" - that would
		// make "enabled" ambiguous against difficulty::enabled, still in scope from above.
		ok &= WriteBool(kRegenSection, "bEnabled", regeneration::enabled);

		for (RegenPerDifficultyField& field : kRegenPerDifficultyFields)
		{
			for (std::size_t i = 0; i < regeneration::kDifficultyCount; ++i)
			{
				const std::string key = std::string(field.baseName) + kDifficultySuffix[i];
				ok &= WriteFloat(kRegenSection, key.c_str(), (*field.values)[i]);
			}
		}

		for (RegenGlobalField& field : kRegenGlobalFields)
		{
			ok &= WriteFloat(kRegenSection, field.name, *field.value);
		}

		ok &= FlushPendingWrites();

		pendingWrites.clear();

		if (ok)
		{
			logger::info("Saved settings to {}", iniPath);
		}
		else
		{
			logger::error("Failed to save settings to {}", iniPath);
		}

		return ok;
	}

	void RestoreDefaults()
	{
		using namespace difficulty;

		debug::logLevel = defaults.logLevel;

		enabled = defaults.enabled;
		toPCVE = defaults.toPCVE; toPCE = defaults.toPCE; toPCN = defaults.toPCN;
		toPCH = defaults.toPCH; toPCVH = defaults.toPCVH; toPCL = defaults.toPCL;
		byPCVE = defaults.byPCVE; byPCE = defaults.byPCE; byPCN = defaults.byPCN;
		byPCH = defaults.byPCH; byPCVH = defaults.byPCVH; byPCL = defaults.byPCL;

		// The regeneration FLOAT settings are deliberately NOT reset here - there is no
		// compile-time default for them (see the Defaults struct's own comment above), so their
		// restore path is Regeneration::RestoreDefaults(), sourced from the real vanilla value
		// captured live at Regeneration::Init(). Call it alongside this function, matching
		// UI.cpp's "Restore defaults" button.
		regeneration::enabled = defaults.regenEnabled;
	}

	const std::string& GetIniPath() { return iniPath; }
}
