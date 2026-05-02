// made by fokkonaut

#include "playermapping.h"

#include <base/system.h>

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/server/gamemodes/DDRace.h>

void CPlayerMapping::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aMap[i].Init(i, this);
}

void CPlayerMapping::Tick()
{
	UpdatePlayerMap(-1);

	// Translate StrongWeakID to clamp it to 64 players
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!GameServer()->m_apPlayers[i] || GameServer()->m_apPlayers[i]->m_IsDummy)
			continue;

		int StrongWeakID = 0;
		for (CCharacter *pChar = (CCharacter *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
		{
			int ID = pChar->GetPlayer()->GetCID();
			if (Server()->Translate(ID, i))
			{
				GameServer()->m_apPlayers[i]->m_aStrongWeakID[ID] = StrongWeakID;
				StrongWeakID++;
			}
		}
	}
}

int CPlayerMapping::GetSeeOthersID(int ClientID)
{
	// 0.7 or if no flags been used on the map
	if (!Server()->IsSevendown(ClientID) || !GameServer()->m_World.FlagsUsed())
		return Server()->GetMaxClients(ClientID) - 2;
	// 0.6 AND flags
	return GetSpecSelectFlag(ClientID, SPEC_FLAGBLUE) - 1;
}

bool CPlayerMapping::DoSeeOthers(int ClientId, int SelectedId, bool DoByVote)
{
	if(SelectedId == GetSeeOthersID(ClientId))
	{
		if(DoByVote)
		{
			m_aMap[ClientId].m_DoSeeOthersByVote = true;
		}
		m_aMap[ClientId].DoSeeOthers();
		return true;
	}
	return false;
}

void CPlayerMapping::ResetSeeOthers(int ClientID)
{
	m_aMap[ClientID].ResetSeeOthers();
}

int CPlayerMapping::GetTotalOverhang(int ClientID)
{
	return m_aMap[ClientID].m_TotalOverhang;
}

void CPlayerMapping::UpdatePlayerMap(int ClientID)
{
	if (ClientID == -1)
	{
		bool Update = Server()->Tick() % Config()->m_SvMapUpdateRate == 0;

		// We use m_Teams.Count in ReserveTeamSlots
		/*if (Update && !Config()->m_SvSoloServer)
		{
			// Cache team sizes to avoid more loops
			std::fill(std::begin(m_aTeamSizes), std::end(m_aTeamSizes), 0);
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if(!pPlayer)
					continue;
				int DDTeam = GameServer()->GetDDRaceTeam(i);
				m_aTeamSizes[DDTeam]++;
			}
		}*/

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!GameServer()->m_apPlayers[i])
				continue;

			// Calculate overhang every tick, not only when the map updates
			int Overhang = maximum(0, Server()->NumClients() - m_aMap[i].GetMapSize());
			if (Overhang != m_aMap[i].m_TotalOverhang)
			{
				m_aMap[i].m_TotalOverhang = Overhang;
				m_aMap[i].m_NumPages = maximum(1, (Overhang + PlayerMap::SSeeOthers::MAX_NUM_SEE_OTHERS - 1) / PlayerMap::SSeeOthers::MAX_NUM_SEE_OTHERS);
				if (m_aMap[i].m_TotalOverhang <= 0 && m_aMap[i].m_SeeOthersState != PlayerMap::SSeeOthers::STATE_NONE)
					m_aMap[i].ResetSeeOthers();

				m_aMap[i].UpdateSeeOthers();
				m_aMap[i].m_UpdateTeamsState = true;
			}

			if (Update)
			{
				m_aMap[i].Update();
			}
		}
	}
	else
	{
		m_aMap[ClientID].Update();
	}
}

int CPlayerMapping::GetSeeOthersInd(int ClientID, int MapID)
{
	if (m_aMap[ClientID].m_TotalOverhang && MapID == GetSeeOthersID(ClientID))
		return SEE_OTHERS_IND_BUTTON;
	if (m_aMap[ClientID].m_NumSeeOthers && MapID >= m_aMap[ClientID].GetMapSize() - m_aMap[ClientID].m_NumSeeOthers && MapID < m_aMap[ClientID].GetMapSize())
		return SEE_OTHERS_IND_PLAYER;
	return -1;
}

