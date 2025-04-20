// made by fokkonaut

#include "localization.h"

#include <engine/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <algorithm>
#include <engine/shared/config.h>
#include <fstream>
#include <game/server/gamecontext.h>

const char *Localize(const char *pStr, int Language, const char *pContext)
{
	const char *pNewStr = g_Localization.FindString(str_quickhash_raw(pStr), str_quickhash(pContext), Language);
	return pNewStr ? pNewStr : pStr;
}

void CLocalizationDatabase::LoadIndexfile(IStorage *pStorage, CConfig *pConfig)
{
	m_pStorage = pStorage;
	m_pConfig = pConfig;
	m_vLanguages.clear();

	const std::vector<std::string> vEnglishLanguageCodes = {"en"};
	m_vLanguages.emplace_back("English", "english", 826, vEnglishLanguageCodes);
	m_vLanguages[0].m_Loaded = true;
	m_vLanguages[0].m_Available = true;

	char aFile[256];
	str_format(aFile, sizeof(aFile), "%s/index.txt", m_pConfig->m_SvLanguagesPath);
	// Use fstream instead of CLineReader and OpenFile, because of paths like this: "../data/languages"
	std::fstream File(aFile);
	if (!File.is_open())
	{
		dbg_msg("localization", "Couldn't open file '%s'", aFile);
		return;
	}

	std::string data;
	while(getline(File, data))
	{
		const char *pLine = data.c_str();

		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		char aEnglishName[128];
		str_copy(aEnglishName, pLine);

		getline(File, data);
		pLine = data.c_str();
		if(!pLine)
		{
			dbg_msg("localization", "Unexpected end of index file after language '%s'", aEnglishName);
			break;
		}
		if(!str_startswith(pLine, "== "))
		{
			dbg_msg("localization", "Missing native name for language '%s'", aEnglishName);
			(void)getline(File, data);
			(void)getline(File, data);
			continue;
		}
		char aNativeName[128];
		str_copy(aNativeName, pLine + 3);

		getline(File, data);
		pLine = data.c_str();
		if(!pLine)
		{
			dbg_msg("localization", "Unexpected end of index file after language '%s'", aEnglishName);
			break;
		}
		if(!str_startswith(pLine, "== "))
		{
			dbg_msg("localization", "Missing country code for language '%s'", aEnglishName);
			(void)getline(File, data);
			continue;
		}
		char aCountryCode[128];
		str_copy(aCountryCode, pLine + 3);

		getline(File, data);
		pLine = data.c_str();
		if(!pLine)
		{
			dbg_msg("localization", "Unexpected end of index file after language '%s'", aEnglishName);
			break;
		}
		if(!str_startswith(pLine, "== "))
		{
			dbg_msg("localization", "Missing language codes for language '%s'", aEnglishName);
			continue;
		}
		const char *pLanguageCodes = pLine + 3;
		char aLanguageCode[256];
		std::vector<std::string> vLanguageCodes;
		while((pLanguageCodes = str_next_token(pLanguageCodes, ";", aLanguageCode, sizeof(aLanguageCode))))
		{
			if(aLanguageCode[0])
			{
				vLanguageCodes.emplace_back(aLanguageCode);
			}
		}
		if(vLanguageCodes.empty())
		{
			dbg_msg("localization", "At least one language code required for language '%s'", aEnglishName);
			continue;
		}

		m_vLanguages.emplace_back(aNativeName, aEnglishName, str_toint(aCountryCode), vLanguageCodes);
	}
	// Load all files once and check if any strings appear, to set m_Available
	for (unsigned int i = 1; i < m_vLanguages.size(); i++)
	{
		Load(i);
		Unload(i);
	}
	std::sort(m_vLanguages.begin(), m_vLanguages.end());

	dbg_msg("localization", "Following available languages were found: %s", ListAvailable());
}

