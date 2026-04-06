// made by timakro

#include <game/server/gamecontext.h>
#include "stable_projectile.h"
#include <game/server/teams.h>
#include <game/server/player.h>
#include "character.h"

CStableProjectile::CStableProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, bool HideOnSpec, bool OnlyShowOwner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_STABLE_PROJECTILE, Pos)
{
	m_Type = GameServer()->GetWeaponType(Type);
	m_Pos = Pos;
	m_PrevPos = Pos;
	for (int i = 0; i < NUM_SNAPINFO; i++)
	{
		m_aSnap[i].m_LastResetTick = Server()->Tick();
		m_aSnap[i].m_LastResetPos = Pos;
	}
	m_Owner = Owner;
	m_HideOnSpec = HideOnSpec;
	m_CalculatedVel = false;
	m_TeamMask = Mask128();
	m_OnlyShowOwner = OnlyShowOwner;

	GameWorld()->InsertEntity(this);
}

void CStableProjectile::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CStableProjectile::TickDeferred()
{
	if(Server()->Tick()%2 == 1)
	{
		m_aSnap[SNAPINFO_LOWBANDWIDTH].m_LastResetPos = m_Pos;
		m_aSnap[SNAPINFO_LOWBANDWIDTH].m_LastResetTick = Server()->Tick();
	}
	m_aSnap[SNAPINFO_HIGHBANDWIDTH].m_LastResetTick = Server()->Tick() - 1;
	m_aSnap[SNAPINFO_HIGHBANDWIDTH].m_LastResetPos = m_PrevPos;
	m_PrevPos = m_Pos;

	m_CalculatedVel = false;
	m_TeamMask = GameServer()->GetPlayerChar(m_Owner) ? GameServer()->GetPlayerChar(m_Owner)->TeamMask() : Mask128();
}

void CStableProjectile::CalculateVel()
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
			Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			Curvature = GameServer()->Tuning()->m_GunCurvature;
			Speed = GameServer()->Tuning()->m_GunSpeed;
			break;
	}

	for (int i = 0; i < NUM_SNAPINFO; i++)
	{
		float Time = (Server()->Tick()-m_aSnap[i].m_LastResetTick)/(float)Server()->TickSpeed();
		m_aSnap[i].m_Vel.x = ((m_Pos.x - m_aSnap[i].m_LastResetPos.x)/Time/Speed) * 100;
		m_aSnap[i].m_Vel.y = ((m_Pos.y - m_aSnap[i].m_LastResetPos.y)/Time/Speed - Time*Speed*Curvature/10000) * 100;
	}

	m_CalculatedVel = true;
}

void CStableProjectile::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if (m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner])
	{
		Reset();
		return;
	}

	if (m_OnlyShowOwner && SnappingClient != m_Owner)
		return;

	if (!CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (m_HideOnSpec && pOwner && pOwner->IsPaused())
		return;

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(!pProj)
		return;

	if(!m_CalculatedVel)
		CalculateVel();

	int i = Server()->GetHighBandwidth(SnappingClient) ? SNAPINFO_HIGHBANDWIDTH : SNAPINFO_LOWBANDWIDTH;
	pProj->m_X = round_to_int(m_aSnap[i].m_LastResetPos.x);
	pProj->m_Y = round_to_int(m_aSnap[i].m_LastResetPos.y);
	pProj->m_VelX = m_aSnap[i].m_Vel.x;
	pProj->m_VelY = m_aSnap[i].m_Vel.y;
	if (Server()->IsSevendown(SnappingClient) && m_aSnap[i].m_Vel.y >= 0 && (m_aSnap[i].m_Vel.y & 512) != 0) // dont send PROJECTILEFLAG_IS_DDNET
		pProj->m_VelY = 0;
	pProj->m_StartTick = m_aSnap[i].m_LastResetTick;
	pProj->m_Type = m_Type;
}