const char *CPlayerMapping::GetSeeOthersName(int ClientID)
{
	static char aName[MAX_NAME_LENGTH];
	int State = m_aMap[ClientID].m_SeeOthersState;
	const char *pDot = "\xe2\x8b\x85";

	int Page = State + 1;
	if (m_aMap[ClientID].m_NumPages > 1 && Page == m_aMap[ClientID].m_NumPages)
	{
		str_format(aName, sizeof(aName), "%s %d/%d | Close", pDot, Page, Page);
	}
	else if (State != PlayerMap::SSeeOthers::STATE_NONE)
	{
		if (m_aMap[ClientID].m_TotalOverhang > PlayerMap::SSeeOthers::MAX_NUM_SEE_OTHERS)
			str_format(aName, sizeof(aName), "%s %d/%d", pDot, Page, m_aMap[ClientID].m_NumPages);
		else
			str_format(aName, sizeof(aName), "%s Close", pDot);
	}
	else
	{
		str_format(aName, sizeof(aName), "%s %d others", pDot, m_aMap[ClientID].m_TotalOverhang);
	}
	return aName;
}

void CPlayerMapping::PlayerMap::CycleSeeOthers()
{
	if (m_TotalOverhang <= 0)
		return;

	for (int i = 0; i < m_pPlayerMapping->Server()->GetMaxClients(m_ClientID); i++)
		if (m_pMap[i] != -1)
			m_aWasSeeOthers[m_pMap[i]] = true;

	int Size = minimum(m_TotalOverhang, (int)PlayerMap::SSeeOthers::MAX_NUM_SEE_OTHERS);
	int Added = 0;
	int MapID = GetMapSize()-1;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!m_pPlayerMapping->GameServer()->m_apPlayers[i] || m_aWasSeeOthers[i])
			continue;

		Add(MapID, i);
		m_aWasSeeOthers[i] = true;
		Added++;
		MapID--;

		if (Added >= Size)
			break;
	}

	m_NumSeeOthers = Added;
}

void CPlayerMapping::PlayerMap::DoSeeOthers()
{
	if (m_TotalOverhang <= 0)
		return;

	m_SeeOthersState++;
	UpdateSeeOthers();

	CycleSeeOthers();

	// aggressively trigger reset now
	if (m_NumSeeOthers == 0)
	{
		// Reset these for the next cycle so we can get the fresh page we had before
		for (int i = 0; i < MAX_CLIENTS; i++)
			m_aWasSeeOthers[i] = false;
		CycleSeeOthers();
		ResetSeeOthers();
	}

	// instantly update so we dont have to wait for the map to be executed
	m_UpdateTeamsState = true;
	Update();
}

void CPlayerMapping::PlayerMap::ResetSeeOthers()
{
	m_SeeOthersState = SSeeOthers::STATE_NONE;
	m_NumSeeOthers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aWasSeeOthers[i] = false;
	m_UpdateTeamsState = true;
	UpdateSeeOthers();
}

int CPlayerMapping::PlayerMap::GetSpecSelectFlag(int SpecFlag)
{
	if (SpecFlag != SPEC_FLAGRED && SpecFlag != SPEC_FLAGBLUE)
		return -1;
	return m_pPlayerMapping->Server()->GetMaxClients(m_ClientID) - SpecFlag;
}

void CPlayerMapping::PlayerMap::AddToNumReserved(int Summand)
{
	// Remove old players from map if we take more space at the end
	if (m_NumReserved + Summand > m_NumReserved)
	{
		for (int i = GetMapSize()-1; i >= GetMapSize()-Summand; i--)
		{
			Remove(i);
		}
	}
	m_NumReserved += Summand;
}

