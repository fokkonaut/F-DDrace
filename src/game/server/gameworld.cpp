/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entities/character.h"
#include "entity.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"
#include <algorithm>
#include <utility>
#include <engine/shared/config.h>
#include "gamemodes/DDRace.h"

void CSelectedArea::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	for (int i = 0; i < 4; i++)
		m_aID[i] = m_pGameServer->Server()->SnapNewID();
}

CSelectedArea::~CSelectedArea()
{
	for (int i = 0; i < 4; i++)
		m_pGameServer->Server()->SnapFreeID(m_aID[i]);
}


//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld()
{
	m_pGameServer = 0x0;
	m_pConfig = 0x0;
	m_pServer = 0x0;

	m_Paused = false;
	m_ResetRequested = false;
	for(int i = 0; i < NUM_ENTTYPES; i++)
		m_apFirstEntityTypes[i] = 0;
}

CGameWorld::~CGameWorld()
{
	// delete all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		while(m_apFirstEntityTypes[i])
			delete m_apFirstEntityTypes[i];
}

void CGameWorld::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aMap[i].Init(i, this);
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? 0 : m_apFirstEntityTypes[Type];
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type];	pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(distance(pEnt->m_Pos, Pos) < Radius+pEnt->m_ProximityRadius)
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

void CGameWorld::InsertEntity(CEntity *pEnt)
{
#ifdef CONF_DEBUG
	for(CEntity *pCur = m_apFirstEntityTypes[pEnt->m_ObjType]; pCur; pCur = pCur->m_pNextTypeEntity)
		dbg_assert(pCur != pEnt, "err");
#endif

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = 0x0;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::DestroyEntity(CEntity *pEnt)
{
	pEnt->MarkForDestroy();
}

void CGameWorld::RemoveEntity(CEntity *pEnt)
{
	// not in the list
	if(!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity && m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
		return;

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	if(m_pNextTraverseEntity == pEnt)
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

	pEnt->m_pNextTypeEntity = 0;
	pEnt->m_pPrevTypeEntity = 0;
}

//
void CGameWorld::Snap(int SnappingClient)
{
	std::vector<CEntity *> vpPlotObjects;
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if (pEnt->m_PlotID >= 0)
				vpPlotObjects.push_back(pEnt);
			else
				pEnt->Snap(SnappingClient);
			pEnt = m_pNextTraverseEntity;
		}

	// snap plot objects after we got everything else, so we dont fill the snap with plot objects before everything important
	for (unsigned int i = 0; i < vpPlotObjects.size(); i++)
		vpPlotObjects[i]->Snap(SnappingClient);
}

void CGameWorld::PostSnap()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->PostSnap();
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Reset()
{
	// reset all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	m_ResetRequested = false;
}

void CGameWorld::RemoveEntities()
{
	// destroy objects marked for destruction
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(pEnt->IsMarkedForDestroy())
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
			}
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::UpdatePlayerMap(int ClientID)
{
	if (ClientID == -1)
	{
		if (Server()->Tick() % Config()->m_SvMapUpdateRate == 0)
			for (int i = 0; i < MAX_CLIENTS; i++)
				m_aMap[i].Update();
	}
	else
	{
		m_aMap[ClientID].Update();
	}
}

void CGameWorld::PlayerMap::Init(int ClientID, CGameWorld *pGameWorld)
{
	m_ClientID = ClientID;
	m_pGameWorld = pGameWorld;
	m_pMap = m_pGameWorld->Server()->GetIdMap(m_ClientID);
	m_pReverseMap = m_pGameWorld->Server()->GetReverseIdMap(m_ClientID);
	m_UpdateTeamsState = false;
}

void CGameWorld::PlayerMap::InitPlayer(bool Rejoin)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aReserved[i] = false;

	// make sure no rests from before are in the client, so we can freshly start and insert our stuff
	if (Rejoin)
	{
		m_UpdateTeamsState = true; // to get flag spectators back and all teams aswell

		for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
			Remove(i);
	}

	for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		m_pMap[i] = -1;

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_pReverseMap[i] = -1;

	if (GetPlayer()->m_IsDummy)
		return; // just need to initialize the arrays

	int NextFreeID = 0;
	NETADDR OwnAddr, Addr;
	m_pGameWorld->Server()->GetClientAddr(m_ClientID, &OwnAddr);
	while (1)
	{
		bool Break = true;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!m_pGameWorld->GameServer()->m_apPlayers[i] || m_pGameWorld->GameServer()->m_apPlayers[i]->m_IsDummy || i == m_ClientID)
				continue;

			m_pGameWorld->Server()->GetClientAddr(i, &Addr);
			if (net_addr_comp(&OwnAddr, &Addr, false) == 0)
			{
				if (m_pGameWorld->m_aMap[i].m_pReverseMap[i] == NextFreeID)
				{
					NextFreeID++;
					Break = false;
				}
			}
		}

		if (Break)
			break;
	}

	m_NumReserved = 1;
	m_pMap[VANILLA_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs

	if (!m_pGameWorld->Server()->IsSevendown(m_ClientID))
	{
		CNetMsg_Sv_ClientInfo FakeInfo;
		FakeInfo.m_ClientID = VANILLA_MAX_CLIENTS-1;
		FakeInfo.m_Local = 0;
		FakeInfo.m_Team = TEAM_SPECTATORS;
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
		m_pGameWorld->Server()->SendPackMsg(&FakeInfo, MSGFLAG_VITAL|MSGFLAG_NORECORD|MSGFLAG_NOTRANSLATE, m_ClientID);
	}
	else
	{
		if (m_pGameWorld->GameServer()->FlagsUsed())
		{
			m_NumReserved += 2;
			m_pMap[SPEC_SELECT_FLAG_RED] = -1;
			m_pMap[SPEC_SELECT_FLAG_BLUE] = -1;
		}
	}

	if (NextFreeID < VANILLA_MAX_CLIENTS-m_NumReserved)
	{
		m_aReserved[m_ClientID] = true;
		Add(NextFreeID, m_ClientID);
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!m_pGameWorld->GameServer()->m_apPlayers[i] || m_pGameWorld->GameServer()->m_apPlayers[i]->m_IsDummy || i == m_ClientID)
			continue;

		m_pGameWorld->Server()->GetClientAddr(i, &Addr);
		if (net_addr_comp(&OwnAddr, &Addr, false) != 0)
			continue;

		// update us with other same ip player infos
		if (m_pGameWorld->m_aMap[i].m_pReverseMap[i] < VANILLA_MAX_CLIENTS-m_NumReserved)
		{
			m_aReserved[i] = true;
			Add(m_pGameWorld->m_aMap[i].m_pReverseMap[i], i);
		}

		// update other same ip players with our info
		if (NextFreeID < VANILLA_MAX_CLIENTS-m_pGameWorld->m_aMap[i].m_NumReserved)
		{
			m_pGameWorld->m_aMap[i].m_aReserved[m_ClientID] = true;
			m_pGameWorld->m_aMap[i].Add(NextFreeID, m_ClientID);
		}
	}
}

