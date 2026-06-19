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
#include "entities/weapons/projectile.h"
#include "entities/weapons/custom_projectile.h"
#include "entities/weapons/missile.h"
#include "entities/misc/lasertext.h"
#include "entities/interactive/vehicle/spider.h"
#include "entities/interactive/vehicle/helicopter.h"

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
}

CEntity *CGameWorld::FindFirst(int Type)
{
	return Type < 0 || Type >= NUM_ENTTYPES ? 0 : m_apFirstEntityTypes[Type];
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type, int Team)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type];	pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if (Team != -1 && Team != TEAM_SUPER)
		{
			if (Type == ENTTYPE_CHARACTER && Team != ((CCharacter*)pEnt)->Team())
				continue;
			if (pEnt->IsAdvancedEntity() && Team != ((CAdvancedEntity*)pEnt)->GetDDTeam())
				continue;
		}

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
	for(CEntity *pEnt = m_apFirstEntityTypes[ENTTYPE_CHARACTER]; pEnt;)
	{
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
		pEnt->Snap(SnappingClient);
		pEnt = m_pNextTraverseEntity;
	}

	std::vector<CEntity *> vpPlotObjects;
	for(int i = 0; i < NUM_ENTTYPES; i++)
	{
		if(i == ENTTYPE_CHARACTER || i == ENTTYPE_DRAWTILE)
			continue;

		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if (pEnt->m_PlotID >= 0)
				vpPlotObjects.push_back(pEnt);
			else
				pEnt->Snap(SnappingClient);
			pEnt = m_pNextTraverseEntity;
		}
	}

	// never iterate through all drawtiles
	m_DrawTiles.ProcessRemovals();
	for (auto &pDrawTile : m_DrawTiles.ResponsibleTiles())
		pDrawTile->Snap(SnappingClient);

	// snap plot objects after we got everything else, so we dont fill the snap with plot objects before everything important
	for (unsigned int i = 0; i < vpPlotObjects.size(); i++)
		vpPlotObjects[i]->Snap(SnappingClient);
}

void CGameWorld::PostSnap()
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
	{
		if (i == ENTTYPE_DRAWTILE)
			continue;

		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->PostSnap();
			pEnt = m_pNextTraverseEntity;
		}
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

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();

	if(m_Paused)
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
		{
			if (i == ENTTYPE_DRAWTILE)
				continue;
				
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
		}
	}
	else
	{
		// process lightning laser before character, so that vel set to vec2(0, 0) will make a chr fall slowly, and make him slightly movable
		for(CEntity *pEnt = m_apFirstEntityTypes[ENTTYPE_LIGHTNING_LASER]; pEnt; )
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Tick();
			pEnt = m_pNextTraverseEntity;
		}

		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
		{
			// processed above
			if (i == ENTTYPE_LIGHTNING_LASER || i == ENTTYPE_DRAWTILE)
				continue;

			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}
		}

		// process between tick and tick deferred
		IntraTick();

		for(int i = 0; i < NUM_ENTTYPES; i++)
		{
			if (i == ENTTYPE_DRAWTILE)
				continue;
				
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; )
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDeferred();
				pEnt = m_pNextTraverseEntity;
			}
		}
	}

	RemoveEntities();

	int StrongWeakID = 0;
	for (CCharacter *pChar = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChar; pChar = (CCharacter *)pChar->TypeNext())
	{
		pChar->m_StrongWeakID = StrongWeakID;
		StrongWeakID++;
	}
}

void CGameWorld::IntraTick()
{
    int NumCharacters = 0;
    m_PoliceFarm.m_NumPoliceTilePlayers = 0;
    m_Hotzone.m_PlayersInHotzone = 0;
    
    for (CCharacter *pChr = (CCharacter *)FindFirst(ENTTYPE_CHARACTER); pChr; pChr = (CCharacter *)pChr->TypeNext())
    {
        pChr->m_Snake.Tick();
        if (pChr->GetPlayer()->m_IsDummy)
            continue;
        NumCharacters++;
        if (pChr->m_MoneyTile == CCharacter::MONEYTILE_POLICE && !pChr->m_Passive && !pChr->GetPlayer()->IsMinigame())
        {
            m_PoliceFarm.m_NumPoliceTilePlayers++;
        }
        
        if (pChr->m_HotzoneTile && !pChr->m_Passive && !pChr->GetPlayer()->IsMinigame())
        {
            m_Hotzone.m_PlayersInHotzone++;
        }
    }

    const int Limit = Config()->m_SvPoliceFarmLimit;
    m_PoliceFarm.m_MaxPoliceTilePlayers = Limit != -1 ? Limit : clamp((int)floor(NumCharacters * 0.125f + 3), 3, 16);
}