void CLocalizationDatabase::SelectDefaultLanguage(char *pFilename, size_t Length)
{
	if(Languages().empty())
		return;
	if(Languages().size() == 1)
	{
		str_copy(pFilename, m_vLanguages[0].m_FileName.c_str(), Length);
		return;
	}

	char aLocaleStr[128];
	os_locale_str(aLocaleStr, sizeof(aLocaleStr));

	dbg_msg("localization", "Choosing default language based on user locale '%s'", aLocaleStr);

	while(true)
	{
		CLanguage *pPrefixMatch = nullptr;
		for(auto &Language : Languages())
		{
			for(const auto &LanguageCode : Language.m_vLanguageCodes)
			{
				if(LanguageCode == aLocaleStr)
				{
					// Exact match found, use it immediately
					str_copy(pFilename, Language.m_FileName.c_str(), Length);
					return;
				}
				else if(LanguageCode.rfind(aLocaleStr, 0) == 0)
				{
					// Locale is prefix of language code, e.g. locale is "en" and current language is "en-US"
					pPrefixMatch = &Language;
				}
			}
		}
		// Use prefix match if no exact match was found
		if(pPrefixMatch)
		{
			str_copy(pFilename, pPrefixMatch->m_FileName.c_str(), Length);
			return;
		}

		// Remove last segment of locale string and try again with more generic locale, e.g. "en-US" -> "en"
		int i = str_length(aLocaleStr) - 1;
		for(; i >= 0; --i)
		{
			if(aLocaleStr[i] == '-')
			{
				aLocaleStr[i] = '\0';
				break;
			}
		}

		// Stop if no more locale segments are left
		if(i <= 0)
			break;
	}
}

bool CLocalizationDatabase::Load(int Language)
{
	if (m_vLanguages[Language].m_Loaded)
		return false;

	char aFile[256];
	str_format(aFile, sizeof(aFile), "%s/%s.txt", m_pConfig->m_SvLanguagesPath, m_vLanguages[Language].m_FileName.c_str());
	std::fstream File(aFile);
	if (!File.is_open())
	{
		dbg_msg("localization", "Couldn't open file '%s'", aFile);
		return false;
	}

	Unload(Language);
	m_vLanguages[Language].m_pStringsHeap = new CHeap();

	char aContext[512];
	char aOrigin[512];
	int Line = 0;
	std::string data;
	while(getline(File, data))
	{
		const char *pLine = data.c_str();
		Line++;
		if(!str_length(pLine))
			continue;

		if(pLine[0] == '#') // skip comments
			continue;

		if(pLine[0] == '{') // context
		{
			size_t Len = str_length(pLine);
			if(Len < 1 || pLine[Len - 1] != '}')
			{
				dbg_msg("localization", "malformed context '%s' on line %d", pLine, Line);
				continue;
			}
			str_truncate(aContext, sizeof(aContext), pLine + 1, Len - 2);
			getline(File, data);
			pLine = data.c_str();
			if(!pLine)
			{
				dbg_msg("localization", "unexpected end of file after context line '%s' on line %d", aContext, Line);
				break;
			}
			Line++;
		}
		else
		{
			aContext[0] = '\0';
		}

		str_copy(aOrigin, pLine);
		getline(File, data);
		const char *pReplacement = data.c_str();
		if(!pReplacement)
		{
			dbg_msg("localization", "unexpected end of file after original '%s' on line %d", aOrigin, Line);
			break;
		}
		Line++;

		if(pReplacement[0] != '=' || pReplacement[1] != '=' || pReplacement[2] != ' ')
		{
			dbg_msg("localization", "malformed replacement '%s' for original '%s' on line %d", pReplacement, aOrigin, Line);
			continue;
		}

		pReplacement += 3;
		if (pReplacement && pReplacement[0])
		{
			AddString(aOrigin, pReplacement, aContext, Language);
			m_vLanguages[Language].m_Available = true;
		}
	}
	std::sort(m_vLanguages[Language].m_vStrings.begin(), m_vLanguages[Language].m_vStrings.end());
	m_vLanguages[Language].m_Loaded = true;
	return true;
}

bool CLocalizationDatabase::Load(const char *pFilename, bool Force)
{
	int Language = GetLanguage(pFilename);
	if (Language == -1)
		return false;
	if (Force)
	{
		if (str_comp(m_vLanguages[Language].m_Name.c_str(), "English") == 0)
			return true;
		m_vLanguages[Language].m_Loaded = false;
	}
	return Load(Language);
}