void CPlayerMapping::PlayerMap::UpdateSeeOthers()
{
	if (m_pPlayerMapping->Server()->IsSevendown(m_ClientID))
		return;

	int SeeOthersID = m_pPlayerMapping->GetSeeOthersID(m_ClientID);
	CNetMsg_Sv_ClientDrop ClientDropMsg;
	ClientDropMsg.m_ClientID = SeeOthersID;
	ClientDropMsg.m_pReason = "";
	ClientDropMsg.m_Silent = 1;

	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = SeeOthersID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = TEAM_BLUE;
	NewClientInfoMsg.m_pName = m_pPlayerMapping->GetSeeOthersName(m_ClientID);
	NewClientInfoMsg.m_pClan = "";
	NewClientInfoMsg.m_Country = -1;
	NewClientInfoMsg.m_Silent = 1;
	for (int p = 0; p < NUM_SKINPARTS; p++)
	{
		bool Colored = p == SKINPART_BODY || p == SKINPART_FEET;
		NewClientInfoMsg.m_apSkinPartNames[p] = "standard";
		NewClientInfoMsg.m_aUseCustomColors[p] = (int)Colored;
		NewClientInfoMsg.m_aSkinPartColors[p] = Colored ? 5963600 : 0;
	}

	m_pPlayerMapping->Server()->SendPackMsg(&ClientDropMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
	m_pPlayerMapping->Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
}

void CPlayerMapping::PlayerMap::Init(int ClientID, CPlayerMapping *pPlayerMapping)
{
	m_ClientID = ClientID;
	m_pPlayerMapping = pPlayerMapping;
	m_pMap = m_pPlayerMapping->Server()->GetIdMap(m_ClientID);
	m_pReverseMap = m_pPlayerMapping->Server()->GetReverseIdMap(m_ClientID);
	m_ResortReserved = false;
	m_NumPages = 0;
	m_TotalOverhang = 0;
	m_NumReserved = 0;
	m_DoSeeOthersByVote = false;
	ResetSeeOthers();
}

void CPlayerMapping::PlayerMap::InitPlayer(bool Rejoin, bool Timeout)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aReserved[i] = false;

	int NextFreeID = 0;
	NETADDR OwnAddr, Addr;
	m_pPlayerMapping->Server()->GetClientAddr(m_ClientID, &OwnAddr);
	while (true && !GetPlayer()->m_IsDummy)
	{
		bool Break = true;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!m_pPlayerMapping->GameServer()->m_apPlayers[i] || m_pPlayerMapping->GameServer()->m_apPlayers[i]->m_IsDummy)
				continue;

			m_pPlayerMapping->Server()->GetClientAddr(i, &Addr);
			if (net_addr_comp(&OwnAddr, &Addr, false) == 0)
			{
				// For 0.7 timeout: Rejoin has to check ourselves because it's the id of the old connection that we want to skip
				// Do not access our own reverse map on initial initialization, as it's only initialized below
				if ((i != m_ClientID || Timeout) && m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] == NextFreeID)
				{
					NextFreeID++;
					Break = false;
				}
			}
		}

		if (Break)
			break;
	}

	// make sure no rests from before are in the client, so we can freshly start and insert our stuff
	if (Rejoin)
	{
		m_UpdateTeamsState = true; // to get flag spectators back and all teams aswell
		for (int i = 0; i < MAX_CLIENTS; i++)
			Remove(i);
	}

	// Clear map, for 0.7 timeouts do this after we got our id back
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_pMap[i] = -1;
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_pReverseMap[i] = -1;

	if (GetPlayer()->m_IsDummy)
		return; // just need to initialize the arrays

	m_NumReserved = 1;
	m_pMap[m_pPlayerMapping->Server()->GetMaxClients(m_ClientID) - 1] = -1; // player with empty name to say chat msgs

	// see others in spec menu
	m_NumReserved++;
	m_pMap[m_pPlayerMapping->GetSeeOthersID(m_ClientID)] = -1;
	m_TotalOverhang = 0;

	if (!m_pPlayerMapping->Server()->IsSevendown(m_ClientID))
	{
		CNetMsg_Sv_ClientInfo FakeInfo;
		FakeInfo.m_ClientID = VANILLA_MAX_CLIENTS-1;
		FakeInfo.m_Local = 0;
		FakeInfo.m_Team = TEAM_BLUE;
		FakeInfo.m_pName = " ";
		FakeInfo.m_pClan = "";
		FakeInfo.m_Country = -1;
		FakeInfo.m_Silent = 1;
		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			FakeInfo.m_apSkinPartNames[p] = "standard";
			FakeInfo.m_aUseCustomColors[p] = 0;
			FakeInfo.m_aSkinPartColors[p] = 0;
		}
		m_pPlayerMapping->Server()->SendPackMsg(&FakeInfo, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
		// see others
		UpdateSeeOthers();
	}
	else
	{
		if (m_pPlayerMapping->GameServer()->m_World.FlagsUsed())
		{
			m_NumReserved += 2;
			m_pMap[GetSpecSelectFlag(SPEC_FLAGRED)] = -1;
			m_pMap[GetSpecSelectFlag(SPEC_FLAGBLUE)] = -1;
		}
	}

	if (NextFreeID < GetMapSize())
	{
		m_aReserved[m_ClientID] = true;
		Add(NextFreeID, m_ClientID);
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!m_pPlayerMapping->GameServer()->m_apPlayers[i] || m_pPlayerMapping->GameServer()->m_apPlayers[i]->m_IsDummy || i == m_ClientID)
			continue;

		m_pPlayerMapping->Server()->GetClientAddr(i, &Addr);
		if (net_addr_comp(&OwnAddr, &Addr, false) != 0)
			continue;

		// update us with other same ip player infos
		if (m_pPlayerMapping->m_aMap[i].m_pReverseMap[i] < GetMapSize())
		{
			m_aReserved[i] = true;
			Add(m_pPlayerMapping->m_aMap[i].m_pReverseMap[i], i);
		}

		// update other same ip players with our info
		if (NextFreeID < m_pPlayerMapping->m_aMap[i].GetMapSize())
		{
			m_pPlayerMapping->m_aMap[i].m_aReserved[m_ClientID] = true;
			m_pPlayerMapping->m_aMap[i].Add(NextFreeID, m_ClientID);
		}
	}
}

