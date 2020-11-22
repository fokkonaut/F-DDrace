// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "money.h"

CMoney::CMoney(CGameWorld *pGameWorld, vec2 Pos, int64 Amount, int Owner, float Direction)
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_MONEY, Pos, GetRadius(Amount)*2, Owner)
{
	m_Pos = Pos;
	m_Amount = Amount;
	m_Vel = vec2(5*Direction, Direction == 0 ? 0 : -5);

	m_MergePos = vec2(-1, -1);
	m_MergeTick = 0;

	m_StartTick = Server()->Tick();

	for (int i = 0; i < NUM_MONEY_IDS; i++)
		m_aID[i] = Server()->SnapNewID();

	GameWorld()->InsertEntity(this);
}

CMoney::~CMoney()
{
	for (int i = 0; i < NUM_MONEY_IDS; i++)
		Server()->SnapFreeID(m_aID[i]);
}

void CMoney::Tick()
{
	if (IsMarkedForDestroy())
		return;

	CAdvancedEntity::Tick();
	HandleDropped();

	// Remove small money drops after 10 minutes
	if (m_Amount <= SMALL_MONEY_AMOUNT && m_StartTick < Server()->Tick() - Server()->TickSpeed() * 60 * 10)
	{
		Reset();
		return;
	}

	CCharacter *pClosest = GameWorld()->ClosestCharacter(m_Pos, GetProximityRadius(), 0);
	// Owner can pick up the money after 2 seconds, everyone else immediately
	if (pClosest && (pClosest != m_pOwner || m_StartTick < Server()->Tick() - Server()->TickSpeed() * 2))
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Collected %lld money", m_Amount);
		GameServer()->SendChatTarget(pClosest->GetPlayer()->GetCID(), aBuf);
		pClosest->GetPlayer()->WalletTransaction(m_Amount, aBuf);

		str_format(aBuf, sizeof(aBuf), "+%lld", m_Amount);
		GameServer()->CreateLaserText(m_Pos, pClosest->GetPlayer()->GetCID(), aBuf);

		Reset();
		return;
	}

	CMoney *apEnts[16];
	int Num = GameWorld()->FindEntities(m_Pos, MERGE_RADIUS, (CEntity**)apEnts, 16, CGameWorld::ENTTYPE_MONEY);
	for (int i = 0; i < Num; i++)
	{
		if (apEnts[i] == this || apEnts[i]->IsMarkedForDestroy() || GameServer()->Collision()->IntersectLine(m_Pos, apEnts[i]->GetPos(), 0, 0))
			continue;

		m_Amount += apEnts[i]->m_Amount;
		m_MergePos = apEnts[i]->GetPos();
		m_MergeTick = Server()->Tick();
		apEnts[i]->Reset();
	}

	m_PrevPos = m_Pos;
}

void CMoney::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CNetObj_Projectile *pBullet = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(!pBullet)
		return;

	pBullet->m_X = (int)m_Pos.x;
	pBullet->m_Y = (int)m_Pos.y;
	pBullet->m_VelX = 0;
	pBullet->m_VelY = 0;
	pBullet->m_StartTick = 0;
	pBullet->m_Type = WEAPON_SHOTGUN;

	float AngleStep = 2.0f * pi / GetNumDots();
	for(int i = 0; i < GetNumDots(); i++)
	{
		vec2 Pos = m_Pos + vec2(GetRadius() * cos(AngleStep*i), GetRadius() * sin(AngleStep*i));
		
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_aID[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_X = (int)Pos.x;
		pObj->m_Y = (int)Pos.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = 0;
		pObj->m_Type = WEAPON_HAMMER;
	}

	if (m_MergeTick > Server()->Tick() - 5)
	{
		CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_aID[NUM_DOTS_BIG], sizeof(CNetObj_Laser)));
		if (!pObj)
			return;

		pObj->m_X = round_to_int(m_MergePos.x);
		pObj->m_Y = round_to_int(m_MergePos.y);
		pObj->m_FromX = round_to_int(m_Pos.x);
		pObj->m_FromY = round_to_int(m_Pos.y);
		pObj->m_StartTick = m_MergeTick;
	}
}
