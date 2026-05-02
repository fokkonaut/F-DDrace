// made by fokkonaut

#include "savedtees.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include <engine/server/server.h>

CGameContext *CSavedTees::GameServer() const { return m_pGameServer; }
IServer *CSavedTees::Server() const { return GameServer()->Server(); }
CConfig *CSavedTees::Config() const { return GameServer()->Config(); }

void CSavedTees::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	ReadSavedPlayersFile();
	ExpireSavedIdentities();
}

void CSavedTees::Tick()
{
	if (GameServer()->IsFullHour())
	{
		ExpireSavedIdentities();
	}
}

void CSavedTees::ReadSavedPlayersFile()
{
	m_vSavedIdentities.clear();
	m_vSavedIdentitiesFiles.clear();

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "dumps/%s/%s", Config()->m_SvSavedTeesFilePath, Server()->GetCurrentMapName());
	GameServer()->Storage()->ListDirectory(IStorage::TYPE_ALL, aPath, LoadSavedPlayersCallback, this);

	for (unsigned int i = 0; i < m_vSavedIdentitiesFiles.size(); i++)
	{
		str_format(aPath, sizeof(aPath), "dumps/%s/%s/%s.save", Config()->m_SvSavedTeesFilePath, Server()->GetCurrentMapName(), m_vSavedIdentitiesFiles[i].c_str());
		CSaveTee SaveTee;
		if (SaveTee.LoadFile(aPath, 0, GameServer()) && SaveTee.HasSavedIdentity())
		{
			if (GameServer()->IsExpired(SaveTee.GetIdentity().m_ExpireDate))
			{
				RemoveSavedIdentityFile(SaveTee.GetIdentity());
				continue;
			}

			m_vSavedIdentities.push_back(SaveTee.GetIdentity());
		}
	}

	// Clear
	m_vSavedIdentitiesFiles.clear();

	// Check for redirect tile saves
	str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile", Config()->m_SvSavedTeesFilePath);
	GameServer()->Storage()->ListDirectory(IStorage::TYPE_ALL, aPath, LoadSavedPlayersCallback, this);

	for (unsigned int i = 0; i < m_vSavedIdentitiesFiles.size(); i++)
	{
		str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile/%s.save", Config()->m_SvSavedTeesFilePath, m_vSavedIdentitiesFiles[i].c_str());
		CSaveTee SaveTee;
		if (SaveTee.LoadFile(aPath, 0, GameServer()) && SaveTee.HasSavedIdentity())
		{
			if (GameServer()->IsExpired(SaveTee.GetIdentity().m_ExpireDate))
			{
				RemoveSavedIdentityFile(SaveTee.GetIdentity());
				continue;
			}

			if (SaveTee.GetIdentity().m_RedirectTilePort == Config()->m_SvPort)
			{
				m_vSavedIdentities.push_back(SaveTee.GetIdentity());
			}
		}
	}

	// Clear
	m_vSavedIdentitiesFiles.clear();
}

int CSavedTees::LoadSavedPlayersCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CSavedTees *pSelf = (CSavedTees *)pUser;
	if (!IsDir && str_endswith(pName, ".save"))
	{
		std::string Name = pName;
		pSelf->m_vSavedIdentitiesFiles.push_back(Name.erase(Name.size()-5)); // remove .save
	}
	return 0;
}

void CSavedTees::ExpireSavedIdentities()
{
	for (int i = (int)m_vSavedIdentities.size() - 1; i >= 0; i--)
	{
		if (GameServer()->IsExpired(m_vSavedIdentities[i].m_ExpireDate))
		{
			RemoveSavedIdentityFile(m_vSavedIdentities[i]);
			m_vSavedIdentities.erase(m_vSavedIdentities.begin() + i);
		}
	}
}

void CSavedTees::RemoveSavedIdentityFile(SSavedIdentity SavedIdentity)
{
	char aPath[IO_MAX_PATH_LENGTH];
	if (SavedIdentity.m_RedirectTilePort)
		str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile/%s.save", Config()->m_SvSavedTeesFilePath, GetSavedIdentityHash(SavedIdentity));
	else
		str_format(aPath, sizeof(aPath), "dumps/%s/%s/%s.save", Config()->m_SvSavedTeesFilePath, Server()->GetCurrentMapName(), GetSavedIdentityHash(SavedIdentity));
	dbg_msg("save", "%s: removing saved identity due to expiration", SavedIdentity.m_aName);
	GameServer()->Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
}