bool CGameWorld::FlagsUsed()
{
	return (GameServer()->m_pController->GetGameFlags()&GAMEFLAG_FLAGS);
}

void CGameWorld::ReleaseHooked(int ClientID)
{
	CCharacter* pChr = (CCharacter*)CGameWorld::FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for (; pChr; pChr = (CCharacter*)pChr->TypeNext())
	{
		CCharacterCore* Core = pChr->Core();
		if (Core->HookedPlayer() == ClientID && !pChr->m_Super)
		{
			Core->SetHookedPlayer(-1);
			Core->m_HookState = HOOK_RETRACTED;
		}
	}
}

void CGameWorld::UnsetTelekinesis(CEntity *pEntity)
{
	CCharacter* pChr = (CCharacter*)CGameWorld::FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for (; pChr; pChr = (CCharacter*)pChr->TypeNext())
	{
		if (pChr->m_pTelekinesisEntity == pEntity)
		{
			pChr->m_pTelekinesisEntity = 0;
			break; // can break here, every entity can only be picked by one player using telekinesis at the time
		}
	}
}

void CGameWorld::UnsetKiller(int ClientID)
{
	CCharacter* pChr = (CCharacter*)CGameWorld::FindFirst(CGameWorld::ENTTYPE_CHARACTER);
	for (; pChr; pChr = (CCharacter*)pChr->TypeNext())
	{
		if (ClientID != pChr->GetPlayer()->GetCID() && pChr->Core()->m_Killer.m_ClientID == ClientID)
		{
			pChr->Core()->m_Killer.m_ClientID = -1;
			pChr->Core()->m_Killer.m_Weapon = -1;
		}
	}
}

int CGameWorld::MoneyLaserTextTime(int64 Amount)
{
	return Amount < SMALL_MONEY_AMOUNT ? 1 : 3;
}

CLaserText *CGameWorld::CreateLaserText(vec2 Pos, int Owner, const char *pText, int Seconds, bool AboveTee)
{
	if (AboveTee)
	{
		Pos.y -= 70.f;
	}
	Pos.y -= 32.f;
	Pos.x -= 16.f;
	return new CLaserText(this, Pos, Owner, Seconds > 0 ? Server()->TickSpeed() * Seconds : -1, pText, (int)(strlen(pText)));
}

bool CGameWorld::SpawnSpider(int Spawner, int Team, vec2 Pos, float Scale, bool SpawnOnFloor, int Number)
{
	Scale = clamp(Scale, SPIDER_MIN_SCALE, SPIDER_MAX_SCALE);
	vec2 ResultingHitbox = IVehicle::MinimumVehicleHitbox(SPIDER_PHYSSIZE * Scale);
	ResultingHitbox = vec2(maximum(ResultingHitbox.x, 28.0f), maximum(ResultingHitbox.y, 28.0f));
	if (SpawnOnFloor)
		Pos.y -= ResultingHitbox.y / 2.f - CCharacterCore::PHYS_SIZE / 2.f;

	if (GameServer()->Collision()->TestBoxBig(Pos, ResultingHitbox))
		return false;

	new CSpider(this, Spawner, Team, Pos, Scale, Server()->TickSpeed() * 1, Number);

	return true;
}