CPlayer *CGameWorld::PlayerMap::GetPlayer()
{
	return m_pGameWorld->GameServer()->m_apPlayers[m_ClientID];
}

void CGameWorld::PlayerMap::Add(int MapID, int ClientID)
{
	if (MapID == -1 || ClientID == -1 || m_pReverseMap[ClientID] == MapID)
		return;

	Remove(m_pReverseMap[ClientID]);

	int OldClientID = Remove(MapID);
	if ((OldClientID == -1 && m_pGameWorld->GameServer()->GetDDRaceTeam(ClientID) > 0)
		|| (OldClientID != -1 && m_pGameWorld->GameServer()->GetDDRaceTeam(OldClientID) != m_pGameWorld->GameServer()->GetDDRaceTeam(ClientID)))
		m_UpdateTeamsState = true;

	if (m_aReserved[ClientID])
		m_ResortReserved = true;

	m_pMap[MapID] = ClientID;
	m_pReverseMap[ClientID] = MapID;
	GetPlayer()->SendConnect(MapID, ClientID);
}

int CGameWorld::PlayerMap::Remove(int MapID)
{
	if (MapID == -1)
		return -1;

	int ClientID = m_pMap[MapID];
	if (ClientID != -1)
	{
		if (m_pGameWorld->GameServer()->GetDDRaceTeam(ClientID) > 0)
			m_UpdateTeamsState = true;

		if (m_aReserved[ClientID])
			m_ResortReserved = true;

		GetPlayer()->SendDisconnect(MapID);
		m_pReverseMap[ClientID] = -1;
		m_pMap[MapID] = -1;
	}
	return ClientID;
}