CPlayer *CPlayerMapping::PlayerMap::GetPlayer()
{
	return m_pPlayerMapping->GameServer()->m_apPlayers[m_ClientID];
}

void CPlayerMapping::PlayerMap::Add(int MapID, int ClientID)
{
	if (MapID == -1 || ClientID == -1 || m_pReverseMap[ClientID] == MapID)
		return;

	Remove(m_pReverseMap[ClientID]);

	int OldClientID = Remove(MapID);
	// update teams state for teams and safe area (not a real team, as it's still individual per team actually)
	CTeamsCore *pTeamsCore = &((CGameControllerDDRace *)m_pPlayerMapping->GameServer()->m_pController)->m_Teams.m_Core;
	if (OldClientID == -1)
	{
		if (m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientID) > 0 || !pTeamsCore->GetInGame(ClientID))
			m_UpdateTeamsState = true;
	}
	else
	{
		if (m_pPlayerMapping->GameServer()->GetDDRaceTeam(OldClientID) != m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientID))
			m_UpdateTeamsState = true;
		if (pTeamsCore->GetInGame(OldClientID) != pTeamsCore->GetInGame(ClientID))
			m_UpdateTeamsState = true;
	}

	if (m_aReserved[ClientID])
		m_ResortReserved = true;

	m_pMap[MapID] = ClientID;
	m_pReverseMap[ClientID] = MapID;
	GetPlayer()->SendConnect(MapID, ClientID);
}

int CPlayerMapping::PlayerMap::Remove(int MapID)
{
	if (MapID == -1)
		return -1;

	int ClientID = m_pMap[MapID];
	if (ClientID != -1)
	{
		CTeamsCore *pTeamsCore = &((CGameControllerDDRace *)m_pPlayerMapping->GameServer()->m_pController)->m_Teams.m_Core;
		if (m_pPlayerMapping->GameServer()->GetDDRaceTeam(ClientID) > 0 || !pTeamsCore->GetInGame(ClientID))
			m_UpdateTeamsState = true;

		if (m_aReserved[ClientID])
			m_ResortReserved = true;

		GetPlayer()->SendDisconnect(MapID);
		m_pReverseMap[ClientID] = -1;
		m_pMap[MapID] = -1;
	}
	return ClientID;
}