bool CGameWorld::SpawnHelicopter(int Spawner, int Team, vec2 Pos, int HelicopterType, int TurretType, float Scale, bool SpawnOnFloor, int Number)
{
	Scale = clamp(Scale, HELICOPTER_MIN_SCALE, HELICOPTER_MAX_SCALE);
	vec2 ResultingHitbox = IVehicle::MinimumVehicleHitbox(HELICOPTER_PHYSSIZE * Scale);
	if (SpawnOnFloor)
		Pos.y -= ResultingHitbox.y / 2.f - CCharacterCore::PHYS_SIZE / 2.f;

	if (GameServer()->Collision()->TestBoxBig(Pos, ResultingHitbox))
		return false;

	if (HelicopterType < 0 || HelicopterType >= NUM_HELICOPTER_TYPES)
		return false;

	CHelicopter *pHelicopter = new CHelicopter(this, HelicopterType, Spawner, Team, Pos, Scale, Server()->TickSpeed() * 1, Number, TurretType);
	if (TurretType > TURRETTYPE_NONE && TurretType < NUM_TURRET_TYPES)
	{
		pHelicopter->AllocateNumAttachments(1);

		IVehicleTurret *pTurret = nullptr;
		if (TurretType == TURRETTYPE_MINIGUN)
			pTurret = new CMinigunTurret();
		else if (TurretType == TURRETTYPE_LAUNCHER)
			pTurret = new CLauncherTurret();

		if (!pHelicopter->TryAttach(pTurret))
			delete pTurret; // Failed to assign ownership
	}

	return true;
}

int CGameWorld::GetHelicopterTileType()
{
	if (Config()->m_SvHeliTileType == NUM_HELICOPTER_TYPES)
		return random_int(HELICOPTER_DEFAULT, NUM_HELICOPTER_TYPES - 1);
	return Config()->m_SvHeliTileType;
}