void CLocalizationDatabase::AddString(const char *pOrgStr, const char *pNewStr, const char *pContext, int Language)
{
	m_vLanguages[Language].m_vStrings.emplace_back(str_quickhash(pOrgStr), str_quickhash(pContext),
		m_vLanguages[Language].m_pStringsHeap->StoreString(*pNewStr ? pNewStr : pOrgStr));
}

const char *CLocalizationDatabase::FindString(unsigned Hash, unsigned ContextHash, int Language)
{
	if (Language == -1)
		return nullptr;

	Load(Language);

	CLanguage::CString String;
	String.m_Hash = Hash;
	String.m_ContextHash = ContextHash;
	String.m_pReplacement = nullptr;
	auto Range1 = std::equal_range(m_vLanguages[Language].m_vStrings.begin(), m_vLanguages[Language].m_vStrings.end(), String);
	if(std::distance(Range1.first, Range1.second) == 1)
		return Range1.first->m_pReplacement;

	const unsigned DefaultHash = str_quickhash("");
	if(ContextHash != DefaultHash)
	{
		// Do another lookup with the default context hash
		String.m_ContextHash = DefaultHash;
		auto Range2 = std::equal_range(m_vLanguages[Language].m_vStrings.begin(), m_vLanguages[Language].m_vStrings.end(), String);
		if(std::distance(Range2.first, Range2.second) == 1)
			return Range2.first->m_pReplacement;
	}

	return nullptr;
}

bool CLocalizationDatabase::TryUnload(CGameContext *pGameServer, int Language)
{
	if (!pGameServer || Language == -1 || !m_vLanguages[Language].m_Loaded)
		return false;

	if (str_comp(m_vLanguages[Language].m_Name.c_str(), "English") == 0)
		return false;

	bool CanUnloadLanguage = true;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (pGameServer->m_apPlayers[i] && pGameServer->m_apPlayers[i]->m_Language == Language)
		{
			CanUnloadLanguage = false;
			break;
		}
	}

	// Unload if nobody uses this language
	if (CanUnloadLanguage)
	{
		Unload(Language);
		return true;
	}
	return false;
}

void CLocalizationDatabase::Unload(int Language)
{
	m_vLanguages[Language].m_Loaded = false;
	m_vLanguages[Language].m_vStrings.clear();
	if (m_vLanguages[Language].m_pStringsHeap)
	{
		delete m_vLanguages[Language].m_pStringsHeap;
		m_vLanguages[Language].m_pStringsHeap = 0;
	}
}

const char *CLocalizationDatabase::ListAvailable()
{
	static char aBuf[256];
	char aTemp[128] = { 0 };
	bool Append = false;
	for (int i = 0; i < (int)m_vLanguages.size(); i++)
	{
		if (!m_vLanguages[i].m_Available)
			continue;
		const char *pLanguage = GetLanguageFileName(i);
		if (Append)
		{
			str_format(aTemp, sizeof(aTemp), "%s, ", aBuf);
			str_format(aBuf, sizeof(aBuf), "%s%s", aTemp, pLanguage);
		}
		else
		{
			str_copy(aBuf, pLanguage, sizeof(aBuf));
			Append = true;
		}
	}
	return aBuf;
}

const char *CLocalizationDatabase::GetLanguageString(int Language)
{
	if (Language == -1)
		return "English";
	return m_vLanguages[Language].m_Name.c_str();
}

const char *CLocalizationDatabase::GetLanguageFileName(int Language)
{
	if (Language == -1)
		return "english";
	return m_vLanguages[Language].m_FileName.c_str();
}

int CLocalizationDatabase::GetLanguage(const char *pFileName)
{
	for (unsigned int i = 0; i < m_vLanguages.size(); i++)
		if (str_comp_nocase(m_vLanguages[i].m_FileName.c_str(), pFileName) == 0)
			return i;
	return -1;
}

int CLocalizationDatabase::GetLanguageByCode(const char *pLanguageCode)
{
	for (unsigned int i = 0; i < m_vLanguages.size(); i++)
		for (unsigned int c = 0; c < m_vLanguages[i].m_vLanguageCodes.size(); c++)
			if (str_comp_nocase(m_vLanguages[i].m_vLanguageCodes[c].c_str(), pLanguageCode) == 0)
				return i;
	return -1;
}

CLocalizationDatabase g_Localization;
