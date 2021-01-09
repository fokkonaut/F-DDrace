// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include "portal.h"
#include "flag.h"
#include "money.h"
#include <engine/shared/config.h>
#include <algorithm>

CPortal::CPortal(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTAL, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_StartTick = Server()->Tick();
	m_pLinkedPortal = 0;
	m_LinkedTick = 0;

	for (int i = 0; i < NUM_IDS; i++)
		m_aID[i] = Server()->SnapNewID();

	GameWorld()->InsertEntity(this);
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
	if (pChr)
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, pChr->Teams()->TeamMask(pChr->Team(), -1, m_Owner));
}

CPortal::~CPortal()
{
	for (int i = 0; i < NUM_IDS; i++)
		Server()->SnapFreeID(m_aID[i]);
}

void CPortal::Reset()
{
	if (m_Owner != -1 && GameServer()->m_apPlayers[m_Owner])
		for (int i = 0; i < NUM_PORTALS; i++)
			GameServer()->m_apPlayers[m_Owner]->m_pPortal[i] = 0;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	Mask128 TeamMask = pOwner ? pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner) : Mask128();
	GameServer()->CreateDeath(m_Pos, m_Owner, TeamMask);
	GameWorld()->DestroyEntity(this);
}

void CPortal::SetLinkedPortal(CPortal *pPortal)
{
	m_pLinkedPortal = pPortal;
	m_LinkedTick = Server()->Tick();
}

void CPortal::DestroyLinkedPortal()
{
	if (m_pLinkedPortal)
		m_pLinkedPortal->Reset();
}

void CPortal::Tick()
{
	if (m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner])
		m_Owner = -1;

	if (m_Owner != -1 && !GameServer()->GetPlayerChar(m_Owner))
	{
		Reset();
		return;
	}
	 
	if ((m_LinkedTick == 0 && m_StartTick < Server()->Tick() - Server()->TickSpeed() * Config()->m_SvPortalDetonation)
		|| (m_LinkedTick != 0 && m_LinkedTick < Server()->Tick() - Server()->TickSpeed() * Config()->m_SvPortalDetonationLinked))
	{
		Reset();
		return;
	}

	EntitiesEnter();
}

