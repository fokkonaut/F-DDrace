// made by fokkonaut

#ifndef GAME_LOCALIZATION_H
#define GAME_LOCALIZATION_H

#include <base/system.h> // GNUC_ATTRIBUTE

#include <engine/shared/memheap.h>

#include <string>
#include <vector>

/**
* An empty function that suits as a helper to identify strings that might get localized later
*/
static constexpr const char *Localizable(const char *pStr, const char *pContext = "")
{
	return pStr;
}

class CLanguage
{
public:
	class CString
	{
	public:
		unsigned m_Hash;
		unsigned m_ContextHash;
		const char *m_pReplacement;

		CString() {}
		CString(unsigned Hash, unsigned ContextHash, const char *pReplacement) :
			m_Hash(Hash), m_ContextHash(ContextHash), m_pReplacement(pReplacement)
		{
		}

		bool operator<(const CString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash < Other.m_ContextHash); }
		bool operator<=(const CString &Other) const { return m_Hash < Other.m_Hash || (m_Hash == Other.m_Hash && m_ContextHash <= Other.m_ContextHash); }
		bool operator==(const CString &Other) const { return m_Hash == Other.m_Hash && m_ContextHash == Other.m_ContextHash; }
	};

	CLanguage() = default;
	CLanguage(const char *pName, const char *pFileName, int Code, const std::vector<std::string> &vLanguageCodes) :
		m_Name(pName), m_FileName(pFileName), m_CountryCode(Code), m_vLanguageCodes(vLanguageCodes)
	{
		m_pStringsHeap = 0;
		m_Loaded = false;
		m_Available = false;
	}
	~CLanguage()
	{
		m_vStrings.clear();
		delete m_pStringsHeap;
		m_pStringsHeap = 0;
	}

	std::string m_Name;
	std::string m_FileName;
	int m_CountryCode;
	std::vector<std::string> m_vLanguageCodes;
	std::vector<CString> m_vStrings;
	CHeap *m_pStringsHeap;
	bool m_Loaded;
	bool m_Available;

	bool operator<(const CLanguage &Other) const { return m_Name < Other.m_Name; }
};

class CLocalizationDatabase
{
	std::vector<CLanguage> m_vLanguages;
	class IStorage *m_pStorage;
	class CConfig *m_pConfig;

	void Unload(int Language);

public:
	void LoadIndexfile(class IStorage *pStorage, class CConfig *pConfig);
	std::vector<CLanguage> &Languages() { return m_vLanguages; }
	void SelectDefaultLanguage(char *pFilename, size_t Length);

	void AddString(const char *pOrgStr, const char *pNewStr, const char *pContext, int Language);
	const char *FindString(unsigned Hash, unsigned ContextHash, int Language);

	bool Load(const char *pFilename, bool Force = false);
	bool Load(int Language);
	bool TryUnload(class CGameContext *pGameServer, int Language);
	int GetLanguage(const char *pFileName);
	int GetLanguageByCode(const char *pLanguageCode);
	const char *GetLanguageString(int Language);
	const char *GetLanguageFileName(int Language);
	const char *ListAvailable();
};

extern CLocalizationDatabase g_Localization;

extern const char *Localize(const char *pStr, int Language, const char *pContext = "")
	GNUC_ATTRIBUTE((format_arg(1)));
#endif
