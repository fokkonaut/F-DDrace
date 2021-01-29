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
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Snap(SnappingClient);
			pEnt = m_pNextTraverseEntity;
		}
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

bool distCompare(std::pair<float,int> a, std::pair<float,int> b)
{
	return (a.first < b.first);
}

void CGameWorld::UpdatePlayerMaps(int ForcedID)
{
	if (ForcedID == -1 && Server()->Tick() % Config()->m_SvMapUpdateRate != 0)
		return;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (ForcedID != -1 && i != ForcedID) continue; // only update specific player
		if (!Server()->ClientIngame(i) || (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_IsDummy)) continue;
		int *pMap = Server()->GetIdMap(i);

		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		pMap[VANILLA_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs
		int Reserved = 1;

		if (Server()->IsSevendown(i) && GameServer()->FlagsUsed())
		{
			// reserved for flag selecting
			pMap[SPEC_SELECT_FLAG_RED] = -1;
			pMap[SPEC_SELECT_FLAG_BLUE] = -1;
			Reserved = VANILLA_MAX_CLIENTS - SPEC_SELECT_FLAG_BLUE;
		}

		int rMap[MAX_CLIENTS];
		for (int j = 0; j < MAX_CLIENTS; j++)
			rMap[j] = -1;
		for (int j = 0; j < VANILLA_MAX_CLIENTS; j++)
			if (pMap[j] != -1)
				rMap[pMap[j]] = j;

		bool UpdateTeamsStates = false;
		for (int j = 0; j < MAX_CLIENTS; j++)
		{
			if (i == j)
				continue;

			CPlayer *pChecked = GameServer()->m_apPlayers[j];

			int Free = -1;
			for (int k = 0; k < VANILLA_MAX_CLIENTS-Reserved; k++)
			{
				if (pMap[k] == -1 || (pPlayer->m_aSameIP[j] && k == GameServer()->m_apPlayers[j]->m_FakeID))
				{
					Free = k;
					break;
				}
			}

			if (!pPlayer->m_aSameIP[j] && (!Server()->ClientIngame(j) || !pChecked || (!pChecked->GetCharacter() && Free == -1)))
			{
				if (rMap[j] != -1)
				{
					pPlayer->SendDisconnect(j, rMap[j]);
					pMap[rMap[j]] = -1;
					rMap[j] = -1;
				}
				continue;
			}

			if (rMap[j] == -1 && (Free != -1 || (pChecked->GetCharacter() && !pChecked->GetCharacter()->NetworkClipped(i, false)) || pPlayer->m_aSameIP[j]))
			{
				if (Free != -1)
				{
					if (pMap[Free] == -1)
					{
						pPlayer->SendConnect(j, Free);
					}
					else if (pMap[Free] != j)
					{
						pPlayer->SendDisconnect(pMap[Free], Free);
						pPlayer->SendConnect(j, Free);
						rMap[pMap[Free]] = -1;

						if (GameServer()->GetDDRaceTeam(pMap[Free]) != GameServer()->GetDDRaceTeam(j))
							UpdateTeamsStates = true;
					}
					pMap[Free] = j;
					rMap[j] = Free;
				}
				else
				{
					for (int k = 0; k < MAX_CLIENTS; k++)
					{
						if (k != i && rMap[k] != -1 && !pPlayer->m_aSameIP[k] && GameServer()->GetPlayerChar(k) && GameServer()->GetPlayerChar(k)->NetworkClipped(i, false))
						{
							pPlayer->SendDisconnect(k, rMap[k]);
							rMap[j] = rMap[k];
							pMap[rMap[j]] = j;
							rMap[k] = -1;
							pPlayer->SendConnect(j, rMap[j]);

							if (GameServer()->GetDDRaceTeam(k) != GameServer()->GetDDRaceTeam(j))
								UpdateTeamsStates = true;
							break;
						}
					}
				}
			}
		}

		if (UpdateTeamsStates)
			((CGameControllerDDRace *)GameServer()->m_pController)->m_Teams.SendTeamsState(i);
	}
}