// Find functions

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

		vec2 IntersectPos;
		if(closest_point_on_line(Pos0, Pos1, p->m_Pos, IntersectPos))
		{
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

CEntity *CGameWorld::ClosestEntity(vec2 Pos, float Radius, int Type, CEntity *pNotThis, int Team, bool CheckWall)
{
	// Find other players
	float ClosestRange = Radius*2;
	CEntity *pClosest = 0;

	CEntity *p = FindFirst(Type);
	for(; p; p = p->TypeNext())
 	{
		if(p == pNotThis)
			continue;

		if (Team != -1 && Team != TEAM_SUPER)
		{
			if (Type == ENTTYPE_CHARACTER && Team != ((CCharacter*)p)->Team())
				continue;
			if (p->IsAdvancedEntity() && Team != ((CAdvancedEntity*)p)->GetDDTeam())
				continue;
		}

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

CCharacter* CGameWorld::ClosestCharacter(vec2 Pos, float Radius, CEntity* pNotThis, int CollideWith, int Team, int Flags)
{
	// Default flags if nothing is specified
	if (Flags == -1)
	{
		Flags = EFindEntFlag::PASSIVE | EFindEntFlag::IN_HELICOPTER | EFindEntFlag::SAFE_AREA;
	}

	// Find other players
	float ClosestRange = Radius * 2;
	CCharacter* pClosest = 0;

	CCharacter* p = (CCharacter*)FindFirst(ENTTYPE_CHARACTER);
	for (; p; p = (CCharacter*)p->TypeNext())
	{
		if (p == pNotThis)
			continue;

		if (Team != -1 && Team != p->Team())
			continue;

		if (CollideWith != -1 && !p->CanCollide(CollideWith, Flags & EFindEntFlag::PASSIVE, Flags & EFindEntFlag::SAFE_AREA))
			continue;

		if (Flags & EFindEntFlag::IN_HELICOPTER && p->m_pVehicle)
			continue;

		float Len = distance(Pos, p->m_Pos);
		if (Flags & EFindEntFlag::MINIGAME_TEE && p->GetPlayer()->IsMinigame() && p->GetPlayer()->m_SavedMinigameTee)
		{
			float LenMinigame = distance(Pos, p->GetPlayer()->m_MinigameTee.GetPos());
			if (LenMinigame < Len)
				Len = LenMinigame;
		}

		if (Len < p->m_ProximityRadius + Radius)
		{
			if (Len < ClosestRange)
			{
				if (Flags & EFindEntFlag::WALL && GameServer()->Collision()->IntersectLine(Pos, p->GetPos(), 0, 0))
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

		vec2 IntersectPos;
		if(closest_point_on_line(Pos0, Pos1, pChr->m_Pos, IntersectPos))
		{
			float Len = distance(pChr->m_Pos, IntersectPos);
			if (Len < pChr->m_ProximityRadius + Radius)
			{
				pChr->m_Intersection = IntersectPos;
				listOfChars.push_back(pChr);
			}
		}
	}
	return listOfChars;
}

// F-DDrace

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

CEntity *CGameWorld::ClosestEntityTypes(vec2 Pos, float Radius, int64 Types, CEntity *pNotThis, int CollideWith, int Flags)
{
	for (int i = 0; i < NUM_ENTTYPES; i++)
	{
		if (!(Types&(1ULL<<i)))
			continue;

		if (i == ENTTYPE_CHARACTER)
		{
			CCharacter* pChr = ClosestCharacter(Pos, Radius, pNotThis, CollideWith, -1, Flags);
			if (pChr)
				return pChr;
		}
		else
		{
			bool CheckWall = Flags != -1 && Flags & EFindEntFlag::WALL;
			CEntity* pEntity = ClosestEntity(Pos, Radius, i, pNotThis, -1, CheckWall);
			if (pEntity)
				return pEntity;
		}
	}

	return 0;
}

static const float s_ProjectileHammerRadius = CCharacterCore::PHYS_SIZE * 2.f;

int CGameWorld::FindEntitiesTypes(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int64 Types, int Team, bool ProjHammer)
{
	int Num = 0;

	for (int i = 0; i < NUM_ENTTYPES; i++)
	{
		if (!(Types&(1ULL<<i)))
			continue;

		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
		{
			if (Team != -1 && Team != TEAM_SUPER)
			{
				if (i == ENTTYPE_CHARACTER && Team != ((CCharacter*)pEnt)->Team())
					continue;
				if (pEnt->IsAdvancedEntity() && Team != ((CAdvancedEntity*)pEnt)->GetDDTeam())
					continue;
				if (i == ENTTYPE_PROJECTILE && Team != ((CProjectile*)pEnt)->DDTeam())
					continue;
				if (i == ENTTYPE_CUSTOM_PROJECTILE && Team != ((CCustomProjectile*)pEnt)->DDTeam())
					continue;
				if (i == ENTTYPE_MISSILE && Team != ((CMissile*)pEnt)->DDTeam())
					continue;
			}

			vec2 EntPos = pEnt->m_Pos;
			float EntRadius = pEnt->m_ProximityRadius;
			if (ProjHammer && (i == ENTTYPE_PROJECTILE || i == ENTTYPE_CUSTOM_PROJECTILE || i == ENTTYPE_MISSILE))
			{
				// projectiles have a ProximityRadius of 0, unhittable
				EntRadius = s_ProjectileHammerRadius;
				
				if (i == ENTTYPE_PROJECTILE) // fetch current position
				{
					// only allow projectiles shot by players
					if (((CProjectile *)pEnt)->GetOwner() == -1)
						continue;
					EntPos = ((CProjectile *)pEnt)->m_CurPos;
				}
				else if (i == ENTTYPE_CUSTOM_PROJECTILE)
				{
					// only allow projectiles shot by players, even though custom projectiles currently cant be map placed
					if (((CCustomProjectile *)pEnt)->GetOwner() == -1)
						continue;
				}
				else if (i == ENTTYPE_MISSILE)
				{
					if (((CMissile*)pEnt)->GetOwner() == -1)
						continue;
				}
			}

			if(distance(EntPos, Pos) < Radius+EntRadius)
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

CEntity *CGameWorld::IntersectEntityTypes(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, const CNotTheseEntities& NotThese, int CollideWith, int64 Types, CCharacter *pThisOnly, int Flags)
{
	if (Flags == -1)
	{
		Flags = EIntersectEntTypesFlag::IN_VEHICLE;
	}

	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CEntity *pClosest = 0;

	int Team = CollideWith == -1 ? 0 : GameServer()->GetDDRaceTeam(CollideWith);
	bool CheckPlotTaserDestroy = Flags & EIntersectEntTypesFlag::PLOT_TASER_DESTROY;

	for (int i = 0; i < NUM_ENTTYPES; i++)
	{
		if (!(Types&(1ULL<<i)))
			continue;

		CEntity *p = FindFirst(i);
		for(; p; p = p->TypeNext())
 		{
			float ProximityRadius = p->m_ProximityRadius;
			bool MarkForPredictPrevent = false;
			CCharacter *pChr = 0;

			bool EntTypeDestroyable = i == ENTTYPE_DOOR || i == ENTTYPE_PICKUP || i == ENTTYPE_BUTTON || i == ENTTYPE_SPEEDUP || i == ENTTYPE_TELEPORTER || i == ENTTYPE_DRAWTILE;
			if ((CheckPlotTaserDestroy && !EntTypeDestroyable) || !CheckPlotTaserDestroy)
			{
				if(NotThese.IsExcluded(p))
					continue;

				if (pThisOnly && p != pThisOnly)
					continue;

				if (i == ENTTYPE_CHARACTER && Flags & EIntersectEntTypesFlag::IN_VEHICLE && ((CCharacter *)p)->m_pVehicle)
					continue;

				if (i == ENTTYPE_FLAG && ((CFlag *)p)->GetCarrier())
					continue;

				if ((i == ENTTYPE_HELICOPTER || i == ENTTYPE_SPIDER) && ((IVehicle *)p)->IsInvincible())
					continue;

				if (CollideWith != -1)
				{
					if (i == ENTTYPE_CHARACTER)
					{
						pChr = (CCharacter *)p;
						if ((Flags & EIntersectEntTypesFlag::PREVENT_EVENT_PREDICTION) && pChr->IsActiveProjectileHammer() && pChr->GetPlayer()->AntiPing())
						{
							// prevent explosion and damageind prediction as we redirect the projectile
							MarkForPredictPrevent = true;
							ProximityRadius = s_ProjectileHammerRadius * 3.f;
						}
					}
					else if (p->IsAdvancedEntity())
					{
						pChr = ((CAdvancedEntity *)p)->GetOwner();
						if (((CAdvancedEntity *)p)->GetDDTeam() != Team)
							continue;
					}

					if (pChr && !pChr->CanCollide(CollideWith))
						continue;
				}
			}
			else
			{
				int PlotID = p->m_PlotID;
				if (!GameServer()->m_Plots.PlotCanBeRaided(PlotID))
					continue;

				if (i == ENTTYPE_DOOR)
				{
					bool IsPlotDoor = p->IsPlotDoor();
					if (p->m_BrushCID != -1 || p->m_TransformCID != -1 || (Flags & EIntersectEntTypesFlag::PLOT_DOOR_ONLY && !IsPlotDoor))
						continue;

					CDoor *pDoor = (CDoor *)p;
					if (IsPlotDoor)
					{
						if (!GameServer()->Collision()->m_pSwitchers || !GameServer()->Collision()->m_pSwitchers[pDoor->m_Number].m_Status[Team])
							continue;
					}

					vec2 ClosestPoint;
					if (pDoor->GetIntersectPos(Pos0, Pos1, 32.f/2, &ClosestPoint))
					{
						float Len = distance(Pos0, ClosestPoint);
						if (Len < ClosestLen)
						{
							NewPos = ClosestPoint;
							ClosestLen = Len;
							pClosest = p;
						}
					}
					continue;
				}
				else if (i == ENTTYPE_TELEPORTER)
				{
					// Virtually increase teleporter entity size because we're checking for taser destroy
					// we don't want the taser to be teleported by a weapon teleporter instead of getting destroyed
					ProximityRadius *= 1.75f;
				}
			}

			vec2 IntersectPos;
			if (closest_point_on_line(Pos0, Pos1, p->m_Pos, IntersectPos))
			{
				float Len = distance(p->m_Pos, IntersectPos);
				if(Len < ProximityRadius+Radius)
				{
					if (MarkForPredictPrevent)
					{
						pChr->PreventEventPrediction();
						// dont process if character is not actually nearby.
						if(Len >= pChr->GetProximityRadius()+Radius)
							continue;
					}

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
	}

	return pClosest;
}

bool CGameWorld::IntersectLinePortalBlocker(vec2 Pos0, vec2 Pos1)
{
	CPortalBlocker *pPortalBlocker = (CPortalBlocker *)FindFirst(ENTTYPE_PORTAL_BLOCKER);
	for (; pPortalBlocker; pPortalBlocker = (CPortalBlocker *)pPortalBlocker->TypeNext())
		if (pPortalBlocker->IsPlaced() && intersect_segments(Pos0, Pos1, pPortalBlocker->GetPos(), pPortalBlocker->GetStartPos()))
			return true;
	return false;
}

int CGameWorld::IntersectDoorsUniqueNumbers(vec2 Pos, float Radius, CDoor **ppDoors, int Max)
{
	int Num = 0;
	CDoor *pDoor = (CDoor *)FindFirst(ENTTYPE_DOOR);
	for (; pDoor; pDoor = (CDoor *)pDoor->TypeNext())
	{
		if (pDoor->m_Number == 0)
			continue;

		vec2 DistanceToLine, ClosestPoint;
		if(closest_point_on_line(pDoor->GetPos(), pDoor->GetToPos(), Pos, ClosestPoint))
		{
			DistanceToLine = Pos - ClosestPoint;
		}
		else
		{
			// No line section was passed but two equal points
			DistanceToLine = Pos - pDoor->GetPos();
		}

		if (absolute(DistanceToLine.x) < Radius && absolute(DistanceToLine.y) < Radius)
		{
			if(ppDoors)
			{
				bool Continue = false;
				for (int i = 0; i < Num; i++)
				{
					// only unique doors, since they overlap most of the time and it would put both in then
					if (pDoor->m_Number == ppDoors[i]->m_Number)
					{
						Continue = true;
						break;
					}
				}

				if (Continue)
					continue;

				ppDoors[Num] = pDoor;
			}
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

// only used for dummymodes
CCharacter* CGameWorld::ClosestCharacterMode(vec2 Pos, CCharacter* pNotThis, int CollideWith, int Mode)
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
			if ((!GameServer()->m_Accounts.Get(p->GetPlayer()->GetAccID()).m_PoliceLevel && !p->m_PoliceHelper) || p->GetPlayer()->m_EscapeTime || p->m_FreezeTime == 0 || p->m_Pos.y > 438 * 32 || p->m_Pos.x < 430 * 32 || p->m_Pos.x > 445 * 32 || p->m_Pos.y < 423 * 32)
				continue;
		}
		else if (Mode == 2) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 416 * 32 || p->m_Pos.x > 446 * 32 || p->m_Pos.y < 198 * 32)
				continue;
		}
		else if (Mode == 3) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 434 * 32 || p->m_Pos.x > 441 * 32 || p->m_Pos.y < 198 * 32)
				continue;
		}
		else if (Mode == 4) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 417 * 32 || p->m_Pos.x > 444 * 32 || p->m_Pos.y < 198 * 32)
				continue;
		}
		else if (Mode == 5) // for dummy 29
		{
			if (p->m_Pos.y < 213 * 32 || p->m_Pos.x > 429 * 32 || p->m_Pos.x < 419 * 32 || p->m_Pos.y > 218 * 32 + 60)
				continue;
		}
		else if (Mode == 6) // for dummy 29
		{
			if (p->m_Pos.y > 213 * 32 || p->m_Pos.x < 416 * 32 || p->m_Pos.x > 417 * 32 - 10 || p->m_Pos.y < 198 * 32)
				continue;
		}
		else if (Mode == 7) // for dummy 23
		{
			if (p->m_Pos.y > 200 * 32 || p->m_Pos.x < 466 * 32)
				continue;
		}
		else if (Mode == 8) // for dummy 23
		{
			if (p->m_FreezeTime == 0)
				continue;
		}
		else if (Mode == 9) // for shopbot
		{
			if (GameServer()->IsHouseDummy(p->GetPlayer()->GetCID()))
				continue;
		}
		else if (Mode == 10) // BlmapChill police freeze pit left side
		{
			if ((!GameServer()->m_Accounts.Get(p->GetPlayer()->GetAccID()).m_PoliceLevel && !p->m_PoliceHelper) || p->GetPlayer()->m_EscapeTime || p->m_FreezeTime == 0 || p->m_Pos.y > 436 * 32 || p->m_Pos.x < 363 * 32 || p->m_Pos.x > 381 * 32 || p->m_Pos.y < 420 * 32)
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