void CGameWorld::PlayerMap::Update()
{
	if (!m_pGameWorld->Server()->ClientIngame(m_ClientID) || !GetPlayer() || GetPlayer()->m_IsDummy)
		return;

	bool ResortReserved = m_ResortReserved;
	m_ResortReserved = false;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i == m_ClientID)
			continue;

		CPlayer *pPlayer = m_pGameWorld->GameServer()->m_apPlayers[i];

		if (!m_pGameWorld->Server()->ClientIngame(i) || !pPlayer)
		{
			Remove(m_pReverseMap[i]);
			m_aReserved[i] = false;
			continue;
		}

		if (m_aReserved[i])
		{
			NETADDR OwnAddr, Addr;
			m_pGameWorld->Server()->GetClientAddr(m_ClientID, &OwnAddr);
			m_pGameWorld->Server()->GetClientAddr(i, &Addr);
			if (net_addr_comp(&OwnAddr, &Addr, false) != 0)
			{
				if (ResortReserved || !m_pGameWorld->GameServer()->GetDDRaceTeam(i)) // condition to unset reserved slot
					m_aReserved[i] = false;
			}
			continue;
		}
		else if (ResortReserved)
			continue;

		int Insert = -1;
		if (m_pGameWorld->GameServer()->GetDDRaceTeam(i))
		{
			for (int j = 0; j < VANILLA_MAX_CLIENTS-m_NumReserved; j++)
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
			for (int j = 0; j < VANILLA_MAX_CLIENTS-m_NumReserved; j++)
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
		((CGameControllerDDRace *)m_pGameWorld->GameServer()->m_pController)->m_Teams.SendTeamsState(m_ClientID);
		m_UpdateTeamsState = false;
	}
}