void CPlayerMapping::PlayerMap::Update()
{
	if (!m_pPlayerMapping->Server()->ClientIngame(m_ClientID) || !GetPlayer() || GetPlayer()->m_IsDummy)
		return;

	if(m_DoSeeOthersByVote)
	{
		CCharacter *pChr = m_pPlayerMapping->GameServer()->GetPlayerChar(m_ClientID);
		if(pChr && !pChr->IsIdle())
		{
			ResetSeeOthers();
			m_DoSeeOthersByVote = false;
		}
	}

	bool ResortReserved = m_ResortReserved;
	m_ResortReserved = false;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i == m_ClientID)
			continue;

		CPlayer *pPlayer = m_pPlayerMapping->GameServer()->m_apPlayers[i];

		if (!m_pPlayerMapping->Server()->ClientIngame(i) || !pPlayer)
		{
			Remove(m_pReverseMap[i]);
			m_aReserved[i] = false;
			continue;
		}

		// If a team (not 0) has more than 10 players, do not reserve their slots because it can get messy quickly if a few huge teams form.
		// To keep teams state the same on main and dummy big teams do not get highlighted at all.
		int DDTeam = m_pPlayerMapping->GameServer()->GetDDRaceTeam(i);
		bool ReserveTeamSlots = m_pPlayerMapping->ReserveTeamSlots(DDTeam, i);
		bool IsInSafeArea = pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsInSafeArea();

		if (m_aReserved[i])
		{
			NETADDR OwnAddr, Addr;
			m_pPlayerMapping->Server()->GetClientAddr(m_ClientID, &OwnAddr);
			m_pPlayerMapping->Server()->GetClientAddr(i, &Addr);
			if (net_addr_comp(&OwnAddr, &Addr, false) != 0)
			{
				bool UnsetReservedSlot = !ReserveTeamSlots && !IsInSafeArea; // condition to unset reserved slot
				if (ResortReserved || UnsetReservedSlot)
				{
					m_aReserved[i] = false;

					// reset our team to 0 when we are in a big team for example
					if(DDTeam != TEAM_FLOCK)
						m_UpdateTeamsState = true;
				}
			}
			continue;
		}
		else if (ResortReserved)
			continue;

		int Insert = -1;
		if ((DDTeam != TEAM_FLOCK && ReserveTeamSlots) || IsInSafeArea)
		{
			for (int j = 0; j < GetMapSize()-m_NumSeeOthers; j++)
			{
				int CID = m_pMap[j];
				if (CID == -1 || !m_aReserved[CID])
				{
					Insert = j;
					m_aReserved[i] = true;
					break;
				}
			}
		}
		else if (m_pReverseMap[i] != -1)
		{
			Insert = m_pReverseMap[i];
		}
		else
		{
			for (int j = 0; j < GetMapSize()-m_NumSeeOthers; j++)
				if (m_pMap[j] == -1)
				{
					Insert = j;
					break;
				}
		}

		if (Insert != -1)
		{
			Add(Insert, i);
		}
		else if (pPlayer->GetCharacter() && !pPlayer->GetCharacter()->NetworkClipped(m_ClientID))
		{
			InsertNextEmpty(i);
		}
	}

	if (m_UpdateTeamsState)
	{
		((CGameControllerDDRace *)m_pPlayerMapping->GameServer()->m_pController)->m_Teams.SendTeamsState(m_ClientID);
		m_UpdateTeamsState = false;
	}
}

void CPlayerMapping::PlayerMap::InsertNextEmpty(int ClientID)
{
	if (ClientID == -1 || m_pReverseMap[ClientID] != -1)
		return;

	for (int i = 0; i < GetMapSize()-m_NumSeeOthers; i++)
	{
		int CID = m_pMap[i];
		if (CID != -1 && m_aReserved[CID])
			continue;

		if (CID == -1 || (!m_pPlayerMapping->GameServer()->GetPlayerChar(CID) || m_pPlayerMapping->GameServer()->GetPlayerChar(CID)->NetworkClipped(m_ClientID)))
		{
			Add(i, ClientID);
			break;
		}
	}
}

bool CPlayerMapping::ReserveTeamSlots(int DDTeam, int AskerID)
{
	if (GameServer()->GetClientDDNetVersion(AskerID) >= VERSION_DDNET_128)
		return true;

	//int TeamSize = m_aTeamSizes[DDTeam];
	CGameControllerDDRace *pController = (CGameControllerDDRace*)GameServer()->m_pController;
	int TeamSize = pController->m_Teams.Count(DDTeam);
	return !Config()->m_SvSoloServer && DDTeam != TEAM_FLOCK && TeamSize <= Config()->m_SvPlayerMapMaxTeamSize;
}

int CPlayerMapping::PlayerMap::GetMapSize()
{
	return m_pPlayerMapping->Server()->GetMaxClients(m_ClientID) - m_NumReserved;
}