/*void CGameWorld::UpdatePlayerMaps()
{
	if (Server()->Tick() % Config()->m_SvMapUpdateRate != 0) return;

	std::pair<float,int> Dist[MAX_CLIENTS];
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!Server()->ClientIngame(i) || (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_IsDummy)) continue;
		int *pMap = Server()->GetIdMap(i);

		// compute reverse map
		int rMap[MAX_CLIENTS];

		// compute distances
		for (int j = 0; j < MAX_CLIENTS; j++)
		{
			rMap[j] = -1;

			Dist[j].second = j;
			if (!Server()->ClientIngame(j) || !GameServer()->m_apPlayers[j])
			{
				Dist[j].first = 1e10;
				continue;
			}

			// set distance for same ip players to very close, so we always send it
			if (GameServer()->m_apPlayers[i]->m_aSameIP[j])
			{
				Dist[j].first = 0;
				continue;
			}

			CCharacter* ch = GameServer()->m_apPlayers[j]->GetCharacter();
			if (!ch)
			{
				Dist[j].first = 1e8;
				continue;
			}
			// copypasted chunk from character.cpp Snap() follows
			CCharacter* SnapChar = GameServer()->GetPlayerChar(i);
			if(SnapChar && !SnapChar->m_Super &&
				!GameServer()->m_apPlayers[i]->IsPaused() && GameServer()->m_apPlayers[i]->GetTeam() != -1 &&
				!ch->CanCollide(i, false)
			)
				Dist[j].first = 1e7;
			else
				Dist[j].first = 0;

			Dist[j].first += distance(GameServer()->m_apPlayers[i]->m_ViewPos, GameServer()->m_apPlayers[j]->GetCharacter()->m_Pos);
		}

		// always send the player himself
		Dist[i].first = 0;

		for (int j = 0; j < VANILLA_MAX_CLIENTS; j++)
		{
			if (pMap[j] == -1) continue;
			// the ip check here so we keep the ones with the same ip so we can set the rMap[j] below on the second run of this function
			if (GameServer()->m_apPlayers[i]->m_aSameIP[pMap[j]]) continue;
			if (Dist[pMap[j]].first > 5e9) pMap[j] = -1;
			else rMap[pMap[j]] = j;
		}

		std::nth_element(&Dist[0], &Dist[VANILLA_MAX_CLIENTS - 1], &Dist[MAX_CLIENTS], distCompare);

		// get amount of same ip players
		int SameIP = 0;
		for (int j = 0; j < MAX_CLIENTS; j++)
		{
			if (GameServer()->m_apPlayers[i]->m_aSameIP[j])
			{
				SameIP++;

				// manually insert players with same ip, they wont get inserted by the algorithm
				// set rMap[j] on the second run of this function, so we can first send disconnect of the tee before
				int FakeID = GameServer()->m_apPlayers[j]->m_FakeID;
				if (pMap[FakeID] == j)
					rMap[j] = FakeID;
				pMap[FakeID] = j;
			}
		}

		int Mapc = SameIP;
		int Demand = 0;
		for (int j = 0; j < VANILLA_MAX_CLIENTS - 1; j++)
		{
			int k = Dist[j].second;
			// skip player with same ip, manually inserted it already
			if (GameServer()->m_apPlayers[i]->m_aSameIP[k]) continue;
			if (rMap[k] != -1 || Dist[j].first > 5e9) continue;
			while (Mapc < VANILLA_MAX_CLIENTS && pMap[Mapc] != -1) Mapc++;
			if (Mapc < VANILLA_MAX_CLIENTS - 1)
				pMap[Mapc] = k;
			else
				Demand++;
		}
		for (int j = MAX_CLIENTS - 1; j > VANILLA_MAX_CLIENTS - 2; j--)
		{
			int k = Dist[j].second;
			// skip player with same ip, manually inserted it already
			if (GameServer()->m_apPlayers[i]->m_aSameIP[k]) continue;
			if (rMap[k] != -1 && Demand-- > 0)
				pMap[rMap[k]] = -1;
		}

		if (!Server()->IsSevendown(i))
		{
			for (int j = 0; j < MAX_CLIENTS; j++)
			{
				int id = j;
				if (!Server()->Translate(id, i))
				{
					if (rMap[j] != -1)
						GameServer()->m_apPlayers[i]->SendDisconnect(j, rMap[j]);
				}
				else
				{
					if (rMap[j] == -1)
						GameServer()->m_apPlayers[i]->SendConnect(j, id);
				}
			}
		}

		pMap[VANILLA_MAX_CLIENTS - 1] = -1; // player with empty name to say chat msgs
	}
}*/

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

		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDefered();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();

	UpdatePlayerMaps();

	int StrongWeakID = 0;
	for (CCharacter* pChar = (CCharacter*)FindFirst(ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		pChar->m_StrongWeakID = StrongWeakID;
		StrongWeakID++;
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

		if (CheckWall && GameServer()->Collision()->IntersectLine(Pos, p->GetPos(), 0, 0))
			continue;

		float Len = distance(Pos, p->m_Pos);
		if(Len < p->m_ProximityRadius+Radius)
		{
			if(Len < ClosestRange)
			{
				ClosestRange = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

CCharacter* CGameWorld::ClosestCharacter(vec2 Pos, float Radius, CEntity* pNotThis, int CollideWith, bool CheckPassive, bool CheckWall)
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

		if (CheckWall && GameServer()->Collision()->IntersectLine(Pos, p->GetPos(), 0, 0))
			continue;

		float Len = distance(Pos, p->m_Pos);
		if (Len < p->m_ProximityRadius + Radius)
		{
			if (Len < ClosestRange)
			{
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
