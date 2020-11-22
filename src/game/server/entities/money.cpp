// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "money.h"

CMoney::CMoney(CGameWorld *pGameWorld, vec2 Pos, int64 Amount, float Direction)
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_MONEY, Pos, MONEY_DROP_RADIUS*2)
{
	m_Pos = Pos;
	m_Amount = Amount;
	m_Vel = vec2(5*Direction, Direction == 0 ? 0 : -5);

	GameWorld()->InsertEntity(this);

	for (int i = 0; i < NUM_DOTS; i++)
		m_aID[i] = Server()->SnapNewID();
}

CMoney::~CMoney()
{
	for (int i = 0; i < NUM_DOTS; i++)
		Server()->SnapFreeID(m_aID[i]);
}

void CMoney::Tick()
{
	CAdvancedEntity::Tick();
	HandleDropped();

	CCharacter *pClosest = GameWorld()->ClosestCharacter(m_Pos, GetProximityRadius(), 0);
	if (pClosest)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Collected %lld money", m_Amount);
		GameServer()->SendChatTarget(pClosest->GetPlayer()->GetCID(), aBuf);
		pClosest->GetPlayer()->WalletTransaction(m_Amount, aBuf);
		Reset();
		return;
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

	float AngleStep = 2.0f * pi / NUM_DOTS;
	for(int i = 0; i < NUM_DOTS; i++)
	{
		vec2 Pos = m_Pos + vec2(MONEY_DROP_RADIUS * cos(AngleStep*i), MONEY_DROP_RADIUS * sin(AngleStep*i));
		
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
}