void CGameWorld::PlayerMap::InsertNextEmpty(int ClientID)
{
	if (ClientID == -1 || m_pReverseMap[ClientID] != -1)
		return;

	for (int i = 0; i < VANILLA_MAX_CLIENTS-m_NumReserved; i++)
	{
		int CID = m_pMap[i];
		if (CID != -1 && m_aReserved[CID])
			continue;

		if (CID == -1 || (!m_pGameWorld->GameServer()->GetPlayerChar(CID) || m_pGameWorld->GameServer()->GetPlayerChar(CID)->NetworkClipped(m_ClientID)))
		{
			Add(i, ClientID);
			break;
		}
	}
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();

	if(m_Paused)
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
	}
	else
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}

		// we need to do this between core tick and Move of all the players, because otherwise its getting jiggly for those whose coretick didnt happen yet
		for (CCharacter *pChr = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChr; pChr = (CCharacter *)pChr->TypeNext())
		{
			pChr->m_Snake.Tick();
		}

		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDefered();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();

	UpdatePlayerMap(-1);

	int StrongWeakID = 0;
	for (CCharacter *pChar = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
	{
		pChar->m_StrongWeakID = StrongWeakID;
		StrongWeakID++;
	}

	// Translate StrongWeakID to clamp it to 64 players
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!GameServer()->m_apPlayers[i] || GameServer()->m_apPlayers[i]->m_IsDummy)
			continue;

		int StrongWeakID = 0;
		for (CCharacter *pChar = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
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


// TODO: should be more general
CCharacter* CGameWorld::IntersectCharacter(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CCharacter* pNotThis, int CollideWith, class CCharacter* pThisOnly)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = 0;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
 	{
		if(p == pNotThis)
			continue;

		if (pThisOnly && p != pThisOnly)
			continue;

		if (CollideWith != -1 && !p->CanCollide(CollideWith))
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		float Len = distance(p->m_Pos, IntersectPos);
		if(Len < p->m_ProximityRadius+Radius)
		{
			Len = distance(Pos0, IntersectPos);
			if(Len < ClosestLen)
			{
				NewPos = IntersectPos;
				ClosestLen = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}


CEntity *CGameWorld::ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis, bool CheckWall)
{
	// Find other players
	float ClosestRange = Radius*2;
	CEntity *pClosest = 0;

	CEntity *p = FindFirst(Type);
	for(; p; p = p->TypeNext())
 	{
		if(p == pNotThis)
			continue;

		float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius+Radius)
		{
			if(Len < ClosestRange)
			{
				if (CheckWall && GameServer()->Collision()->IntersectLine(Pos, p->GetPos(), 0, 0))
					continue;

				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

CCharacter* CGameWorld::ClosestCharacter(vec2 Pos, float Radius, CEntity* pNotThis, int CollideWith, bool CheckPassive, bool CheckWall, bool CheckMinigameTee)
{
	// Find other players
	float ClosestRange = Radius * 2;
	CCharacter* pClosest = 0;

	CCharacter* p = (CCharacter*)FindFirst(ENTTYPE_CHARACTER);
	for (; p; p = (CCharacter*)p->TypeNext())
	{
		if (p == pNotThis)
			continue;

		if (CollideWith != -1 && !p->CanCollide(CollideWith, CheckPassive))
			continue;

		float Len = distance(Pos, p->m_Pos);
		if (CheckMinigameTee && p->GetPlayer()->IsMinigame() && p->GetPlayer()->m_SavedMinigameTee)
		{
			float LenMinigame = distance(Pos, p->GetPlayer()->m_MinigameTee.GetPos());
			if (LenMinigame < Len)
				Len = LenMinigame;
		}

		if (Len < p->m_ProximityRadius + Radius)
		{
			if (Len < ClosestRange)
			{
				if (CheckWall && GameServer()->Collision()->IntersectLine(Pos, p->GetPos(), 0, 0))
					continue;

				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

std::list<class CCharacter*> CGameWorld::IntersectedCharacters(vec2 Pos0, vec2 Pos1, float Radius, class CEntity* pNotThis, int CollideWith)
{
	std::list< CCharacter* > listOfChars;

	CCharacter* pChr = (CCharacter*)FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for (; pChr; pChr = (CCharacter*)pChr->TypeNext())
	{
		if (pChr == pNotThis)
			continue;

		if (CollideWith != -1 && !pChr->CanCollide(CollideWith))
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, pChr->m_Pos);
		float Len = distance(pChr->m_Pos, IntersectPos);
		if (Len < pChr->m_ProximityRadius + Radius)
		{
			pChr->m_Intersection = IntersectPos;
			listOfChars.push_back(pChr);
		}
	}
	return listOfChars;
}

void CGameWorld::ReleaseHooked(int ClientID)
{
	CCharacter* pChr = (CCharacter*)CGameWorld::FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for (; pChr; pChr = (CCharacter*)pChr->TypeNext())
	{
		CCharacterCore* Core = pChr->Core();
		if (Core->m_HookedPlayer == ClientID && !pChr->m_Super)
		{
			Core->m_HookedPlayer = -1;
			Core->m_HookState = HOOK_RETRACTED;
		}
	}
}

// F-DDrace

CCharacter* CGameWorld::ClosestCharacter(vec2 Pos, CCharacter* pNotThis, int CollideWith, int Mode)
{
	// Find other players
	float ClosestRange = 0.f;
	CCharacter* pClosest = 0;

	CCharacter* p = (CCharacter*)FindFirst(ENTTYPE_CHARACTER);
	for (; p; p = (CCharacter*)p->TypeNext())
	{
		if (p == pNotThis)
			continue;

		bool CheckPassive = !GameServer()->IsHouseDummy(CollideWith);
		if (CollideWith != -1 && !p->CanCollide(CollideWith, CheckPassive))
			continue;

		if (Mode == 1) // BlmapChill police freeze hole right side
		{
			if ((!GameServer()->m_Accounts[p->GetPlayer()->GetAccID()].m_PoliceLevel && !p->m_PoliceHelper) || p->m_FreezeTime == 0 || p->m_Pos.y > 438 * 32 || p->m_Pos.x < 430 * 32 || p->m_Pos.x > 445 * 32 || p->m_Pos.y < 423 * 32)
				continue;
		}
		if (Mode == 2) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 416 * 32 || p->m_Pos.x > 446 * 32 || p->m_Pos.y < 198 * 32)
				continue;
		}
		if (Mode == 3) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 434 * 32 || p->m_Pos.x > 441 * 32 || p->m_Pos.y < 198 * 32)
				continue;
		}
		if (Mode == 4) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 417 * 32 || p->m_Pos.x > 444 * 32 || p->m_Pos.y < 198 * 32)
				continue;
		}
		if (Mode == 5) // for dummy 29
		{
			if (p->m_Pos.y < 213 * 32 || p->m_Pos.x > 429 * 32 || p->m_Pos.x < 419 * 32 || p->m_Pos.y > 218 * 32 + 60)
				continue;
		}
		if (Mode == 6) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 416 * 32 || p->m_Pos.x > 417 * 32 - 10 || p->m_Pos.y < 198 * 32)
				continue;
		}
		if (Mode == 7) // for dummy 23
		{
			if (p->m_Pos.y > 200 * 32 || p->m_Pos.x < 466 * 32)
				continue;
		}
		if (Mode == 8) // for dummy 23
		{
			if (p->m_FreezeTime == 0)
				continue;
		}
		if (Mode == 9) // for shopbot
		{
			if (GameServer()->IsHouseDummy(p->GetPlayer()->GetCID()))
				continue;
		}
		if (Mode == 10) // BlmapChill police freeze pit left side
		{
			if ((!GameServer()->m_Accounts[p->GetPlayer()->GetAccID()].m_PoliceLevel && !p->m_PoliceHelper) || p->m_FreezeTime == 0 || p->m_Pos.y > 436 * 32 || p->m_Pos.x < 363 * 32 || p->m_Pos.x > 381 * 32 || p->m_Pos.y < 420 * 32)
				continue;
		}

		float Len = distance(Pos, p->m_Pos);
		if (Len < ClosestRange || !ClosestRange)
		{
			ClosestRange = Len;
			pClosest = p;
		}
	}

	return pClosest;
}