void CSavedTees::SaveDrop(int ClientID, float Hours, const char *pReason)
{
	if (!GameServer()->GetPlayerChar(ClientID) || GameServer()->m_apPlayers[ClientID]->m_IsDummy)
		return;

	// Save character
	SaveCharacter(ClientID, SAVE_WALLET, Hours);

	// Drop the client
	((CServer *)Server())->m_NetServer.Drop(ClientID, pReason);
}

SSavedIdentity *CSavedTees::SaveCharacter(int ClientID, int Flags, float Hours)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if (!pChr || pChr->GetPlayer()->m_IsDummy)
		return nullptr;

	// Pretend we leave the minigame, so that the shutdown save saves our main tee, not the minigame :D
	// We cant use SetMinigame(MINIGAME_NONE) here because that would kill the character, ending in a crash
	if (pChr->GetPlayer()->IsMinigame())
		pChr->GetPlayer()->m_MinigameTee.Load(pChr, 0);

	// if character got saved and during restart the plot expires, it would be not good if the tee keeps his editor
	pChr->GetPlayer()->StopPlotEditing();
	pChr->UnsetSpookyGhost();

	// Pretend we leave no bonus area so we can save the real values, and later override it by calling this function again
	if (pChr->m_NoBonusContext.m_InArea)
	{
		pChr->OnNoBonusArea(false);
		// Pretend we left the no bonus area on redirect. If wanted, the other map should add a tile to enable it again, same like after jail release, its not set.
		if (!(Flags & SAVE_REDIRECT))
		{
			pChr->m_NoBonusContext.m_InArea = true;
		}
	}

	if (pChr->IsInSafeArea())
	{
		pChr->SetSafeArea(false);
		if (!(Flags & SAVE_REDIRECT))
		{
			pChr->SetInGame(false);
		}
	}

	if (Flags & SAVE_REDIRECT)
	{
		// reset solo so it cant be taken to another map
		pChr->SetSolo(false);
	}

	// save identity to cache
	SSavedIdentity Info;
	Server()->GetClientAddr(ClientID, &Info.m_Addr);
	str_copy(Info.m_aName, Server()->ClientName(ClientID), sizeof(Info.m_aName));
	str_copy(Info.m_aAccUsername, GameServer()->m_Accounts.Get(GameServer()->m_apPlayers[ClientID]->GetAccID()).m_Username, sizeof(Info.m_aAccUsername));
	Info.m_TeeInfo = GameServer()->m_apPlayers[ClientID]->m_TeeInfos;
	str_copy(Info.m_aTimeoutCode, GameServer()->m_apPlayers[ClientID]->m_TimeoutCode, sizeof(Info.m_aTimeoutCode));
	Info.m_ExpireDate = 0;
	if (Hours != -1)
		GameServer()->SetExpireDate(&Info.m_ExpireDate, Hours);
	Info.m_RedirectTilePort = pChr->m_RedirectTilePort;
	m_vSavedIdentities.push_back(Info);

	// create file and save the character
	char aFilename[IO_MAX_PATH_LENGTH];
	if (!(Flags & SAVE_REDIRECT))
	{
		str_format(aFilename, sizeof(aFilename), "dumps/%s/%s/%s.save", Config()->m_SvSavedTeesFilePath, Server()->GetCurrentMapName(), GetSavedIdentityHash(Info));
	}
	else
	{
		str_format(aFilename, sizeof(aFilename), "dumps/%s/x_redirect_tile/%s.save", Config()->m_SvSavedTeesFilePath, GetSavedIdentityHash(Info));
	}
	CSaveTee SaveTee(Flags|SAVE_IDENTITY);
	SaveTee.SaveFile(aFilename, pChr);

	// Remove wallet money so we dont automatically drop it on disconnect because it is saved already
	if (Flags & SAVE_WALLET)
	{
		GameServer()->m_apPlayers[ClientID]->SetWalletMoney(0);
	}
	if (!(Flags & SAVE_JAIL))
	{
		// Reset, so CPlayer::OnDisconnect() will not create a jail savetee when we have this already.
		GameServer()->m_apPlayers[ClientID]->m_EscapeTime = 0;
		GameServer()->m_apPlayers[ClientID]->m_JailTime = 0;
	}
	if (Flags & SAVE_SHUTDOWN)
	{
		// Reset, so CPlayer::OnDisconnect() will not create a disconnect savetee when we have this already.
		GameServer()->m_apPlayers[ClientID]->m_SavePlayerDisconnect = false;
	}

	// return index of newly added identity
	return &m_vSavedIdentities[m_vSavedIdentities.size() - 1];
}

