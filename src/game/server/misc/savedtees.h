// made by fokkonaut

#ifndef GAME_SERVER_MISC_SAVEDTEES_H
#define GAME_SERVER_MISC_SAVEDTEES_H

#include <base/hash_ctxt.h>
#include <generated/protocol.h>
#include <engine/shared/protocol.h>
#include <game/server/save.h>

class CGameContext;
class IServer;
class CConfig;

class CSavedTees
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const;
	IServer *Server() const;
	CConfig *Config() const;

	std::vector<SSavedIdentity> m_vSavedIdentities;
	std::vector<std::string> m_vSavedIdentitiesFiles; // only for init, to read all saved identities structs to m_vSavedIdentities
	static int LoadSavedPlayersCallback(const char *pName, int IsDir, int StorageType, void *pUser);

public:
	void Init(CGameContext *pGameServer);
	void Tick();

	SSavedIdentity *SaveCharacter(int ClientID, int Flags = 0, float Hours = -1);
	SSavedIdentity *FindSavedPlayer(int ClientID);
	bool CheckLoadPlayer(int ClientID, bool Force = false);
	bool TryLoadPlayer(int ClientID, SSavedIdentity *pSavedIdentity, bool RedirectTile);
	const char *GetSavedIdentityHash(SSavedIdentity Info);

	void ReadSavedPlayersFile();
	void ExpireSavedIdentities();
	void RemoveSavedIdentityFile(SSavedIdentity SavedIdentity);

	int GetIdentityIndexByHash(const char *pHash);
	void OnRedirectSaveTeeAdd(const char *pHash);
	void OnRedirectSaveTeeRemove(const char *pHash);

	void SaveDrop(int ClientID, float Hours, const char *pReason);
	void PrintSavedTeesList();
};

#endif // GAME_SERVER_MISC_SAVEDTEES_H