void CPortal::EntitiesEnter()
{
	if (!m_pLinkedPortal)
		return;

	for (unsigned i = 0; i < m_vTeleported.size(); i++)
	{
		CEntity *pEnt = m_vTeleported[i];
		vec2 Pos;
		if (pEnt)
		{
			Pos = pEnt->GetPos();
			if (pEnt->GetObjType() == CGameWorld::ENTTYPE_CHARACTER)
				Pos = ((CCharacter *)pEnt)->Core()->m_Pos;
		}

		if (!pEnt || (distance(Pos, m_Pos) > Config()->m_SvPortalRadius + pEnt->GetProximityRadius()))
			m_vTeleported.erase(m_vTeleported.begin() + i);
	}

	int Types = (1<<CGameWorld::ENTTYPE_CHARACTER) | (1<<CGameWorld::ENTTYPE_FLAG) | (1<<CGameWorld::ENTTYPE_PICKUP_DROP) | (1<<CGameWorld::ENTTYPE_MONEY);
	CEntity *apEnts[128];
	int Num = GameWorld()->FindEntitiesTypes(m_Pos, Config()->m_SvPortalRadius, (CEntity**)apEnts, 128, Types);

	for (int i = 0; i < Num; i++)
	{
		// continue when we just got teleported, so we dont keep teleporting back and forth
		if (std::find(m_vTeleported.begin(), m_vTeleported.end(), apEnts[i]) != m_vTeleported.end())
			continue;

		// dont allow travelling when touching the portal through a wall
		if (GameServer()->Collision()->IntersectLine(m_Pos, apEnts[i]->GetPos(), 0, 0))
			continue;

		CCharacter *pAffectedChr = 0;
		switch (apEnts[i]->GetObjType())
		{
		case CGameWorld::ENTTYPE_CHARACTER:
			{
				CCharacter *pChr = (CCharacter *)apEnts[i];
				pAffectedChr = pChr;
				break;
			}
		case CGameWorld::ENTTYPE_FLAG:
			{
				CFlag *pFlag = (CFlag *)apEnts[i];
				if (pFlag->GetCarrier())
					continue; // owner is getting teleported with the flag

				if (pFlag->GetLastCarrier())
					pAffectedChr = pFlag->GetLastCarrier();
				break;
			}
		case CGameWorld::ENTTYPE_PICKUP_DROP:
			{
				CPickupDrop *pPickup = (CPickupDrop *)apEnts[i];
				if (pPickup->GetOwner())
					pAffectedChr = pPickup->GetOwner();
				break;
			}
		case CGameWorld::ENTTYPE_MONEY:
			{
				CMoney *pMoney = (CMoney *)apEnts[i];
				if (pMoney->GetOwner())
					pAffectedChr = pMoney->GetOwner();
				break;
			}
		}

		// disallow using the portal if its placed on the other side of a plot door
		int Team = pAffectedChr ? pAffectedChr->Team() : 0;
		if (GameServer()->IntersectedLineDoor(m_Pos, apEnts[i]->GetPos(), Team, true))
			continue;

		switch (apEnts[i]->GetObjType())
		{
		case CGameWorld::ENTTYPE_CHARACTER:
			{
				CCharacter *pChr = (CCharacter *)apEnts[i];

				if (!pAffectedChr->CanCollide(m_Owner, false))
					continue;

				pChr->ReleaseHook();
				pChr->ForceSetPos(m_pLinkedPortal->m_Pos);
				pChr->m_DDRaceState = DDRACE_CHEAT;

				pChr->Core()->m_Killer.m_ClientID = m_Owner;
				pChr->Core()->m_Killer.m_Weapon = -1;
				pChr->m_LastTouchedPortalBy = m_Owner;
				break;
			}
		case CGameWorld::ENTTYPE_FLAG:
			{
				CFlag *pFlag = (CFlag *)apEnts[i];
				pFlag->SetPos(m_pLinkedPortal->m_Pos);
				pFlag->SetPrevPos(m_pLinkedPortal->m_Pos);
				break;
			}
		case CGameWorld::ENTTYPE_PICKUP_DROP:
			{
				CPickupDrop *pPickup = (CPickupDrop *)apEnts[i];
				pPickup->SetPos(m_pLinkedPortal->m_Pos);
				pPickup->SetPrevPos(m_pLinkedPortal->m_Pos);
				break;
			}
		case CGameWorld::ENTTYPE_MONEY:
			{
				CMoney *pMoney = (CMoney *)apEnts[i];
				pMoney->SetPos(m_pLinkedPortal->m_Pos);
				pMoney->SetPrevPos(m_pLinkedPortal->m_Pos);
				break;
			}
		}

		int ID = pAffectedChr ? pAffectedChr->GetPlayer()->GetCID() : -1;
		Mask128 TeamMask = pAffectedChr ? pAffectedChr->Teams()->TeamMask(pAffectedChr->Team(), -1, ID) : Mask128();

		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, TeamMask);
		GameServer()->CreateDeath(m_Pos, ID, TeamMask);
		GameServer()->CreatePlayerSpawn(m_pLinkedPortal->m_Pos, TeamMask);

		m_pLinkedPortal->m_vTeleported.push_back(apEnts[i]);
		m_vTeleported.push_back(apEnts[i]);
	}
}

void CPortal::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (GameServer()->GetPlayerChar(SnappingClient) && m_Owner != -1)
	{
		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		if (pOwner)
		{
			Mask128 TeamMask = pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner);
			if (!CmaskIsSet(TeamMask, SnappingClient))
				return;
		}
	}
	
	int Radius = Config()->m_SvPortalRadius;
	float AngleStep = 2.0f * pi / NUM_SIDE;

	for(int i = 0; i < NUM_SIDE; i++)
	{
		vec2 PartPosStart = m_Pos + vec2(Radius * cos(AngleStep*i), Radius * sin(AngleStep*i));
		vec2 PartPosEnd = m_Pos + vec2(Radius * cos(AngleStep*(i+1)), Radius * sin(AngleStep*(i+1)));
		
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aID[i], sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = (int)PartPosStart.x;
		pObj->m_Y = (int)PartPosStart.y;
		pObj->m_FromX = (int)PartPosEnd.x;
		pObj->m_FromY = (int)PartPosEnd.y;
		pObj->m_StartTick = Server()->Tick();
	}

	if (!m_pLinkedPortal)
		return;

	for(int i = 0; i < NUM_PARTICLES; i++)
	{
		float RandomRadius = frandom()*(Radius-4.0f);
		float RandomAngle = 2.0f * pi * frandom();
		vec2 ParticlePos = m_Pos + vec2(RandomRadius * cos(RandomAngle), RandomRadius * sin(RandomAngle));
			
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aID[NUM_SIDE+i], sizeof(CNetObj_Projectile)));
		if(pObj)
		{
			pObj->m_X = (int)ParticlePos.x;
			pObj->m_Y = (int)ParticlePos.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = Server()->Tick();
			pObj->m_Type = WEAPON_HAMMER;
		}
	}
}
