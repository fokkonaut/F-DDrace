// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include "portal.h"
#include <engine/shared/config.h>

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

	for (int i = 0; i < MAX_CLIENTS; i++)
		m_aLastUse[i] = 0;

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
	int64_t TeamMask = pOwner ? pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner) : -1LL;
	GameServer()->CreateDeath(m_Pos, m_Owner, TeamMask);
	GameWorld()->DestroyEntity(this);
}

void CPortal::SetLinkedPortal(CPortal *pPortal)
{
	m_pLinkedPortal = pPortal;
	m_LinkedTick = Server()->Tick();
}

void CPortal::Tick()
{
	if (m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner])
		m_Owner = -1;

	if (m_Owner != -1&& !GameServer()->GetPlayerChar(m_Owner))
		Reset();

	if ((m_LinkedTick == 0 && m_StartTick < Server()->Tick() - Server()->TickSpeed() * Config()->m_SvPortalDetonation)
		|| (m_LinkedTick != 0 && m_LinkedTick < Server()->Tick() - Server()->TickSpeed() * Config()->m_SvPortalDetonationLinked))
	{
		Reset();
		return;
	}

	PlayerEnter();
}

void CPortal::PlayerEnter()
{
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, Config()->m_SvPortalRadius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for (int i = 0; i < Num; i++)
	{
		CCharacter *pChr = apEnts[i];
		int ID = pChr->GetPlayer()->GetCID();

		if (!m_pLinkedPortal || m_aLastUse[ID] > Server()->Tick() - Server()->TickSpeed() * 2)
			continue;

		int64_t TeamMask = pChr->Teams()->TeamMask(pChr->Team(), -1, ID);

		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, TeamMask);
		GameServer()->CreateDeath(m_Pos, ID, TeamMask);

		pChr->ReleaseHook();
		pChr->Core()->m_Pos = pChr->m_PrevPos = m_pLinkedPortal->m_Pos;
		pChr->m_DDRaceState = DDRACE_CHEAT;

		GameServer()->CreatePlayerSpawn(m_pLinkedPortal->m_Pos, TeamMask);

		m_pLinkedPortal->m_aLastUse[ID] = m_aLastUse[ID] = Server()->Tick();
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
			int64_t TeamMask = pOwner->Teams()->TeamMask(pOwner->Team(), -1, m_Owner);
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