SSavedIdentity *CSavedTees::FindSavedPlayer(int ClientID)
{
	if (!GameServer()->m_apPlayers[ClientID] || GameServer()->m_apPlayers[ClientID]->m_IsDummy)
		return nullptr;

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);

	int Found = -1;
	for (unsigned int i = 0; i < m_vSavedIdentities.size(); i++)
	{
		SSavedIdentity Info = m_vSavedIdentities[i];
		bool SameAddrAndPort = net_addr_comp(&Addr, &Info.m_Addr, true) == 0;
		if (Found != -1)
		{
			if (SameAddrAndPort)
				Found = i; // for getting the correct savedidentity when shutting down or saving a tee for save drop
			continue;
		}

		bool SameAddr = net_addr_comp(&Addr, &Info.m_Addr, false) == 0;
		bool SameTimeoutCode = Info.m_aTimeoutCode[0] != '\0' && str_comp(Info.m_aTimeoutCode, GameServer()->m_apPlayers[ClientID]->m_TimeoutCode) == 0;
		bool SameAcc = Info.m_aAccUsername[0] != '\0' && str_comp(Info.m_aAccUsername, GameServer()->m_Accounts.Get(GameServer()->m_apPlayers[ClientID]->GetAccID()).m_Username) == 0;
		bool SameName = str_comp(Info.m_aName, Server()->ClientName(ClientID)) == 0;
		//bool SameTeeInfo = mem_comp(&Info.m_TeeInfo, &m_apPlayers[ClientID]->m_TeeInfos, sizeof(CTeeInfo)) == 0;

		// SameTeeInfo is not really used, since players with the same skin and ip would get fucked up otherwise, in CSaveTee::Save() the identity of e.g. dummy would get saved then
		bool SameClientInfo = SameAddr && SameName;
		// always match account for extensive bypassing
		//SameAcc = SameAcc && (SameAddr || SameName || SameTeeInfo || SameTimeoutCode);
		if (SameAddrAndPort || SameAcc || SameTimeoutCode || SameClientInfo)
		{
			Found = i;
		}
	}

	return Found == -1 ? nullptr : &m_vSavedIdentities[Found];
}

const char *CSavedTees::GetSavedIdentityHash(SSavedIdentity Info)
{
	SHA256_CTX Sha256Ctx;
	sha256_init(&Sha256Ctx);

	// manually update sha256 to no get bytes after 0 bytes in or so
	sha256_update(&Sha256Ctx, &Info.m_aAccUsername, str_length(Info.m_aAccUsername));
	sha256_update(&Sha256Ctx, &Info.m_Addr, sizeof(Info.m_Addr));
	sha256_update(&Sha256Ctx, &Info.m_aTimeoutCode, str_length(Info.m_aTimeoutCode));
	sha256_update(&Sha256Ctx, &Info.m_aName, str_length(Info.m_aName));
	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		sha256_update(&Sha256Ctx, Info.m_TeeInfo.GetSkinPartName(p), str_length(Info.m_TeeInfo.m_aaSkinPartNames[p]));
		sha256_update(&Sha256Ctx, &Info.m_TeeInfo.m_aUseCustomColors[p], sizeof(Info.m_TeeInfo.m_aUseCustomColors[p]));
		sha256_update(&Sha256Ctx, &Info.m_TeeInfo.m_aSkinPartColors[p], sizeof(Info.m_TeeInfo.m_aSkinPartColors[p]));
	}
	sha256_update(&Sha256Ctx, &Info.m_TeeInfo.m_Sevendown.m_SkinName, str_length(Info.m_TeeInfo.m_Sevendown.m_SkinName));
	sha256_update(&Sha256Ctx, &Info.m_TeeInfo.m_Sevendown.m_UseCustomColor, sizeof(Info.m_TeeInfo.m_Sevendown.m_UseCustomColor));
	sha256_update(&Sha256Ctx, &Info.m_TeeInfo.m_Sevendown.m_ColorBody, sizeof(Info.m_TeeInfo.m_Sevendown.m_ColorBody));
	sha256_update(&Sha256Ctx, &Info.m_TeeInfo.m_Sevendown.m_ColorFeet, sizeof(Info.m_TeeInfo.m_Sevendown.m_ColorFeet));

	static char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(sha256_finish(&Sha256Ctx), aSha256, sizeof(aSha256));
	return aSha256;
}

