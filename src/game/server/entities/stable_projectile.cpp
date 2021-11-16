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
	m_LastResetPos = Pos;
	m_Owner = Owner;
	m_HideOnSpec = HideOnSpec;
	m_LastResetTick = Server()->Tick();
	m_CalculatedVel = false;
	m_OnlyShowOwner = OnlyShowOwner;

	GameWorld()->InsertEntity(this);
}

void CStableProjectile::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CStableProjectile::TickDefered()
{
	if(Server()->Tick()%4 == 1)
	{
		m_LastResetPos = m_Pos;
		m_LastResetTick = Server()->Tick();
	}
	m_CalculatedVel = false;
}

void CStableProjectile::CalculateVel()
{
	float Time = (Server()->Tick()-m_LastResetTick)/(float)Server()->TickSpeed();
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

	m_VelX = ((m_Pos.x - m_LastResetPos.x)/Time/Speed) * 100;
	m_VelY = ((m_Pos.y - m_LastResetPos.y)/Time/Speed - Time*Speed*Curvature/10000) * 100;

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

	CCharacter* pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	CCharacter* pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (pOwner && pSnapChar)
	{
		Mask128 TeamMask = pOwner->TeamMask();
		if (!CmaskIsSet(TeamMask, SnappingClient))
			return;
	}

	if (m_HideOnSpec && pOwner && pOwner->IsPaused())
		return;

	if (m_OnlyShowOwner && SnappingClient != m_Owner)
		return;

	if(!m_CalculatedVel)
		CalculateVel();

	CNetObj_DDNetProjectile DDNetProjectile;
	if(SnappingClient != -1 && GameServer()->GetClientDDNetVersion(SnappingClient) >= VERSION_DDNET_PROJECTILE && FillExtraInfo(&DDNetProjectile))
	{
		CNetObj_DDNetProjectile *pProj = static_cast<CNetObj_DDNetProjectile *>(Server()->SnapNewItem(NETOBJTYPE_DDNETPROJECTILE, GetID(), sizeof(CNetObj_DDNetProjectile)));
		if(!pProj)
			return;
		mem_copy(pProj, &DDNetProjectile, sizeof(DDNetProjectile));
	}
	else
	{
		CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
		if(!pProj)
			return;

		pProj->m_X = round_to_int(m_LastResetPos.x);
		pProj->m_Y = round_to_int(m_LastResetPos.y);
		pProj->m_VelX = m_VelX;
		pProj->m_VelY = m_VelY;
		pProj->m_StartTick = m_LastResetTick;
		pProj->m_Type = m_Type;
	}
}

bool CStableProjectile::FillExtraInfo(CNetObj_DDNetProjectile *pProj)
{
	const int MaxPos = 0x7fffffff / 100;
	if(abs((int)m_LastResetPos.y) + 1 >= MaxPos || abs((int)m_LastResetPos.x) + 1 >= MaxPos)
	{
		//If the modified data would be too large to fit in an integer, send normal data instead
		return false;
	}
	//Send additional/modified info, by modifiying the fields of the netobj
	float Angle = -atan2f(m_VelX/100.f, m_VelY/100.f);

	int Data = 0;
	Data |= (abs(m_Owner) & 255) << 0;
	if(m_Owner < 0)
		Data |= PROJECTILEFLAG_NO_OWNER;
	//This bit tells the client to use the extra info
	Data |= PROJECTILEFLAG_IS_DDNET;
	// PROJECTILEFLAG_BOUNCE_HORIZONTAL, PROJECTILEFLAG_BOUNCE_VERTICAL
	/*Data |= (m_Bouncing & 3) << 10;
	if(m_Explosive)
		Data |= PROJECTILEFLAG_EXPLOSIVE;
	if(m_Freeze)
		Data |= PROJECTILEFLAG_FREEZE;*/

	pProj->m_X = (int)(m_LastResetPos.x * 100.0f);
	pProj->m_Y = (int)(m_LastResetPos.y * 100.0f);
	pProj->m_Angle = (int)(Angle * 1000000.0f);
	pProj->m_Data = Data;
	pProj->m_StartTick = m_LastResetTick;
	pProj->m_Type = m_Type;
	return true;
}
