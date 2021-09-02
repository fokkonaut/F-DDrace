#include <game/server/gamecontext.h>
#include "flyingpoint.h"

CFlyingPoint::CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLYINGPOINT, Pos)
{
	m_Pos = Pos;
	m_InitialVel = InitialVel;
	m_To = To;
	m_Owner = Owner;
	m_InitialAmount = 1.0f;
	GameWorld()->InsertEntity(this);
}

void CFlyingPoint::Reset()
{
	int ID = GameServer()->GetPlayerChar(m_To) ? m_To : GameServer()->GetPlayerChar(m_Owner) ? m_Owner : -1;
	vec2 Pos = GameServer()->GetPlayerChar(ID) ? GameServer()->GetPlayerChar(ID)->GetPos() : m_Pos;
	GameServer()->CreateDeath(Pos, ID);
	GameWorld()->DestroyEntity(this);
}

void CFlyingPoint::Tick()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_To);
	if (!pChr)
	{
		Reset();
		return;
	}

	float Dist = distance(m_Pos, pChr->GetPos());
	if(Dist < 24.0f)
	{
		Reset();
		return;
	}

	vec2 Dir = normalize(pChr->GetPos() - m_Pos);
	m_Pos += Dir*clamp(Dist, 0.0f, 16.0f) * (1.0f - m_InitialAmount) + m_InitialVel * m_InitialAmount;
	
	m_InitialAmount *= 0.98f;
}

void CFlyingPoint::Snap(int SnappingClient)
{
	if (GameServer()->GetPlayerChar(SnappingClient))
	{
		if (GameServer()->GetPlayerChar(m_To) && !CmaskIsSet(GameServer()->GetPlayerChar(m_To)->TeamMask(), SnappingClient))
			return;
	}

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(pObj)
	{
		pObj->m_X = round_to_int(m_Pos.x);
		pObj->m_Y = round_to_int(m_Pos.y);
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick();
		pObj->m_Type = WEAPON_HAMMER;
	}
}