bool CSavedTees::CheckLoadPlayer(int ClientID, bool Force)
{
	// dont load two saves for one tee, because they are probably two clients with identical information, sending a info change can cause a second load, of the tee thats the 2nd client
	if (!GameServer()->m_apPlayers[ClientID] || (!Force && GameServer()->m_apPlayers[ClientID]->m_LoadedSavedPlayer))
		return false;

	SSavedIdentity *pSavedIdentity = FindSavedPlayer(ClientID);
	if (!pSavedIdentity)
		return false;

	// Get path and load
	bool Success = TryLoadPlayer(ClientID, pSavedIdentity, false);
	if (!Success)
	{
		// Normal path didn't work, let's see if we can find the file in the redirect tile folder
		Success = TryLoadPlayer(ClientID, pSavedIdentity, true);
	}
	return Success;
}

bool CSavedTees::TryLoadPlayer(int ClientID, SSavedIdentity *pSavedIdentity, bool RedirectTile)
{
	const char *pHash = GetSavedIdentityHash(*pSavedIdentity);
	char aPath[IO_MAX_PATH_LENGTH];
	if (RedirectTile)
		str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile/%s.save", Config()->m_SvSavedTeesFilePath, pHash);
	else
		str_format(aPath, sizeof(aPath), "dumps/%s/%s/%s.save", Config()->m_SvSavedTeesFilePath, Server()->GetCurrentMapName(), pHash);
	CSaveTee SaveTee;
	if (SaveTee.LoadFile(aPath, GameServer()->m_apPlayers[ClientID]->GetCharacter()))
	{
		Server()->SendRedirectSaveTeeRemove(SaveTee.GetPreviousPort(), pHash);
		// Remove file, this save has been used now
		dbg_msg("save", "%d:%s used his save, removing save file", ClientID, Server()->ClientName(ClientID));
		GameServer()->Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
		auto Iterator = m_vSavedIdentities.begin() + (pSavedIdentity - m_vSavedIdentities.data());
		m_vSavedIdentities.erase(Iterator);
		GameServer()->m_apPlayers[ClientID]->m_LoadedSavedPlayer = true;
		return true;
	}
	return false;
}

void CSavedTees::OnRedirectSaveTeeAdd(const char *pHash)
{
	if (!pHash[0])
		return;

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile/%s.save", Config()->m_SvSavedTeesFilePath, pHash);
	CSaveTee SaveTee;
	if (SaveTee.LoadFile(aPath, 0, GameServer()) && SaveTee.HasSavedIdentity() && SaveTee.GetIdentity().m_RedirectTilePort == Config()->m_SvPort)
	{
		int Index = GetIdentityIndexByHash(pHash);
		if (Index == -1)
			m_vSavedIdentities.push_back(SaveTee.GetIdentity());
	}
}

void CSavedTees::OnRedirectSaveTeeRemove(const char *pHash)
{
	int Index = GetIdentityIndexByHash(pHash);
	if (Index == -1)
		return;
	m_vSavedIdentities.erase(m_vSavedIdentities.begin() + Index);
}

int CSavedTees::GetIdentityIndexByHash(const char *pHash)
{
	if (pHash[0])
		for (unsigned int i = 0; i < m_vSavedIdentities.size(); i++)
			if (str_comp(GetSavedIdentityHash(m_vSavedIdentities[i]), pHash) == 0)
				return i;
	return -1;
}

void CSavedTees::PrintSavedTeesList()
{
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "console", "Listing all saved identities:");
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "console", "----------------------------------");
	for (int i = 0; i < (int)m_vSavedIdentities.size(); i++)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "| %s | %s | '%s' | %s | %d |", GetSavedIdentityHash(m_vSavedIdentities[i]), GameServer()->GetDate(m_vSavedIdentities[i].m_ExpireDate),
			m_vSavedIdentities[i].m_aName, m_vSavedIdentities[i].m_aAccUsername[0] ? m_vSavedIdentities[i].m_aAccUsername : "<no_acc>", m_vSavedIdentities[i].m_RedirectTilePort);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "console", aBuf);
	}
}