int CGameWorld::GetClosestHouseDummy(vec2 Pos, CCharacter* pNotThis, int Type, int CollideWith)
{
	// Find other players
	float ClosestRange = 0.f;
	CCharacter* pClosest = 0;

	CCharacter* p = (CCharacter*)FindFirst(ENTTYPE_CHARACTER);
	for (; p; p = (CCharacter*)p->TypeNext())
	{
		if (p == pNotThis)
			continue;

		if (!GameServer()->IsHouseDummy(p->GetPlayer()->GetCID(), Type))
			continue;

		if (CollideWith != -1 && !p->CanCollide(CollideWith, false))
			continue;

		float Len = distance(Pos, p->m_Pos);
		if (Len < ClosestRange || !ClosestRange)
		{
			ClosestRange = Len;
			pClosest = p;
		}
	}

	return pClosest ? pClosest->GetPlayer()->GetCID() : GameServer()->GetHouseDummy(Type);
}

CEntity *CGameWorld::ClosestEntityTypes(vec2 Pos, float Radius, int Types, CEntity *pNotThis, int CollideWith, bool CheckPassive)
{
	for (int i = 0; i < NUM_ENTTYPES; i++)
	{
		if (!(Types&1<<i))
			continue;

		if (i == ENTTYPE_CHARACTER)
		{
			CCharacter* pChr = ClosestCharacter(Pos, Radius, pNotThis, CollideWith, CheckPassive);
			if (pChr)
				return pChr;
		}
		else
		{
			CEntity* pEntity = ClosestEntity(Pos, Radius, i, pNotThis);
			if (pEntity)
				return pEntity;
		}
	}

	return 0;
}

int CGameWorld::FindEntitiesTypes(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Types)
{
	int Num = 0;

	for (int i = 0; i < NUM_ENTTYPES; i++)
	{
		if (!(Types&1<<i))
			continue;

		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
		{
			if(distance(pEnt->m_Pos, Pos) < Radius+pEnt->m_ProximityRadius)
			{
				if(ppEnts)
					ppEnts[Num] = pEnt;
				Num++;
				if(Num == Max)
					break;
			}
		}
	}

	return Num;
}

CEntity *CGameWorld::IntersectEntityTypes(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CEntity *pNotThis, int CollideWith, int Types, CCharacter *pThisOnly)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CEntity *pClosest = 0;

	for (int i = 0; i < NUM_ENTTYPES; i++)
	{
		if (!(Types&1<<i))
			continue;

		CEntity *p = FindFirst(i);
		for(; p; p = p->TypeNext())
 		{
			if(p == pNotThis)
				continue;

			if (pThisOnly && p != pThisOnly)
				continue;

			if (i == ENTTYPE_FLAG && ((CFlag *)p)->GetCarrier())
				continue;

			if (CollideWith != -1)
			{
				CCharacter *pChr = 0;
				if (i == ENTTYPE_CHARACTER)
					pChr = (CCharacter *)p;
				else if (i == ENTTYPE_FLAG || i == ENTTYPE_PICKUP_DROP || i == ENTTYPE_MONEY)
					pChr = ((CAdvancedEntity *)p)->GetOwner();

				if (pChr && !pChr->CanCollide(CollideWith))
					continue;
			}

			vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
			float Len = distance(p->m_Pos, IntersectPos);
			if(Len < p->m_ProximityRadius+Radius)
			{
				Len = distance(Pos0, IntersectPos);
				if(Len < ClosestLen)
				{
					NewPos = IntersectPos;
					ClosestLen = Len;
					pClosest = p;
				}
			}
		}
	}

	return pClosest;
}
