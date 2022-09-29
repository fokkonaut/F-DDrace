#include <game/server/gamecontext.h>
#include "flyingpoint.h"

CFlyingPoint::CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel, vec2 ToPos, bool PortalBlocker)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLYINGPOINT, Pos)
{
	m_Pos = Pos;
	m_PrevPos = Pos;
	m_InitialVel = InitialVel;
	m_Owner = Owner;
	m_PortalBlocker = PortalBlocker;
	m_To = To;
	m_ToPos = ToPos;
	m_InitialAmount = 1.0f;
	GameWorld()->InsertEntity(this);
}

void CFlyingPoint::Reset()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_To);
	vec2 Pos = pChr ? pChr->GetPos() : m_Pos;
	int ID = pChr ? m_To : m_Owner;
	GameServer()->CreateDeath(Pos, ID);
	GameWorld()->DestroyEntity(this);
}

void CFlyingPoint::Tick()
{
	vec2 ToPos = m_ToPos;
	if (m_To != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_To);
		if (!pChr)
		{
			Reset();
			return;
		}

		ToPos = pChr->GetPos();
	}

	float Dist = distance(m_Pos, ToPos);
	if(Dist < 24.0f)
	{
		Reset();
		return;
	}

	vec2 Dir = normalize(ToPos - m_Pos);
	m_Pos += Dir*clamp(Dist, 0.0f, 16.0f) * (1.0f - m_InitialAmount) + m_InitialVel * m_InitialAmount;
	
	m_InitialAmount *= 0.98f;

	if (m_PortalBlocker && GameWorld()->IntersectLinePortalBlocker(m_Pos, m_PrevPos))
	{
		Reset();
		return;
	}

	m_PrevPos = m_Pos;
}

void CFlyingPoint::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if (pOwner && !CmaskIsSet(pOwner->TeamMask(), SnappingClient))
		return;

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